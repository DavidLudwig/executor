Executor 2000
=============

This is a modernized (as of 2019) fork of Executor, the "original" version
of which you can still find upstream at https://github.com/ctm/executor.
The home of the "Executor 2000" fork is at https://github.com/autc04/executor.

Why "2000"? Because that's what we used to call the future.

Executor was a commercially available Mac emulator in the 90s -
what makes it different from other Mac emulators is that it doesn't need a
ROM file or any original Apple software to run; rather, it attempts to
re-implement the classic Mac OS APIs, just as WINE does for Windows.

Executor 2000 feature highlights:

  - Builds and runs on modern 64-bit Linux and macOS (Windows support is planned).
  - Rootless - emulated windows are part of your desktop.
  - PowerPC support (well, not many Apps will run, but it's there)
  - Support for native Mac resource forks (Mac version)
  - Exchange files with Basilisk & SheepShaver
  - Lots of code cleanup, according to my own definition of "clean"
  - included debugger based on cxmon from Basilisk
  - started a test suite
  - Removed old, no longer functioning, DOS, NeXT and Windows ports.
  - I probably broke lots of other things that used to work.

You can reach the maintainer of this fork at wolfgang.thaller@gmx.net or via
the github issues page at https://github.com/autc4/executor/issues.

License
-------

MIT-Style license, see COPYING.

The cxmon monitor, on which Executor's new built-in debugger is based,
is released under GPL v2+, so you are essentially bound by the GPL's terms
for all of Executor; however, cxmon is an optional component, which can easily
be removed should a non-copyleft version of Executor be needed.

Credits
-------

Executor was originally developped at Abacus Research and Development, Inc (ARDI)
and initially released in 1990. In 2008, Clifford Matthews (https://github.com/ctm)
open-sourced Executor under a MIT license.
The original Credits read:

```
Bill Goldman - Browser, Testing
Mat Hostetter - Syn68k, Low Level Graphics, DOS port, more...
Joel Hunter - Low Level DOS Sound
Sam Lantinga - Win32 port
Patrick LoPresti - High Level Sound, Low Level Linux Sound
Cliff Matthews - this credit list (and most things not listed)
Cotton Seed - High Level Graphics, Apple Events, more...
Lauri Pesonen - Low Level Win32 CD-ROM access (Executor 2.1)
Samuel Vincent - Low Level DOS Serial Port Support
and all the engineers and testers who helped us build version 1.x

Windows Appearance:

The windows appearance option uses "Jim's CDEFs" copyright Jim Stout and the "Infinity Windoid" copyright Troy Gaul.

Primary Pre-Beta Testers:

Jon Abbott - Testing, Icon Design
Ziv Arazi - Testing
Edmund Ronald - Advice, Testing
K. Harrison Liang - Testing
Hugh Mclenaghan - Testing
Emilio Moreno - Testing, Spanish Translation + Keyboard, Icon Design
Ernst Oud - Documentation, Testing
```

After Executor's open-sourcing, C.W. Betts (https://github.com/MaddTheSane)
picked it up and ported the whole thing from plain C to C++.
And after that, yours truly, Wolfgang Thaller (https://github.com/autc04) took
posession of it...

Building and Running
--------------------

Needs a modern C++ compiler, CMake, and Qt 5.
SDL2, SDL1.2 or X11 libraries are required to build some of the alternate front-ends.

```
git submodule init
git submodule update
mkdir build
cd build
cmake ..
make
```

When `./build/src/executor` is first invoked, it will automatically install its
fake Mac system file and the `Browser` finder replacement to `~/.executor/`, so
no further setup should be needed.

If you're using Wayland, you will need to force Qt to use its X11 backend, as their
Wayland backend hopelessly fails for executor:

```
export QT_QPA_PLATFORM=xcb
```

Executor 2000 should be able to use native Mac files on macOS, AppleDouble 
file pairs (`foo` and `%foo`) as used by older executor verions, as well as
files written by Basilisk or SheepShaver (`foo`, `.rsrc/foo` and `.finf/foo`).
