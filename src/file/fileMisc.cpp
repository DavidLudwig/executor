/* Copyright 1986-1997 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

/* Forward declarations in FileMgr.h (DO NOT DELETE THIS LINE) */

#include <base/common.h>
#include <QuickDraw.h>
#include <FileMgr.h>
#include <OSEvent.h>
#include <VRetraceMgr.h>
#include <OSUtil.h>
#include <MemoryMgr.h>
#include <StdFilePkg.h>

#include <hfs/hfs.h>
#include <file/file.h>
#include <hfs/futzwithdosdisks.h>
#include <print/ini.h>
#include <rsys/string.h>
#include <rsys/segment.h>
#include <rsys/paths.h>
#include <rsys/macstrings.h>

#if !defined(WIN32)
#include <pwd.h>
#else
#include "winfs.h"
//#include "dosdisk.h"
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) ((m) & S_IFDIR)
#endif

#include <ctype.h>
#include <algorithm>

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(resources);

using namespace Executor;

namespace Executor
{
int ROMlib_nosync = 0; /* if non-zero, we don't call sync () or fsync () */
fs::path ROMlib_startdir;
std::string ROMlib_appname;
}


#if !defined(NDEBUG)
void Executor::fs_err_hook(OSErr err)
{
}
#endif

int ROMlib_lasterrnomapped;

#define MAX_ERRNO 50

#define install_errno(uerr, merr)         \
    do                                    \
    {                                     \
        gui_assert(uerr < NELEM(xtable)); \
        xtable[uerr] = merr;              \
    } while(false);

OSErr Executor::ROMlib_maperrno() /* INTERNAL */
{
    OSErr retval;
    static OSErr xtable[MAX_ERRNO + 1];
    static char been_here = false;
    int errno_save;

    if(!been_here)
    {
        int i;

        for(i = 0; i < (int)NELEM(xtable); ++i)
            xtable[i] = fsDSIntErr;

        install_errno(0, noErr);
        install_errno(EPERM, permErr);
        install_errno(ENOENT, fnfErr);
        install_errno(EIO, ioErr);
        install_errno(ENXIO, paramErr);
        install_errno(EBADF, fnOpnErr);
        install_errno(EAGAIN, fLckdErr);
        install_errno(ENOMEM, memFullErr);
        install_errno(EACCES, permErr);
        install_errno(EFAULT, paramErr);
        install_errno(EBUSY, fBsyErr);
        install_errno(EEXIST, dupFNErr);
        install_errno(EXDEV, fsRnErr);
        install_errno(ENODEV, nsvErr);
        install_errno(ENOTDIR, dirNFErr);
        install_errno(EINVAL, paramErr);
        install_errno(ENFILE, tmfoErr);
        install_errno(EMFILE, tmfoErr);
        install_errno(EFBIG, dskFulErr);
        install_errno(ENOSPC, dskFulErr);
        install_errno(ESPIPE, posErr);
        install_errno(EROFS, wPrErr);
        install_errno(EMLINK, dirFulErr);
#if !defined(WIN32)
        install_errno(ETXTBSY, fBsyErr);
        install_errno(EWOULDBLOCK, permErr);
#endif

        been_here = true;
    }

    errno_save = errno;
    ROMlib_lasterrnomapped = errno_save;

    if(errno_save < 0 || errno_save >= (int)NELEM(xtable))
        retval = fsDSIntErr;
    else
        retval = xtable[errno_save];

    if(retval == fsDSIntErr)
        warning_unexpected("fsDSIntErr errno = %d", errno_save);

    if(retval == dirNFErr)
        warning_trace_info("dirNFErr errno = %d", errno_save);

    fs_err_hook(retval);
    return retval;
}


Byte
Executor::open_attrib_bits(LONGINT file_id, VCB *vcbp, GUEST<INTEGER> *refnump)
{
    Byte retval;
    int i;

    retval = 0;
    *refnump = 0;
    for(i = 0; i < NFCB; i++)
    {
        if(ROMlib_fcblocks[i].fdfnum == file_id
           && ROMlib_fcblocks[i].fcvptr == vcbp)
        {
            if(*refnump == 0)
                *refnump = i * 94 + 2;
            if(ROMlib_fcblocks[i].fcflags & fcfisres)
                retval |= ATTRIB_RESOPEN;
            else
                retval |= ATTRIB_DATAOPEN;
        }
    }
    if(retval & (ATTRIB_RESOPEN | ATTRIB_DATAOPEN))
        retval |= ATTRIB_ISOPEN;
    return retval;
}

/* NOTE:  calling most of the routines here is a sign that the user may
	  be depending on the internal layout of things a bit too much */

void Executor::C_FInitQueue() /* IMIV-128 */
{
}

QHdrPtr Executor::C_GetFSQHdr() /* IMIV-175 */
{
    return (&LM(FSQHdr)); /* in UNIX domain, everything is synchronous */
}

QHdrPtr Executor::C_GetVCBQHdr() /* IMIV-178 */
{
    return (&LM(VCBQHdr));
}

QHdrPtr Executor::C_GetDrvQHdr() /* IMIV-182 */
{
    return (&LM(DrvQHdr));
}

/*
 * NOTE:  This is *the* PCBGetFCBInfo that we use whether
 *	  we're looking at hfs or ufs stuff.
 */

OSErr Executor::PBGetFCBInfo(FCBPBPtr pb, BOOLEAN async)
{
    filecontrolblock *fcbp, *efcbp;
    INTEGER i;

    if((i = pb->ioFCBIndx) > 0)
    {
        fcbp = (filecontrolblock *)(LM(FCBSPtr) + sizeof(INTEGER));
        efcbp = (filecontrolblock *)(LM(FCBSPtr) + *(GUEST<INTEGER> *)LM(FCBSPtr));
        if(pb->ioVRefNum < 0)
        {
            for(; fcbp != efcbp; fcbp++)
                if(fcbp->fcbFlNum && fcbp->fcbVPtr->vcbVRefNum == pb->ioVRefNum && --i <= 0)
                    break;
        }
        else if(pb->ioVRefNum == 0)
        {
            for(; fcbp != efcbp && (fcbp->fcbFlNum == 0 || --i > 0); fcbp++)
                ;
        }
        else /* if (pb->ioVRefNum > 0 */
        {
            for(; fcbp != efcbp; fcbp++)
                if(fcbp->fcbFlNum && fcbp->fcbVPtr->vcbDrvNum == pb->ioVRefNum && --i <= 0)
                    break;
        }
        if(fcbp == efcbp)
            PBRETURN(pb, fnOpnErr);
        pb->ioRefNum = (char *)fcbp - (char *)LM(FCBSPtr);
    }
    else
    {
        fcbp = ROMlib_refnumtofcbp(pb->ioRefNum);
        if(!fcbp)
            PBRETURN(pb, rfNumErr);
    }
    if(pb->ioNamePtr)
        str255assign(pb->ioNamePtr, fcbp->fcbCName);
    pb->ioFCBFlNm = fcbp->fcbFlNum;
    pb->ioFCBFlags = (fcbp->fcbMdRByt << 8) | (unsigned char)fcbp->fcbTypByt;
    pb->ioFCBStBlk = fcbp->fcbSBlk;
    pb->ioFCBEOF = fcbp->fcbEOF;
    pb->ioFCBCrPs = fcbp->fcbCrPs; 
    pb->ioFCBPLen = fcbp->fcbPLen;
    pb->ioFCBVRefNum = fcbp->fcbVPtr->vcbVRefNum;
    if(pb->ioFCBIndx <= 0 || pb->ioVRefNum == 0)
        pb->ioVRefNum = pb->ioFCBVRefNum;
    pb->ioFCBClpSiz = fcbp->fcbClmpSize;
    pb->ioFCBParID = fcbp->fcbDirID;
    PBRETURN(pb, noErr);
}

std::string
Executor::expandPath(std::string name)
{
    if(name.empty())
        return "";

    switch(name[0])
    {
        case '+':
            name = ROMlib_startdir.string() + name.substr(1);
            break;
        case '~':
        {
            auto home = getenv("HOME");
            if(home)
            {
                name = home + name.substr(1);
                break;
            }
        }
        break;
    }

#if defined(WIN32)
    std::replace(name.begin(), name.end(), '/', '\\');
#endif

    return name;
}

StringPtr Executor::ROMlib_exefname;

std::string Executor::ROMlib_ConfigurationFolder;
static std::string ROMlib_SystemFolder;
fs::path Executor::ROMlib_DirectoryMap;
static std::string ROMlib_MacVolumes;
std::string Executor::ROMlib_ScreenDumpFile;
static std::string ROMlib_OffsetFile;

LONGINT Executor::ROMlib_magic_offset = -1;

static void
skip_comments(FILE *fp)
{
    int c;

    while((c = getc(fp)) == '#')
    {
        while(c != '\n')
            c = getc(fp);
    }
    ungetc(c, fp);
}

static void
parse_offset_file(void)
{
    FILE *fp;

    fp = Ufopen(ROMlib_OffsetFile.c_str(), "r");
    if(!fp)
    {
#if 0
      warning_unexpected ("Couldn't open \"%s\"", ROMlib_OffsetFile);
#endif
    }
    else
    {
        int n_found;

        skip_comments(fp);
        n_found = fscanf(fp, "0x%08x", &ROMlib_magic_offset);
        if(n_found != 1)
            warning_unexpected("n_found = %d", n_found);
        fclose(fp);
    }
}

static bool
is_unix_path(const char *pathname)
{
    bool retval;

#if defined(MSDOS) || defined(CYGWIN32)
    if(pathname[0] && pathname[1] == ':' && (pathname[2] == '/' || pathname[2] == '\\'))
        pathname += 3;
#endif
    retval = strchr(pathname, ':') == 0;
    return retval;
}

std::optional<FSSpec> Executor::cmdlinePathToFSSpec(const std::string& path)
{
    if(is_unix_path(path.c_str()))
    {
        if(auto native = nativePathToFSSpec(path))
            return native;
    }

    VCBExtra *vcbp;

    std::string swappedString = path;
    for(char& c : swappedString)
    {
        if(c == ':')
            c = '/';
        else if(c == '/')
            c = ':';
    }

    std::vector<mac_string> elements;
    
    for(auto sep = swappedString.begin();
        sep != swappedString.end();
        )
    {            
        auto p = sep;
        sep = std::find(p, swappedString.end(), '/');

        if(p != sep)
            elements.push_back(toMacRomanFilename(std::string(p, sep)));

        if(sep != swappedString.end())
            ++sep;
    }

    if(elements.empty())
        return {};

    for(vcbp = (VCBExtra *)LM(VCBQHdr).qHead; vcbp; vcbp = (VCBExtra *)vcbp->vcb.qLink)
    {
        if(mac_string_view(vcbp->vcb.vcbVN) == elements[0])
        {
            long dirID = 2;
            
            for(int i = 1; i < elements.size() -1; i++)
            {
                CInfoPBRec pb;
                pb.dirInfo.ioCompletion = nullptr;
                pb.dirInfo.ioVRefNum = vcbp->vcb.vcbVRefNum;
                pb.dirInfo.ioDrDirID = dirID;
                pb.dirInfo.ioFDirIndex = 0;

                unsigned char strbuf[64];
                std::copy(elements[i].begin(), elements[i].end(), strbuf+1);
                strbuf[0] = elements[i].size();
                
                pb.dirInfo.ioNamePtr = strbuf;
                OSErr err = PBGetCatInfoSync(&pb);
                if(err != noErr)
                    return {};

                dirID = pb.dirInfo.ioDrDirID;
            }

            FSSpec spec;
            spec.vRefNum = vcbp->vcb.vcbVRefNum;
            spec.parID = dirID;

            if(elements.size() >= 2)
            {
                std::copy(elements.back().begin(), elements.back().end(), spec.name+1);
                spec.name[0] = elements.back().size();
            }
            else
                spec.name[0] = 0;
            return spec;
        }
    }
    return {};
}

static void MountMacVolumes(std::string macVolumes)
{
    GUEST<LONGINT> m;
    char *p, *ep;
    struct stat sbuf;

    m = 0;
    p = (char *)alloca(macVolumes.size() + 1);
    strcpy(p, macVolumes.c_str());
    while(p && *p)
    {
        ep = strchr(p, ';');
        if(ep)
            *ep = 0;
        std::string newp = expandPath(p);
        if(Ustat(newp.c_str(), &sbuf) == 0)
        {
            if(!S_ISDIR(sbuf.st_mode))
                ROMlib_openharddisk(newp.c_str(), &m);
            else
            {
                DIR *dirp;

                dirp = Uopendir(newp.c_str());
                if(dirp)
                {
#if defined(USE_STRUCT_DIRECT)
                    struct direct *direntp;
#else
                    struct dirent *direntp;
#endif

                    while((direntp = readdir(dirp)))
                    {
                        int namelen;

                        namelen = strlen(direntp->d_name);
                        if(namelen >= 4 && (strcasecmp(direntp->d_name + namelen - 4, ".hfv")
                                                == 0
                                            || strcasecmp(direntp->d_name + namelen - 4, ".ima")
                                                == 0))
                        {
                            ROMlib_openharddisk((newp + "/" + direntp->d_name).c_str(), &m);
                        }
                    }
                    closedir(dirp);
                }
            }
        }
        if(ep)
        {
            *ep = ';';
            p = ep + 1;
        }
        else
            p = 0;
    }

    futzwithdosdisks();
}

void Executor::InitSystemFolder(std::string systemFolder)
{
    CInfoPBRec cpb;

    unsigned char nameBuf[64];
    if(auto sysFolderSpec = cmdlinePathToFSSpec(systemFolder))
    {
        mac_string systemFilePath = 
            mac_string((unsigned char*)":") + 
            mac_string(mac_string_view(sysFolderSpec->name)) + 
            (unsigned char*)":" + 
            mac_string(mac_string_view((unsigned char*)SYSMACNAME));
        cpb.hFileInfo.ioNamePtr = assignPString(nameBuf, systemFilePath, 63);
        cpb.hFileInfo.ioVRefNum = sysFolderSpec->vRefNum;
        cpb.hFileInfo.ioDirID = sysFolderSpec->parID;            
    }
    else
    {
        fprintf(stderr, "Couldn't find '%s'\n", systemFolder.c_str());
        exit(1);
    }
    
    cpb.hFileInfo.ioFDirIndex = 0;
    if(PBGetCatInfo(&cpb, false) == noErr)
    {
        WDPBRec wpb;
        wpb.ioNamePtr = 0;
        wpb.ioVRefNum = cpb.hFileInfo.ioVRefNum;
        wpb.ioWDProcID = TICK("unix");
        wpb.ioWDDirID = cpb.hFileInfo.ioFlParID;
        if(PBOpenWD(&wpb, false) == noErr)
            LM(BootDrive) = wpb.ioVRefNum;
    }
    else
    {
        fprintf(stderr, "Couldn't open System: '%s'\n", ROMlib_SystemFolder.c_str());
        exit(1);
    }
}

void Executor::InitPaths()
{
    auto initpath = [](const char *varname, const char *defval) {
        if(auto v = getenv(varname))
            return expandPath(v);
        else
            return expandPath(defval);
    };

    ROMlib_ConfigurationFolder = initpath("Configuration", "~/.executor/Configuration");
    ROMlib_SystemFolder = initpath("SystemFolder", "~/.executor/System Folder");
    ROMlib_DirectoryMap = initpath("ExecutorDirectoryMap", "~/.executor/cnidmap");
    ROMlib_MacVolumes = initpath("MacVolumes", "~/.executor/images");
    ROMlib_ScreenDumpFile = initpath("ScreenDumpFile", "/tmp/excscrn*.tif");
    ROMlib_OffsetFile = initpath("OffsetFile", "~/.executor/offset_file");
    ROMlib_PrintersIni = initpath("PrintersIni", "~/.executor/printers.ini");
    ROMlib_PrintDef = initpath("PrintDef", "~/.executor/printdef.ini");

#if !defined(LITTLEENDIAN)
    ROMlib_DirectoryMap += "-be";
#endif /* !defined(LITTLEENDIAN) */
    if constexpr(sizeof(void*) == 4)
        ROMlib_DirectoryMap += "32";

    auto efs = cmrc::resources::get_filesystem();
    fs::path systemFolder = ROMlib_SystemFolder;

    auto copyfile = [&](fs::path dest, auto from) {
        fs::ofstream out(dest, std::ios::binary);
        std::ostreambuf_iterator<char> begin_dest(out);
        std::copy(from.begin(), from.end(), begin_dest);
    };

    auto ensureFile = [&](fs::path dest) {
        if(!fs::exists(dest))
        {
            copyfile(dest, efs.open(dest.filename().string()));
            if(efs.is_file(dest.filename().string() + ".ad"))
                copyfile(dest.parent_path() / ("%" + dest.filename().string()),
                            efs.open(dest.filename().string() + ".ad"));
        }
    };

    fs::create_directories(systemFolder);
    fs::create_directories(systemFolder / "Preferences");
    ensureFile(systemFolder / "System");
    ensureFile(systemFolder / "Browser");
    ensureFile(systemFolder / "Printer");
    ensureFile(systemFolder / "godata.sav");
    ensureFile(ROMlib_PrintDef);
    ensureFile(ROMlib_PrintersIni);
    fs::create_directories(ROMlib_ConfigurationFolder);
}

void Executor::ROMlib_fileinit() /* INTERNAL */
{
    GUEST<THz> savezone;

    LM(CurDirStore) = 2;
    memset(&LM(DrvQHdr), 0, sizeof(LM(DrvQHdr)));
    memset(&LM(VCBQHdr), 0, sizeof(LM(VCBQHdr)));
    memset(&LM(FSQHdr), 0, sizeof(LM(FSQHdr)));
    LM(DefVCBPtr) = 0;
    LM(FSFCBLen) = 94;

    savezone = LM(TheZone);
    LM(TheZone) = LM(SysZone);
    LM(FCBSPtr) = NewPtr((Size)sizeof(fcbhidden));
    ((fcbhidden *)LM(FCBSPtr))->nbytes = sizeof(fcbhidden);

    for(int i = 0; i < NFCB; i++)
    {
        ROMlib_fcblocks[i].fdfnum = 0;
        ROMlib_fcblocks[i].fcleof = i + 1;
        ROMlib_fcblocks[i].fcbTypByt = 0;
        ROMlib_fcblocks[i].fcbSBlk = 0;
        ROMlib_fcblocks[i].fcPLen = 0;
        ROMlib_fcblocks[i].fcbCrPs = 0;
        ROMlib_fcblocks[i].fcbBfAdr = 0;
        ROMlib_fcblocks[i].fcbFlPos = 0;
        ROMlib_fcblocks[i].fcbClmpSize = 1;
        ROMlib_fcblocks[i].fcbFType = 0;
        ROMlib_fcblocks[i].zero[0] = 0;
        ROMlib_fcblocks[i].zero[1] = 0;
        ROMlib_fcblocks[i].zero[2] = 0;
        ROMlib_fcblocks[i].fcname[0] = 0;
    }
    ROMlib_fcblocks[NFCB - 1].fcleof = -1;

#define NWDENTRIES 40
    INTEGER wdlen = NWDENTRIES * sizeof(wdentry) + sizeof(INTEGER);
    LM(WDCBsPtr) = NewPtr((Size)wdlen);
    LM(TheZone) = savezone;
    memset(LM(WDCBsPtr), 0, wdlen);
    *(GUEST<INTEGER> *)LM(WDCBsPtr) = wdlen;

    InitPaths();

    parse_offset_file();
    ROMlib_hfsinit();
    MountLocalVolume();

    MountMacVolumes(ROMlib_MacVolumes);

    InitSystemFolder(ROMlib_SystemFolder);
}

fcbrec *
Executor::PRNTOFPERR(INTEGER prn, OSErr *errp)
{
    fcbrec *retval;
    OSErr err;

    if(prn < 0 || prn >= *(GUEST<INTEGER> *)LM(FCBSPtr) || (prn % 94) != 2)
    {
        retval = 0;
        err = rfNumErr;
    }
    else
    {
        retval = (fcbrec *)((char *)LM(FCBSPtr) + prn);
        if(!retval->fdfnum)
        {
            retval = 0;
            err = rfNumErr;
        }
        else
            err = noErr;
    }
    *errp = err;
    return retval;
}
