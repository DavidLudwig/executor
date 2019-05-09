/* Copyright 1994 - 1996 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

#include <base/common.h>
#include <base/debugger.h>

#include <QuickDraw.h>
#include <CQuickDraw.h>
#include <ResourceMgr.h>
#include <SegmentLdr.h>
#include <MemoryMgr.h>
#include <StdFilePkg.h>
#include <EventMgr.h>
#include <VRetraceMgr.h>
#include <OSUtil.h>
#include <FontMgr.h>
#include <ScrapMgr.h>
#include <ToolboxUtil.h>
#include <FileMgr.h>
#include <ControlMgr.h>
#include <DeviceMgr.h>
#include <SoundDvr.h>
#include <TextEdit.h>
#include <SysErr.h>
#include <MenuMgr.h>
#include <ScriptMgr.h>
#include <DeskMgr.h>
#include <AppleTalk.h>
#include <PrintMgr.h>
#include <StartMgr.h>
#include <ToolboxEvent.h>
#include <TimeMgr.h>
#include <ProcessMgr.h>
#include <AppleEvents.h>
#include <Gestalt.h>
#include <Package.h>
#include <AliasMgr.h>
#include <OSEvent.h>
#include <ADB.h>

#include <base/trapglue.h>
#include <file/file.h>
#include <sound/sounddriver.h>
#include <prefs/prefs.h>
#include <commandline/flags.h>
#include <rsys/segment.h>
#include <textedit/tesave.h>
#include <res/resource.h>
#include <hfs/hfs.h>
#include <rsys/osutil.h>
#include <rsys/stdfile.h>
#include <ctl/ctl.h>
#include <vdriver/refresh.h>

#include <prefs/options.h>
#include <quickdraw/cquick.h>
#include <rsys/desk.h>
#include <prefs/parse.h>
#include <rsys/executor.h>
#include <prefs/crc.h>
#include <sane/float.h>
#include <mman/mman.h>
#include <vdriver/vdriver.h>
#include <quickdraw/font.h>
#include <base/emustubs.h>

#include <rsys/adb.h>
#include <print/print.h>
#include <rsys/gestalt.h>
#include <osevent/osevent.h>
#include <time/time.h>

#include <rsys/cfm.h>
#include <rsys/launch.h>
#include <rsys/version.h>
#include <rsys/appearance.h>

#include <base/logging.h>

#include <rsys/paths.h>

#include <base/builtinlibs.h>
#include <base/cpu.h>
#include <PowerCore.h>

#include <rsys/macstrings.h>
#include <algorithm>

using namespace Executor;

static bool ppc_launch_p = false;

void Executor::ROMlib_set_ppc(bool val)
{
    ppc_launch_p = val;
}


static void beginexecutingat(LONGINT startpc)
{
    EM_D0 = 0;
    EM_D1 = 0xFFFC000;
    EM_D2 = 0;
    EM_D3 = 0x800008;
    EM_D4 = 0x408;
    EM_D5 = 0x10204000;
    EM_D6 = 0x7F0000;
    EM_D7 = 0x80000800;

    EM_A0 = 0x3EF796;
    EM_A1 = 0x910;
    EM_A2 = EM_D3;
    EM_A3 = 0;
    EM_A4 = 0;
    EM_A5 = guest_cast<LONGINT>(LM(CurrentA5)); /* was smashed when we
					   initialized above */
    EM_A6 = 0x1EF;

    base::Debugger::instance->initProcess(startpc);
    execute68K(startpc);
    C_ExitToShell();
}

size_info_t Executor::size_info;
LONGINT Executor::ROMlib_creator;


uint32_t Executor::ROMlib_version_long;

static bool
cfrg_match(const cfir_t *cfirp, GUEST<OSType> arch_x, uint8_t type_x, Str255 name)
{
    bool retval;

    retval = (CFIR_ISA(cfirp) == arch_x && CFIR_TYPE(cfirp) == type_x && (!name[0] || EqualString(name, (StringPtr)CFIR_NAME(cfirp),
                                                                                                      false, true)));
    return retval;
}

cfir_t *
Executor::ROMlib_find_cfrg(Handle cfrg, OSType arch, uint8_t type, Str255 name)
{
    cfrg_resource_t *cfrgp;
    int n_descripts;
    cfir_t *cfirp;
    GUEST<OSType> desired_arch_x;
    uint8_t type_x;
    cfir_t *retval;

    cfrgp = (cfrg_resource_t *)*cfrg;
    cfirp = (cfir_t *)((char *)cfrgp + sizeof *cfrgp);
    desired_arch_x = arch;
    type_x = type;
    for(n_descripts = CFRG_N_DESCRIPTS(cfrgp);
        n_descripts > 0 && !cfrg_match(cfirp, desired_arch_x, type_x, name);
        --n_descripts, cfirp = (cfir_t *)((char *)cfirp + CFIR_LENGTH(cfirp)))
        ;
    retval = n_descripts > 0 ? cfirp : 0;

    return retval;
}

static void
cfm_launch(Handle cfrg0, OSType desired_arch, FSSpecPtr fsp)
{
    cfir_t *cfirp;

    cfirp = ROMlib_find_cfrg(cfrg0, desired_arch, kApplicationCFrag,
                             (StringPtr) "");

    if(cfirp)
    {
        GUEST<Ptr> mainAddr;
        Str255 errName;
        GUEST<ConnectionID> c_id;

        unsigned char empty[] = { 0 };

        //ROMlib_release_tracking_values();

        if(CFIR_LOCATION(cfirp) == kOnDiskFlat)
        {
            // #warning were ignoring a lot of the cfir attributes
           OSErr err = GetDiskFragment(fsp, CFIR_OFFSET_TO_FRAGMENT(cfirp),
                            CFIR_FRAGMENT_LENGTH(cfirp), empty,
                            kLoadLib,
                            &c_id,
                            &mainAddr,
                            errName);
            fprintf(stderr, "GetDiskFragment -> err == %d\n", err);

        }
        else if(CFIR_LOCATION(cfirp) == kOnDiskSegmented)
        {
            ResType typ;
            INTEGER id;
            Handle h;

            typ = (ResType)CFIR_OFFSET_TO_FRAGMENT(cfirp);
            id = CFIR_FRAGMENT_LENGTH(cfirp);
            h = GetResource(typ, id);
            HLock(h);
            GetMemFragment(*h, GetHandleSize(h), empty, kLoadLib,
                           &c_id, &mainAddr, errName);

            fprintf(stderr, "Memory leak from segmented fragment\n");
        }
        {
            void *mainAddr1 = (void*) mainAddr;
            uint32_t new_toc = ((GUEST<uint32_t>*)mainAddr1)[1];
            uint32_t new_pc = ((GUEST<uint32_t>*)mainAddr1)[0];

            printf("ppc start: r2 = %08x, %08x\n", new_toc, new_pc);

            PowerCore& cpu = getPowerCore();
            cpu.r[2] = new_toc;
            cpu.r[1] = EM_A7-1024;

            cpu.syscall = &builtinlibs::handleSC;

#if SIZEOF_CHAR_P == 4
            cpu.memoryBases[0] = (void*)ROMlib_offset;
            cpu.memoryBases[1] = nullptr;
            cpu.memoryBases[2] = nullptr;
            cpu.memoryBases[3] = nullptr;
#elif SIZEOF_CHAR_P >= 8
            cpu.memoryBases[0] = (void*)ROMlib_offsets[0];
            cpu.memoryBases[1] = (void*)ROMlib_offsets[1];
            cpu.memoryBases[2] = (void*)ROMlib_offsets[2];
            cpu.memoryBases[3] = (void*)ROMlib_offsets[3];
#endif

            base::Debugger::instance->initProcess(new_pc);
            executePPC(new_pc);
        }
    }
    else
        fprintf(stderr, "Code fragment not found\n");
    C_ExitToShell();
}

launch_failure_t Executor::ROMlib_launch_failure = launch_no_failure;
INTEGER Executor::ROMlib_exevrefnum;

static void launchchain(StringPtr fName, INTEGER vRefNum, BOOLEAN resetmemory,
                        LaunchParamBlockRec *lpbp)
{
    OSErr err;
    FInfo finfo;
    Handle code0;
    Handle cfrg0;
    Handle h;
    vers_t *vp;
    WDPBRec wdpb;

    {
        INTEGER toskip;
        Byte *p;
        for(p = fName + fName[0] + 1; p > fName && *--p != ':';)
            ;
        toskip = p - fName;
        LM(CurApName)[0] = std::min(fName[0] - toskip, 31);
        BlockMoveData((Ptr)fName + 1 + toskip, (Ptr)LM(CurApName) + 1,
                    (Size)LM(CurApName)[0]);
    }
    std::string appNameUTF8 = toUnicodeFilename(LM(CurApName)).string();
#if 0
    Munger(LM(AppParmHandle), 2L*sizeof(INTEGER), (Ptr) 0,
				  (LONGINT) sizeof(AppFile), (Ptr) "", 0L);
#endif
    if(!lpbp || lpbp->launchBlockID != extendedBlock)
    {
        CInfoPBRec hpb;

        hpb.hFileInfo.ioNamePtr = &fName[0];
        hpb.hFileInfo.ioVRefNum = vRefNum;
        hpb.hFileInfo.ioFDirIndex = 0;
        hpb.hFileInfo.ioDirID = 0;
        PBGetCatInfo(&hpb, false);
        wdpb.ioVRefNum = vRefNum;
        wdpb.ioWDDirID = hpb.hFileInfo.ioFlParID;
    }
    else
    {
        FSSpecPtr fsp;

        fsp = lpbp->launchAppSpec;
        wdpb.ioVRefNum = fsp->vRefNum;
        wdpb.ioWDDirID = fsp->parID;
    }
    /* Do not do this -- Loser does it SFSaveDisk_Update (vRefNum, fName); */
    wdpb.ioWDProcID = TICK("Xctr");
    wdpb.ioNamePtr = 0;
    PBOpenWD(&wdpb, false);
    ROMlib_exevrefnum = wdpb.ioVRefNum;
    ROMlib_exefname = LM(CurApName);
#if 0
/* I'm skeptical that this is correct */
    if (LM(CurMap) != LM(SysMap))
	CloseResFile(LM(CurMap));
#endif
    SetVol((StringPtr)0, ROMlib_exevrefnum);
    LM(CurApRefNum) = OpenResFile(ROMlib_exefname);

    err = GetFInfo(ROMlib_exefname, ROMlib_exevrefnum, &finfo);

    process_create(false, finfo.fdType, finfo.fdCreator);

    ROMlib_creator = finfo.fdCreator;

#define LEMMINGSHACK
#if defined(LEMMINGSHACK)
    {
        if(finfo.fdCreator == TICK("Psyg")
           || finfo.fdCreator == TICK("Psod"))
            ROMlib_flushoften = true;
    }
#endif /* defined(LEMMINGSHACK) */

#if defined(ULTIMA_III_HACK)
    ROMlib_ultima_iii_hack = (finfo.fdCreator == TICK("Ult3"));
#endif

    h = GetResource(FOURCC('v', 'e', 'r', 's'), 2);
    if(!h)
        h = GetResource(FOURCC('v', 'e', 'r', 's'), 1);

    ROMlib_version_long = 0;
    if(h)
    {
        vp = (vers_t *)*h;
        ROMlib_version_long = ((vp->c[0] << 24) | (vp->c[1] << 16) | (vp->c[2] << 8) | (vp->c[3] << 0));
    }
    

    ROMlib_ScreenSize.first = INITIALPAIRVALUE;
    ROMlib_MacSize.first = INITIALPAIRVALUE;
    ROMlib_directdiskaccess = false;
    ROMlib_clear_gestalt_list();
    ParseConfigFile("ExecutorDefault", 0);
    ParseConfigFile(appNameUTF8, err == noErr ? finfo.fdCreator.raw() : 0);
    ROMlib_clockonoff(!ROMlib_noclock);
    if((ROMlib_ScreenSize.first != INITIALPAIRVALUE
        || ROMlib_MacSize.first != INITIALPAIRVALUE))
    {
        if(ROMlib_ScreenSize.first == INITIALPAIRVALUE)
            ROMlib_ScreenSize = ROMlib_MacSize;
        if(ROMlib_MacSize.first == INITIALPAIRVALUE)
            ROMlib_MacSize = ROMlib_ScreenSize;
    }
    code0 = Get1Resource(FOURCC('C', 'O', 'D', 'E'), 0);
    cfrg0 = Get1Resource(FOURCC('c', 'f', 'r', 'g'), 0);

    bool havePowerPCCode = cfrg0
            && ROMlib_find_cfrg(cfrg0, FOURCC('p', 'w', 'p', 'c'),
                                kApplicationCFrag, (StringPtr) "");

    if(havePowerPCCode && ppc_launch_p)
        code0 = nullptr;
    
    if(!havePowerPCCode && !code0)
    {
        if(cfrg0 && ROMlib_find_cfrg(cfrg0, FOURCC('m', '6', '8', 'k'),
                                     kApplicationCFrag, (StringPtr) ""))
            ROMlib_launch_failure = launch_cfm_requiring;
        else
            ROMlib_launch_failure = launch_damaged;
        C_ExitToShell();
    }

    {
        Handle size_resource_h = Get1Resource(FOURCC('S', 'I', 'Z', 'E'), 0);
        if(size_resource_h == nullptr)
            size_resource_h = Get1Resource(FOURCC('S', 'I', 'Z', 'E'), -1);
        if(size_resource_h)
        {
            SIZEResource *size_resource;

            size_resource = (SIZEResource *)*size_resource_h;
            size_info.size_flags = size_resource->size_flags;
            size_info.preferred_size = size_resource->preferred_size;
            size_info.minimum_size = size_resource->minimum_size;
            size_info.size_resource_present_p = true;
        }
        else
        {
            memset(&size_info, '\000', sizeof size_info);
            size_info.size_resource_present_p = false;
        }
        size_info.application_p = true;

        /* we don't accept open app events until a handler is installed */
        application_accepts_open_app_aevt_p = false;
        send_application_open_aevt_p
            = system_version >= 0x700
            && ((size_info.size_flags & SZisHighLevelEventAware)
                == SZisHighLevelEventAware);
    }

    LM(BufPtr) = LM(MemTop);

    if(!code0)
    {
        LM(CurrentA5) = LM(BufPtr) - 4;
        LM(CurStackBase) = LM(CurrentA5);       
    }
    else
    {
        HLock(code0);

        auto lp = (GUEST<LONGINT> *)*code0;
        LONGINT abovea5 = *lp++;
        LONGINT belowa5 = *lp++;
        LONGINT jumplen = *lp++;
        LONGINT jumpoff = *lp++;

        /*
         * NOTE: The stack initialization code that was here has been moved
         *	     to ROMlib_InitZones in mman.c
         */
        /* #warning Stack is getting reinitialized even when Chain is called ... */

        LM(CurrentA5) = LM(BufPtr) - abovea5;
        LM(CurStackBase) = LM(CurrentA5) - belowa5;

        LM(CurJTOffset) = jumpoff;
        memcpy(LM(CurrentA5) + jumpoff, lp, jumplen); /* copy in the
							 jump table */
    }
    EM_A7 = ptr_to_longint(LM(CurStackBase));
    EM_A5 = ptr_to_longint(LM(CurrentA5));

    ROMlib_destroy_blocks(0, ~0, false);

    GetDateTime(&LM(Time));
    LM(ROMBase) = (Ptr)ROMlib_phoneyrom;
    LM(dodusesit) = LM(ROMBase);
    LM(QDExist) = LM(WWExist) = EXIST_NO;
    LM(TheZone) = LM(ApplZone);

    // disallow moving relocatable blocks until this flag is reset
    // in OSEventCommon(). Allegedly (according to some old comment found
    // in that function), this is needed for some unspecified version of Excel.
    ROMlib_memnomove_p = true;

    SetCursor(*GetCursor(watchCursor));

    /* Call this routine in case the refresh value changed, either just
    * now or when the config file was parsed.  We want to do this
    * before the screen's depth might be changed.
    */
    {
        int save_ROMlib_refresh = ROMlib_refresh;
        dequeue_refresh_task();
        set_refresh_rate(0);
        set_refresh_rate(save_ROMlib_refresh);
    }

    if(code0)
        beginexecutingat(guest_cast<LONGINT>(LM(CurrentA5)) + LM(CurJTOffset) + 2);
    else
    {
        FSSpec fs;

        FSMakeFSSpec(ROMlib_exevrefnum, 0, ROMlib_exefname, &fs);
        cfm_launch(cfrg0, FOURCC('p', 'w', 'p', 'c'), &fs);
    }
}

void Executor::Chain(StringPtr fName, INTEGER vRefNum)
{
    launchchain(fName, vRefNum, false, 0);
}

static void reset_low_globals(void)
{
    /*
     * we're about to smash applzone ... we may want to verify a few low-mem
     * globals beforehand
     */

    auto saveSysZone = LM(SysZone);
    auto saveTicks = LM(Ticks);
    auto saveBootDrive = LM(BootDrive);
    auto saveLo3Bytes = LM(Lo3Bytes);
    auto save20 = *(GUEST<LONGINT> *)SYN68K_TO_US(0x20);
    auto save28 = *(GUEST<LONGINT> *)SYN68K_TO_US(0x28);
    auto save58 = *(GUEST<LONGINT> *)SYN68K_TO_US(0x58);
    auto save5C = *(GUEST<LONGINT> *)SYN68K_TO_US(0x5C);
    auto saveVIA = LM(VIA);
    auto saveSCCRd = LM(SCCRd);
    auto saveSCCWr = LM(SCCWr);
    auto saveSoundBase = LM(SoundBase);
    auto saveAppParmHandle = LM(AppParmHandle);
    auto saveVCBQHdr = LM(VCBQHdr);
    auto saveFSQHdr = LM(FSQHdr);
    auto saveDrvQHdr = LM(DrvQHdr);
    auto saveFCBSPtr = LM(FCBSPtr);
    auto saveWDCBsPtr = LM(WDCBsPtr);
    auto saveSFSaveDisk = LM(SFSaveDisk);
    auto saveCurDirStore = LM(CurDirStore);
    auto saveEventQueue = LM(EventQueue);
    auto saveVBLQueue = LM(VBLQueue);
    auto saveDefVCBPtr = LM(DefVCBPtr);
 
    GUEST<char> saveCurApName[sizeof(LM(CurApName))];
    memcpy(saveCurApName, LM(CurApName), sizeof(LM(CurApName)));
    
    auto saveCurApRefNum = LM(CurApRefNum);
    auto saveCurMap = LM(CurMap);
    auto saveTopMapHndl = LM(TopMapHndl);
    auto saveSysMapHndl = LM(SysMapHndl);
    auto saveSysMap = LM(SysMap);
    auto saveScrapSize = LM(ScrapSize);
    auto saveScrapHandle = LM(ScrapHandle);
    auto saveScrapCount = LM(ScrapCount);
    auto saveScrapState = LM(ScrapState);
    auto saveScrapName = LM(ScrapName);
    auto saveROMFont0 = LM(ROMFont0);
    auto saveWidthListHand = LM(WidthListHand);
    auto saveSPValid = LM(SPValid);
    auto saveSPATalkA = LM(SPATalkA);
    auto saveSPATalkB = LM(SPATalkB);
    auto saveSPConfig = LM(SPConfig);
    auto saveSPPortA = LM(SPPortA);
    auto saveSPPortB = LM(SPPortB);
    auto saveSPAlarm = LM(SPAlarm);
    auto saveSPFont = LM(SPFont);
    auto saveSPKbd = LM(SPKbd);
    auto saveSPPrint = LM(SPPrint);
    auto saveSPVolCtl = LM(SPVolCtl);
    auto saveSPClikCaret = LM(SPClikCaret);
    auto saveSPMisc2 = LM(SPMisc2);
    auto saveKeyThresh = LM(KeyThresh);
    auto saveKeyRepThresh = LM(KeyRepThresh);
    auto saveMenuFlash = LM(MenuFlash);
    auto saveCaretTime = LM(CaretTime);
    auto saveDoubleTime = LM(DoubleTime);
    auto saveDefDirID = DefDirID;
 
    auto saveHiliteRGB = LM(HiliteRGB);
    auto saveTheGDevice = LM(TheGDevice);
    auto saveMainDevice = LM(MainDevice);
    auto saveDeviceList = LM(DeviceList);
    
    GUEST<Handle> saveDAStrings[4];
    saveDAStrings[0] = LM(DAStrings)[0];
    saveDAStrings[1] = LM(DAStrings)[1];
    saveDAStrings[2] = LM(DAStrings)[2];
    saveDAStrings[3] = LM(DAStrings)[3];

    auto saveMemTop = LM(MemTop);
    auto saveUTableBase = LM(UTableBase);
    auto saveUnitNtryCnt = LM(UnitNtryCnt);
 
    auto saveMouseLocation = LM(MouseLocation);
    auto saveDABeeper = LM(DABeeper);

    GUEST<Byte> saveFinderName[sizeof(LM(FinderName))];
    memcpy(saveFinderName, LM(FinderName), sizeof(saveFinderName));
    
    auto saveWMgrCPort = LM(WMgrCPort);
    auto saveMBDFHndl = LM(MBDFHndl);

    auto saveJCrsrTask = LM(JCrsrTask);

    auto saveAE_info = LM(AE_info);
    
    GUEST<char> saveKeyMap[sizeof_KeyMap];
    memcpy(saveKeyMap, LM(KeyMap), sizeof_KeyMap);

    /* Set low globals to 0xFF, but don't touch exception vectors. */
    memset((char *)&LM(nilhandle) + 64 * sizeof(ULONGINT),
           0xFF,
           ((char *)&LM(lastlowglobal) - (char *)&LM(nilhandle)
            - 64 * sizeof(ULONGINT)));

    LM(AE_info) = saveAE_info;

    LM(JCrsrTask) = saveJCrsrTask;

    LM(MBDFHndl) = saveMBDFHndl;
    LM(WMgrCPort) = saveWMgrCPort;
    memcpy(LM(FinderName), saveFinderName, sizeof(LM(FinderName)));

    LM(DABeeper) = saveDABeeper;
    LM(MouseLocation) = saveMouseLocation;
    LM(MouseLocation2) = saveMouseLocation;

    LM(UTableBase) = saveUTableBase;
    LM(UnitNtryCnt) = saveUnitNtryCnt;
    LM(MemTop) = saveMemTop;
    LM(DAStrings)[3] = saveDAStrings[3];
    LM(DAStrings)[2] = saveDAStrings[2];
    LM(DAStrings)[1] = saveDAStrings[1];
    LM(DAStrings)[0] = saveDAStrings[0];
    DefDirID = saveDefDirID;
    LM(DoubleTime) = saveDoubleTime;
    LM(CaretTime) = saveCaretTime;
    LM(MenuFlash) = saveMenuFlash;
    LM(KeyRepThresh) = saveKeyRepThresh;
    LM(KeyThresh) = saveKeyThresh;
    LM(SPMisc2) = saveSPMisc2;
    LM(SPClikCaret) = saveSPClikCaret;
    LM(SPVolCtl) = saveSPVolCtl;
    LM(SPPrint) = saveSPPrint;
    LM(SPKbd) = saveSPKbd;
    LM(SPFont) = saveSPFont;
    LM(SPAlarm) = saveSPAlarm;
    LM(SPPortB) = saveSPPortB;
    LM(SPPortA) = saveSPPortA;
    LM(SPConfig) = saveSPConfig;
    LM(SPATalkB) = saveSPATalkB;
    LM(SPATalkA) = saveSPATalkA;
    LM(SPValid) = saveSPValid;
    LM(WidthListHand) = saveWidthListHand;
    LM(ROMFont0) = saveROMFont0;
    LM(ScrapName) = saveScrapName;
    LM(ScrapState) = saveScrapState;
    LM(ScrapCount) = saveScrapCount;
    LM(ScrapHandle) = saveScrapHandle;
    LM(ScrapSize) = saveScrapSize;
    LM(SysMap) = saveSysMap;
    LM(SysMapHndl) = saveSysMapHndl;
    LM(TopMapHndl) = saveTopMapHndl;
    LM(CurMap) = saveCurMap;
    LM(CurApRefNum) = saveCurApRefNum;
    memcpy(LM(CurApName), saveCurApName, sizeof(LM(CurApName)));
    LM(DefVCBPtr) = saveDefVCBPtr;
    LM(VBLQueue) = saveVBLQueue;
    LM(EventQueue) = saveEventQueue;
    LM(CurDirStore) = saveCurDirStore;
    LM(SFSaveDisk) = saveSFSaveDisk;
    LM(WDCBsPtr) = saveWDCBsPtr;
    LM(FCBSPtr) = saveFCBSPtr;
    LM(DrvQHdr) = saveDrvQHdr;
    LM(FSQHdr) = saveFSQHdr;
    LM(VCBQHdr) = saveVCBQHdr;
    LM(Lo3Bytes) = saveLo3Bytes;
    LM(VIA) = saveVIA;
    LM(SCCRd) = saveSCCRd;
    LM(SCCWr) = saveSCCWr;
    LM(SoundBase) = saveSoundBase;
    LM(Ticks) = saveTicks;
    LM(SysZone) = saveSysZone;
    LM(BootDrive) = saveBootDrive;
    LM(AppParmHandle) = saveAppParmHandle;

    LM(HiliteRGB) = saveHiliteRGB;
    LM(TheGDevice) = saveTheGDevice;
    LM(MainDevice) = saveMainDevice;
    LM(DeviceList) = saveDeviceList;

    *(GUEST<LONGINT> *)SYN68K_TO_US(0x20) = save20;
    *(GUEST<LONGINT> *)SYN68K_TO_US(0x28) = save28;
    *(GUEST<LONGINT> *)SYN68K_TO_US(0x58) = save58;
    *(GUEST<LONGINT> *)SYN68K_TO_US(0x5C) = save5C;

    LM(nilhandle) = 0; /* so nil dereferences "work" */
    LM(WindowList) = nullptr;

    LM(CrsrBusy) = 0;
    LM(TESysJust) = 0;
    LM(DSAlertTab) = 0;
    LM(ResumeProc) = 0;
    LM(GZRootHnd) = 0;
    LM(ANumber) = 0;
    LM(ResErrProc) = 0;
#if 0
    LM(FractEnable) = 0xff;	/* NEW MOD -- QUESTIONABLE */
#else
    LM(FractEnable) = 0;
#endif
    LM(SEvtEnb) = 0;
    LM(MenuList) = 0;
    LM(MBarEnable) = 0;
    LM(MenuFlash) = 0;
    LM(TheMenu) = 0;
    LM(MBarHook) = 0;
    LM(MenuHook) = 0;
    LM(HeapEnd) = 0;
    LM(ApplLimit) = 0;
    LM(SoundActive) = soundactiveoff;
    LM(PortBUse) = 2; /* configured for Serial driver */

    memcpy(LM(KeyMap), saveKeyMap, sizeof_KeyMap);
    LM(OneOne) = 0x00010001;
    LM(DragHook) = 0;
    LM(MBDFHndl) = 0;
    LM(MenuList) = 0;
    LM(MBSaveLoc) = 0;
    LM(SysFontFam) = 0;

    LM(SysVersion) = system_version;
    LM(FSFCBLen) = 94;


    LM(TEDoText) = (ProcPtr)&ROMlib_dotext; /* where should this go ? */

    LM(WWExist) = LM(QDExist) = EXIST_NO; /* TODO:  LOOK HERE! */
    LM(SCSIFlags) = 0xEC00; /* scsi+clock+xparam+mmu+adb
				 (no fpu,aux or pwrmgr) */

    LM(MMUType) = 5;
    LM(KbdType) = 2;

    LM(MCLKPCmiss1) = 0; /* &LM(MCLKPCmiss1) = 0x358 + 72 (MacLinkPC starts
			   adding the 72 byte offset to VCB pointers too
			   soon, starting with 0x358, which is not the
			   address of a VCB) */

    LM(MCLKPCmiss2) = 0; /* &LM(MCLKPCmiss1) = 0x358 + 78 (MacLinkPC misses) */
    LM(AuxCtlHead) = 0;
    LM(CurDeactive) = 0;
    LM(CurActivate) = 0;
    LM(macfpstate)[0] = 0;
    LM(fondid) = 0;
    LM(PrintErr) = 0;
    LM(mouseoffset) = 0;
    LM(heapcheck) = 0;
    LM(DefltStack) = 0x2000; /* nobody really cares about these two */
    LM(MinStack) = 0x400; /* values ... */
    LM(IAZNotify) = 0;
    LM(CurPitch) = 0;
    LM(JSwapFont) = (ProcPtr)&FMSwapFont;
    LM(JInitCrsr) = (ProcPtr)&InitCursor;

    LM(JHideCursor) = (ProcPtr)&HideCursor;
    LM(JShowCursor) = (ProcPtr)&ShowCursor;
    LM(JShieldCursor) = (ProcPtr)&ShieldCursor;
    LM(JSetCrsr) = (ProcPtr)&SetCursor;
    LM(JCrsrObscure) = (ProcPtr)&ObscureCursor;

    LM(JUnknown574) = (ProcPtr)&unknown574;

    LM(Key1Trans) = (Ptr)&stub_Key1Trans;
    LM(Key2Trans) = (Ptr)&stub_Key2Trans;
    LM(JFLUSH) = &FlushCodeCache;
    LM(JResUnknown1) = LM(JFLUSH); /* I don't know what these are supposed to */
    LM(JResUnknown2) = LM(JFLUSH); /* do, but they're not called enough for
				   us to worry about the cache flushing
				   overhead */

    LM(CPUFlag) = 4; /* mc68040 */
    LM(UnitNtryCnt) = 0; /* how many units in the table */

    LM(TheZone) = LM(ApplZone);


    LM(HiliteMode) = 0xFF; /* I think this is correct */
    LM(ROM85) = 0x3FFF; /* We be color now */
    LM(MMU32Bit) = 0x01;
    LM(loadtrap) = 0;
    *(GUEST<LONGINT> *)SYN68K_TO_US(0x1008) = 0x4; /* Quark XPress 3.0 references 0x1008
					explicitly.  It takes the value
					found there, subtracts four from
					it and dereferences that value.
					Yahoo */
    *(GUEST<int16_t> *)SYN68K_TO_US(4) = 0x4e75; /* RTS, so when we dynamically recompile
				    code starting at 0 we won't get far */

    /* Micro-cap dereferences location one of the LM(AppPacks) locations */

    {
        int i;

        for(i = 0; i < (int)NELEM(LM(AppPacks)); ++i)
            LM(AppPacks)[i] = 0;
    }
    LM(SysEvtMask) = ~(1L << keyUp); /* EVERYTHING except keyUp */
    LM(SdVolume) = 7; /* for Beebop 2 */
    LM(CurrentA5) = guest_cast<Ptr>(EM_A5);

    /*
     * TODO:  how does this relate to Launch?
     */
    /* Set up default floating point environment. */
    {
        INTEGER env = 0;
        ROMlib_Fsetenv(&env, 0);
    }
}

static void reset_traps(void)
{
    static syn68k_addr_t savetooltraptable[0x400];
    static syn68k_addr_t saveostraptable[0x100];
    static BOOLEAN beenhere = false;

    ROMlib_reset_bad_trap_addresses();
    if(!beenhere)
    {
        memcpy(saveostraptable, ostraptable, sizeof(saveostraptable));
        memcpy(savetooltraptable, tooltraptable, sizeof(savetooltraptable));
        beenhere = true;
    }
    else
    {
        /*
 * NOTE: I'm not preserving patches that go into the SystemZone.  Right now
 *       that just seems like an unnecessary source of mystery bugs.
 */
        memcpy(ostraptable, saveostraptable, sizeof(saveostraptable));
        memcpy(tooltraptable, savetooltraptable, sizeof(savetooltraptable));
    }
}

static bool
our_special_map(resmaphand map)
{
    bool retval;
    Handle h;

    LM(CurMap) = (*map)->resfn;
    h = Get1Resource(TICK("nUSE"), 0);
    retval = h ? true : false;

    return retval;
}

void Executor::empty_timer_queues(void)
{
    TMTask *tp, *nexttp;
    VBLTaskPtr vp, nextvp;

    dequeue_refresh_task();
    clear_pending_sounds();
    for(vp = (VBLTaskPtr)LM(VBLQueue).qHead; vp; vp = nextvp)
    {
        nextvp = (VBLTaskPtr)vp->qLink;
        VRemove(vp);
    }
    for(tp = (TMTask *)ROMlib_timehead.qHead; tp; tp = nexttp)
    {
        nexttp = (TMTask *)tp->qLink;
        RmvTime((QElemPtr)tp);
    }
}

static void reinitialize_things(void)
{
    resmaphand map, nextmap;
    filecontrolblock *fcbp, *efcbp;
    INTEGER length;
    INTEGER special_fn;
    int i;

    ROMlib_shutdown_font_manager();
    SetZone(LM(SysZone));
    /* NOTE: we really shouldn't be closing desk accessories at all, but
       since we don't properly handle them when they're left open, it is
       better to close them down than not.  */

    for(i = DESK_ACC_MIN; i <= DESK_ACC_MAX; ++i)
        CloseDriver(-i - 1);

    empty_timer_queues();
    ROMlib_clock = 0; /* CLOCKOFF */

    special_fn = 0;
    for(map = (resmaphand)LM(TopMapHndl); map; map = nextmap)
    {
        nextmap = (resmaphand)(*map)->nextmap;
        if((*map)->resfn == LM(SysMap))
            UpdateResFile((*map)->resfn);
        else
        {
            if(!our_special_map(map))
                CloseResFile((*map)->resfn);
            else
            {
                special_fn = (*map)->resfn;
                UpdateResFile(special_fn);
            }
        }
    }

    length = *(GUEST<int16_t> *)LM(FCBSPtr);
    fcbp = (filecontrolblock *)((short *)LM(FCBSPtr) + 1);
    efcbp = (filecontrolblock *)((char *)LM(FCBSPtr) + length);
    for(; fcbp < efcbp;
        fcbp = (filecontrolblock *)((char *)fcbp + LM(FSFCBLen)))
    {
        INTEGER rn;

        rn = (char *)fcbp - (char *)LM(FCBSPtr);
        if(fcbp->fcbCName[0]
           /* && rn != Param_ram_rn */
           && rn != LM(SysMap)
           && rn != special_fn)
            FSClose((char *)fcbp - (char *)LM(FCBSPtr));
    }

    LM(CurMap) = (*(resmaphand)LM(TopMapHndl))->resfn;

    ROMlib_destroy_blocks(0, ~0, false);
}

OSErr
Executor::NewLaunch(StringPtr fName_arg, INTEGER vRefNum_arg, LaunchParamBlockRec *lpbp)
{
    OSErr retval;
    static char beenhere = false;
    static jmp_buf buf;
    static Str255 fName;
    static INTEGER vRefNum;
    static LaunchParamBlockRec lpb;
    BOOLEAN extended_p;

    retval = noErr;
    if(lpbp && lpbp->launchBlockID == extendedBlock)
    {
        lpb = *lpbp;
        str255assign(fName, (lpbp->launchAppSpec)->name);
        extended_p = true;
    }
    else
    {
        lpb.launchBlockID = 0;
        str255assign(fName, fName_arg);
        vRefNum = vRefNum_arg;
        extended_p = false;
    }

#if 0
    if(extended_p && (lpbp->launchControlFlags & launchContinue))
    {
        int n_filenames;
        char **filenames;
        int i;
        AppParametersPtr ap;
        int n_filename_bytes;

#if !defined(LETGCCWAIL)
        ap = nullptr;
#endif
        n_filenames = 1;
        if(lpbp->launchAppParameters)
        {
            ap = lpbp->launchAppParameters;
            n_filenames += ap->n_fsspec;
        }
        n_filename_bytes = n_filenames * sizeof *filenames;
        filenames = (char **)alloca(n_filename_bytes);
        memset(filenames, 0, n_filename_bytes);
        retval = ROMlib_filename_from_fsspec(&filenames[0],
                                             lpbp->launchAppSpec);
        for(i = 1; retval == noErr && i < n_filenames; ++i)
            retval = ROMlib_filename_from_fsspec(&filenames[1],
                                                 &ap->fsspec[i - 1]);
        if(retval == noErr)
            retval = ROMlib_launch_native_app(n_filenames, filenames);
        for(i = 0; i < n_filenames; ++i)
            free(filenames[i]);
    }
    else
#endif
    {
        /* This setjmp/longjmp code might be better put in launchchain */
        if(!beenhere)
        {
            beenhere = true;
            setjmp(buf);
        }
        else
            longjmp(buf, 1);
        logging::resetNestingLevel();

        retval = noErr;
        reset_adb_vector();
        reinitialize_things();
        reset_traps();
        reset_low_globals();
        ROMlib_InitZones();
        ROMlib_InitGWorlds();
        hle_reinit();
        AE_reinit();
        print_reinit();

        gd_set_bpp(LM(MainDevice), !vdriver->isGrayscale(), vdriver->isFixedCLUT(),
                   vdriver->bpp());
        ROMlib_init_stdfile();

        launchchain(fName, vRefNum, true, &lpb);
    }
    return retval;
}

void Executor::Launch(StringPtr fName_arg, INTEGER vRefNum_arg)
{
    NewLaunch(fName_arg, vRefNum_arg, 0);
}
