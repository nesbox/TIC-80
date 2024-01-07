cmake_minimum_required(VERSION 3.3)

include(cmake/version.cmake)

project(TIC-80 VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION} LANGUAGES C CXX)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
endif()

message("Building for target : ${CMAKE_SYSTEM_NAME}")
message("PROJECT_VERSION: ${PROJECT_VERSION}${VERSION_STATUS}")
message("VERSION_HASH: ${VERSION_HASH}")

configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")

if(ANDROID OR EMSCRIPTEN OR N3DS OR RPI)
    set(BUILD_LIBRETRO_DEFAULT OFF)
    set(BUILD_DEMO_CARTS_DEFAULT OFF)
    set(BUILD_PLAYER_DEFAULT OFF)
else()
    set(BUILD_LIBRETRO_DEFAULT ON)
    set(BUILD_DEMO_CARTS_DEFAULT ON)
    set(BUILD_PLAYER_DEFAULT ON)
endif()

set(BUILD_TOUCH_INPUT_DEFAULT ${ANDROID})

option(BUILD_SDL "SDL Enabled" ON)
option(BUILD_SDLGPU "SDL GPU Enabled" OFF)
option(BUILD_SOKOL "Sokol Enabled" OFF)
option(BUILD_LIBRETRO "libretro Enabled" ${BUILD_LIBRETRO_DEFAULT})
option(BUILD_DEMO_CARTS "Demo Carts Enabled" ${BUILD_DEMO_CARTS_DEFAULT})
option(BUILD_PRO "Build PRO version" FALSE)
option(BUILD_PLAYER "Build standalone players" ${BUILD_PLAYER_DEFAULT})
option(BUILD_TOUCH_INPUT "Build with touch input support" ${BUILD_TOUCH_INPUT_DEFAULT})
option(BUILD_STUB "Build stub without editors" OFF)

if(NOT BUILD_SDL)
    set(BUILD_SDLGPU OFF)
endif()

message("BUILD_SDLGPU: ${BUILD_SDLGPU}")
message("BUILD_TOUCH_INPUT: ${BUILD_TOUCH_INPUT}")
message("BUILD_STUB: ${BUILD_STUB}")

if (N3DS)
    set(BUILD_SDL OFF)
endif()

if (BAREMETALPI)
    set(BUILD_SDL OFF)
    set(BUILD_DEMO_CARTS OFF)
endif()

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT N3DS)
    set(LINUX TRUE)

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
        set(FREEBSD TRUE)
    endif()
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
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

include(cmake/gif.cmake)
include(cmake/blipbuf.cmake)

include(cmake/lua.cmake)
include(cmake/wren.cmake)
include(cmake/mruby.cmake)
include(cmake/wasm.cmake)
include(cmake/scheme.cmake)
include(cmake/squirrel.cmake)
include(cmake/pocketpy.cmake)
include(cmake/quickjs.cmake)
include(cmake/janet.cmake)

include(cmake/core.cmake)

include(cmake/zlib.cmake)
include(cmake/zip.cmake)
include(cmake/tools.cmake)
include(cmake/wave.cmake)
include(cmake/argparse.cmake)
include(cmake/naett.cmake)
include(cmake/png.cmake)

include(cmake/studio.cmake)

include(cmake/sdl.cmake)
include(cmake/sokol.cmake)
include(cmake/libretro.cmake)
include(cmake/n3ds.cmake)

include(cmake/stub.cmake)
include(cmake/install.cmake)
