#include <prefs/options.h>
#include <commandline/flags.h>
#include <prefs/prefs.h>

// This file contains the definitions for the strange set of
// global variables whose declarations are spread between flags.h, options.h and prefs.h
// according to some inscrutable criteria.

// These variables influence Executor's behaviour in various ways.
// They are set at some time between Executor bootup and Launch(),
// and are controlled by one or more of
//  - command line arguments
//  - hard-coded constants
//  - the application's SIZE resource
//  - hard-coded special cases based on the application's signature
//  - per-application options files (edited via the command-shift-5 "prefs" dialog).

// They are collected here to simplify executor's inter-module dependency structure,
// and in the hopes of cleaning up at some point in the future.

namespace Executor
{
// prefs.h 
WriteWhenType ROMlib_when = WriteInOSEvent;
sound_t ROMlib_PretendSound = soundpretend;
bool ROMlib_cacheheuristic = false;     // unused
//int ROMlib_clock;   // maybe this is actually internal state of vbl.cpp
bool ROMlib_directdiskaccess = false;
bool ROMlib_flushoften = false;         // ROMlib_destroy_blocks always flushes all blocks

bool ROMlib_fontsubstitution = false;    // printing
bool ROMlib_passpostscript = true;

bool ROMlib_newlinetocr = true; /* 1 means map '\n' to '\r' in newline mode */
bool ROMlib_nowarn32 = false;
int ROMlib_refresh = 0;
int ROMlib_delay = 0;  /* number of ticks to wait when we haven't gotten anything interesting */
bool ROMlib_noclock = false;
bool ROMlib_pretend_help = false;
bool ROMlib_pretend_alias = false;
bool ROMlib_pretend_script = false;
bool ROMlib_pretend_edition = false;
bool ROMlib_speech_enabled = false;

/* the system version that executor is currently reporting to
   applications, set through the `-system' options.  contains the
   version number in the form `0xABC' which corresponds to system
   version A.B.C */
uint32_t system_version = 0x700; /* keep this in sync with Browser's .ecf file */

std::string ROMlib_configfilename;
FILE *configfile;   // hidden parameter for options parser

// flags.h

/* false if we are blitting straight to screen memory, true if we are
   blitting to a shadow screen */
bool ROMlib_shadow_screen_p = true; // currently always remains true

/* Initial screen size.  This can be changed dynamically. */
/* 0 means "use default". */
int flag_width, flag_height;

/* Initial bits per pixel.  The screen depth can be changed dynamically. */
/* 0 means "use default". */
int flag_bpp;

/* Initial grayscale flag. */
bool flag_grayscale;

/* Approximate command line; argv[] elements separated by spaces. */
const char *ROMlib_command_line;

/* 0 means try running browser, 1 means don't */
bool ROMlib_nobrowser = false;

/* true if there is a version skew between the system file version and
   the required system version.  set by `InitResources ()', and used
   by `InitWindows ()' */
bool system_file_version_skew_p = false;

bool ROMlib_dirtyvariant = false;   // unused

// options.h

std::pair<int,int> ROMlib_ScreenSize = { INITIALPAIRVALUE, INITIALPAIRVALUE };  // unused
std::pair<int,int> ROMlib_MacSize = { INITIALPAIRVALUE, INITIALPAIRVALUE };     // unused
std::pair<int,int> ROMlib_ScreenLocation = { INITIALPAIRVALUE, INITIALPAIRVALUE };  // unused
int32_t ROMlib_options = 0;
std::string ROMlib_WindowName;
std::string ROMlib_Comments;      // unused on purpose
int ROMlib_desired_bpp = 0;

bool ROMlib_sticky_menus_p = false;

bool ROMlib_print = false;

bool nodrivesearch_p = false;
bool do_autorefresh_p = false;

uint32_t ROMlib_PrDrvrVers = 70;

/* true iff we should try to log errors to RAM when possible. */
bool log_err_to_ram_p = false;
}