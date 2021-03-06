/* Copyright 1986, 1988, 1989, 1990 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

/* Forward declarations in DialogMgr.h (DO NOT DELETE THIS LINE) */

#include <base/common.h>

#include <DialogMgr.h>
#include <FontMgr.h>
#include <OSUtil.h>
#include <MemoryMgr.h>

#include <dial/itm.h>
#include <mman/mman.h>

using namespace Executor;

void Executor::C_ROMlib_mysound(INTEGER i)
{
    while(i--)
        SysBeep(5);
}

void Executor::C_ErrorSound(SoundProcPtr sp) /* IMI-411 */
{
    LM(DABeeper) = sp;
}

void Executor::C_InitDialogs(ProcPtr rp) /* IMI-411 */
{
    Ptr nothing = (Ptr) "";

    TheZoneGuard guard(LM(SysZone));

    LM(DlgFont) = systemFont;
    LM(ResumeProc) = rp;
    ErrorSound(&ROMlib_mysound);
    GUEST<Handle> tmp;
    PtrToHand(nothing, &tmp, (LONGINT)1);
    LM(DAStrings)[0] = tmp;
    PtrToHand(nothing, &tmp, (LONGINT)1);
    LM(DAStrings)[1] = tmp;
    PtrToHand(nothing, &tmp, (LONGINT)1);
    LM(DAStrings)[2] = tmp;
    PtrToHand(nothing, &tmp, (LONGINT)1);
    LM(DAStrings)[3] = tmp;
}

void Executor::SetDialogFont(INTEGER i) /* IMI-412 */
{
    LM(DlgFont) = i;
}
