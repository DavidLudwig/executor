#include "localvolume.h"
#include <rsys/common.h>
#include <rsys/byteswap.h>
#include <FileMgr.h>
#include <MemoryMgr.h>
#include <rsys/file.h>
#include <rsys/hfs.h>
#include <map>
#include <iostream>
#include "item.h"

#include "plain.h"
#include "basilisk.h"
#include "mac.h"
#include "appledouble.h"

#include "simplecnidmapper.h"
#include "lmdbcnidmapper.h"
#include "itemcache.h"

using namespace Executor;


ItemPtr ExtensionItemFactory::createItemForDirEntry(ItemCache& itemcache, CNID parID, CNID cnid, const fs::directory_entry& e)
{
    if(fs::is_regular_file(e.path()))
    {
        return std::make_shared<PlainFileItem>(itemcache, parID, cnid, e.path());
    }
    return nullptr;
}

LocalVolume::LocalVolume(VCB& vcb, fs::path root)
    : Volume(vcb), root(root)
{
    itemCache = std::make_unique<ItemCache>(
            root,
            getVolumeName(),
            std::make_unique<SimpleCNIDMapper>(root, getVolumeName()),
            //std::make_unique<LMDBCNIDMapper>(root, getVolumeName()),
            static_cast<ItemFactory*>(this)
        );

    itemFactories.push_back(std::make_unique<DirectoryItemFactory>());
    itemFactories.push_back(std::make_unique<AppleSingleItemFactory>());
    //defaultItemFactory = itemFactories.back().get();
    itemFactories.push_back(std::make_unique<AppleDoubleItemFactory>());
    itemFactories.push_back(std::make_unique<BasiliskItemFactory>());
    defaultItemFactory = itemFactories.back().get();
#ifdef MACOSX
    itemFactories.push_back(std::make_unique<MacItemFactory>());
    defaultItemFactory = itemFactories.back().get();
#endif
    itemFactories.push_back(std::make_unique<ExtensionItemFactory>());
}

bool LocalVolume::isHidden(const fs::directory_entry& e)
{
    for(auto& itemFactory : itemFactories)
    {
        if(itemFactory->isHidden(e))
            return true;
    }
    return false;
}

ItemPtr LocalVolume::createItemForDirEntry(ItemCache& itemcache, CNID parID, CNID cnid, const fs::directory_entry& e)
{
    for(auto& itemFactory : itemFactories)
    {
        if(ItemPtr item = itemFactory->createItemForDirEntry(itemcache, parID, cnid, e))
            return item;
    }
    return {};
}

std::shared_ptr<DirectoryItem> LocalVolume::resolveDir(long dirID)
{
    if(auto dirItem = std::dynamic_pointer_cast<DirectoryItem>(itemCache->tryResolve(dirID)))
        return dirItem;
    else
        throw OSErrorException(dirNFErr); // not a directory
}

std::shared_ptr<DirectoryItem> LocalVolume::resolveDir(short vRef, long dirID)
{
    if(dirID)
        return resolveDir(dirID);
    else if(ISWDNUM(vRef))
    {
        auto wdp = WDNUMTOWDP(vRef);
        return resolveDir(CL(wdp->dirid));
    }
    else if(vRef == 0)
    {
        return resolveDir(CL(DefDirID));
    }
    else
    {
        // "poor man's search path": not implemented
        return resolveDir(2);
    }
}

ItemPtr LocalVolume::resolve(mac_string_view name, short vRef, long dirID)
{
    if(name.empty())
        return resolveDir(vRef, dirID);

    size_t colon = name.find(':');

        // special case: directory ID 1 is a funny thing...
    if(dirID == 1 && (colon == 0 || colon == mac_string_view::npos))
    {
        std::shared_ptr<DirectoryItem> dir = resolveDir(vRef, 2);

        size_t afterColon = colon == 0 ? 1 : 0;
        size_t rest = colon == 0 ? name.find(':', 1) : mac_string_view::npos;

        if(name.substr(afterColon, rest-afterColon) != getVolumeName())
        {
            if(rest != mac_string_view::npos)
                throw OSErrorException(dirNFErr);
            else
                throw OSErrorException(fnfErr);
        }

        if(rest == mac_string_view::npos)
            return dir;
        else
            return resolveRelative(dir, name.substr(rest));
    }
    else if(colon != mac_string_view::npos)
    {
        std::shared_ptr<DirectoryItem> dir = resolveDir(vRef, colon == 0 ? dirID : 2);
        return resolveRelative(dir, name.substr(colon));
    }
    else
    {
        std::shared_ptr<DirectoryItem> dir = resolveDir(vRef, dirID);
        itemCache->cacheDirectory(dir);
        if(auto file = dir->tryResolve(name))
            return file;
        else
            throw OSErrorException(fnfErr);
    }
}

ItemPtr LocalVolume::resolveRelative(const std::shared_ptr<DirectoryItem>& base, mac_string_view name)
{
    auto p = name.begin();

    if(p == name.end())
        return base;

    ++p;

    if(p == name.end())
        return base;

    auto colon = std::find(p, name.end(), ':');

    ItemPtr item;
    
    if(colon == p)
    {
        item = resolveDir(base->parID());
    }
    else
    {
        itemCache->cacheDirectory(base);
        item = base->tryResolve(mac_string_view(p, colon));
        if(!item)
        {
            if(colon == name.end())
                throw OSErrorException(fnfErr);
            else
                throw OSErrorException(dirNFErr);
        }
    }

    if(colon == name.end())
        return item;
    else if(auto dir = std::dynamic_pointer_cast<DirectoryItem>(item))
    {
        return resolveRelative(dir, mac_string_view(colon, name.end()));
    }
    else
        throw OSErrorException(fnfErr);
}

ItemPtr LocalVolume::resolveForInfo(mac_string_view name, short vRef, long dirID, short index, bool includeDirectories)
{
    if(index > 0)
    {
        auto dir = resolveDir(vRef, dirID);
        itemCache->cacheDirectory(dir);
        return dir->resolve(index, includeDirectories);
    }
    else if(index == 0 && !name.empty())
        return resolve(name, vRef, dirID);
    else
        return resolveDir(vRef, dirID);
}

mac_string LocalVolume::getVolumeName() const
{
    return mac_string(mac_string_view(vcb.vcbVN));
}


struct LocalVolume::FCBExtension
{
    filecontrolblock *fcb;
    short refNum;
    std::unique_ptr<OpenFile> access;
    std::shared_ptr<FileItem> file;

    FCBExtension() = default;
    FCBExtension(filecontrolblock *fcb, short refNum)
        : fcb(fcb), refNum(refNum)
    {}
};

LocalVolume::FCBExtension& LocalVolume::getFCBX(short refNum)
{
    if(refNum < 0 || refNum >= fcbExtensions.size())
        throw OSErrorException(paramErr);
    return fcbExtensions[refNum];
}

LocalVolume::FCBExtension& LocalVolume::openFCBX()
{
    filecontrolblock* fcb = ROMlib_getfreefcbp();
    short refNum = (char *)fcb - (char *)MR(LM(FCBSPtr));

    memset(fcb, 0, sizeof(*fcb));
    fcb->fcbVPtr = RM(&vcb);

    if(fcbExtensions.size() < refNum + 1)
        fcbExtensions.resize(refNum+1);
    fcbExtensions[refNum] = FCBExtension(fcb, refNum);

    return fcbExtensions[refNum];
}

void LocalVolume::openCommon(GUEST<short>& refNum, ItemPtr item, Fork fork, int8_t permission)
{
    if(auto fileItem = std::dynamic_pointer_cast<FileItem>(item))
    {
        short& writeAccessRef = fork == Fork::resource ? fileItem->resWriteAccessRefNum : fileItem->dataWriteAccessRefNum;

        if(permission != fsRdPerm)
        {
            if(writeAccessRef > 0)
            {
                refNum = CW(writeAccessRef);
                throw OSErrorException(opWrErr);
            }
        }

        auto access = fork == Fork::resource ? fileItem->openRF() : fileItem->open();
        FCBExtension& fcbx = openFCBX();
        fcbx.access = std::move(access);
        fcbx.fcb->fcbFlNum = CL(fileItem->cnid());
        fcbx.fcb->fcbMdRByt = 0;
        if(fork == Fork::resource)
            fcbx.fcb->fcbMdRByt |= RESOURCEBIT;
        if(permission != fsRdPerm)
            fcbx.fcb->fcbMdRByt |= WRITEBIT;
        fcbx.file = fileItem;
        refNum = CW(fcbx.refNum);

        if(permission != fsRdPerm)
        {
            writeAccessRef = fcbx.refNum;
        }

    }
    else
        throw OSErrorException(paramErr);    // TODO: what's the correct error?
}

void LocalVolume::PBHOpenDF(HParmBlkPtr pb)
{
    openCommon(pb->ioParam.ioRefNum, resolve(MR(pb->ioParam.ioNamePtr), CW(pb->ioParam.ioVRefNum), CL(pb->fileParam.ioDirID)), Fork::data, pb->ioParam.ioPermssn);
}
void LocalVolume::PBHOpenRF(HParmBlkPtr pb)
{
    openCommon(pb->ioParam.ioRefNum, resolve(MR(pb->ioParam.ioNamePtr), CW(pb->ioParam.ioVRefNum), CL(pb->fileParam.ioDirID)), Fork::resource, pb->ioParam.ioPermssn);
}

void LocalVolume::PBOpenDF(ParmBlkPtr pb)
{
    openCommon(pb->ioParam.ioRefNum, resolve(MR(pb->ioParam.ioNamePtr), CW(pb->ioParam.ioVRefNum), 0), Fork::data, pb->ioParam.ioPermssn);
}
void LocalVolume::PBOpenRF(ParmBlkPtr pb)
{
    openCommon(pb->ioParam.ioRefNum, resolve(MR(pb->ioParam.ioNamePtr), CW(pb->ioParam.ioVRefNum), 0), Fork::resource, pb->ioParam.ioPermssn);
}



void LocalVolume::getInfoCommon(CInfoPBPtr pb, InfoKind infoKind)
{
    ItemPtr item = resolveForInfo(MR(pb->hFileInfo.ioNamePtr),
        CW(pb->hFileInfo.ioVRefNum), infoKind == InfoKind::FInfo ? 0 : CL(pb->hFileInfo.ioDirID), CW(pb->hFileInfo.ioFDirIndex),
        infoKind == InfoKind::CatInfo);

    if(CW(pb->hFileInfo.ioFDirIndex) != 0)
    {
        if(StringPtr outputName = MR(pb->hFileInfo.ioNamePtr))
        {
            const mac_string name = item->name();
            size_t n = std::min(name.size(), (size_t)255);
            memcpy(outputName+1, name.data(), n);
            outputName[0] = n;
        }
    }

    pb->hFileInfo.ioDirID = CL(item->cnid());   // a.k.a. pb->dirInfo.ioDrDirID

    if(infoKind == InfoKind::CatInfo)
    {
        pb->hFileInfo.ioFlParID = CL(item->parID());    // a.k.a. pb->dirInfo.ioDrParID
    }

    if(DirectoryItemPtr dirItem = std::dynamic_pointer_cast<DirectoryItem>(item))
    {
        if(infoKind != InfoKind::CatInfo)
            throw OSErrorException(fnfErr);

        pb->dirInfo.ioFlAttrib = ATTRIB_ISADIR;
        
        itemCache->cacheDirectory(dirItem);
        pb->dirInfo.ioDrNmFls = CW(dirItem->countItems());

        // TODO:
        // ioACUser
        // ioDrUserWds
        // ioDrCrDat
        // ioDrMdDat
        // ioDrBkDat
        // ioDrFndrInfo

        pb->dirInfo.ioACUser = 0;

    }
    else if(FileItem *fileItem = dynamic_cast<FileItem*>(item.get()))
    {
        pb->hFileInfo.ioFlAttrib = open_attrib_bits(item->cnid(), &vcb, &pb->hFileInfo.ioFRefNum);
        pb->hFileInfo.ioFlFndrInfo = fileItem->getFInfo();
        
        size_t dsize = fileItem->open()->getEOF();
        size_t rsize = fileItem->openRF()->getEOF();
        
        pb->hFileInfo.ioFlLgLen = CL(dsize);
        pb->hFileInfo.ioFlPyLen = CL(dsize);
        pb->hFileInfo.ioFlRLgLen = CL(rsize);
        pb->hFileInfo.ioFlRLgLen = CL(rsize);

        // TODO:
        // ioFlCrDat
        // ioFlMdDat

        if(infoKind == InfoKind::CatInfo)
        {
            // ioFlBkDat
            // ioFlXFndrInfo

            // ioFlClpSiz
        }
    }
    else
        std::abort();
}

void LocalVolume::PBGetCatInfo(CInfoPBPtr pb)
{
    getInfoCommon(pb, InfoKind::CatInfo);
}
void LocalVolume::PBHGetFInfo(HParmBlkPtr pb)
{
    getInfoCommon(reinterpret_cast<CInfoPBPtr>(pb), InfoKind::HFInfo);
}
void LocalVolume::PBGetFInfo(ParmBlkPtr pb)
{
    getInfoCommon(reinterpret_cast<CInfoPBPtr>(pb), InfoKind::FInfo);
}

void LocalVolume::setFInfoCommon(Item& item, ParmBlkPtr pb)
{
    if(FileItem *fitem = dynamic_cast<FileItem*>(&item))
    {
        fitem->setFInfo(pb->fileParam.ioFlFndrInfo);
    }
    else
        throw OSErrorException(paramErr);   // TODO: item is a directory
    
    // pb->fileParam.ioFlCrDat      TODO
    // pb->fileParam.ioFlMdDat      TODO
}

void LocalVolume::PBSetCatInfo(CInfoPBPtr pb)
{
    setFInfoCommon(*resolve(MR(pb->hFileInfo.ioNamePtr), CW(pb->hFileInfo.ioVRefNum), CL(pb->hFileInfo.ioDirID)),
        reinterpret_cast<ParmBlkPtr>(pb));
}
void LocalVolume::PBSetFInfo(ParmBlkPtr pb)
{
    setFInfoCommon(*resolve(MR(pb->fileParam.ioNamePtr), CW(pb->fileParam.ioVRefNum), 0), pb);
}
void LocalVolume::PBHSetFInfo(HParmBlkPtr pb)
{
    setFInfoCommon(*resolve(MR(pb->fileParam.ioNamePtr), CW(pb->fileParam.ioVRefNum), CL(pb->fileParam.ioDirID)),
        reinterpret_cast<ParmBlkPtr>(pb));
}


LocalVolume::NonexistentFile LocalVolume::resolveForCreate(mac_string_view name, short vRefNum, CNID dirID)
{
    std::shared_ptr<DirectoryItem> parent;
    auto colon = name.rfind(':');

    if(colon == mac_string_view::npos)
        parent = resolveDir(vRefNum, dirID);
    else
    {
        parent = std::dynamic_pointer_cast<DirectoryItem>(resolve(mac_string_view(name.begin(), colon), vRefNum, dirID));
        if(!parent)
            throw OSErrorException(dirNFErr);
        name = mac_string_view(name.begin() + colon+1, name.end());
    }

    itemCache->cacheDirectory(parent);
    if(parent->tryResolve(name))
        throw OSErrorException(dupFNErr);
    
    return NonexistentFile { parent, name };
}

void LocalVolume::createCommon(NonexistentFile file)
{
    fs::path parentPath = file.parent->path();
    defaultItemFactory->createFile(parentPath, file.name);
    itemCache->flushDirectoryCache(file.parent);
}
void LocalVolume::PBCreate(ParmBlkPtr pb)
{
    createCommon(resolveForCreate(MR(pb->ioParam.ioNamePtr), CW(pb->fileParam.ioVRefNum), 0));
}
void LocalVolume::PBHCreate(HParmBlkPtr pb)
{
    createCommon(resolveForCreate(MR(pb->ioParam.ioNamePtr), CW(pb->fileParam.ioVRefNum), CL(pb->fileParam.ioDirID)));
}

void LocalVolume::PBDirCreate(HParmBlkPtr pb)
{
    auto [parent, name] = resolveForCreate(MR(pb->ioParam.ioNamePtr), CW(pb->fileParam.ioVRefNum), CL(pb->fileParam.ioDirID));
    fs::path fn = toUnicodeFilename(name);
    fs::create_directory(parent->path() / fn);

    itemCache->flushDirectoryCache(parent);

    itemCache->cacheDirectory(parent);
    auto item = parent->tryResolve(name);
    if(!item)
        throw OSErrorException(ioErr);
    pb->fileParam.ioDirID = CL(item->cnid());
}


void LocalVolume::deleteCommon(ItemPtr item)
{
    itemCache->deleteItem(item);
}

void LocalVolume::PBDelete(ParmBlkPtr pb)
{
    deleteCommon(resolve(MR(pb->ioParam.ioNamePtr), CW(pb->ioParam.ioVRefNum), 0));
}
void LocalVolume::PBHDelete(HParmBlkPtr pb)
{
    deleteCommon(resolve(MR(pb->ioParam.ioNamePtr), CW(pb->ioParam.ioVRefNum), CL(pb->fileParam.ioDirID)));
}

void LocalVolume::renameCommon(ItemPtr item, mac_string_view newName)
{
    if(newName.find(':') != mac_string_view::npos)
        throw OSErrorException(bdNamErr);

    auto parent = resolveDir(item->parID());
    itemCache->cacheDirectory(parent);
    if(parent->tryResolve(newName))
        throw OSErrorException(dupFNErr);
    
    itemCache->renameItem(item, newName);
}


void LocalVolume::PBRename(ParmBlkPtr pb)
{
    renameCommon(resolve(MR(pb->ioParam.ioNamePtr), CW(pb->ioParam.ioVRefNum), 0), 
                MR(guest_cast<StringPtr>(pb->ioParam.ioMisc)));
}

void LocalVolume::PBHRename(HParmBlkPtr pb)
{
    renameCommon(resolve(MR(pb->ioParam.ioNamePtr), CW(pb->ioParam.ioVRefNum), CL(pb->fileParam.ioDirID)), 
                MR(guest_cast<StringPtr>(pb->ioParam.ioMisc)));
}

void LocalVolume::PBOpenWD(WDPBPtr pb)
{
    ItemPtr item = resolve(MR(pb->ioNamePtr), CW(pb->ioVRefNum), CL(pb->ioWDDirID));
    std::cout << "PBOpenWD: " << item->path() << std::endl;

    long dirID;
    if(DirectoryItem *dirItem = dynamic_cast<DirectoryItem*>(item.get()))
        dirID = dirItem->dirID();
    else
        dirID = item->parID();

    OSErr err =  ROMlib_mkwd(pb, &vcb, dirID, Cx(pb->ioWDProcID));
    if(err)
        throw OSErrorException(err);
}

void LocalVolume::PBRead(ParmBlkPtr pb)
{
    pb->ioParam.ioActCount = 0;
    PBSetFPos(pb);
    auto& fcbx = getFCBX(CW(pb->ioParam.ioRefNum));
    size_t req = CL(pb->ioParam.ioReqCount);
    size_t act = fcbx.access->read(CL(fcbx.fcb->fcbCrPs), MR(pb->ioParam.ioBuffer), req);
    pb->ioParam.ioActCount = CL(act);
    pb->ioParam.ioPosOffset = fcbx.fcb->fcbCrPs = CL( CL(fcbx.fcb->fcbCrPs) + act );

    if(act < req)
        throw OSErrorException(eofErr);
}
void LocalVolume::PBWrite(ParmBlkPtr pb)
{
    pb->ioParam.ioActCount = 0;
    setFPosCommon(pb, false);
    auto& fcbx = getFCBX(CW(pb->ioParam.ioRefNum));
    size_t n = fcbx.access->write(CL(fcbx.fcb->fcbCrPs), MR(pb->ioParam.ioBuffer), CL(pb->ioParam.ioReqCount));
    pb->ioParam.ioActCount = CL(n);
    pb->ioParam.ioPosOffset = fcbx.fcb->fcbCrPs = CL( CL(fcbx.fcb->fcbCrPs) + n );
}
void LocalVolume::PBClose(ParmBlkPtr pb)
{
    short refNum = CW(pb->ioParam.ioRefNum);
    auto& fcbx = getFCBX(refNum);
    fcbx.fcb->fcbFlNum = 0;

    if(fcbx.file->resWriteAccessRefNum == refNum)
        fcbx.file->resWriteAccessRefNum = -1;
    if(fcbx.file->dataWriteAccessRefNum == refNum)
        fcbx.file->dataWriteAccessRefNum = -1;

    fcbx = FCBExtension();
}

void LocalVolume::PBGetFPos(ParmBlkPtr pb)
{
    auto& fcbx = getFCBX(CW(pb->ioParam.ioRefNum));
    pb->ioParam.ioReqCount = CL(0);
    pb->ioParam.ioActCount = CL(0);
    pb->ioParam.ioPosMode = CW(0);
    pb->ioParam.ioPosOffset = fcbx.fcb->fcbCrPs;
}

void LocalVolume::setFPosCommon(ParmBlkPtr pb, bool checkEOF)
{
    auto& fcbx = getFCBX(CW(pb->ioParam.ioRefNum));
    ssize_t eof = (ssize_t) fcbx.access->getEOF();
    ssize_t newPos = CL(fcbx.fcb->fcbCrPs);
    
    switch(CW(pb->ioParam.ioPosMode))
    {
        case fsAtMark:
            break;
        case fsFromStart:
            newPos = CL(pb->ioParam.ioPosOffset);
            break;
        case fsFromLEOF:
            newPos = eof + CL(pb->ioParam.ioPosOffset);
            break;
        case fsFromMark:
            newPos = CL(fcbx.fcb->fcbCrPs) + CL(pb->ioParam.ioPosOffset);
            break;
    }

    OSErr err = noErr;
    if(newPos > eof && checkEOF)
    {
        newPos = eof;
        err = eofErr;
    }
    
    if(newPos < 0)
    {
        newPos = CL(fcbx.fcb->fcbCrPs);     // MacOS 9 behavior.
                                            // System 6 actually sets fcbCrPs to a negative value.
        err = posErr;
    }

    pb->ioParam.ioPosOffset = fcbx.fcb->fcbCrPs = CL(newPos);

    if(err)
        throw OSErrorException(err);
}
void LocalVolume::PBSetFPos(ParmBlkPtr pb)
{
    setFPosCommon(pb, true);
}
void LocalVolume::PBGetEOF(ParmBlkPtr pb)
{
    auto& fcbx = getFCBX(CW(pb->ioParam.ioRefNum));
    size_t n = fcbx.access->getEOF();
    pb->ioParam.ioMisc = CL(n);
}

void LocalVolume::PBSetEOF(ParmBlkPtr pb)
{
    auto& fcbx = getFCBX(CW(pb->ioParam.ioRefNum));
    fcbx.access->setEOF(CL(pb->ioParam.ioMisc));
}

void LocalVolume::PBCatMove(CMovePBPtr pb)
{
    ItemPtr item = resolve(MR(pb->ioNamePtr), CW(pb->ioVRefNum), CL(pb->ioDirID));
    auto newParent = std::dynamic_pointer_cast<DirectoryItem>(resolve(MR(pb->ioNewName), CW(pb->ioVRefNum), CL(pb->ioNewDirID)));

    if(!newParent)
        throw OSErrorException(dirNFErr);   // check this?

    auto name = item->name();

    itemCache->cacheDirectory(newParent);
    if(newParent->tryResolve(mac_string_view(name.data(), name.size())))
        throw OSErrorException(dupFNErr);

    itemCache->moveItem(item, newParent);
}

void LocalVolume::PBFlushFile(ParmBlkPtr pb)
{
}
void LocalVolume::PBFlushVol(ParmBlkPtr pb)
{
}

void LocalVolume::PBAllocate(ParmBlkPtr pb)
{
    // no-op... emulated programs will have a hard time noticing that we're not actually pre-allocating blocks
    // just pretend that everything worked.
    pb->ioParam.ioActCount = CL((CL(pb->ioParam.ioReqCount) + 511) & ~511);
}
void LocalVolume::PBAllocContig(ParmBlkPtr pb)
{
    // no-op, like PBALlocate above
    pb->ioParam.ioActCount = CL((CL(pb->ioParam.ioReqCount) + 511) & ~511);
}
void LocalVolume::PBLockRange(ParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBUnlockRange(ParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBSetFLock(ParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBHSetFLock(HParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBRstFLock(ParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBHRstFLock(HParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBSetFVers(ParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}

void LocalVolume::PBSetVInfo(HParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBUnmountVol(ParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBEject(ParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}
void LocalVolume::PBOffLine(ParmBlkPtr pb)
{
    throw OSErrorException(paramErr);
}

std::optional<FSSpec> LocalVolume::nativePathToFSSpec(const fs::path& inPath)
{
    boost::system::error_code ec;

    if(!fs::exists(inPath))
        return std::nullopt;

    auto path = fs::canonical(inPath, ec);
    if(ec)
        return {};
    ItemPtr item = itemCache->tryResolve(path);

    if(item)
    {
        FSSpec spec;
        const auto& name = item->name();
        spec.vRefNum = vcb.vcbVRefNum;
        spec.parID = CL(item->parID());
        memcpy(&spec.name[1], name.c_str(), name.size());
        spec.name[0] = name.size();
        return spec;
    }
    else
        return std::nullopt;
}

void Executor::initLocalVol()
{
    VCBExtra *vp;
    GUEST<THz> savezone;

    savezone = LM(TheZone);
    LM(TheZone) = LM(SysZone);
    vp = (VCBExtra *)NewPtr(sizeof(VCBExtra));
    LM(TheZone) = savezone;

    if(!vp)
        return;
    memset(vp, 0, sizeof(VCBExtra));
    vp->vcb.vcbDrvNum = CWC(42);//pb->ioParam.ioVRefNum;

    --ROMlib_nextvrn;
    vp->vcb.vcbVRefNum = CW(ROMlib_nextvrn);

    
    strcpy((char *)vp->vcb.vcbVN + 1, "vol");
    vp->vcb.vcbVN[0] = strlen((char *)vp->vcb.vcbVN+1);

    vp->vcb.vcbSigWord = CWC(0x4244); /* IMIV-188 */
    vp->vcb.vcbFreeBks = CWC(20480); /* arbitrary */
    vp->vcb.vcbCrDate = 0; /* I'm lying here */
    vp->vcb.vcbVolBkUp = 0;
    vp->vcb.vcbAtrb = CWC(VNONEJECTABLEBIT);
    vp->vcb.vcbNmFls = CWC(100);
    vp->vcb.vcbNmAlBlks = CWC(300);
    vp->vcb.vcbAlBlkSiz = CLC(512);
    vp->vcb.vcbClpSiz = CLC(1);
    vp->vcb.vcbAlBlSt = CWC(10);
    vp->vcb.vcbNxtCNID = CLC(1000);
    if(!LM(DefVCBPtr))
    {
        LM(DefVCBPtr) = RM((VCB *)vp);
        LM(DefVRefNum) = vp->vcb.vcbVRefNum;
        DefDirID = CLC(2); /* top level */
    }
    Enqueue((QElemPtr)vp, &LM(VCBQHdr));

    vp->volume = new LocalVolume(vp->vcb, "/");
}

std::optional<FSSpec> Executor::nativePathToFSSpec(const fs::path& p)
{
    VCBExtra *vcbp;

    for(vcbp = (VCBExtra *)MR(LM(VCBQHdr).qHead); vcbp; vcbp = (VCBExtra *)MR(vcbp->vcb.qLink))
    {
        if(auto volume = dynamic_cast<LocalVolume*>(vcbp->volume))
        {
            if(auto spec = volume->nativePathToFSSpec(p))
                return spec;
        }
    }

    return std::nullopt;
}
