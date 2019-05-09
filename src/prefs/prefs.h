#if !defined(__RSYS_PREFS__)
#define __RSYS_PREFS__

#include <string>
#include <stdint.h>
#include <ExMacTypes.h>

namespace Executor
{

typedef enum {
    WriteAlways,
    WriteInBltrgn,
    WriteInOSEvent,
    WriteAtEndOfTrap,
    WriteNever
} WriteWhenType; /* This is an extension */

typedef enum {
    soundoff,
    soundpretend,
    soundon
} sound_t;


extern WriteWhenType ROMlib_when;
extern sound_t ROMlib_PretendSound;
extern bool ROMlib_cacheheuristic;
extern int ROMlib_clock;
extern bool ROMlib_directdiskaccess;
extern bool ROMlib_flushoften;
extern bool ROMlib_fontsubstitution;
extern bool ROMlib_newlinetocr;
extern bool ROMlib_nowarn32;
extern bool ROMlib_passpostscript;
extern int ROMlib_refresh;
extern int ROMlib_delay;
extern bool ROMlib_noclock;

extern bool ROMlib_pretend_help;
extern bool ROMlib_pretend_alias;
extern bool ROMlib_pretend_script;
extern bool ROMlib_pretend_edition;
extern bool ROMlib_speech_enabled;

extern bool ROMlib_sticky_menus_p;

extern bool nodrivesearch_p;

extern bool do_autorefresh_p;
extern bool log_err_to_ram_p;

extern bool ROMlib_print;
extern uint32_t ROMlib_PrDrvrVers;

extern uint32_t system_version;

#define ROMLIB_DEBUG_BIT (1 << 1)

extern void ROMlib_WriteWhen(WriteWhenType when);

extern void do_dump_screen(void);
extern std::string ROMlib_configfilename;
extern FILE *configfile;

int saveprefvalues(const char *savefilename);
void ParseConfigFile(std::string appname, OSType type);


#define C_STRING_FROM_SYSTEM_VERSION()        \
    ({                                        \
        char *retval;                         \
        retval = (char *)alloca(9);           \
        sprintf(retval, "%d.%d.%d",           \
                (system_version >> 8) & 0xF,  \
                (system_version >> 4) & 0xF,  \
                (system_version >> 0) & 0xF); \
        retval;                               \
    })

}

#endif /* !defined(__RSYS_PREFS__) */
