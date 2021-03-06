#if !defined(__SEGMENT__)
#define __SEGMENT__

/*
 * Copyright 1989 - 1995 by Abacus Research and Development, Inc.
 * All rights reserved.
 *

 */

#include <ExMacTypes.h>
#include <base/lowglobals.h>

#define MODULE_NAME SegmentLdr
#include <base/api-module.h>

namespace Executor
{
//extern _NORET_1_ void C_ExitToShell(void) _NORET_2_;
extern void C_ExitToShell(void);
PASCAL_TRAP(ExitToShell, 0xA9F4);

enum
{
    appOpen = 0,
    appPrint = 1,
};

struct AppFile
{
    GUEST_STRUCT;
    GUEST<INTEGER> vRefNum;
    GUEST<OSType> fType;
    GUEST<INTEGER> versNum;
    GUEST<Str255> fName;
};

const LowMemGlobal<Byte> loadtrap { 0x12D }; // SegmentLdr SysEqu.a (true-b);
const LowMemGlobal<Byte[16]> FinderName { 0x2E0 }; // SegmentLdr IMII-59 (true);
const LowMemGlobal<INTEGER> CurApRefNum { 0x900 }; // SegmentLdr IMII-58 (true);
/*
 * NOTE: IMIII says CurApName is 32 bytes long, but it looks to me like
 * it is really 34 bytes long.
 */
const LowMemGlobal<Byte[34]> CurApName { 0x910 }; // SegmentLdr IMII-58 (true);
const LowMemGlobal<INTEGER> CurJTOffset { 0x934 }; // SegmentLdr IMII-62 (true-b);
const LowMemGlobal<INTEGER> CurPageOption { 0x936 }; // SegmentLdr IMII-60 (true);
const LowMemGlobal<Handle> AppParmHandle { 0xAEC }; // SegmentLdr IMII-57 (true);

extern void CountAppFiles(GUEST<INTEGER> *messagep,
                          GUEST<INTEGER> *countp);
extern void GetAppFiles(INTEGER index, AppFile *filep);
extern void ClrAppFiles(INTEGER index);
extern void Launch(StringPtr appl, INTEGER vrefnum);
extern void Chain(StringPtr appl, INTEGER vrefnum);

extern void C_GetAppParms(StringPtr namep,
                                      GUEST<INTEGER> *rnp, GUEST<Handle> *aphandp);
PASCAL_TRAP(GetAppParms, 0xA9F5);

extern void C_UnloadSeg(Ptr addr);
PASCAL_TRAP(UnloadSeg, 0xA9F1);

extern void C_LoadSeg(INTEGER volatile segno);

static_assert(sizeof(AppFile) == 264);
}
#endif /* __SEGMENT__ */
