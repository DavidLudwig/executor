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
  - Removed old, no longer functioning, DOS, NeXT and Windows ports. (Windows
    still works, though, via Qt and SDL ports.)
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
posession of it.  Other contributors include:

  - David Ludwig (https://github.com/DavidLudwig) - Windows build fixes

Building and Running
--------------------

Needs a modern C++ compiler, CMake, and Qt 5, plus 'perl' and 'bison' commands.
SDL2, SDL1.2 or X11 libraries are required to build some of the alternate front-ends.

```
git submodule init
git submodule update
mkdir build
cd build
cmake ..
cmake --build .
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


### Microsoft Windows

Executor 2000 can be built for Microsoft Windows, albeit only as a 32-bit
Windows Desktop (i.e. Win32) application.  The build procedure is nearly the
same, however, some additional dependencies need to be installed, and then,
some extra options need to be passed into CMake.

The following procedure has been used to prepare a Windows 10 machine to build
Executor 2000 as a Windows app.  Some of Microsoft's
[free, time-use-limited Windows VM images](https://developer.microsoft.com/en-us/windows/downloads/virtual-machines)
have, in the past, been sufficient to build Executor 2000.  If you try this,
please note that these VMs did have some, but not all of the above dependencies
already installed.

Command line steps be performed once Visual Studio 2017 is installed, by
running the Start Menu searchable program, ```Developer Command Prompt for VS 2017```

1. install [Git](https://git-scm.com/download)
2. install [CMake](https://cmake.org/download/)
3. install [LLVM](http://releases.llvm.org/download.html). (8.0.0, pre-built
   binaries have been used, for example.)
4. install [Visual Studio 2017](https://visualstudio.microsoft.com/), plus it's
   [Desktop Development with C++](https://docs.microsoft.com/en-us/cpp/build/vscpp-step-0-installation?view=vs-2017)
   option 
5. install the VS-2017 add-on,
   [LLVM Compiler Toolchain](https://marketplace.visualstudio.com/items?itemName=LLVMExtensions.llvm-toolchain)
6. install perl and bison, via MinGW-get's packages for
   ```msys-bison-bin``` and ```msys-perl-bin``` - [MinGW Getting Started](http://www.mingw.org/wiki/getting_started)
7. add perl and bison to path.  If MinGW is installed to C:\MinGW, the
   directory to add to PATH should be, ```C:\MinGW\msys\1.0\bin```.
8. install [vcpkg](https://github.com/Microsoft/vcpkg).  Installation to
   ```C:\vcpkg``` is recommended (but not strictly required, perhaps).
9. use vcpkg to build and install Qt5, SDL2, and Boost.  This may take
   multiple hours to perform (due primarily to Qt).
   1. ```cd C:\vcpkg```
   2. ```vcpkg install sdl2 boost-filesystem boost-system qt5-base```

With the above steps performed, building was done using the normal procedure,
as listed above), but with the cmake-configuration step (aka. ```cmake ..```)
being issued as such:

```cmake -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -T"llvm" ..```

The important and new parts here are:

  - ```-DCMAKE_TOOLCHAIN_FILE=...```: lets the build system know about the
    vcpkg-installed dependencies.
  - ```-T"llvm"```: lets the build system know to use Clang to compile
    C/C++ code (rather than Microsoft's Visual C++ engine, for example).

By default, the build step (of ```cmake --build .```) will generate a Debug
build.  To make a Release build, run the build step as such:

```cmake --build . --config Release```

The resulting ```executor.exe``` will be in the ```Release``` sub-folder
(or ```Debug```, for Debug builds).
