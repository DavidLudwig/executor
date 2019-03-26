#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <dirent.h>
#include <io.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#ifndef _MSC_VER
#include <unistd.h>
//#include <sys/vfs.h>
#include <sys/param.h>
//#include <sys/errno.h>
#endif

#define CONFIG_OFFSET_P 1 /* Use offset memory, at least for the first port */

typedef int ssize_t;

extern int ROMlib_launch_native_app(int n_filenames, char **filenames);

//#define WIN32

typedef struct
{
    char *dptr;
    unsigned dsize;
} datum;

//inline int geteuid() { return 1; }

#include <malloc.h>