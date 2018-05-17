project(tic80)

set(CMAKE_C_STANDARD 99)

################################
# LUA
################################

set(LUA_DIR 3rd-party/lua-5.3.1/src)
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
	${LUA_DIR}/lbitlib.c
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

################################
# LPEG
################################

set(LPEG_DIR 3rd-party/lpeg-1.0.1)
set(LPEG_SRC 
	${LPEG_DIR}/lpcap.c
	${LPEG_DIR}/lpcode.c
	${LPEG_DIR}/lpprint.c
	${LPEG_DIR}/lptree.c
	${LPEG_DIR}/lpvm.c
)

add_library(lpeg STATIC ${LPEG_SRC})
target_include_directories(lpeg PRIVATE 3rd-party/lua-5.3.1/src)

set(TIC80_DIR src)
set(TIC80_SRC
    ${TIC80_DIR}/tic80.c
	${TIC80_DIR}/tic.c 
	${TIC80_DIR}/tools.c 
	${TIC80_DIR}/jsapi.c 
	${TIC80_DIR}/luaapi.c 
	${TIC80_DIR}/wrenapi.c 
	${TIC80_DIR}/ext/gif.c
	3rd-party/blip-buf/blip_buf.c # TODO: link it as lib?
	3rd-party/duktape-2.2.0/src/duktape.c # TODO: link it as lib?
)

################################
# WREN
################################

set(WREN_DIR 3rd-party/wren-0.1.0/src)
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
target_include_directories(wren PRIVATE 3rd-party/wren-0.1.0/src/include)
target_include_directories(wren PRIVATE 3rd-party/wren-0.1.0/src/optional)
target_include_directories(wren PRIVATE 3rd-party/wren-0.1.0/src/vm)

################################
# GIFLIB
################################

set(GIFLIB_DIR 3rd-party/giflib-5.1.4/lib)
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
target_include_directories(giflib PRIVATE ${GIFLIB_DIR})

################################
# TIC-80 lib
################################

add_library(tic80lib STATIC ${TIC80_SRC})

target_include_directories(tic80lib PRIVATE include)
target_include_directories(tic80lib PRIVATE 3rd-party/blip-buf)
target_include_directories(tic80lib PRIVATE 3rd-party/duktape-2.2.0/src)
target_include_directories(tic80lib PRIVATE 3rd-party/lua-5.3.1/src)
target_include_directories(tic80lib PRIVATE 3rd-party/giflib-5.1.4/lib)
target_include_directories(tic80lib PRIVATE 3rd-party/wren-0.1.0/src/include)
target_include_directories(tic80lib PRIVATE 3rd-party/moonscript)
target_include_directories(tic80lib PRIVATE 3rd-party/fennel)

add_dependencies(tic80lib lua lpeg wren giflib)
target_link_libraries(tic80lib lua lpeg wren giflib)

add_subdirectory(3rd-party/SDL2-2.0.7)

set(EXAMPLE_DIR examples)
set(EXAMPLE_SRC 
    ${EXAMPLE_DIR}/sdl-renderer.c
)

add_executable(example ${EXAMPLE_SRC})

target_include_directories(example PRIVATE 3rd-party/SDL2-2.0.7/include)
target_include_directories(example PRIVATE include)
target_include_directories(example PRIVATE src)

add_dependencies(example tic80lib SDL2 SDL2main)
target_link_libraries(example tic80lib SDL2 SDL2main)
