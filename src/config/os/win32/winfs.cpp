/*
 * Copyright 1997-2000 by Abacus Research and Development, Inc.
 * All rights reserved.
 */

#include "rsys/common.h"

#include <windows.h>
#include <stdio.h>
#include <errno.h>

#include "winfs.h"
#include "rsys/lockunlock.h"

using namespace Executor;

int
fsync(int fd)
{
    int retval;

    retval = 0;
    return retval;
}

int
sync(void)
{
    int retval;

    retval = 0;
    return retval;
}

char *
getwd(char *buf)
{
    char *retval;

    retval = getcwd(buf, MAXPATHLEN);
    return retval;
}

#warning link is just a spoof -- I think it will work for our purposes though
/* verify that we can spoof in all contexts that it's used */
/* serial.c and main.c */

int
link(const char *oldpath, const char *newpath)
{
    int retval;
    int hand;

    hand = creat(newpath, O_RDWR);
    if(hand == -1)
        retval = -1;
    else
    {
        close(hand);
        retval = 0;
    }
    return retval;
}

int
ROMlib_lockunlockrange(int fd, uint32_t begin, uint32_t count, lockunlock_t op)
{
    int retval;
    BOOL WINAPI (*routine)(HANDLE, DWORD, DWORD, DWORD, DWORD);

    warning_trace_info("fd = %d, begin = %d, count = %d, op = %d",
                       fd, begin, count, op);
    switch(op)
    {
        case lock:
            routine = LockFile;
            break;
        case unlock:
            routine = UnlockFile;
            break;
        default:
            warning_unexpected("op = %d", op);
            routine = 0;
            break;
    }

    if(!routine)
        retval = paramErr;
    else
    {
        BOOL success;
        HANDLE h;

        h = (HANDLE)_get_osfhandle(fd);
        success = routine(h, begin, 0, count, 0);
        if(success)
            retval = noErr;
        else
        {
            DWORD err;

            err = GetLastError();
            switch(err)
            {
                case ERROR_LOCK_VIOLATION:
                    retval = fLckdErr;
                    break;
                case ERROR_NOT_LOCKED:
                    retval = afpRangeNotLocked;
                    break;
                case ERROR_LOCK_FAILED:
                    retval = afpRangeOverlap;
                    break;
                default:
                    warning_unexpected("err = %ld, h = %p", err, h);
                    retval = noErr;
                    break;
            }
        }
    }
    return retval;
}
