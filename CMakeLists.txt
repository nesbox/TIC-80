cmake_minimum_required(VERSION 3.10)

include(cmake/version.cmake)

project(TIC-80 VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION} LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        set(LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF")
    else()
        set(FLAGS_RELEASE "-fno-function-sections -fno-data-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions")

        if(APPLE)
            set(LINKER_FLAGS_RELEASE "-Wl,-dead_strip")
        elseif(NINTENDO_3DS)
        else()
            set(LINKER_FLAGS_RELEASE "-Wl,--gc-sections -s")
        endif()
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS_RELEASE}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS_RELEASE}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS_RELEASE}")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS_RELEASE}")
endif()

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
endif()

message("Building for target : ${CMAKE_SYSTEM_NAME}")
message("PROJECT_VERSION: ${PROJECT_VERSION}${VERSION_STATUS}")
message("VERSION_HASH: ${VERSION_HASH}")

configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")

set(BUILD_STATIC_DEFAULT ON)

set(BUILD_PLAYER_DEFAULT OFF)
set(BUILD_LIBRETRO_DEFAULT OFF)
set(BUILD_WITH_ALL_DEFAULT OFF)

if(NINTENDO_3DS OR BAREMETALPI)
    set(BUILD_SOKOL_DEFAULT OFF)
else()
    set(BUILD_SOKOL_DEFAULT ON)
endif()

option(BUILD_IPO "IPO enabled" OFF)
option(BUILD_STATIC "Static runtime" ${BUILD_STATIC_DEFAULT})
option(BUILD_WITH_ALL "Build all supported scripts" ${BUILD_WITH_ALL_DEFAULT})
option(BUILD_SDL "SDL Enabled" OFF)
option(BUILD_SOKOL "Sokol Enabled" ${BUILD_SOKOL_DEFAULT})
option(BUILD_SOKOL_WEBGPU "Sokol WebGPU Enabled" OFF)
option(BUILD_LIBRETRO "libretro Enabled" ${BUILD_LIBRETRO_DEFAULT})
option(BUILD_TOOLS "prj2cart" OFF)
option(BUILD_EDITORS "Build cart editors" ON)
option(BUILD_PRO "Build PRO version" OFF)
option(BUILD_PLAYER "Build standalone players" ${BUILD_PLAYER_DEFAULT})
option(BUILD_NO_OPTIMIZATION "Build without optimizations for debugging" OFF)
option(BUILD_ASAN_DEBUG "Build with AddressSanitizer" OFF)
option(BUILD_WITH_ZLIB "Build with zlib linked" ON)
option(TIC80_TARGET "Target binary suffix")
option(PREFER_SYSTEM_LIBRARIES "Prefer link with system libraries" OFF)

if(BUILD_IPO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(BUILD_SDL)
    set(BUILD_SOKOL OFF CACHE BOOL "Sokol Enabled" FORCE)
endif()

if(NOT TIC80_TARGET)
    set(TIC80_TARGET tic80)
endif()

message("PREFER_SYSTEM_LIBRARIES: ${PREFER_SYSTEM_LIBRARIES}")
message("BUILD_STATIC: ${BUILD_STATIC}")
message("BUILD_SOKOL: ${BUILD_SOKOL}")
message("BUILD_SDL: ${BUILD_SDL}")
message("BUILD_WITH_ALL: ${BUILD_WITH_ALL}")
message("BUILD_PLAYER: ${BUILD_PLAYER}")
message("BUILD_IPO: ${BUILD_IPO}")
message("BUILD_SOKOL_WEBGPU: ${BUILD_SOKOL_WEBGPU}")

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT NINTENDO_3DS)
    set(LINUX TRUE)

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
        set(FREEBSD TRUE)
    endif()
endif()

if (BUILD_NO_OPTIMIZATION)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
endif()

if (BUILD_ASAN_DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

    if (LINUX AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -shared-libasan")
    endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if (NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(MSVC)

    add_definitions("/D\"_CRT_SECURE_NO_WARNINGS\"")
    add_definitions("/D\"_CRT_NONSTDC_NO_DEPRECATE\"")

    foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
        string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
        set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib )
        set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib )
        set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin )

        # use static runtime
        # !TODO: use set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") from cmake 3.15 instead
        if(CMAKE_C_FLAGS_${OUTPUTCONFIG} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" CMAKE_C_FLAGS_${OUTPUTCONFIG} "${CMAKE_C_FLAGS_${OUTPUTCONFIG}}")
        endif()

        if(CMAKE_CXX_FLAGS_${OUTPUTCONFIG} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_${OUTPUTCONFIG} "${CMAKE_CXX_FLAGS_${OUTPUTCONFIG}}")
        endif()
    endforeach()

else()

    set(CMAKE_C_STANDARD 11)
endif()

if(RPI)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
endif()

set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/vendor)
set(DEMO_CARTS_IN ${CMAKE_SOURCE_DIR}/demos)

include(cmake/assets.cmake)
include(cmake/runtime.cmake)

include(cmake/gif.cmake)
include(cmake/blipbuf.cmake)
include(cmake/zlib.cmake)
include(cmake/tools.cmake)

include(cmake/lua.cmake)
include(cmake/moon.cmake)
include(cmake/fennel.cmake)
include(cmake/wren.cmake)
include(cmake/mruby.cmake)
include(cmake/wasm.cmake)
include(cmake/scheme.cmake)
include(cmake/squirrel.cmake)
include(cmake/pocketpy.cmake)
include(cmake/quickjs.cmake)
include(cmake/janet.cmake)

include(cmake/core.cmake)
include(cmake/wave.cmake)
include(cmake/argparse.cmake)
include(cmake/naett.cmake)
include(cmake/png.cmake)
include(cmake/studio.cmake)

include(cmake/sdl.cmake)
include(cmake/sokol.cmake)
include(cmake/libretro.cmake)
include(cmake/n3ds.cmake)

include(cmake/install.cmake)
