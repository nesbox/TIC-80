cmake_minimum_required(VERSION 3.6)
project(TIC-80)
set(CMAKE_C_STANDARD 99)
message("Building for target : ${CMAKE_SYSTEM_NAME}")


include(FindPkgConfig)
if(NOT PKG_CONFIG_FOUND AND CMAKE_SYSTEM_NAME STREQUAL "Linux") 
 message(FATAL_ERROR "We need pkg-config to compile this project")
endif()

# Emscripten
find_path(EMDIR
 DOC("Location of the emscripten binaries")
)
if(NOT EMDIR)
 message(FATAL "We need the emscipten compiler.")
endif()
set(EMCC ${EMDIR}/emcc)
set(EMMAKE ${EMDIR}/emmake)


########################
# 3rd party dependencies
########################

#platform specific includes
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
 pkg_check_modules(DEPS REQUIRED gtk+-3.0)
endif()

#TODO
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
endif()


#On windows we just get a replacement dirent.
if(WIN32)
 include_directories(windows)

 file(DOWNLOAD 
  "https://github.com/tronkko/dirent/archive/1.23.1.tar.gz"
  ${CMAKE_SOURCE_DIR}/dirent.tar.gz
  # EXPECTED_HASH SHA256=c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1
 )
 execute_process(
     COMMAND ${CMAKE_COMMAND} -E tar xf dirent.tar.gz 
     WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
 )

 include_directories(dirent-1.23.1/include)
endif()

#We need zlib
file(DOWNLOAD 
 "https://zlib.net/zlib-1.2.11.tar.gz"
 ${CMAKE_SOURCE_DIR}/zlib.tar.gz
 EXPECTED_HASH SHA256=c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1
)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf zlib.tar.gz 
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#We need SDL2 too
file(DOWNLOAD 
 "https://libsdl.org/release/SDL2-2.0.6.tar.gz"
 ${CMAKE_SOURCE_DIR}/sdl.tar.gz
 EXPECTED_HASH SHA256=03658b5660d16d7b31263a691e058ed37acdab155d68dabbad79998fb552c5df
)
execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf sdl.tar.gz 
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#We most definitly need LUA5
file(DOWNLOAD 
  "http://www.lua.org/ftp/lua-5.3.4.tar.gz"
  ${CMAKE_SOURCE_DIR}/lua.tar.gz
  EXPECTED_HASH SHA256=f681aa518233bc407e23acf0f5887c884f17436f000d453b2491a9f11a52400c
)
execute_process(
  COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/lua.tar.gz 
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#The newer lua math functions
file(DOWNLOAD 
 "http://www.inf.puc-rio.br/~roberto/lpeg/lpeg-1.0.1.tar.gz"
 ${CMAKE_SOURCE_DIR}/lpeg.tar.gz
 EXPECTED_HASH SHA256=62d9f7a9ea3c1f215c77e0cadd8534c6ad9af0fb711c3f89188a8891c72f026b
)
execute_process(
  COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/lpeg.tar.gz 
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

#Giflib
file(DOWNLOAD 
  "https://sourceforge.net/projects/giflib/files/giflib-5.1.4.tar.gz/download"
  ${CMAKE_SOURCE_DIR}/giflib.tar.gz
  EXPECTED_HASH SHA256=34a7377ba834397db019e8eb122e551a49c98f49df75ec3fcc92b9a794a4f6d1
)
execute_process(
  COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_SOURCE_DIR}/giflib.tar.gz
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)


set(BUILD_SHARED_LIBS FALSE)
add_subdirectory(zlib-1.2.11)
include_directories(zlib-1.2.11 ${CMAKE_BINARY_DIR}/zlib-1.2.11)

########################
#/3rd party dependencies
########################


set(SDL_SHARED FALSE)
add_subdirectory(SDL2-2.0.6)
include_directories(SDL2-2.0.6/include)

set(LUADIR lua-5.3.4/src)
set(LUASRCS 
 ${LUADIR}/lapi.c
 ${LUADIR}/lcode.c
 ${LUADIR}/lctype.c
 ${LUADIR}/ldebug.c
 ${LUADIR}/ldo.c
 ${LUADIR}/ldump.c
 ${LUADIR}/lfunc.c
 ${LUADIR}/lgc.c
 ${LUADIR}/llex.c
 ${LUADIR}/lmem.c
 ${LUADIR}/lobject.c
 ${LUADIR}/lopcodes.c
 ${LUADIR}/lparser.c
 ${LUADIR}/lstate.c
 ${LUADIR}/lstring.c
 ${LUADIR}/ltable.c
 ${LUADIR}/ltm.c
 ${LUADIR}/lundump.c
 ${LUADIR}/lvm.c
 ${LUADIR}/lzio.c

 ${LUADIR}/lauxlib.c
 ${LUADIR}/lbaselib.c
 ${LUADIR}/lbitlib.c
 ${LUADIR}/lcorolib.c
 ${LUADIR}/ldblib.c
 ${LUADIR}/liolib.c
 ${LUADIR}/lmathlib.c
 ${LUADIR}/loslib.c
 ${LUADIR}/lstrlib.c
 ${LUADIR}/ltablib.c
 ${LUADIR}/lutf8lib.c
 ${LUADIR}/loadlib.c
 #${LUADIR}/lua.c
 #${LUADIR}/luac.c
 ${LUADIR}/linit.c
)
add_library(lua5 STATIC 
 ${LUASRCS}
)

set(LPEGDIR lpeg-1.0.1)
set(LPEGSRCS 
 ${LPEGDIR}/lpcap.c
 ${LPEGDIR}/lpcode.c
 ${LPEGDIR}/lpprint.c
 ${LPEGDIR}/lptree.c
 ${LPEGDIR}/lpvm.c
)
add_library(lpeg STATIC 
 ${LPEGSRCS}
)
add_dependencies(lpeg lua5)

set(GIFLIBDIR giflib-5.1.4/lib)
set(GIFLIBSRCS 
 ${GIFLIBDIR}/dgif_lib.c
 ${GIFLIBDIR}/egif_lib.c
 ${GIFLIBDIR}/gif_err.c
 ${GIFLIBDIR}/gif_font.c
 ${GIFLIBDIR}/gif_hash.c
 ${GIFLIBDIR}/gifalloc.c
 ${GIFLIBDIR}/openbsd-reallocarray.c
)
add_library(giflibstatic STATIC 
 ${GIFLIBSRCS}
)
include_directories(${GIFLIBDIR})


set(INCLUDES
 include/tic80
 ${CMAKE_BINARY_DIR}
 ${LUADIR}
 ${LPEGDIR}
 ${GIFLIBDIR}
)

set(EMS_INCLUDES "")
foreach(INCL ${INCLUDES})
 set(EMS_INCLUDES ${EMS_INCLUDES} -I${INCL})
endforeach()

set(EMS_OPT -O3 -Wall -std=c99 -D_GNU_SOURCE -Wno-typedef-redefinition -s USE_ZLIB=1 -s USE_SDL=2 -s TOTAL_MEMORY=67108864 --llvm-lto 1 --memory-init-file 0 --pre-js ${CMAKE_SOURCE_DIR}/lib/emscripten/prejs.js) 
set(EMS_LINKER_FLAGS 
 -L${THIRDPARTY_EMSCRIPTED}/lib
 #-Llib/emscripten 
 -lzlib
)


set(INCLUDES ${INCLUDES}
 ${DEPS_INCLUDE_DIRS}
)
include_directories(${INCLUDES})

link_directories(
 ${DEPS_LIBRARY_DIRS}
)


#the actual machine
set(SOURCES
 src/studio.c 
 src/console.c 
 src/run.c 
 src/ext/file_dialog.c 
 src/ext/md5.c 
 src/ext/gif.c 
 src/ext/net/SDLnet.c 
 src/ext/net/SDLnetTCP.c 
 src/ext/net/SDLnetselect.c 
 src/fs.c 
 src/tools.c 
 src/start.c 
 src/sprite.c 
 src/map.c 
 src/sfx.c 
 src/music.c 
 src/history.c 
 src/world.c 
 src/config.c 
 src/keymap.c 
 src/code.c 
 src/dialog.c 
 src/menu.c 
 src/net.c 
 src/surf.c
)

set(SOURCES_EXT
 src/html.c
)

SET(TIC80_SRC 
 src/tic80.c 
 src/tic.c 
 src/ext/blip_buf.c 
 src/jsapi.c 
 src/luaapi.c 
 src/ext/duktape/duktape.c
)

set(TIC80_H
 include/tic80/tic80_types.h 
 include/tic80/tic80.h 
 include/tic80/tic80_config.h 
 src/tic.h src/ticapi.h 
 src/machine.h
)

file(GLOB TIC_H_1 "src/*.h")
file(GLOB TIC_H_2 "src/src/*.h")
set(TIC_H ${TIC_H_1} ${TIC_H_2})

set(DEMO_ASSETS
 demos/fire.tic
 demos/p3d.tic
 demos/palette.tic
 demos/quest.tic
 demos/sfx.tic
 demos/music.tic
 demos/font.tic
 demos/tetris.tic
 demos/jsdemo.tic
 demos/luademo.tic
 demos/moondemo.tic
 config.tic
 build/html/index.html
)
#to create the .dat versions of the above files we 
#need bin2txt
add_subdirectory(tools)

#the macro to convert the .tic (and others) to .dat
macro(CREATE_ASSET INNAME DESTLIST)
 get_filename_component(FILENAME ${INNAME} NAME)
#I don't like this, but this way the old makefiles should continue to work.
 set(OUTNAME ${CMAKE_SOURCE_DIR}/bin/assets/${FILENAME}.dat)
 message("Asset : ${INNAME} -> ${OUTNAME}")
 add_custom_command(OUTPUT ${OUTNAME}
    COMMAND bin2txt ${INNAME} ${OUTNAME} -z
    MAIN_DEPENDENCY ${INNAME}
 )
 list(APPEND ${DESTLIST} ${OUTNAME})
endmacro()


set(BIN_ASSETS "")
foreach(TICFILE ${DEMO_ASSETS})
 CREATE_ASSET(${CMAKE_SOURCE_DIR}/${TICFILE} BIN_ASSETS)
endforeach()
#the .dat files are dropped into the binary folder.
include_directories(${CMAKE_BINARY_DIR})

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/tic.js
    DEPENDS ${BIN_ASSETS}
    COMMAND ${EMCC} ${EMS_INCLUDES} ${EMS_OPT} ${SOURCES} ${TIC80_SRC} ${LPEGSRCS} ${LUASRCS} ${GIFLIBSRCS} ${OPT} ${EMS_INCLUDES} ${EMS_OPT} ${EMS_LINKER_FLAGS} -o ${CMAKE_BINARY_DIR}/tic.js
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

CREATE_ASSET(${CMAKE_BINARY_DIR}/tic.js BIN_ASSETS)
add_library(tic80static STATIC ${TIC80_SRC})
add_dependencies(tic80static lua5 giflibstatic)

add_executable(tic ${SOURCES} ${SOURCES_EXT} ${BIN_ASSETS} ${CMAKE_BINARY_DIR}/tic.js ${CMAKE_SOURCE_DIR}/bin/assets/tic.js.dat)
set_target_properties(tic PROPERTIES
 COMPILE_DEFINITIONS main=SDL_main
 COMPILE_DEFINITIONS D_FILE_OFFSET_BITS=64
 INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR} 
)

if(WIN32)
 set(EXTRA_DEPS Ws2_32)
else() 
 set_target_properties(tic PROPERTIES
 COMPILE_DEFINITIONS _GNU_SOURCE=1
)
 set_target_properties(tic80static PROPERTIES
 COMPILE_DEFINITIONS _GNU_SOURCE=1
)
endif()
target_link_libraries(tic tic80static lpeg giflibstatic lua5 ${DEPS_LIBRARIES} ${EXTRA_DEPS} ${EXTRA_LIBS} SDL2main SDL2-static zlibstatic )
