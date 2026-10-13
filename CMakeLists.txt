cmake_minimum_required(VERSION 3.10)

include(cmake/version.cmake)

project(TIC-80 VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION} LANGUAGES C CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
set(CMAKE_C_FLAGS_RELEASE "-O2 -flto -ffat-lto-objects ${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -flto -ffat-lto-objects ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang"  OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
set(CMAKE_C_FLAGS_RELEASE "-O2 -flto ${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -flto ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
endif()

message("Building for target : ${CMAKE_SYSTEM_NAME}")
message("PROJECT_VERSION: ${PROJECT_VERSION}${VERSION_STATUS}")
message("VERSION_HASH: ${VERSION_HASH}")

configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")

# The static runtime is what every artifact the project ships is built with,
# so it is the default everywhere and the build that wants the shared one —
# a local run of a language module under a debugger, mostly — asks for it.
set(BUILD_STATIC_DEFAULT ON)

set(BUILD_PLAYER_DEFAULT OFF)
set(BUILD_LIBRETRO_DEFAULT OFF)
# Touch input is off unless a build asks for it: the platforms with a screen
# to touch — Android, the Switch, the browser a phone opens the player in —
# pass -DBUILD_TOUCH_INPUT=ON themselves, and a build that does not is a build
# without the controls, their layout and their textures.
set(BUILD_TOUCH_INPUT_DEFAULT OFF)
set(BUILD_WITH_ALL_DEFAULT OFF)

option(BUILD_STATIC "Static runtime" ${BUILD_STATIC_DEFAULT})
option(BUILD_WITH_ALL "Build all supported scripts" ${BUILD_WITH_ALL_DEFAULT})
option(BUILD_APPLE "Apple Native Backend Enabled" OFF)
option(BUILD_RENDER_CACHE "Build with render cache optimization" OFF)
if(BUILD_RENDER_CACHE)
    add_definitions(-DBUILD_RENDER_CACHE)
endif()
# The desktop builds the sokol layer where no layer is named, as it built the
# SDL one before the migration. The platforms that are not the desktop keep
# the SDL default — Android builds it, the consoles and the Apple backend turn
# it off for a layer of their own, and the web jobs name sokol outright — and
# so do the two desktops sokol has no backend for: the Pi's sysroot, whose GL
# is Broadcom's, and FreeBSD.
if(ANDROID OR EMSCRIPTEN OR NINTENDO_3DS OR NINTENDO_SWITCH OR BAREMETALPI OR RPI OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(BUILD_SDL_DEFAULT ON)
    set(BUILD_SOKOL_DEFAULT OFF)
else()
    set(BUILD_SDL_DEFAULT OFF)
    set(BUILD_SOKOL_DEFAULT ON)
endif()

option(BUILD_SDL "SDL Enabled" ${BUILD_SDL_DEFAULT})
option(BUILD_SOKOL "Sokol Enabled" ${BUILD_SOKOL_DEFAULT})
if(BUILD_APPLE)
    set(BUILD_SDL OFF)
    set(BUILD_SOKOL OFF)
endif()
if(BUILD_SDL AND BUILD_SOKOL)
    message(FATAL_ERROR "BUILD_SDL and BUILD_SOKOL are two system layers: enable exactly one")
endif()
option(BUILD_SDLGPU "SDL GPU Enabled" OFF)
option(BUILD_LIBRETRO "libretro Enabled" ${BUILD_LIBRETRO_DEFAULT})
option(BUILD_TOOLS "bin2txt prj2cart" OFF)
option(BUILD_EDITORS "Build cart editors" ON)
option(BUILD_SURF "Build the SURF game browser" ON)
option(BUILD_PRO "Build PRO version" FALSE)
option(BUILD_PLAYER "Build standalone players" ${BUILD_PLAYER_DEFAULT})
option(BUILD_TOUCH_INPUT "Build with touch input support" ${BUILD_TOUCH_INPUT_DEFAULT})
option(BUILD_NO_OPTIMIZATION "Build without optimizations for debugging" OFF)
option(BUILD_ASAN_DEBUG "Build with AddressSanitizer" OFF)
option(BUILD_WITH_ZLIB "Build with zlib linked" ON)
option(TIC80_TARGET "Target binary suffix")
option(PREFER_SYSTEM_LIBRARIES "Prefer link with system libraries" OFF)

# The browser is part of the default build; the builds that should not carry
# one — the export stubs, the GLES2 player — pass -DBUILD_SURF=OFF.

if(NOT TIC80_TARGET)
    set(TIC80_TARGET tic80)
endif()

if(NOT BUILD_SDL)
    set(BUILD_SDLGPU OFF)
endif()

if(NINTENDO_SWITCH AND NOT LIBRETRO_STATIC)
    set(PREFER_SYSTEM_LIBRARIES ON)
    if(NOT TIC_DATA_PATH)
        set(TIC_DATA_PATH "/switch/tic80")
    endif()
endif()

add_library(runtime INTERFACE)

if(TIC_DATA_PATH)
    target_compile_definitions(runtime INTERFACE TIC_DATA_PATH="${TIC_DATA_PATH}")
endif()

if(BUILD_STATIC)
    set(TIC_RUNTIME STATIC)
    target_compile_definitions(runtime INTERFACE TIC_RUNTIME_STATIC)
else()
    set(TIC_RUNTIME SHARED)
endif()

target_compile_definitions(runtime INTERFACE BUILD_DEPRECATED)

message("PREFER_SYSTEM_LIBRARIES: ${PREFER_SYSTEM_LIBRARIES}")
message("BUILD_STATIC: ${BUILD_STATIC}")
message("BUILD_SDLGPU: ${BUILD_SDLGPU}")
message("BUILD_TOUCH_INPUT: ${BUILD_TOUCH_INPUT}")
message("BUILD_WITH_ALL: ${BUILD_WITH_ALL}")

if(NINTENDO_3DS OR BAREMETALPI)
    set(BUILD_SDL OFF)
endif()

if (NINTENDO_3DS)
    set(PREFER_SYSTEM_LIBRARIES ON)
endif()

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT ANDROID)
    set(LINUX TRUE)

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "FreeBSD")
        set(FREEBSD TRUE)
    endif()
endif()

option(PREFER_SYSTEM_SDL2 "Prefer system SDL2" ${PREFER_SYSTEM_LIBRARIES})
option(USE_GLES2 "Use GLES2" OFF)
option(USE_EGL "Use EGL" OFF)

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

include(cmake/warnings.cmake)
include(cmake/gif.cmake)
include(cmake/blipbuf.cmake)
include(cmake/zlib.cmake)
include(cmake/zip.cmake)
include(cmake/png.cmake)

include(cmake/lua.cmake)
include(cmake/moon.cmake)
include(cmake/yue.cmake)
include(cmake/fennel.cmake)
include(cmake/wren.cmake)
include(cmake/mruby.cmake)
include(cmake/wasm.cmake)
include(cmake/scheme.cmake)
include(cmake/squirrel.cmake)
include(cmake/pocketpy.cmake)
include(cmake/quickjs.cmake)
include(cmake/janet.cmake)
include(cmake/forth.cmake)
include(cmake/miniscript.cmake)

include(cmake/runtime_versions.cmake)

# After the languages: the tools check what those options came out as.
include(cmake/tools.cmake)

include(cmake/core.cmake)
include(cmake/wave.cmake)
include(cmake/argparse.cmake)
include(cmake/naett.cmake)
include(cmake/studio.cmake)

# After the core and the languages: the tests link what those came out as.
include(cmake/tests.cmake)

include(cmake/sdl.cmake)
include(cmake/sokol.cmake)
include(cmake/apple.cmake)
include(cmake/libretro.cmake)
include(cmake/n3ds.cmake)
include(cmake/nswitch.cmake)

# After every layer: the Linux desktop entry, its icon and the mime type
# belong to the app rather than to the layer that built it, and so does the
# install rule for the binary. Whichever layer a build names, a Linux package
# carries these — the ARM build packs them beside a sokol binary. A build that
# names no layer at all — the conformance runner's — has no binary to install
# and skips the block.
if(LINUX AND TARGET ${TIC80_TARGET})

    configure_file("${PROJECT_SOURCE_DIR}/build/linux/tic80.desktop.in" "${PROJECT_SOURCE_DIR}/build/linux/tic80.desktop")

    install(TARGETS ${TIC80_TARGET} DESTINATION bin)

    set(TIC80_DESKTOP_DIR       "share/applications/")
    set(TIC80_MIME_DIR          "share/mime/packages/")
    set(TIC80_PIXMAPS_DIR       "share/icons/hicolor/256x256/apps")
    set(TIC80_MIMETYPE_ICON_DIR "share/icons/hicolor/256x256/mimetypes")

    install(FILES ${PROJECT_SOURCE_DIR}/build/linux/tic80.desktop DESTINATION ${TIC80_DESKTOP_DIR})
    install(FILES ${PROJECT_SOURCE_DIR}/build/linux/tic80.xml DESTINATION ${TIC80_MIME_DIR})
    install(FILES ${PROJECT_SOURCE_DIR}/build/linux/tic80.png DESTINATION ${TIC80_PIXMAPS_DIR})
    install(FILES ${PROJECT_SOURCE_DIR}/build/linux/tic80.png DESTINATION ${TIC80_MIMETYPE_ICON_DIR} RENAME application-x-tic80-item.png)

endif()

include(cmake/install.cmake)
