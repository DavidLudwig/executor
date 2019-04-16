cmake_minimum_required(VERSION 3.10)

# Use packaged CMake modules for use with find_package(...), if and as needed
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

project(Executor)

set(CMAKE_CXX_STANDARD 17)

cmake_policy(SET CMP0020 NEW) # Automatically link Qt executables to qtmain target on Windows.
cmake_policy(SET CMP0074 NEW) # don't ignore _ROOT variables (Boost_ROOT)

set(CMAKE_VS_GLOBALS "SpectreMitigation=false")

if(WIN32)
    add_compile_options(
        # Prevent currently-non-compiling assembly functions from being included 
        -DNO_FAST_CC_FUNCS

        # Don't warn if older C-runtime functions get used
        -D_CRT_SECURE_NO_WARNINGS

        # FIXME: disable common warnings, for now
        -Wno-unused-variable
        -Wno-deprecated-declarations
        -Wno-unused-function
        -Wno-unused-lambda-capture
    )

    include_directories(BEFORE ${CMAKE_CURRENT_LIST_DIR}/windows)
endif()

if( NOT EXISTS "${CMAKE_SOURCE_DIR}/syn68k/.git" )
    message( FATAL_ERROR
        "The git submodule 'syn68k' is not available. Please run\n"
        "git submodule update --init --recursive" )
endif()

enable_testing()

message("DEBUG VARS 1:")
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

add_library(lmdb
    lmdb/libraries/liblmdb/lmdb.h lmdb/libraries/liblmdb/mdb.c
    lmdb/libraries/liblmdb/midl.h lmdb/libraries/liblmdb/midl.c
    lmdbxx/lmdb++.h)
target_include_directories(lmdb PUBLIC lmdb/libraries/liblmdb lmdbxx)

include(cmrc/CMakeRC.cmake)
cmrc_add_resource_library(resources
        WHENCE res
        res/System
        res/System.ad
        res/Browser
        res/Browser.ad
        res/Printer
        res/Printer.ad
        res/godata.sav
        res/printdef.ini
        res/printers.ini

        res/about/1_License.txt
        res/about/2_Credits.txt
        res/about/3_Tips.txt
    )

add_subdirectory(syn68k)
add_subdirectory(PowerCore)
add_subdirectory(cxmon)
add_subdirectory(src)

message("DEBUG VARS 2:")
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

add_subdirectory(tests)

message("DEBUG VARS 3:")
get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()
