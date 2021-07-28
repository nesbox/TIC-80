cmake_minimum_required(VERSION 3.3)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 90)
set(VERSION_REVISION 0)
set(VERSION_STATUS "-dev")
string(TIMESTAMP VERSION_YEAR "%Y")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(VERSION_BUILD ".dbg" )
endif()

find_package(Git)
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        ERROR_VARIABLE RESULT_STRING
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(LENGTH "${RESULT_STRING}" LENGTH_RESULT_STRING)

    if(${LENGTH_RESULT_STRING} EQUAL 0)

        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 --format=%H
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        string(SUBSTRING ${GIT_COMMIT_HASH} 0 7 GIT_COMMIT_HASH)
        set(VERSION_HASH ${GIT_COMMIT_HASH} )

        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list HEAD --count
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE VERSION_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

    endif()
endif()

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
endif()

project(TIC-80 VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION} LANGUAGES C CXX)
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
    set(BUILD_SDL off)
endif()

if (BAREMETALPI)
    set(BUILD_SDL OFF)
    set(BUILD_DEMO_CARTS OFF)
endif()

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT ANDROID AND NOT N3DS)
    set(LINUX TRUE)
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

set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/vendor)

################################
# LUA
################################

set(LUA_DIR ${THIRDPARTY_DIR}/lua)
set(LUA_SRC 
    ${LUA_DIR}/lapi.c
    ${LUA_DIR}/lcode.c
    ${LUA_DIR}/lctype.c
    ${LUA_DIR}/ldebug.c
    ${LUA_DIR}/ldo.c
    ${LUA_DIR}/ldump.c
    ${LUA_DIR}/lfunc.c
    ${LUA_DIR}/lgc.c
    ${LUA_DIR}/llex.c
    ${LUA_DIR}/lmem.c
    ${LUA_DIR}/lobject.c
    ${LUA_DIR}/lopcodes.c
    ${LUA_DIR}/lparser.c
    ${LUA_DIR}/lstate.c
    ${LUA_DIR}/lstring.c
    ${LUA_DIR}/ltable.c
    ${LUA_DIR}/ltm.c
    ${LUA_DIR}/lundump.c
    ${LUA_DIR}/lvm.c
    ${LUA_DIR}/lzio.c
    ${LUA_DIR}/lauxlib.c
    ${LUA_DIR}/lbaselib.c
    ${LUA_DIR}/lcorolib.c
    ${LUA_DIR}/ldblib.c
    ${LUA_DIR}/liolib.c
    ${LUA_DIR}/lmathlib.c
    ${LUA_DIR}/loslib.c
    ${LUA_DIR}/lstrlib.c
    ${LUA_DIR}/ltablib.c
    ${LUA_DIR}/lutf8lib.c
    ${LUA_DIR}/loadlib.c
    ${LUA_DIR}/linit.c
)

add_library(lua STATIC ${LUA_SRC})

target_compile_definitions(lua PRIVATE LUA_COMPAT_5_2)
target_include_directories(lua INTERFACE ${THIRDPARTY_DIR}/lua)

if(N3DS)
    target_compile_definitions(lua PUBLIC LUA_32BITS)
endif()

################################
# LPEG
################################

set(LPEG_DIR ${THIRDPARTY_DIR}/lpeg)
set(LPEG_SRC 
    ${LPEG_DIR}/lpcap.c
    ${LPEG_DIR}/lpcode.c
    ${LPEG_DIR}/lpprint.c
    ${LPEG_DIR}/lptree.c
    ${LPEG_DIR}/lpvm.c
)

add_library(lpeg STATIC ${LPEG_SRC})
target_include_directories(lpeg PRIVATE ${LUA_DIR})

################################
# WREN
################################

set(WREN_DIR ${THIRDPARTY_DIR}/wren/src)
set(WREN_SRC 
    ${WREN_DIR}/optional/wren_opt_meta.c
    ${WREN_DIR}/optional/wren_opt_random.c
    ${WREN_DIR}/vm/wren_compiler.c
    ${WREN_DIR}/vm/wren_core.c
    ${WREN_DIR}/vm/wren_debug.c
    ${WREN_DIR}/vm/wren_primitive.c
    ${WREN_DIR}/vm/wren_utils.c
    ${WREN_DIR}/vm/wren_value.c
    ${WREN_DIR}/vm/wren_vm.c
)

add_library(wren STATIC ${WREN_SRC})
target_include_directories(wren PUBLIC ${THIRDPARTY_DIR}/wren/src/include)
target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/optional)
target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/vm)

################################
# SQUIRREL
################################

set(SQUIRREL_DIR ${THIRDPARTY_DIR}/squirrel)
set(SQUIRREL_SRC
    ${SQUIRREL_DIR}/squirrel/sqapi.cpp
    ${SQUIRREL_DIR}/squirrel/sqbaselib.cpp
    ${SQUIRREL_DIR}/squirrel/sqclass.cpp
    ${SQUIRREL_DIR}/squirrel/sqcompiler.cpp
    ${SQUIRREL_DIR}/squirrel/sqdebug.cpp
    ${SQUIRREL_DIR}/squirrel/sqfuncstate.cpp
    ${SQUIRREL_DIR}/squirrel/sqlexer.cpp
    ${SQUIRREL_DIR}/squirrel/sqmem.cpp
    ${SQUIRREL_DIR}/squirrel/sqobject.cpp
    ${SQUIRREL_DIR}/squirrel/sqstate.cpp
    ${SQUIRREL_DIR}/squirrel/sqtable.cpp
    ${SQUIRREL_DIR}/squirrel/sqvm.cpp
    ${SQUIRREL_DIR}/sqstdlib/sqstdaux.cpp
    ${SQUIRREL_DIR}/sqstdlib/sqstdblob.cpp
    ${SQUIRREL_DIR}/sqstdlib/sqstdio.cpp
    ${SQUIRREL_DIR}/sqstdlib/sqstdmath.cpp
    ${SQUIRREL_DIR}/sqstdlib/sqstdrex.cpp
    ${SQUIRREL_DIR}/sqstdlib/sqstdstream.cpp
    ${SQUIRREL_DIR}/sqstdlib/sqstdstring.cpp
    ${SQUIRREL_DIR}/sqstdlib/sqstdsystem.cpp
)

add_library(squirrel STATIC ${SQUIRREL_SRC})
set_target_properties(squirrel PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(squirrel PUBLIC ${SQUIRREL_DIR}/include)
target_include_directories(squirrel PRIVATE ${SQUIRREL_DIR}/squirrel)
target_include_directories(squirrel PRIVATE ${SQUIRREL_DIR}/sqstdlib)

################################
# GIFLIB
################################

set(GIFLIB_DIR ${THIRDPARTY_DIR}/giflib)
set(GIFLIB_SRC
    ${GIFLIB_DIR}/dgif_lib.c
    ${GIFLIB_DIR}/egif_lib.c
    ${GIFLIB_DIR}/gif_err.c
    ${GIFLIB_DIR}/gif_font.c
    ${GIFLIB_DIR}/gif_hash.c
    ${GIFLIB_DIR}/gifalloc.c
    ${GIFLIB_DIR}/openbsd-reallocarray.c
)
add_library(giflib STATIC ${GIFLIB_SRC})
target_include_directories(giflib 
    PRIVATE ${GIFLIB_DIR}
    INTERFACE ${THIRDPARTY_DIR}/giflib)

################################
# Blipbuf
################################

add_library(blipbuf STATIC ${THIRDPARTY_DIR}/blip-buf/blip_buf.c)
target_include_directories(blipbuf INTERFACE ${THIRDPARTY_DIR}/blip-buf)

################################
# Duktape
################################

add_library(duktape STATIC ${THIRDPARTY_DIR}/duktape/src/duktape.c)
target_include_directories(duktape INTERFACE ${THIRDPARTY_DIR}/duktape/src)

################################
# TIC-80 core
################################

macro(MACRO_CORE SCRIPT DEFINE BUILD_DEPRECATED)

    set(TIC80CORE_DIR ${CMAKE_SOURCE_DIR}/src)
    set(TIC80CORE_SRC
        ${TIC80CORE_DIR}/core/core.c
        ${TIC80CORE_DIR}/core/draw.c
        ${TIC80CORE_DIR}/core/io.c
        ${TIC80CORE_DIR}/core/sound.c
        ${TIC80CORE_DIR}/api/js.c 
        ${TIC80CORE_DIR}/api/lua.c 
        ${TIC80CORE_DIR}/api/wren.c 
        ${TIC80CORE_DIR}/api/squirrel.c
        ${TIC80CORE_DIR}/tic.c
        ${TIC80CORE_DIR}/cart.c
        ${TIC80CORE_DIR}/tools.c 
        ${TIC80CORE_DIR}/tilesheet.c 
    )

    if(${BUILD_DEPRECATED})
        set(TIC80CORE_SRC ${TIC80CORE_SRC} ${TIC80CORE_DIR}/ext/gif.c)
    endif()

    add_library(tic80core${SCRIPT} STATIC ${TIC80CORE_SRC})

    target_include_directories(tic80core${SCRIPT}
        PRIVATE 
            ${THIRDPARTY_DIR}/moonscript
            ${THIRDPARTY_DIR}/fennel
        PUBLIC
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src)

    target_link_libraries(tic80core${SCRIPT}
        lua 
        lpeg 
        wren 
        squirrel 
        duktape 
        blipbuf 
        zlib)

    if(${BUILD_DEPRECATED})
        target_compile_definitions(tic80core${SCRIPT} PRIVATE DEPRECATED_CHUNKS)
        target_link_libraries(tic80core${SCRIPT} giflib)
    endif()

    if(LINUX)
        target_link_libraries(tic80core${SCRIPT} m)
    endif()

    target_compile_definitions(tic80core${SCRIPT} PRIVATE ${DEFINE})

endmacro()

MACRO_CORE("" "" TRUE)

if(BUILD_STUB)

    MACRO_CORE(lua      TIC_BUILD_WITH_LUA      FALSE)
    MACRO_CORE(moon     TIC_BUILD_WITH_MOON     FALSE)
    MACRO_CORE(fennel   TIC_BUILD_WITH_FENNEL   FALSE)
    MACRO_CORE(js       TIC_BUILD_WITH_JS       FALSE)
    MACRO_CORE(wren     TIC_BUILD_WITH_WREN     FALSE)
    MACRO_CORE(squirrel TIC_BUILD_WITH_SQUIRREL FALSE)

endif()

################################
# SDL2
################################
if(BUILD_SDL AND NOT EMSCRIPTEN AND NOT RPI)

    if(WIN32)
        set(HAVE_LIBC TRUE)
    endif()

    if(ANDROID)
        include_directories(${ANDROID_NDK}/sources/android/cpufeatures)
    endif()
    
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)

    add_subdirectory(${THIRDPARTY_DIR}/sdl2)

endif()

################################
# SDL2 standalone cart player
################################

if(BUILD_SDL AND BUILD_PLAYER AND NOT RPI)

    add_executable(player-sdl WIN32 ${CMAKE_SOURCE_DIR}/src/system/sdl/player.c)

    target_include_directories(player-sdl PRIVATE 
        ${THIRDPARTY_DIR}/sdl2/include 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src)

    if(MINGW)
        target_link_libraries(player-sdl mingw32)
        target_link_options(player-sdl PRIVATE -static)
    endif()

    target_link_libraries(player-sdl tic80core SDL2-static SDL2main)
endif()

################################
# Sokol
################################

if(BUILD_SOKOL)
set(SOKOL_LIB_SRC ${CMAKE_SOURCE_DIR}/src/system/sokol/sokol_gfx.c)

if(APPLE)
    set(SOKOL_LIB_SRC ${SOKOL_LIB_SRC} ${CMAKE_SOURCE_DIR}/src/system/sokol/sokol_impl.m)
else()
    set(SOKOL_LIB_SRC ${SOKOL_LIB_SRC} ${CMAKE_SOURCE_DIR}/src/system/sokol/sokol_impl.c)
endif()

add_library(sokol STATIC ${SOKOL_LIB_SRC})

if(APPLE)
    target_compile_definitions(sokol PRIVATE SOKOL_METAL)
elseif(WIN32)
    target_compile_definitions(sokol PRIVATE SOKOL_D3D11 SOKOL_D3D11_SHADER_COMPILER)
elseif(LINUX)
    target_compile_definitions(sokol PRIVATE SOKOL_GLCORE33)
endif()

if(APPLE)
    set_property (TARGET sokol APPEND_STRING PROPERTY 
        COMPILE_FLAGS "-fobjc-arc")

    target_link_libraries(sokol 
        "-framework Cocoa" 
        "-framework QuartzCore"
        "-framework Metal"
        "-framework MetalKit"
        "-framework AudioToolbox"
    )
elseif(WIN32)
    target_link_libraries(sokol D3D11)

    if(MINGW)
        target_link_libraries(sokol D3dcompiler_47)
    endif()
elseif(LINUX)
    find_package (Threads)
    target_link_libraries(sokol X11 GL m dl asound ${CMAKE_THREAD_LIBS_INIT})
endif()

target_include_directories(sokol PRIVATE ${THIRDPARTY_DIR}/sokol)
endif()

################################
# Sokol standalone cart player
################################

if(BUILD_PLAYER AND BUILD_SOKOL)

    add_executable(player-sokol WIN32 ${CMAKE_SOURCE_DIR}/src/system/sokol/player.c)

    if(MINGW)
        target_link_libraries(player-sokol mingw32)
        target_link_options(player-sokol PRIVATE -static)
    endif()

    target_include_directories(player-sokol PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${THIRDPARTY_DIR}/sokol 
        ${CMAKE_SOURCE_DIR}/src)

    target_link_libraries(player-sokol tic80core sokol)
endif()

################################
# libretro renderer example
################################

if(BUILD_LIBRETRO)
    set(LIBRETRO_DIR ${TIC80CORE_DIR}/system/libretro)
    set(LIBRETRO_SRC
        ${LIBRETRO_DIR}/tic80_libretro.c
    )

    if (LIBRETRO_STATIC)
        add_library(tic80_libretro STATIC
            ${LIBRETRO_SRC}
        )
        set_target_properties(tic80_libretro PROPERTIES SUFFIX "_partial.a")
        add_custom_command(TARGET tic80_libretro
               POST_BUILD
                   COMMAND ${CMAKE_SOURCE_DIR}/build/libretro/merge_static.sh $(AR) ${CMAKE_BINARY_DIR}/lib/tic80_libretro${LIBRETRO_SUFFIX}.a ${CMAKE_BINARY_DIR}/lib/tic80_libretro_partial.a ${CMAKE_BINARY_DIR}/lib/libtic80core.a ${CMAKE_BINARY_DIR}/lib/liblua.a ${CMAKE_BINARY_DIR}/lib/libblipbuf.a ${CMAKE_BINARY_DIR}/lib/libduktape.a ${CMAKE_BINARY_DIR}/lib/libwren.a ${CMAKE_BINARY_DIR}/lib/libsquirrel.a ${CMAKE_BINARY_DIR}/lib/libgiflib.a ${CMAKE_BINARY_DIR}/lib/liblpeg.a)
    else()
        add_library(tic80_libretro SHARED
        ${LIBRETRO_SRC}
        )
    endif()

    target_include_directories(tic80_libretro PRIVATE 
        ${CMAKE_CURRENT_BINARY_DIR}
        ${TIC80CORE_DIR})

    if(MINGW)
        target_link_libraries(tic80_libretro mingw32)
    endif()

    if(ANDROID)
        set_target_properties(tic80_libretro PROPERTIES SUFFIX "_android.so")
    endif()

    if(EMSCRIPTEN)
        set_target_properties(tic80_libretro PROPERTIES SUFFIX "_emscripten.bc")
    endif()

    # MSYS2 builds libretro to ./bin, despite it being a DLL. This forces it to ./lib.
    set_target_properties(tic80_libretro PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )

    target_link_libraries(tic80_libretro tic80core)
    set_target_properties(tic80_libretro PROPERTIES PREFIX "")
endif()

################################
# ZLIB
################################

if (NOT N3DS)

set(ZLIB_DIR ${THIRDPARTY_DIR}/zlib)
set(ZLIB_SRC 
    ${ZLIB_DIR}/adler32.c
    ${ZLIB_DIR}/compress.c
    ${ZLIB_DIR}/crc32.c
    ${ZLIB_DIR}/deflate.c
    ${ZLIB_DIR}/inflate.c
    ${ZLIB_DIR}/infback.c
    ${ZLIB_DIR}/inftrees.c
    ${ZLIB_DIR}/inffast.c
    ${ZLIB_DIR}/trees.c
    ${ZLIB_DIR}/uncompr.c
    ${ZLIB_DIR}/zutil.c
)

add_library(zlib STATIC ${ZLIB_SRC})
target_include_directories(zlib INTERFACE ${THIRDPARTY_DIR}/zlib)

else ()

add_library(zlib STATIC IMPORTED)
set_target_properties( zlib PROPERTIES IMPORTED_LOCATION ${DEVKITPRO}/portlibs/3ds/lib/libz.a )
target_include_directories(zlib INTERFACE ${DEVKITPRO}/portlibs/3ds/include)

endif ()

################################
# ZIP
################################

set(CMAKE_DISABLE_TESTING ON CACHE BOOL "" FORCE)
add_subdirectory(${THIRDPARTY_DIR}/zip)

################################
# bin2txt cart2prj prj2cart xplode
################################

if(BUILD_DEMO_CARTS)

    set(TOOLS_DIR ${CMAKE_SOURCE_DIR}/build/tools)

    add_executable(cart2prj ${TOOLS_DIR}/cart2prj.c ${CMAKE_SOURCE_DIR}/src/studio/project.c)
    target_include_directories(cart2prj PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(cart2prj tic80core)

    add_executable(prj2cart ${TOOLS_DIR}/prj2cart.c ${CMAKE_SOURCE_DIR}/src/studio/project.c)
    target_include_directories(prj2cart PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(prj2cart tic80core)

    add_executable(bin2txt ${TOOLS_DIR}/bin2txt.c)
    target_link_libraries(bin2txt zlib)

    add_executable(xplode 
        ${TOOLS_DIR}/xplode.c 
        ${CMAKE_SOURCE_DIR}/src/ext/gif.c
        ${CMAKE_SOURCE_DIR}/src/ext/png.c
        ${CMAKE_SOURCE_DIR}/src/studio/project.c)

    target_include_directories(xplode PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(xplode tic80core png giflib)

    if(LINUX)
        target_link_libraries(xplode m)
    endif()

    file(GLOB DEMO_CARTS 
        ${CMAKE_SOURCE_DIR}/demos/*.* 
        ${CMAKE_SOURCE_DIR}/demos/bunny/*.*)

    list(APPEND DEMO_CARTS 
        ${CMAKE_SOURCE_DIR}/config.lua
    )

    set(DEMO_CARTS_OUT)

    foreach(CART_FILE ${DEMO_CARTS})

        get_filename_component(CART_NAME ${CART_FILE} NAME_WE)

        set(OUTNAME ${CMAKE_SOURCE_DIR}/build/assets/${CART_NAME}.tic.dat)
        set(OUTPRJ ${CMAKE_SOURCE_DIR}/build/${CART_NAME}.tic)

        list(APPEND DEMO_CARTS_OUT ${OUTNAME})

        add_custom_command(OUTPUT ${OUTNAME}
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/prj2cart ${CART_FILE} ${OUTPRJ} && ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2txt ${OUTPRJ} ${OUTNAME} -z
            DEPENDS bin2txt prj2cart ${CART_FILE}
        )

    endforeach(CART_FILE)

    add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/build/assets/cart.png.dat
        COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2txt ${CMAKE_SOURCE_DIR}/build/cart.png ${CMAKE_SOURCE_DIR}/build/assets/cart.png.dat
        DEPENDS bin2txt ${CMAKE_SOURCE_DIR}/build/cart.png)

endif()

################################
# Wave writer
################################

add_library(wave_writer STATIC ${THIRDPARTY_DIR}/blip-buf/wave_writer.c)
target_include_directories(wave_writer INTERFACE ${THIRDPARTY_DIR}/blip-buf)

################################
# ArgParse lib
################################

add_library(argparse STATIC ${THIRDPARTY_DIR}/argparse/argparse.c)
target_include_directories(argparse INTERFACE ${THIRDPARTY_DIR}/argparse)

################################
# CURL
################################

if(NOT N3DS AND NOT EMSCRIPTEN AND NOT BAREMETALPI)
    set(USE_CURL TRUE)
endif()

if (USE_CURL)

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

    if(WIN32 AND NOT MINGW)
        set(CURL_STATIC_CRT ON CACHE BOOL "")
    endif()

    set(CMAKE_USE_OPENSSL OFF CACHE BOOL "" )
    set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "")
    set(HTTP_ONLY ON CACHE BOOL "")
    set(BUILD_CURL_EXE OFF CACHE BOOL "")
    set(CURL_CA_BUNDLE "none" CACHE STRING "")
    set(CURL_CA_PATH "none" CACHE STRING "")
    set(CURL_DISABLE_TESTS ON CACHE BOOL "")

    if(RPI)
        set(CURL_ZLIB OFF CACHE BOOL "" )
    endif()

    add_subdirectory(${THIRDPARTY_DIR}/curl)

endif()

################################
# libuv
################################

if(USE_LIBUV)
    add_subdirectory(${THIRDPARTY_DIR}/libuv)
endif()

################################
# HTTP parser
################################

if(USE_LIBUV)
    add_library(http_parser STATIC ${THIRDPARTY_DIR}/http-parser/http_parser.c)
    target_include_directories(http_parser INTERFACE ${THIRDPARTY_DIR}/http-parser)
endif()

################################
# PNG
################################

set(LIBPNG_DIR ${THIRDPARTY_DIR}/libpng)
set(LIBPNG_SRC 
    ${LIBPNG_DIR}/png.c
    ${LIBPNG_DIR}/pngerror.c
    ${LIBPNG_DIR}/pngget.c
    ${LIBPNG_DIR}/pngmem.c
    ${LIBPNG_DIR}/pngpread.c
    ${LIBPNG_DIR}/pngread.c
    ${LIBPNG_DIR}/pngrio.c
    ${LIBPNG_DIR}/pngrtran.c
    ${LIBPNG_DIR}/pngrutil.c
    ${LIBPNG_DIR}/pngset.c
    ${LIBPNG_DIR}/pngtrans.c
    ${LIBPNG_DIR}/pngwio.c
    ${LIBPNG_DIR}/pngwrite.c
    ${LIBPNG_DIR}/pngwtran.c
    ${LIBPNG_DIR}/pngwutil.c
)

configure_file(${LIBPNG_DIR}/scripts/pnglibconf.h.prebuilt ${CMAKE_CURRENT_BINARY_DIR}/pnglibconf.h)

add_library(png STATIC ${LIBPNG_SRC})

target_compile_definitions(png PRIVATE PNG_ARM_NEON_OPT=0)

target_include_directories(png 
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR} 
    PRIVATE ${THIRDPARTY_DIR}/zlib
    INTERFACE ${THIRDPARTY_DIR}/libpng)

################################
# TIC-80 studio
################################

set(TIC80LIB_DIR ${CMAKE_SOURCE_DIR}/src)
set(TIC80STUDIO_SRC
    ${TIC80LIB_DIR}/studio/screens/console.c
    ${TIC80LIB_DIR}/studio/screens/run.c
    ${TIC80LIB_DIR}/studio/screens/dialog.c
    ${TIC80LIB_DIR}/studio/screens/menu.c
    ${TIC80LIB_DIR}/studio/screens/surf.c
    ${TIC80LIB_DIR}/studio/screens/start.c
    ${TIC80LIB_DIR}/studio/editors/code.c
    ${TIC80LIB_DIR}/studio/editors/sprite.c
    ${TIC80LIB_DIR}/studio/editors/map.c
    ${TIC80LIB_DIR}/studio/editors/world.c
    ${TIC80LIB_DIR}/studio/editors/sfx.c
    ${TIC80LIB_DIR}/studio/editors/music.c
    ${TIC80LIB_DIR}/studio/studio.c
    ${TIC80LIB_DIR}/studio/config.c
    ${TIC80LIB_DIR}/studio/fs.c
    ${TIC80LIB_DIR}/studio/net.c
    ${TIC80LIB_DIR}/ext/md5.c
    ${TIC80LIB_DIR}/ext/history.c
    ${TIC80LIB_DIR}/ext/gif.c
    ${TIC80LIB_DIR}/ext/png.c
)

if(${BUILD_PRO})
    set(TIC80STUDIO_SRC ${TIC80STUDIO_SRC} ${TIC80LIB_DIR}/studio/project.c)
endif()

set(TIC80_OUTPUT tic80)

add_library(tic80studio STATIC 
    ${TIC80STUDIO_SRC} 
    ${DEMO_CARTS_OUT} 
    ${CMAKE_SOURCE_DIR}/build/assets/cart.png.dat)

if(WIN32)
    target_include_directories(tic80studio PRIVATE ${THIRDPARTY_DIR}/dirent/include)
endif()

target_include_directories(tic80studio PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(tic80studio tic80core zip wave_writer argparse giflib png)

if(USE_CURL)
    target_link_libraries(tic80studio libcurl)
endif()

if(USE_LIBUV)
    target_compile_definitions(tic80studio PRIVATE USE_LIBUV)
    target_link_libraries(tic80studio uv_a http_parser)
endif()

if(BUILD_PRO)
    target_compile_definitions(tic80studio PRIVATE TIC80_PRO)
endif()

if(BUILD_SDLGPU)
    target_compile_definitions(tic80studio PUBLIC CRT_SHADER_SUPPORT)
endif()

target_compile_definitions(tic80studio PUBLIC BUILD_EDITORS)

################################
# SDL GPU
################################

if(BUILD_SDLGPU)

set(SDLGPU_DIR ${THIRDPARTY_DIR}/sdl-gpu/src)
set(SDLGPU_SRC
    ${SDLGPU_DIR}/renderer_GLES_2.c
    ${SDLGPU_DIR}/SDL_gpu.c
    ${SDLGPU_DIR}/SDL_gpu_matrix.c
    ${SDLGPU_DIR}/SDL_gpu_renderer.c
    ${SDLGPU_DIR}/externals/stb_image/stb_image.c
    ${SDLGPU_DIR}/externals/stb_image_write/stb_image_write.c
)

if(NOT ANDROID)
    list(APPEND SDLGPU_SRC 
        ${SDLGPU_DIR}/renderer_GLES_1.c
        ${SDLGPU_DIR}/renderer_GLES_3.c
        ${SDLGPU_DIR}/renderer_OpenGL_1.c
        ${SDLGPU_DIR}/renderer_OpenGL_1_BASE.c
        ${SDLGPU_DIR}/renderer_OpenGL_2.c
        ${SDLGPU_DIR}/renderer_OpenGL_3.c
        ${SDLGPU_DIR}/renderer_OpenGL_4.c
        ${SDLGPU_DIR}/SDL_gpu_shapes.c  
        ${SDLGPU_DIR}/externals/glew/glew.c
    )
endif()

add_library(sdlgpu STATIC ${SDLGPU_SRC})

if(EMSCRIPTEN OR ANDROID)
    target_compile_definitions(sdlgpu PRIVATE GLEW_STATIC SDL_GPU_DISABLE_GLES_1 SDL_GPU_DISABLE_GLES_3 SDL_GPU_DISABLE_OPENGL)
else()
    target_compile_definitions(sdlgpu PRIVATE GLEW_STATIC SDL_GPU_DISABLE_GLES SDL_GPU_DISABLE_OPENGL_3 SDL_GPU_DISABLE_OPENGL_4)
endif()

target_include_directories(sdlgpu PUBLIC ${THIRDPARTY_DIR}/sdl-gpu/include)
target_include_directories(sdlgpu PRIVATE ${THIRDPARTY_DIR}/sdl-gpu/src/externals/glew)
target_include_directories(sdlgpu PRIVATE ${THIRDPARTY_DIR}/sdl-gpu/src/externals/glew/GL)
target_include_directories(sdlgpu PRIVATE ${THIRDPARTY_DIR}/sdl-gpu/src/externals/stb_image)
target_include_directories(sdlgpu PRIVATE ${THIRDPARTY_DIR}/sdl-gpu/src/externals/stb_image_write)

if(WIN32)
    target_link_libraries(sdlgpu opengl32)
endif()

if(LINUX)
    target_link_libraries(sdlgpu GL)
endif()

if(APPLE)
    find_library(OPENGL_LIBRARY OpenGL)
    target_link_libraries(sdlgpu ${OPENGL_LIBRARY})
endif()

if(ANDROID)
    find_library( ANDROID_LOG_LIBRARY log )
    find_library( ANDROID_GLES2_LIBRARY GLESv2 )
    find_library( ANDROID_GLES1_LIBRARY GLESv1_CM )
    target_link_libraries(sdlgpu
        ${ANDROID_LOG_LIBRARY}
        ${ANDROID_GLES2_LIBRARY}
        ${ANDROID_GLES1_LIBRARY}
    )
endif()

if(NOT EMSCRIPTEN)
    target_link_libraries(sdlgpu SDL2-static)
endif()

endif()

################################
# TIC-80 app
################################

if(BUILD_SDL)

    set(TIC80_SRC src/system/sdl/main.c)

    if(WIN32)
        
        configure_file("${PROJECT_SOURCE_DIR}/build/windows/tic80.rc.in" "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")
        set(TIC80_SRC ${TIC80_SRC} "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")

        add_executable(tic80 ${SYSTEM_TYPE} ${TIC80_SRC})

    elseif(ANDROID)

        set(TIC80_SRC ${TIC80_SRC} ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)

        add_library(tic80 SHARED ${TIC80_SRC})

        target_link_libraries(tic80 hidapi)

    else()
        add_executable(tic80 ${TIC80_SRC})
    endif()

    if(MINGW)
        target_link_libraries(tic80 mingw32)
        target_link_options(tic80 PRIVATE -static -mconsole)
    endif()

    if(EMSCRIPTEN)
        set_target_properties(tic80 PROPERTIES LINK_FLAGS "-s WASM=1 -s USE_SDL=2 -s ALLOW_MEMORY_GROWTH=1 -s FETCH=1 --pre-js ${CMAKE_SOURCE_DIR}/build/html/prejs.js -lidbfs.js")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2")

        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ASSERTIONS=1")
        endif()

    elseif(NOT ANDROID)
        target_link_libraries(tic80 SDL2main)
    endif()

    target_link_libraries(tic80 tic80studio)

    if(BUILD_TOUCH_INPUT)
        target_compile_definitions(tic80 PRIVATE TOUCH_INPUT_SUPPORT)
    endif()

    if(RPI)
        target_include_directories(tic80 PRIVATE ${SYSROOT_PATH}/usr/local/include/SDL2)
        target_link_directories(tic80 PRIVATE ${SYSROOT_PATH}/usr/local/lib ${SYSROOT_PATH}/opt/vc/lib)
        target_compile_definitions(tic80 PRIVATE __RPI__)
    endif()

    if(BUILD_SDLGPU)
        target_link_libraries(tic80 sdlgpu)
    else()
        if(EMSCRIPTEN)
        elseif(RPI)
            target_link_libraries(tic80 libSDL2.a bcm_host)
        else()
            target_link_libraries(tic80 SDL2-static)
        endif()
    endif()

    if(LINUX)

        configure_file("${PROJECT_SOURCE_DIR}/build/linux/tic80.desktop.in" "${PROJECT_SOURCE_DIR}/build/linux/tic80.desktop")

        install(TARGETS tic80 DESTINATION bin)

        SET(TIC80_DESKTOP_DIR     "share/applications/")
        SET(TIC80_PIXMAPS_DIR     "share/icons/")

        install (FILES ${PROJECT_SOURCE_DIR}/build/linux/tic80.desktop DESTINATION ${TIC80_DESKTOP_DIR})
        install (FILES ${PROJECT_SOURCE_DIR}/build/linux/tic80.xml DESTINATION ${TIC80_DESKTOP_DIR})
        install (FILES ${PROJECT_SOURCE_DIR}/build/linux/tic80.png DESTINATION ${TIC80_PIXMAPS_DIR})

    endif()
endif()

################################
# TIC-80 app (Sokol)
################################

if(BUILD_SOKOL)

    set(TIC80_SRC ${CMAKE_SOURCE_DIR}/src/system/sokol/sokol.c)

    if(WIN32)

        configure_file("${PROJECT_SOURCE_DIR}/build/windows/tic80.rc.in" "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")
        set(TIC80_SRC ${TIC80_SRC} "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")
        
        add_executable(tic80-sokol WIN32 ${TIC80_SRC})        
    else()
        add_executable(tic80-sokol ${TIC80_SRC})
    endif()

    target_include_directories(tic80-sokol PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${THIRDPARTY_DIR}/sokol)

    if(MINGW)
        target_link_libraries(tic80-sokol mingw32)
        target_link_options(tic80-sokol PRIVATE -static -mconsole)
    endif()

    target_link_libraries(tic80-sokol tic80studio sokol)

endif()

################################
# TIC-80 app (N3DS)
################################

if(N3DS)
    set(TIC80_SRC ${TIC80_SRC}
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/utils.c
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/keyboard.c
        ${CMAKE_SOURCE_DIR}/src/system/n3ds/main.c
    )

    add_executable(tic80_n3ds ${TIC80_SRC})

    target_include_directories(tic80_n3ds PRIVATE
        ${DEVKITPRO}/portlibs/3ds/include
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src)

    target_link_directories(tic80_n3ds PRIVATE ${DEVKITPRO}/libctru/lib ${DEVKITPRO}/portlibs/3ds/lib)
    target_link_libraries(tic80_n3ds tic80studio png citro3d)

    add_custom_command(TARGET tic80_n3ds
           POST_BUILD
               COMMAND ${CMAKE_SOURCE_DIR}/build/n3ds/elf_to_3dsx.sh
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build
    )
endif()

################################
# TIC-80 stub
################################

if(BUILD_STUB)

    macro(MACRO_STUB SCRIPT)

        set(TIC80_SRC 
            src/system/sdl/main.c
            src/studio/screens/run.c
            src/studio/screens/menu.c
            src/studio/screens/start.c
            src/studio/config.c
            src/studio/studio.c
            src/studio/fs.c
            src/ext/md5.c)

        if(WIN32)
            
            configure_file("${PROJECT_SOURCE_DIR}/build/windows/tic80.rc.in" "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")
            set(TIC80_SRC ${TIC80_SRC} "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")

            add_executable(tic80${SCRIPT} ${SYSTEM_TYPE} ${TIC80_SRC})

            target_include_directories(tic80${SCRIPT} PRIVATE ${THIRDPARTY_DIR}/dirent/include)
        else()
            add_executable(tic80${SCRIPT} ${TIC80_SRC})
        endif()

        target_include_directories(tic80${SCRIPT} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

        if(MINGW)
            target_link_libraries(tic80${SCRIPT} mingw32)
            target_link_options(tic80${SCRIPT} PRIVATE -static -mconsole)
        endif()

        if(EMSCRIPTEN)
            set_target_properties(tic80${SCRIPT} PROPERTIES LINK_FLAGS "-s WASM=1 -s USE_SDL=2 -s ALLOW_MEMORY_GROWTH=1 -s FETCH=1 -lidbfs.js")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2")

            if(CMAKE_BUILD_TYPE STREQUAL "Debug")
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ASSERTIONS=1")
            endif()

        else()
            target_link_libraries(tic80${SCRIPT} SDL2main)
        endif()

        target_link_libraries(tic80${SCRIPT} tic80core${SCRIPT} argparse)

        if(RPI)
            target_include_directories(tic80${SCRIPT} PRIVATE ${SYSROOT_PATH}/usr/local/include/SDL2)
            target_link_directories(tic80${SCRIPT} PRIVATE ${SYSROOT_PATH}/usr/local/lib ${SYSROOT_PATH}/opt/vc/lib)
            target_compile_definitions(tic80${SCRIPT} PRIVATE __RPI__)
        endif()

        if(BUILD_SDLGPU)
            target_link_libraries(tic80${SCRIPT} sdlgpu)
        else()
            if(EMSCRIPTEN)
            elseif(RPI)
                target_link_libraries(tic80${SCRIPT} libSDL2.a bcm_host pthread dl)
            else()
                target_link_libraries(tic80${SCRIPT} SDL2-static)
            endif()
        endif()

    endmacro()

    MACRO_STUB(lua)
    MACRO_STUB(moon)
    MACRO_STUB(fennel)
    MACRO_STUB(js)
    MACRO_STUB(wren)
    MACRO_STUB(squirrel)

endif()

################################
# Install
################################

set(CPACK_PACKAGE_NAME "TIC-80")
set(CPACK_PACKAGE_VENDOR "Nesbox")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fantasy computer for making, playing and sharing tiny games.")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_REVISION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TIC-80")

if (APPLE)

    set(CPACK_GENERATOR "Bundle")
    set(CPACK_BUNDLE_NAME "tic80")

    configure_file(${CMAKE_SOURCE_DIR}/build/macosx/tic80.plist.in ${CMAKE_SOURCE_DIR}/build/macosx/tic80.plist)
    set(CPACK_BUNDLE_PLIST ${CMAKE_SOURCE_DIR}/build/macosx/tic80.plist)

    set(CPACK_BUNDLE_ICON ${CMAKE_SOURCE_DIR}/build/macosx/tic80.icns)
    set(CPACK_BUNDLE_STARTUP_COMMAND "${CMAKE_BINARY_DIR}/bin/tic80")

    install(CODE "set(CMAKE_INSTALL_LOCAL_ONLY true)")
    include(CPack)
elseif (LINUX)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "tic80")
    set(CPACK_DEBIAN_FILE_NAME "tic80.deb")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://tic80.com")
    set(CPACK_DEBIAN_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Nesbox <grigoruk@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "education")

    if(RPI)
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
    endif()

    install(CODE "set(CMAKE_INSTALL_LOCAL_ONLY true)")
    include(CPack)
endif()
