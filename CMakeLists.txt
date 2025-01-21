cmake_minimum_required(VERSION 3.11.0)
project(fifo C CXX)
set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#if (NOT CMAKE_BUILD_TYPE)
#    set(CMAKE_BUILD_TYPE "Debug")
#endif()

if (CMAKE_C_COMPILER_ID MATCHES "GNU")
    set (WARNNORMALOPTS -Wall -Wextra -pedantic
        -Werror=implicit-int -Werror=return-type -Werror=strict-prototypes
        -Werror=implicit-function-declaration -Wmissing-declarations
        -Wsign-conversion -Wconversion -Wshadow
        -Wmissing-field-initializers)
# -Werror
# -Werror=implicit-int -Werror=return-type -Werror=strict-prototypes -Werror=implicit-function-declaration
# -Wno-missing-field-initializers -Wno-format-zero-length -Wno-missing-braces -Wformat-security -Winit-self
# -Wstrict-aliasing -Wshadow -Wcast-align -Wwrite-strings -Wlogical-op -Waggregate-return -Wunsafe-loop-optimizations
# -Winline -Wfloat-equal -Wundef
    #set (WARNEXTRAOPTS "")
#        set (COMPILEROPTS -fshort-enums)
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set (COMPILEROPTS ${COMPILEROPTS} -O0 -fno-inline -fno-omit-frame-pointer)
    endif()
    if (MINGW OR MSYS)
        # __USE_MINGW_ANSI_STDIO - for snprintf/printf: https://sourceforge.net/p/mingw-w64/mailman/message/31241434/
        set (COMPILEROPTS ${COMPILEROPTS} -D__USE_MINGW_ANSI_STDIO=0 -Wno-pedantic-ms-format)
    endif()
#    set(LINKEROPTS "")
elseif (CMAKE_C_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /TC")
    set (WARNNORMALOPTS /W3)
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set (COMPILEROPTS ${COMPILEROPTS} /Od /Zi /EHsc /Ob0 /Oy- /FC /MTd)
    else()
        set (COMPILEROPTS ${COMPILEROPTS} /O2 /Oi /Ot /fp:fast /GL /Gw /Gy /GR- /GS- /MT)
    endif()
    set(COMPILEROPTS ${COMPILEROPTS} /D_USE_MATH_DEFINES /D_CRT_SECURE_NO_WARNINGS)
    set(LINKEROPTS /subsystem:console)
else()
    message(FATAL_ERROR "Compiler ${CMAKE_C_COMPILER_ID} not supported")
endif()

string (REPLACE ";" " " WARNNORMALOPTS "${WARNNORMALOPTS}")
string (REPLACE ";" " " COMPILEROPTS "${COMPILEROPTS}")
string (REPLACE ";" " " LINKEROPTS "${LINKEROPTS}")
#message(NOTICE "WARNNORMALOPTS ${WARNNORMALOPTS}")
#message(NOTICE "COMPILEROPTS ${COMPILEROPTS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNNORMALOPTS} ${COMPILEROPTS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNNORMALOPTS} ${COMPILEROPTS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKEROPTS}")

#include(CheckIncludeFile)
#include(CheckFunctionExists)
include(CheckSymbolExists)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(check REQUIRED IMPORTED_TARGET check>=0.10.0)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${SRC_DIR})
add_subdirectory(${SRC_DIR})
add_subdirectory(test)
