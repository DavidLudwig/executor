/* Copyright 1988 - 2005 by Abacus Research and
 * Development, Inc.  All rights reserved.
 */

/*
 * Errors should be handled more cleanly.
 */

#include <base/common.h>


#include <FontMgr.h>
#include <DialogMgr.h>
#include <AppleEvents.h>


#include <quickdraw/cquick.h>
#include <textedit/tesave.h>
#include <mman/mman.h>
#include <menu/menu.h>
#include <prefs/prefs.h>
#include <commandline/flags.h>
#include <commandline/option.h>
#include <time/syncint.h>
#include <vdriver/vdriver.h>
#include <time/vbl.h>
#include <time/time.h>
#include <rsys/segment.h>
#include <rsys/version.h>
#include <base/m68kint.h>
#include <quickdraw/rgbutil.h>
#include <vdriver/refresh.h>
#include <rsys/executor.h>
#include <wind/wind.h>
#include <osevent/osevent.h>
#include <quickdraw/image.h>
#include <rsys/dump.h>
#include <file/file.h>
#include <ctl/ctl.h>
#include <commandline/parseopt.h>
#include <print/print.h>
#include <mman/memsize.h>
#include <vdriver/autorefresh.h>
#include <sound/sounddriver.h>
#include <hfs/dcache.h>
#include <error/system_error.h>
#include <commandline/option.h>
#include <base/emustubs.h>
#include <sane/float.h>
#include <rsys/paths.h>

#include <rsys/os.h>
#include <rsys/arch.h>
#include <rsys/gestalt.h>
#include <rsys/launch.h>
#include <quickdraw/text.h>
#include <rsys/appearance.h>
#include <hfs/hfs_plus.h>
#include <base/cpu.h>
#include <base/debugger.h>
#include <rsys/mon_debugger.h>
#include <PowerCore.h>

#include "default_vdriver.h"

#if defined(LINUX) && defined(PERSONALITY_HACK)
#include <sys/personality.h>
#define READ_IMPLIES_EXEC 0x0400000
#endif



static void setstartdir(char *);

#include <ctype.h>

#if !defined(WIN32)
#include <sys/wait.h>
#endif

#if defined(CYGWIN32)
#include "winfs.h"
#include "dosdisk.h"
#include "win_except.h"
#include "win_queue.h"
#include "win_clip.h"
#include "win_print.h"
#endif

#include <vector>

using namespace Executor;
using namespace std;


/* Set to true if there was any error parsing arguments. */
static bool bad_arg_p = false;

/* Set to false when we know that the command line switches will result
   in no graphics. */

static bool graphics_p = true;

static bool use_native_code_p = true;
static bool breakOnProcessStart = false;
static bool logtraps = false;
static std::string keyboard;
static bool list_keyboards_p = false;

static const option_vec common_opts = {
    { "sticky", "sticky menus", opt_no_arg, "" },
    { "nobrowser", "don't run Browser", opt_no_arg, "" },
    { "bpp", "default screen depth", opt_sep, "" },
    { "size", "default screen size", opt_sep, "" },
    { "debug",
      ("enable certain debugging output and consistency checks.  This "
       "is primarily used by ARDI developers, but we are making it "
       "available during the pre-beta period to expert users.  The next "
       "argument must be a list of comma-separated words describing which "
       "debug options you want enabled.  You can abbreviate the debug "
       "options as long as the abbreviation is unambiguous.  Here is a "
       "list of the options (some of which will may do nothing):  "
       "\"all\" enables all debugging options, "
       "\"fslog\" enables filesystem call logging, "
       "\"memcheck\" enables heap consistency checking (slow!), "
       "\"textcheck\" enables text record consistency checking (slow!), "
       "\"trace\" enables miscellaneous trace information, "
       "\"sound\" enables miscellaneous sound logging information, "
       "\"trapfailure\" enables warnings when traps return error codes, "
       "\"errno\" enables some C library-related warnings, "
       "\"unexpected\" enables warnings for unexpected events, "
       "\"unimplemented\" enables warnings for unimplemented traps.  "
       "Example: \"executor -debug unimp,trace\""),

      opt_sep, "" },
    { "nodiskcache", "disable internal disk cache.",
      opt_no_arg, "" },
    { "nosound", "disable any sound hardware",
      opt_no_arg, "" },

#if defined(MSDOS) || defined(CYGWIN32)
    { "macdrives", "drive letters that represent Mac formatted media",
      opt_sep, "" },
    { "dosdrives", "drive letters that represent DOS formatted media",
      opt_sep, "" },
    { "skipdrives", "drive letters that represent drives to avoid",
      opt_sep, "" },
#endif
#if defined(LINUX)
    { "nodrivesearch",
      "Do not look for a floppy drive, CD-ROM drive or any other drive "
      "except as specified by the MacVolumes environment variable",
      opt_no_arg, "" },
#endif /* LINUX */
    { "keyboards", "list available keyboard mappings",
      opt_no_arg, "" },
    { "keyboard", "choose a specific keyboard map", opt_sep, "" },
    { "print",
      "tell program to print file; not useful unless you also "
      "specify a program to run and one or more documents to print.",
      opt_no_arg, "" },

#if 0
  { "noclock",     "disable timer",               opt_no_arg,   "" },
#endif

    { "noautorefresh",
      "turns off automatic detection of programs that bypass QuickDraw.",
      opt_no_arg, "" },
    {
        "refresh",
        "handle programs that bypass QuickDraw, at a performance penalty.  "
        "Follow -refresh with an number indicating how many 60ths of a second "
        "to wait between each screen refresh, e.g. \"executor -refresh 10\".",
        opt_optional, "10",
    },
    {
        "help", "print this help message", opt_no_arg, "",
    },
    {
        "version", "print the Executor version", opt_no_arg, "",
    },
    {
        "memory",
        "specify the total memory you want reserved for use by the programs "
        "run under Executor and for Executor's internal system software.  "
        "For example, \"executor -memory 5.5M\" would "
        "make five and a half megabytes available to the virtual machine.  "
        "Executor will require extra memory above and beyond this amount "
        "for other uses.",
        opt_sep, "",
    },
    {
        "applzone",
        "specify the memory to allocate for the application being run, "
        "e.g. \"executor -applzone 4M\" would make four megabytes "
        "of RAM available to the application.  \"applzone\" stands for "
        "\"application zone\".",
        opt_sep, "",
    },
    {
        "stack",
        "like -applzone, but specifies the amount of stack memory to allocate.",
        opt_sep, "",
    },
    {
        "syszone", "like -applzone, but specifies the amount of memory to make "
                   "available to Executor's internal system software.",
        opt_sep, "",
    },
    { "system",
      "specify the system version that executor reports to applications",
      opt_sep, "" },
    { "notnative",
      "don't use native code in syn68k",
      opt_no_arg, "" },

    { "grayscale", "\
specify that executor should run in grayscale mode even if it is \
capable of color.",
      opt_no_arg, "" },

    { "cities", "Don't use Helvetica for Geneva, Courier for Monaco and Times "
                "for New York when generating PostScript",
      opt_no_arg, "" },

#if defined(CYGWIN32)
    { "die", "allow Executor to die instead of catching trap", opt_no_arg,
      "" },
    { "noautoevents", "disable timer driven event checking", opt_no_arg,
      "" },
#endif

    { "prvers",
      "specify the printer version that executor reports to applications",
      opt_sep, "" },

    { "prres",
      "specify an additional resolution available for printing, e.g. "
      "\"executor -prres 600x600\" will make 600dpi resolution available "
      "in addition to the standard 72dpi.  Not all apps will be able to use "
      "this additional resolution.",
      opt_sep, "" },

#if defined(SDL)
    { "fullscreen", "try to run in full-screen mode", opt_no_arg, "" },
    { "hwsurface", "UNSUPPORTED", opt_no_arg, "" },
#if defined(CYGWIN)
    {
        "sdlaudio", "specify the audio driver to attempt to use first, e.g. "
                    "\"executor -sdlaudio waveout\" will tell Executor to use the old "
                    "windows sound drivers and not use DirectSound.",
        opt_sep, "",
    },
#else
    {
        "sdlaudio", "specify the audio driver to attempt to use first, e.g. "
                    "\"executor -sdlaudio esd\" will tell Executor to use esound, "
                    "the Enlightened Sound Daemon instead of /dev/dsp.",
        opt_sep, "",
    },
#endif
#endif

#if defined(CYGWIN32) && defined(SDL)
    { "clipleak", "UNSUPPORTED (ignored)", opt_no_arg, "" },
#endif

    { "scancodes", "different form of key mapping (may be useful in "
                   "conjunction with -keyboard)",
      opt_no_arg, "" }, // FIXME: only applies to some vdrivers (X11, SDL1 on CYGWIN)

    { "ppc", "try to execute the PPC native code if possible (UNSUPPORTED)", opt_no_arg, "" },

    { "appearance", "(mac or windows) specify the appearance of windows and "
                    "menus.  For example \"executor -appearance windows\" will make each "
                    "window have a blue title bar",
      opt_sep, "" },

    { "hfsplusro", "unsupported -- do not use", opt_no_arg, "" },

    { "logtraps", "print every operating system and toolbox calls and their arguments", opt_no_arg, "" },
    { "speech", "enable speech manager (mac hosts only)", opt_no_arg, ""},
    { "break", "break into debugger at program start", opt_no_arg, ""}
};

/* Prints the specified value in a representation appropriate for command
 * line switches.
 */
static void
print_command_line_value(FILE *fp, int v)
{
    if(v > 0 && v % 1024 == 0)
    {
        if(v % (1024 * 1024) == 0)
            fprintf(fp, "%dM", v / (1024 * 1024));
        else
            fprintf(fp, "%dK", v / 1024);
    }
    else
        fprintf(fp, "%d", v);
}

static void
check_arg(string argname, int *arg, int min, int max)
{
    if(*arg < min || *arg > max)
    {
        fprintf(stderr, "%s: invalid value for `%s': must be between ",
                ROMlib_appname.c_str(), argname.c_str());
        print_command_line_value(stderr, min);
        fputs(" and ", stderr);
        print_command_line_value(stderr, max);
        fputs(" inclusive.\n", stderr);
        bad_arg_p = true;
    }
}


#if !defined(LINUX)
#define SHELL "/bin/sh"
#define WHICH "which "
#else
#define SHELL "/bin/bash"
#define WHICH "type -path "
#endif

static void
set_appname(char *argv0)
{
    char *p = strrchr(argv0, '/');
#if defined(MSDOS) || defined(CYGWIN32)
    if(!p)
        p = strrchr(argv0, '\\');
#endif
    if(p)
        ++p;
    else
        p = argv0;
    ROMlib_appname = p;
}

static void setstartdir(char *argv0)
{
#if !defined(WIN32)
    LONGINT p[2], pid;
    char buf[MAXPATHLEN];
    INTEGER nread, arg0len;
    char *lookhere, *suffix, *whichstr;

    if(argv0[0] == '/' || Uaccess(argv0, X_OK) == 0)
        lookhere = argv0;
    else
    {
        pipe(p);
        if((pid = fork()) == 0)
        {
            close(1);
            dup(p[1]);
            arg0len = strlen(argv0);
            whichstr = (char *)alloca(arg0len + sizeof(WHICH));
            memmove(whichstr, WHICH, sizeof(WHICH) - 1);
            memmove(whichstr + sizeof(WHICH) - 1, argv0, arg0len + 1);
            execl(SHELL, "sh", "-c", whichstr, (char *)0);
            fprintf(stderr, "``%s -c \"%s\"'' failed\n", SHELL, whichstr);
            fflush(stderr);
            _exit(127);
/* NOTREACHED */
#if !defined(LETGCCWAIL)
            lookhere = 0;
#endif /* LETGCCWAIL */
        }
        else
        {
            close(p[1]);
            nread = read(p[0], buf, sizeof(buf) - 1);
            waitpid(pid, nullptr, 0);
            if(nread)
                --nread; /* get rid of trailing \n */
            buf[nread] = 0;
            lookhere = buf;
        }
    }
    suffix = rindex(lookhere, '/');
    if(suffix)
        *suffix = 0;
    auto savedir = fs::current_path();
    chdir(lookhere);
    if(suffix)
        *suffix = '/';
    ROMlib_startdir = fs::current_path();
    fs::current_path(savedir);
    
#else /* defined(MSDOS) || defined(CYGWIN32) */
    // TODO: replace by GetModuleFilename()
    ROMlib_startdir = fs::canonical(argv0).parent_path();
#endif /* defined(MSDOS) */
}


/* This is to tell people about the switch from "-applzone 4096" to
 * "-applzone 4M".
 */
static void
note_memory_syntax(const char *arg, unsigned val)
{
    /* Make sane error messages when the supplied value is insane.
   * Otherwise, try to print an example that illustrates how to
   * use the value they are trying to generate.
   */
    if(val < 100 || val > 1000000)
        val = 2048;

    fprintf(stderr, "Specified %s is too small. "
                    "For a %u.%02u\nmegabyte %s you would "
                    "say \"-%s ",
            arg, val / 1024, (val % 1024) * 100 / 1024,
            arg, arg);

    if(val % 1024 == 0)
        fprintf(stderr, "%uM", val / 1024);
    else if(val < 1024)
        fprintf(stderr, "%uK", val);
    else
        fprintf(stderr, "%u.%02uM", val / 1024, (val % 1024) * 100 / 1024);

    fputs("\"\n", stderr);

    bad_arg_p = true;
}

/* Concatenates all strings in the array, separated by spaces. */
static const char *
construct_command_line_string(int argc, char **argv)
{
    char *s;
    unsigned long string_length;
    int i, j;

    for(i = 0, string_length = 0; i < argc; i++)
        string_length += strlen(argv[i]) + 1;
    s = (char *)malloc(string_length + 1);
    s[0] = '\0';
    for(j = 0; j < argc; j++)
        sprintf(s + strlen(s), "%s%s", j ? " " : "", argv[j]);
    return s;
}

static void parseCommandLine(int& argc, char **argv)
{
    opt_database_t common_db;
    string arg;

    opt_init();
    common_db = opt_alloc_db();
    opt_register("common", common_opts);

    opt_register_pre_note("welcome to the executor help message.");
    opt_register_pre_note("usage: `executor [option...] "
                          "[program [document1 document2 ...]]'");

    vdriver->registerOptions();

    if(!bad_arg_p)
        bad_arg_p = opt_parse(common_db, common_opts,
                              &argc, argv);

    if(opt_val(common_db, "version", nullptr))
    {
        fprintf(stdout, "%s\n", EXECUTOR_VERSION);
        exit(0);
    }

    /*
   * If they ask for help, it's not an error -- it should go to stdout
   */

    if(opt_val(common_db, "help", nullptr))
    {
        fprintf(stdout, "%s", opt_help_message());
        exit(0);
    }

    /* Verify that the user input a legal bits per pixel.  "0" is a legal
   * value here; that means "use the vdriver's default."
   */
    opt_int_val(common_db, "bpp", &flag_bpp, &bad_arg_p);
    if(flag_bpp != 0 && flag_bpp != 1
       && flag_bpp != 2 && flag_bpp != 4 && flag_bpp != 8
       && flag_bpp != 16 && flag_bpp != 32)
    {
        fprintf(stderr, "Bits per pixel must be 1, 2, 4, 8, 16 or 32.\n");
        bad_arg_p = true;
    }

#if defined(SDL)
    if(opt_val(common_db, "fullscreen", nullptr))
        ROMlib_fullscreen_p = true;

    if(opt_val(common_db, "hwsurface", nullptr))
        ROMlib_hwsurface_p = true;
#endif

    if(opt_val(common_db, "scancodes", nullptr))
        vdriver->setUseScancodes(true);

#if defined(Sound_SDL_Sound)

    if(opt_val(common_db, "sdlaudio", &arg))
        ROMlib_set_sdl_audio_driver_name(arg);

#endif

    if(opt_val(common_db, "ppc", nullptr))
        ROMlib_set_ppc(true);

    if(opt_val(common_db, "hfsplusro", nullptr))
        ROMlib_hfs_plus_support = true;

    if(opt_val(common_db, "size", &arg))
        bad_arg_p |= !parse_size_opt("size", arg);

    if(opt_val(common_db, "prres", &arg))
        bad_arg_p |= !parse_prres_opt(&ROMlib_optional_res_x,
                                      &ROMlib_optional_res_y, arg);

    if(opt_val(common_db, "debug", &arg))
        bad_arg_p |= !error_parse_option_string(arg);

#if defined(MACOSX)
    // sync() really takes a long time on Mac OS X.
    ROMlib_nosync = true;
#endif

    {
        int skip;
        skip = 0;
        opt_int_val(common_db, "nosound", &skip, &bad_arg_p);
        sound_disabled_p = (skip != 0);
    }

    {
        int nocache;
        nocache = 0;
        opt_int_val(common_db, "nodiskcache", &nocache, &bad_arg_p);
        dcache_set_enabled(!nocache);
    }

    use_native_code_p = !opt_val(common_db, "notnative", nullptr);

    substitute_fonts_p = !opt_val(common_db, "cities", nullptr);

#if defined(CYGWIN32)
    if(opt_val(common_db, "die", nullptr))
        uninstall_exception_handler();
    if(opt_val(common_db, "noautoevents", nullptr))
        set_timer_driven_events(false);
#endif

    /* Parse the "-memory" option. */
    {
        int total_memory;
        if(opt_int_val(common_db, "memory", &total_memory, &bad_arg_p))
        {
            check_arg("memory", &total_memory,
                      (MIN_APPLZONE_SIZE + DEFAULT_SYSZONE_SIZE
                       + DEFAULT_STACK_SIZE),
                      (MAX_APPLZONE_SIZE + DEFAULT_SYSZONE_SIZE
                       + DEFAULT_STACK_SIZE));

            /* Set up the three memory sizes appropriately.  For now we
       * just allocate the defaults for syszone and stack, and
       * put everything else in -applzone.
       */
            ROMlib_syszone_size = DEFAULT_SYSZONE_SIZE;
            ROMlib_stack_size = DEFAULT_STACK_SIZE;
            ROMlib_applzone_size = (total_memory - ROMlib_syszone_size
                                    - ROMlib_stack_size);
        }
    }

    /* I bumped the minimal ROMlib_applzone to 512, since Loser needs
   more than 256.  I guess it's a little unfair to people who bypass
   Loser, but it will prevent confusion.  */
    opt_int_val(common_db, "applzone", &ROMlib_applzone_size, &bad_arg_p);
    if(ROMlib_applzone_size < 65536)
        note_memory_syntax("applzone", ROMlib_applzone_size);
    else
        check_arg("applzone", &ROMlib_applzone_size, MIN_APPLZONE_SIZE,
                  MAX_APPLZONE_SIZE);

    opt_int_val(common_db, "syszone", &ROMlib_syszone_size, &bad_arg_p);
    if(ROMlib_syszone_size < 65536)
        note_memory_syntax("syszone", ROMlib_syszone_size);
    else
        check_arg("syszone", &ROMlib_syszone_size, MIN_SYSZONE_SIZE,
                  MAX_SYSZONE_SIZE);

    opt_int_val(common_db, "stack", &ROMlib_stack_size, &bad_arg_p);
    if(ROMlib_stack_size < 32768)
        note_memory_syntax("stack", ROMlib_stack_size);
    else
        check_arg("stack", &ROMlib_stack_size, MIN_STACK_SIZE, MAX_STACK_SIZE);


    if(opt_val(common_db, "keyboards", nullptr))
        graphics_p = false;


    opt_bool_val(common_db, "sticky", &ROMlib_sticky_menus_p, &bad_arg_p);
    opt_bool_val(common_db, "nobrowser", &ROMlib_nobrowser, &bad_arg_p);
    opt_bool_val(common_db, "print", &ROMlib_print, &bad_arg_p);
    opt_bool_val(common_db, "speech", &ROMlib_speech_enabled, &bad_arg_p);
#if 0
  opt_int_val (common_db, "noclock",     &ROMlib_noclock,   &bad_arg_p);
#endif
    {
        int no_auto = false;
        opt_int_val(common_db, "noautorefresh", &no_auto, &bad_arg_p);
        do_autorefresh_p = !no_auto;
    }

    opt_int_val(common_db, "refresh", &ROMlib_refresh, &bad_arg_p);
    check_arg("refresh", &ROMlib_refresh, 0, 60);

    opt_bool_val(common_db, "grayscale", &flag_grayscale, &bad_arg_p);

#if defined(LINUX)
    opt_bool_val(common_db, "nodrivesearch", &nodrivesearch_p, &bad_arg_p);
#endif

    {
        string str;

        if(opt_val(common_db, "prvers", &str))
        {
            uint32_t vers;

            if(!ROMlib_parse_version(str, &vers))
                bad_arg_p = true;
            else
                ROMlib_PrDrvrVers = (vers >> 8) * 10 + ((vers >> 4) & 0xF);
        }
    }

    {
        string appearance_str;

        if(opt_val(common_db, "appearance", &appearance_str))
            bad_arg_p |= !ROMlib_parse_appearance(appearance_str.c_str());
    }


    /* parse the `-system' option */
    {
        string system_str;

        if(opt_val(common_db, "system", &system_str))
            bad_arg_p |= !parse_system_version(system_str);
    }

    opt_bool_val(common_db, "break", &breakOnProcessStart, &bad_arg_p);
    opt_bool_val(common_db, "logtraps", &logtraps, &bad_arg_p);

    opt_val(common_db, "keyboard", &keyboard);
    opt_bool_val(common_db, "keyboards", &list_keyboards_p, &bad_arg_p);

    /* If we failed to parse our arguments properly, exit now.
   * I don't think we should call ExitToShell yet because the
   * rest of the system isn't initialized.
   */
    if(argc >= 2)
    {
        int a;

        /* Only complain if we see something with a leading dash; anything
	 * else might be a file to launch.
	 */
        for(a = 1; a < argc; a++)
        {
            if(argv[a][0] == '-')
            {
                fprintf(stderr, "%s: unknown option `%s'\n",
                        ROMlib_appname.c_str(), argv[a]);
                bad_arg_p = true;
            }
        }
    }

    if(bad_arg_p)
    {
        fprintf(stderr,
                "Type \"%s -help\" for a list of command-line options.\n",
                ROMlib_appname.c_str());
        exit(-10);
    }
}


int main(int argc, char **argv)
{
    char thingOnStack; /* used to determine an approximation of the stack base address */

#if defined(LINUX) && defined(PERSONALITY_HACK)
    int pers;

    // TODO: figure out how much of this is still necessary.
    // MMAP_PAGE_ZERO should be unnecessary now,
    // but the 32-bit optimized assembly stuff might need READ_IMPLIES_EXEC.
    pers = personality(0xffffffff);
    if((pers & MMAP_PAGE_ZERO) == 0)
    {
        if(personality(pers | MMAP_PAGE_ZERO | READ_IMPLIES_EXEC) == 0)
            execv(argv[0], argv);
    }
#endif

    ROMlib_command_line = construct_command_line_string(argc, argv);

    if(!arch_init())
    {
        fprintf(stderr, "Unable to initialize CPU information.\n");
        exit(-100);
    }

    if(!os_init())
    {
        fprintf(stderr, "Unable to initialize operating system features.\n");
        exit(-101);
    }

    /* Guarantee various time variables are set up properly. */
    msecs_elapsed();

    setstartdir(argv[0]);
    set_appname(argv[0]);

    vdriver = new DefaultVDriver();
    if(!vdriver->parseCommandLine(argc, argv))
    {
        fprintf(stderr, "Unable to initialize video driver.\n");
        exit(-12);
    }

    parseCommandLine(argc, argv);

    InitMemory(&thingOnStack);

    initialize_68k_emulator(nullptr,
                            use_native_code_p,
                            (uint32_t *)SYN68K_TO_US(0),
                            0);

    EM_A7 = ptr_to_longint(LM(CurStackBase));

    Executor::traps::init(logtraps);
    InitLowMem();

    if(graphics_p)
        ROMlib_InitGDevices();
    
    ROMlib_eventinit();
    hle_init();

    ROMlib_fileinit();

    InitUtil();

    InitResources();
    
    ROMlib_set_system_version(system_version);

    {
        bool keyboard_set_failed = false;

        if(!keyboard.empty())
        {
            keyboard_set_failed = !ROMlib_set_keyboard(keyboard.c_str());
            if(keyboard_set_failed)
                printf("``%s'' is not an available keyboard\n", keyboard.c_str());
        }

        if(keyboard_set_failed || list_keyboards_p)
            display_keyboard_choices();
    }

    InitAppFiles(argc, argv);

    InitFonts();

#if !defined(NDEBUG)
    dump_init(nullptr);
#endif

    ROMlib_color_init();

    wind_color_init();
    image_inits();  // must be called after `ROMlib_color_init ()'
    sb_ctl_init();  //  must be after `image_inits ()'

    AE_init();

    {
        INTEGER env = 0;
        ROMlib_Fsetenv(&env, 0);
    }

    syncint_init(); // timer interrupts: must not be inited before cpu & trapvevtors

    sound_init();

    set_refresh_rate(ROMlib_refresh);

#if defined(CYGWIN32)
    complain_if_no_ghostscript();
#endif
    InitMonDebugger();
    base::Debugger::instance->setBreakOnProcessEntry(breakOnProcessStart);

    executor_main();

    ExitToShell();
    /* NOT REACHED */
    return 0;
}
