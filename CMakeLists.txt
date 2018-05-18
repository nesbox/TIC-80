project(TIC-80)

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
# TIC-80 core
################################

set(TIC80CORE_DIR src)
set(TIC80CORE_SRC
    ${TIC80CORE_DIR}/tic80.c
	${TIC80CORE_DIR}/tic.c 
	${TIC80CORE_DIR}/tools.c 
	${TIC80CORE_DIR}/jsapi.c 
	${TIC80CORE_DIR}/luaapi.c 
	${TIC80CORE_DIR}/wrenapi.c 
	${TIC80CORE_DIR}/ext/gif.c
	3rd-party/blip-buf/blip_buf.c # TODO: link it as lib?
	3rd-party/duktape-2.2.0/src/duktape.c # TODO: link it as lib?
)

add_library(tic80core STATIC ${TIC80CORE_SRC})

target_include_directories(tic80core PRIVATE include)
target_include_directories(tic80core PRIVATE 3rd-party/blip-buf)
target_include_directories(tic80core PRIVATE 3rd-party/duktape-2.2.0/src)
target_include_directories(tic80core PRIVATE 3rd-party/lua-5.3.1/src)
target_include_directories(tic80core PRIVATE 3rd-party/giflib-5.1.4/lib)
target_include_directories(tic80core PRIVATE 3rd-party/wren-0.1.0/src/include)
target_include_directories(tic80core PRIVATE 3rd-party/moonscript)
target_include_directories(tic80core PRIVATE 3rd-party/fennel)

add_dependencies(tic80core lua lpeg wren giflib)
target_link_libraries(tic80core lua lpeg wren giflib)

################################
# SDL2
################################

if(WIN32)
	set(HAVE_LIBC TRUE)
endif()

set(SDL_STATIC ON)
add_subdirectory(3rd-party/SDL2-2.0.7)

################################
# SDL2 renderer example
################################

set(EXAMPLE_DIR examples)
set(EXAMPLE_SRC 
    ${EXAMPLE_DIR}/sdl-renderer.c
)

if(WIN32)
	add_executable(sdl-renderer WIN32 ${EXAMPLE_SRC})
else()
	add_executable(sdl-renderer ${EXAMPLE_SRC})
endif()

target_include_directories(sdl-renderer PRIVATE 3rd-party/SDL2-2.0.7/include)
target_include_directories(sdl-renderer PRIVATE include)
target_include_directories(sdl-renderer PRIVATE src)

add_dependencies(sdl-renderer tic80core SDL2-static SDL2main)
target_link_libraries(sdl-renderer tic80core SDL2-static SDL2main)

################################
# SDL GPU
################################

set(SDLGPU_DIR 3rd-party/sdl-gpu/src)
set(SDLGPU_SRC
    ${SDLGPU_DIR}/renderer_GLES_1.c
	${SDLGPU_DIR}/renderer_GLES_2.c
	${SDLGPU_DIR}/renderer_GLES_3.c
	${SDLGPU_DIR}/renderer_OpenGL_1.c
	${SDLGPU_DIR}/renderer_OpenGL_1_BASE.c
	${SDLGPU_DIR}/renderer_OpenGL_2.c
	${SDLGPU_DIR}/renderer_OpenGL_3.c
	${SDLGPU_DIR}/renderer_OpenGL_4.c
	${SDLGPU_DIR}/SDL_gpu.c
	${SDLGPU_DIR}/SDL_gpu_matrix.c
	${SDLGPU_DIR}/SDL_gpu_renderer.c
	${SDLGPU_DIR}/SDL_gpu_shapes.c
	${SDLGPU_DIR}/externals/glew/glew.c
	${SDLGPU_DIR}/externals/stb_image/stb_image.c
	${SDLGPU_DIR}/externals/stb_image_write/stb_image_write.c
)

add_library(sdlgpu STATIC ${SDLGPU_SRC})

target_compile_definitions(sdlgpu PRIVATE GLEW_STATIC SDL_GPU_DISABLE_GLES)

target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/include)
target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/src/externals/glew)
target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/src/externals/glew/GL)
target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/src/externals/stb_image)
target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/src/externals/stb_image_write)
target_include_directories(sdlgpu PRIVATE 3rd-party/SDL2-2.0.7/include)

add_dependencies(sdlgpu SDL2-static)
target_link_libraries(sdlgpu SDL2-static opengl32)

################################
# SDL NET
################################

set(SDLNET_DIR 3rd-party/SDL2_net-2.0.1)
set(SDLNET_SRC 
	${SDLNET_DIR}/SDLnet.c
	${SDLNET_DIR}/SDLnetTCP.c
	${SDLNET_DIR}/SDLnetselect.c
)

add_library(sdlnet STATIC ${SDLNET_SRC})
target_include_directories(sdlnet PRIVATE 3rd-party/SDL2-2.0.7/include)

target_link_libraries(sdlnet ws2_32)

################################
# ZLIB
################################

set(BUILD_SHARED_LIBS FALSE)
add_subdirectory(3rd-party/zlib-1.2.11)

################################
# TIC-80
################################

set(TIC80_DIR src)
set(TIC80_SRC
	${TIC80_DIR}/studio.c
	${TIC80_DIR}/console.c
	${TIC80_DIR}/run.c
	${TIC80_DIR}/ext/file_dialog.c
	${TIC80_DIR}/ext/md5.c
	${TIC80_DIR}/ext/gif.c
	${TIC80_DIR}/fs.c
	${TIC80_DIR}/tools.c
	${TIC80_DIR}/start.c
	${TIC80_DIR}/sprite.c
	${TIC80_DIR}/map.c
	${TIC80_DIR}/sfx.c
	${TIC80_DIR}/music.c
	${TIC80_DIR}/history.c
	${TIC80_DIR}/world.c
	${TIC80_DIR}/config.c
	${TIC80_DIR}/code.c
	${TIC80_DIR}/dialog.c
	${TIC80_DIR}/menu.c
	${TIC80_DIR}/net.c
	${TIC80_DIR}/surf.c
	${TIC80_DIR}/html.c
	${TIC80_DIR}/system.c
)

if(WIN32)
	add_executable(tic80 WIN32 ${TIC80_SRC})
	target_include_directories(tic80 PRIVATE build/windows/include)
else()
	add_executable(tic80 ${TIC80_SRC})
endif()

target_include_directories(tic80 PRIVATE include)
target_include_directories(tic80 PRIVATE 3rd-party/SDL2-2.0.7/include)
target_include_directories(tic80 PRIVATE 3rd-party/sdl-gpu/include)
target_include_directories(tic80 PRIVATE 3rd-party/SDL2_net-2.0.1)
target_include_directories(tic80 PRIVATE 3rd-party/giflib-5.1.4/lib)
target_include_directories(tic80 PRIVATE 3rd-party/zlib-1.2.11)
target_include_directories(tic80 PRIVATE 3rd-party/lua-5.3.1/src)

add_dependencies(tic80 tic80core sdlgpu sdlnet SDL2main zlibstatic)
target_link_libraries(tic80 tic80core sdlgpu sdlnet SDL2main zlibstatic)
