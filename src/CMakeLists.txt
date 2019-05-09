find_program(PERL_COMMAND perl)
if(NOT PERL_COMMAND)
    message(FATAL_ERROR "Cannot find command: 'perl' (needed for building!)")
endif()

find_package(BISON REQUIRED)
message("BISON_EXECUTABLE: (${BISON_EXECUTABLE})")
message("BISON_VERSION: (${BISON_VERSION})")
message("BISON_FOUND: (${BISON_FOUND})")

cmake_policy(SET CMP0074 NEW) # don't ignore _ROOT variables (Boost_ROOT)
find_package(Boost REQUIRED COMPONENTS filesystem system)

find_package(Threads REQUIRED)

include_directories(include/)
include_directories(.)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(FRONT_END qt CACHE STRING "which front-end to use (qt, sdl2, sdl, x)")
set(HOST_ARCH generic_le)

if(WIN32)
set(HOST_OS "win32")
elseif(APPLE AND UNIX)
set(HOST_OS "macosx")
elseif(UNIX AND NOT APPLE)
set(HOST_OS "linux")
endif()


if(CMAKE_COMPILER_IS_GNUCXX)
    # -Wno-enum-compare: Mac OSErr codes are currently spread between different
    #                    anonymous enums in the headers.
    #                    Code like (blah ? noErr : paramErr) triggers the enum-compare warning.

    add_compile_options(
        -Wall
        -Wno-unused-but-set-variable
        -Wno-sign-compare
        -Werror=return-type
        -Wno-enum-compare)
endif()

add_subdirectory(config/front-ends/x)
add_subdirectory(config/front-ends/sdl)
add_subdirectory(config/front-ends/sdl2)
add_subdirectory(config/front-ends/qt)
add_subdirectory(config/arch/generic_le)
add_subdirectory(config/os/win32)
add_subdirectory(config/os/macosx)

function(linkheader from to variant)
    if(DEFINED ARGV3)
        set(input ${ARGV3})
    else()
        set(input ${variant}.h)
    endif()

    configure_file(${from}/${variant}/${input} ${to} COPYONLY)
endfunction()

linkheader("config/os" "host-os-config.h" ${HOST_OS})
linkheader("config/arch" "host-arch-config.h" ${HOST_ARCH})
#linkheader("config/front-ends" "front-end-config.h" ${FRONT_END})
linkheader("config/sound" "sound-config.h" "dummy" "dummy-sound.h")

add_executable(map_to_c map_to_c/map_to_c.cpp)

include_directories(config/os/${HOST_OS})

set(MAP_FILES
        wind/active.map                 ctl/arrow_down_inactive.map
        ctl/arrow_right_active.map      ctl/arrow_up_inactive.map
        wind/ractive.map                wind/zoom.map
        menu/apple.map                  ctl/arrow_left_active.map
        ctl/arrow_right_inactive.map    wind/go_away.map
        ctl/thumb_horiz.map             ctl/arrow_down_active.map
        ctl/arrow_left_inactive.map     ctl/arrow_up_active.map
        wind/grow.map                   ctl/thumb_vert.map
    )
set(GENERATED_FILES)
foreach(mapfile ${MAP_FILES})
    get_filename_component(base ${mapfile} NAME_WE)
    add_custom_command(
        OUTPUT ${base}.cmap
        DEPENDS ${mapfile}
        COMMAND map_to_c < ${CMAKE_CURRENT_SOURCE_DIR}/${mapfile} > ${base}.cmap
        )
    list(APPEND GENERATED_FILES ${base}.cmap)
endforeach()


add_custom_command(OUTPUT parse.tab.cpp
        DEPENDS prefs/parse.ypp
        COMMAND ${BISON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/prefs/parse.ypp)
list(APPEND GENERATED_FILES parse.tab.cpp)


set(ctl_sources
    #include/ControlMgr.h
    ctl/ctl.h
    ctl/ctlArrows.cpp ctl/ctlDisplay.cpp ctl/ctlIMIV.cpp ctl/ctlInit.cpp ctl/ctlMisc.cpp
    ctl/ctlMouse.cpp ctl/ctlSet.cpp ctl/ctlSize.cpp ctl/ctlStddef.cpp ctl/ctlPopup.cpp)
source_group(Controls FILES ${ctl_sources})

set(dial_sources
    #include/DialogMgr.h
    dial/dial.h
    dial/itm.h
    dial/dialAlert.cpp dial/dialCreate.cpp dial/dialHandle.cpp dial/dialInit.cpp
    dial/dialManip.cpp dial/dialDispatch.cpp dial/dialItem.cpp)
source_group(Dialogs FILES ${dial_sources})

set(file_sources
    #include/FileMgr.h
    file/file.h
    file/volume.h

    file/fileMisc.cpp
    file/fileHighlevel.cpp  file/fileUnimplemented.cpp
    file/volume.cpp
    
    file/localvolume/localvolume.h 
    file/localvolume/localvolume.cpp 

    file/localvolume/openfile.h

    file/localvolume/item.h 
    file/localvolume/item.cpp

    file/localvolume/itemcache.h
    file/localvolume/itemcache.cpp

    file/localvolume/basilisk.h
    file/localvolume/basilisk.cpp
    file/localvolume/appledouble.h
    file/localvolume/appledouble.cpp
    file/localvolume/plain.h
    file/localvolume/plain.cpp
    file/localvolume/mac.h
    file/localvolume/mac.cpp

    file/localvolume/cnidmapper.h
    file/localvolume/simplecnidmapper.h
    file/localvolume/simplecnidmapper.cpp
    file/localvolume/lmdbcnidmapper.h
    file/localvolume/lmdbcnidmapper.cpp
    )

source_group(Files FILES ${file_sources})

set(hfs_sources 
    hfs/hfs.h
    hfs/hfs_plus.h
    hfs/partition.h
    hfs/dcache.h
    hfs/drive_flags.h
    hfs/dcache.cpp
    hfs/futzwithdosdisks.h
    hfs/hfsBtree.cpp hfs/hfsChanging.cpp hfs/hfsCreate.cpp hfs/hfsFile.cpp
    hfs/hfsHelper.cpp hfs/hfsHier.cpp hfs/hfsMisc.cpp hfs/hfsVolume.cpp
    hfs/hfsWorkingdir.cpp hfs/hfsXbar.cpp hfs/futzwithdosdisks.cpp)
source_group(HFS FILES ${hfs_sources})

set(list_sources
    list/list.h
    list/listAccess.cpp list/listAddDel.cpp list/listCreate.cpp list/listDisplay.cpp
    list/listMouse.cpp list/listOps.cpp list/listStdLDEF.cpp)
source_group(Lists FILES ${list_sources})

set(menu_sources
    menu/menu.h
    menu/menu.cpp menu/menuColor.cpp menu/menuV.cpp menu/stdmdef.cpp menu/stdmbdf.cpp)
source_group(Menus FILES ${menu_sources})

set(print_sources
    print/print.h
    print/nextprint.h
    print/ini.h
    print/ini.cpp 
    print/PSstrings.h
    print/PSstrings.cpp print/prError.cpp print/prInit.cpp print/prLowLevel.cpp
    print/prPrinting.cpp print/prRecords.cpp print/PSprint.cpp )
source_group(Printing FILES ${print_sources})

set(qd_sources
    quickdraw/quick.h
    quickdraw/cquick.h    
    quickdraw/depthconv.h
    quickdraw/qcolor.h
    quickdraw/rawblt.h
    quickdraw/gworld.h
    quickdraw/font.h
    quickdraw/picture.h
    quickdraw/region.h
    quickdraw/rgbutil.h
    quickdraw/srcblt.h
    quickdraw/stdbits.h
    quickdraw/xdata.h
    quickdraw/xdblt.h
    quickdraw/image.h
    quickdraw/text.h

    quickdraw/hintemplate.h

    quickdraw/qBit.cpp quickdraw/qCConv.cpp quickdraw/qCGrafPort.cpp
    quickdraw/qCRegular.cpp quickdraw/qColor.cpp quickdraw/qColorMgr.cpp
    quickdraw/qColorutil.cpp quickdraw/qCursor.cpp quickdraw/qGrafport.cpp
    quickdraw/qIMIV.cpp quickdraw/qIMV.cpp quickdraw/qIMVxfer.cpp
    quickdraw/qMisc.cpp quickdraw/qPaletteMgr.cpp quickdraw/qPen.cpp
    quickdraw/qPicstuff.cpp quickdraw/qPicture.cpp quickdraw/qPixMapConv.cpp
    quickdraw/qPoint.cpp quickdraw/qPoly.cpp quickdraw/qRect.cpp
    quickdraw/qRegion.cpp quickdraw/qRegular.cpp quickdraw/qScale.cpp
    quickdraw/qStandard.cpp quickdraw/qStdArc.cpp quickdraw/qStdBits.cpp
    quickdraw/qStdLine.cpp quickdraw/qStdOval.cpp quickdraw/qStdPic.cpp
    quickdraw/qStdPoly.cpp quickdraw/qStdRRect.cpp quickdraw/qStdRect.cpp
    quickdraw/qStdRgn.cpp quickdraw/qStdText.cpp quickdraw/qText.cpp
    quickdraw/qGWorld.cpp quickdraw/qGDevice.cpp quickdraw/qIMVI.cpp
    quickdraw/qHooks.cpp quickdraw/xdata.cpp quickdraw/xdblt.cpp
    quickdraw/rawpatblt.cpp quickdraw/rawsrcblt.cpp quickdraw/srcblt.cpp
     quickdraw/qPict2.cpp quickdraw/image.cpp
    quickdraw/image_inits.cpp quickdraw/default_ctab_values.cpp
    quickdraw/dcconvert.cpp quickdraw/rgbutil.cpp quickdraw/dcmaketables.cpp
    quickdraw/fontIMVI.cpp quickdraw/font.cpp
    quickdraw/displays.cpp
)

source_group(QuickDraw FILES ${qd_sources})

add_executable(mkseedtables quickdraw/mkseedtables/mkseedtables.c)
add_executable(mkultable quickdraw/mkultable/mkultable.c)
add_executable(mksspairtable quickdraw/mksspairtable/mksspairtable.c)
add_custom_command(OUTPUT seedtables.ctable COMMAND mkseedtables > seedtables.ctable)
add_custom_command(OUTPUT ultable.ctable COMMAND mkultable > ultable.ctable)
add_custom_command(OUTPUT sspairtable.ctable COMMAND mksspairtable > sspairtable.ctable)
list(APPEND GENERATED_FILES seedtables.ctable ultable.ctable sspairtable.ctable)

add_custom_command(OUTPUT rawpatstubs.ctable
    DEPENDS quickdraw/makerawblt.pl quickdraw/pat-blitters.tmpl
    COMMAND ${PERL_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/quickdraw/makerawblt.pl
        < ${CMAKE_CURRENT_SOURCE_DIR}/quickdraw/pat-blitters.tmpl
        > rawpatstubs.ctable)

add_custom_command(OUTPUT rawsrcstubs.ctable
    DEPENDS quickdraw/makerawblt.pl quickdraw/src-blitters.tmpl
    COMMAND ${PERL_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/quickdraw/makerawblt.pl
        < ${CMAKE_CURRENT_SOURCE_DIR}/quickdraw/src-blitters.tmpl
        > rawsrcstubs.ctable)
list(APPEND GENERATED_FILES rawpatstubs.ctable rawsrcstubs.ctable)


set(res_sources
    res/resource.h

    res/resGet.cpp res/resGetinfo.cpp res/resGettype.cpp res/resIMIV.cpp
    res/resInit.cpp res/resMisc.cpp res/resMod.cpp res/resOpen.cpp
    res/resSetcur.cpp res/resPartial.cpp) 

source_group(Resources FILES ${res_sources})

set(te_sources
    textedit/tesave.h

    textedit/teAccess.cpp textedit/teDisplay.cpp textedit/teEdit.cpp 
    textedit/teIMIV.cpp textedit/teIMV.cpp textedit/teInit.cpp
    textedit/teInsert.cpp textedit/teMisc.cpp textedit/teScrap.cpp)
source_group(TextEdit FILES ${te_sources})

set(wind_sources
    wind/wind.h
    
    wind/windColor.cpp wind/windDisplay.cpp wind/windDocdef.cpp
    wind/windInit.cpp wind/windMisc.cpp wind/windMouse.cpp wind/windSize.cpp
    wind/windUpdate.cpp wind/windRootless.cpp)
source_group(Windows FILES ${wind_sources})

set(ae_sources
    appleevent/apple_events.h
    appleevent/AE.cpp appleevent/AE_desc.cpp appleevent/AE_hdlr.cpp
    appleevent/AE_coercion.cpp)
source_group(AppleEvents FILES ${ae_sources})

set(sound_sources
    sound/sounddriver.h
    sound/soundopts.h
    sound/soundfake.h
    
    sound/sounddriver.cpp sound/sound.cpp sound/soundIMVI.cpp sound/soundfake.cpp
    sound/snth5.cpp

    SpeechManager.cpp)
if(HOST_OS STREQUAL macosx)
set(sound_sources ${sound_sources} SpeechManager-MacBridge.mm)
endif()
source_group(Sound FILES ${sound_sources})

set(num_sources
    sane/float_fcw.h sane/float.h sane/floatconv.h
    sane/float4.cpp sane/float5.cpp sane/float7.cpp sane/floatnext.cpp)
source_group(Numerics FILES ${num_sources})


set(base_sources
        # include/ExMacTypes.h
        
        base/common.h
        base/mactype.h
        base/lowglobals.h
        base/functions.h
        base/functions.impl.h
        base/byteswap.h

        base/m68kint.h
        base/cpu.h
        base/cpu.cpp    

        base/traps.h
        base/traps.impl.h
        base/trapname.h
        base/dispatcher.h
        base/trapglue.h
        base/logging.h
        base/api-module.h
        base/emustubs.h
    
        base/trapname.cpp
        base/traps.cpp
        base/dispatcher.cpp
        base/emustubs.cpp
        base/logging.cpp
    
        base/builtinlibs.h
        base/builtinlibs.cpp

        base/debugger.h
        base/debugger.cpp
    )
source_group(Base FILES ${base_sources})

set(vdriver_sources
        vdriver/vdriver.cpp
        vdriver/refresh.cpp
        vdriver/autorefresh.cpp 
        vdriver/dirtyrect.cpp
        vdriver/dirtyrect.h
        vdriver/autorefresh.h
        vdriver/refresh.h
        vdriver/vdriver.h
    )

set(mman_sources
    mman/memsize.h
    mman/mman.h
    mman/mman_private.h
    mman/tempalloc.h

    mman/mman.cpp mman/mmansubr.cpp
    mman/tempmem.cpp
)

set(time_sources
        time/time.h
        time/vbl.h
        time/syncint.h

        time/time.cpp
        time/vbl.cpp
        time/syncint.cpp
    )

set(osevent_sources
        osevent/osevent.h
        osevent/commonevt.h
        osevent/hle.cpp
        osevent/ibm_keycodes.cpp
        osevent/osevent.cpp
    )

set(error_sources
    error/error.h error/syserr.h error/system_error.h
    error/error.cpp error/syserr.cpp error/system_error.cpp
)

set(commandline_sources
    commandline/option.h
    commandline/option.cpp

    commandline/parseopt.h
    commandline/parsenum.h
    commandline/parseopt.cpp
    commandline/parsenum.cpp

    commandline/flags.h
    )

set(prefs_sources
    prefs/prefs.h
    prefs/parse.h
    prefs/options.h
    prefs/crc.h

    prefs/parse.ypp
    prefs/crc.cpp
    prefs/options.cpp
    prefs/prefs.cpp
)

set(misc_sources
  mon_debugger.cpp
  desk.cpp device.cpp disk.cpp diskinit.cpp dump.cpp
  gestalt.cpp iu.cpp launch.cpp  notify.cpp 
   osutil.cpp pack.cpp scrap.cpp script.cpp segment.cpp serial.cpp
  slash.cpp stdfile.cpp romlib_stubs.cpp  toolevent.cpp toolutil.cpp 
   aboutbox.cpp
  version.cpp shutdown.cpp uniquefile.cpp screen-dump.cpp
  process.cpp alias.cpp string.cpp  edition.cpp  balloon.cpp
   adb.cpp color_wheel_bits.cpp finder.cpp  
  icon.cpp redrawscreen.cpp qt.cpp
  commtool.cpp cfm.cpp pef_hash.cpp
  mixed_mode.cpp appearance.cpp lockrange.cpp
  unix_like.cpp
  executor.cpp  
  prefpanel.cpp
  qColorPicker.cpp
  bindec.cpp 
  toolmath.cpp
  macstrings.cpp
  init.cpp
  )

set(include_sources 
                  include/ADB.h
                  include/AliasMgr.h
                  include/AppleEvents.h
                  include/AppleTalk.h
                  include/BinaryDecimal.h
                  include/CommTool.h
                  include/Components.h
                  include/ControlMgr.h
                  include/CQuickDraw.h
                  include/DeskMgr.h
                  include/DeviceMgr.h
                  include/DialogMgr.h
                  include/Disk.h
                  include/DiskInit.h
                  include/Displays.h
                  include/EditionMgr.h
                  include/EventMgr.h
                  include/FileMgr.h
                  include/Finder.h
                  include/FontMgr.h
                  include/Gestalt.h
                  include/HelpMgr.h
                  include/Iconutil.h
                  include/IntlUtil.h
                  include/ListMgr.h
                  include/ExMacTypes.h
                  include/MemoryMgr.h
                  include/MenuMgr.h
                  include/NotifyMgr.h
                  include/OSEvent.h
                  include/OSUtil.h
                  include/Package.h
                  include/PPC.h
                  include/PrintMgr.h
                  include/ProcessMgr.h
                  include/QuickDraw.h
                  include/QuickTime.h
                  include/ResourceMgr.h
                  include/rsys/aboutbox.h
                  include/rsys/adb.h
                  include/rsys/alias.h
                  include/rsys/appearance.h
                  include/rsys/arch.h
                  include/rsys/arrowkeys.h
                  include/rsys/cfm.h
                  include/rsys/color_wheel_bits.h
                  include/rsys/desk.h
                  include/rsys/device.h
                  include/rsys/dump.h
                  include/rsys/evil.h
                  include/rsys/executor.h
                  include/rsys/filesystem.h
                  include/rsys/gestalt.h
                  include/rsys/hook.h
                  include/rsys/icon.h
                  include/rsys/keyboard.h
                  include/rsys/launch.h
                  include/rsys/lockunlock.h
                  include/rsys/macros.h
                  include/rsys/macstrings.h
                  include/rsys/mixed_mode.h
                  include/rsys/mon_debugger.h
                  include/rsys/noreturn.h
                  include/rsys/os.h
                  include/rsys/osutil.h
                  include/rsys/paths.h
                  include/rsys/pef.h
                  include/rsys/prefpanel.h
                  include/rsys/process.h
                  include/rsys/redrawscreen.h
                  include/rsys/safe_alloca.h
                  include/rsys/scrap.h
                  include/rsys/screen-dump.h
                  include/rsys/segment.h
                  include/rsys/serial.h
                  include/rsys/slash.h
                  include/rsys/stdfile.h
                  include/rsys/string.h
                  include/rsys/toolevent.h
                  include/rsys/toolutil.h
                  include/rsys/uniquefile.h
                  include/rsys/version.h
                  include/SANE.h
                  include/ScrapMgr.h
                  include/ScriptMgr.h
                  include/SegmentLdr.h
                  include/Serial.h
                  include/ShutDown.h
                  include/SoundDvr.h
                  include/SoundMgr.h
                  include/SpeechManager.h
                  include/StartMgr.h
                  include/StdFilePkg.h
                  include/SysErr.h
                  include/TextEdit.h
                  include/TimeMgr.h
                  include/ToolboxEvent.h
                  include/ToolboxUtil.h
                  include/VDriver.h
                  include/VRetraceMgr.h
                  include/WindowMgr.h
                  mkvol/mkvol.h
                  mkvol/mkvol_internal.h)

include(trap_instances/trap_instances.cmake)

if(WIN32)
    set(host_platform_sources ../windows/dirent.c ../windows/gettimeofday.cpp)
else()
    set(host_platform_sources)
endif()
source_group(HostPlatform FILES ${host_platform_sources})

set(sources ${base_sources} ${mman_sources} ${vdriver_sources}
    ${ctl_sources} ${dial_sources} 
    ${list_sources} ${menu_sources}
    ${print_sources} ${qd_sources} ${res_sources}
    ${te_sources} ${wind_sources} ${ae_sources}
    ${sound_sources} ${num_sources} ${misc_sources}
    ${file_sources} ${hfs_sources} ${time_sources} ${osevent_sources}
    ${error_sources} ${commandline_sources} ${prefs_sources}
    ${trap_instance_sources} ${host_platform_sources}
    ${include_sources})

add_library(romlib ${sources}
        ${GENERATED_FILES}
    )

target_include_directories(romlib PUBLIC . include/ ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(romlib arch-${HOST_ARCH})
target_link_libraries(romlib lmdb)

if(HOST_OS STREQUAL win32 OR HOST_OS STREQUAL macosx)
    target_link_libraries(romlib os-${HOST_OS})
endif()

target_link_libraries(romlib Threads::Threads syn68k PowerCore mon)

target_link_libraries(romlib Boost::filesystem)  # mxe doesn't like this
target_link_libraries(romlib resources)

add_executable(executor main.cpp)
target_link_libraries(executor romlib front-end-${FRONT_END})

set_target_properties(executor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(APPLE)
    set_target_properties(executor PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_FRAMEWORK_IDENTIFIER com.github.autc04.executor
        )
endif()
