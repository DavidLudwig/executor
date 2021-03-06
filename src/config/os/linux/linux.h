#if !defined(_OS_LINUX_H_)
#define _OS_LINUX_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <dirent.h>
#include <sys/vfs.h>
#include <sys/param.h>
#include <sys/errno.h>

#include <signal.h>

#include <stdint.h>


#if !defined(LINUX)
#define LINUX
#endif

#if !defined(O_BINARY)
#define O_BINARY 0
#endif

#if !defined(REINSTALL_SIGNAL_HANDLER)
/* define `REINSTALL_SIGNAL_HANDLER' if signal handlers are
   de-installed after the signals occur, and require reinstallation */
#define REINSTALL_SIGNAL_HANDLER
#endif /* !REINSTALL_SIGNAL_HANDLER */

#define HAVE_MMAP

/*
 * In the bad old days we could allocate page 0 and use it and not have
 * to offset memory.  These days that's rarely allowed, and machines are
 * fast enough that it doesn't matter.
 */

#define CONFIG_OFFSET_P 1 /* don't normally use offset memory */

extern int ROMlib_launch_native_app(int n_filenames, char **filenames);

#endif /* !_OS_LINUX_H_ */
