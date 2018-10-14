cmake_minimum_required(VERSION 3.9)

project(TIC-80 C)
message("Building for target : ${CMAKE_SYSTEM_NAME}")

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT ANDROID)
	set(LINUX TRUE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)

	foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
	    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
	    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib )
	    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib )
	    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin )
	endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )
	
	SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /INCREMENTAL:NO")
	SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /OPT:REF /INCREMENTAL:NO")
	SET(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} /OPT:REF /INCREMENTAL:NO")

	# use static runtime
	foreach(flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
		CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)

		if(${flag_var} MATCHES "/MD")
			string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
		endif(${flag_var} MATCHES "/MD")
	endforeach(flag_var)

else()

	set(CMAKE_C_STANDARD 99)

	if(CMAKE_BUILD_TYPE MATCHES "Debug")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb3")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
	endif()

if(EMSCRIPTEN)
	# TODO: add: -s \'EXTRA_EXPORTED_RUNTIME_METHODS=[\"writeArrayToMemory\"]\'
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2 -s TOTAL_MEMORY=67108864 --pre-js build/html/prejs.js --memory-init-file 0")
endif()

endif()

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

target_compile_definitions(lua PRIVATE LUA_COMPAT_5_2)

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

if(NOT EMSCRIPTEN)

	if(WIN32)
		set(HAVE_LIBC TRUE)
	endif()

	set(SDL_SHARED_ENABLED_BY_DEFAULT OFF)
	add_subdirectory(3rd-party/SDL2-2.0.8)

endif()

################################
# SDL2 renderer example
################################

if(NOT EMSCRIPTEN)
	set(EXAMPLE_DIR examples)
	set(EXAMPLE_SRC 
		${EXAMPLE_DIR}/sdl-renderer.c
	)

	if(WIN32)
		add_executable(sdl-renderer WIN32 ${EXAMPLE_SRC})
	else()
		add_executable(sdl-renderer ${EXAMPLE_SRC})
	endif()

	target_include_directories(sdl-renderer PRIVATE 3rd-party/SDL2-2.0.8/include)
	target_include_directories(sdl-renderer PRIVATE include)
	target_include_directories(sdl-renderer PRIVATE src)

	if(MINGW)
		target_link_libraries(sdl-renderer mingw32)
	endif()

	add_dependencies(sdl-renderer tic80core SDL2-static SDL2main)
	target_link_libraries(sdl-renderer tic80core SDL2-static SDL2main)
endif()

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

if(EMSCRIPTEN)
	target_compile_definitions(sdlgpu PRIVATE GLEW_STATIC DSDL_GPU_DISABLE_OPENGL SDL_GPU_DISABLE_GLES_1 SDL_GPU_DISABLE_GLES_3)
else()
	target_compile_definitions(sdlgpu PRIVATE GLEW_STATIC SDL_GPU_DISABLE_GLES SDL_GPU_DISABLE_OPENGL_3 SDL_GPU_DISABLE_OPENGL_4)
endif()

target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/include)
target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/src/externals/glew)
target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/src/externals/glew/GL)
target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/src/externals/stb_image)
target_include_directories(sdlgpu PRIVATE 3rd-party/sdl-gpu/src/externals/stb_image_write)
target_include_directories(sdlgpu PRIVATE 3rd-party/SDL2-2.0.8/include)

if(WIN32)
	target_link_libraries(sdlgpu opengl32)
endif()

if(LINUX)
	target_link_libraries(sdlgpu GL)
endif()

if(APPLE)
	FIND_LIBRARY(OPENGL_LIBRARY OpenGL)
	target_link_libraries(sdlgpu ${OPENGL_LIBRARY})
endif()

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
target_include_directories(sdlnet PRIVATE 3rd-party/SDL2-2.0.8/include)

if(WIN32)
	target_link_libraries(sdlnet ws2_32)
endif()

################################
# ZLIB
################################

set(ZLIB_DIR 3rd-party/zlib-1.2.11)
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

################################
# bin2txt
################################

if(NOT EMSCRIPTEN)
	set(BIN2TXT_DIR tools/bin2txt)
	set(BIN2TXT_SRC 
		${BIN2TXT_DIR}/bin2txt.c
	)

	add_executable(bin2txt ${BIN2TXT_SRC})

	target_include_directories(bin2txt PRIVATE 3rd-party/zlib-1.2.11)

	add_dependencies(bin2txt zlib)
	target_link_libraries(bin2txt zlib)

	file(GLOB DEMO_CARTS ${CMAKE_SOURCE_DIR}/demos/*.tic )

	list(APPEND DEMO_CARTS 
		${CMAKE_SOURCE_DIR}/config.tic
	)

	set(DEMO_CARTS_OUT)

	foreach(CART_FILE ${DEMO_CARTS})

		get_filename_component(cart_name ${CART_FILE} NAME)

		set(OUTNAME ${CMAKE_SOURCE_DIR}/bin/assets/${cart_name}.dat)

		list(APPEND DEMO_CARTS_OUT ${OUTNAME})

		add_custom_command(OUTPUT ${OUTNAME}
			COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2txt ${CART_FILE} ${OUTNAME} -z
			DEPENDS bin2txt ${CART_FILE}
		)

	endforeach(CART_FILE)

endif()

################################
# TIC-80 lib
################################

set(TIC80LIB_DIR src)
set(TIC80LIB_SRC
	${TIC80LIB_DIR}/studio.c
	${TIC80LIB_DIR}/console.c
	${TIC80LIB_DIR}/run.c
	${TIC80LIB_DIR}/ext/md5.c
	${TIC80LIB_DIR}/ext/gif.c
	${TIC80LIB_DIR}/fs.c
	${TIC80LIB_DIR}/tools.c
	${TIC80LIB_DIR}/start.c
	${TIC80LIB_DIR}/sprite.c
	${TIC80LIB_DIR}/map.c
	${TIC80LIB_DIR}/sfx.c
	${TIC80LIB_DIR}/music.c
	${TIC80LIB_DIR}/history.c
	${TIC80LIB_DIR}/world.c
	${TIC80LIB_DIR}/config.c
	${TIC80LIB_DIR}/code.c
	${TIC80LIB_DIR}/dialog.c
	${TIC80LIB_DIR}/menu.c
	${TIC80LIB_DIR}/surf.c
)

set(TIC80_OUTPUTS tic80 tic80pro)

foreach(TIC80_OUTPUT ${TIC80_OUTPUTS})

	add_library(${TIC80_OUTPUT}lib STATIC ${TIC80LIB_SRC} ${DEMO_CARTS_OUT})

	if(WIN32)
		target_include_directories(${TIC80_OUTPUT}lib PRIVATE 3rd-party/dirent)
	endif()

	target_include_directories(${TIC80_OUTPUT}lib PRIVATE include)
	target_include_directories(${TIC80_OUTPUT}lib PRIVATE 3rd-party/giflib-5.1.4/lib)
	target_include_directories(${TIC80_OUTPUT}lib PRIVATE 3rd-party/zlib-1.2.11)
	target_include_directories(${TIC80_OUTPUT}lib PRIVATE 3rd-party/lua-5.3.1/src)

	add_dependencies(${TIC80_OUTPUT}lib tic80core zlib)
	target_link_libraries(${TIC80_OUTPUT}lib tic80core zlib)

endforeach()

target_compile_definitions(tic80prolib PRIVATE TIC80_PRO)

################################
# TIC-80 app
################################

set(TIC80_DIR src)
set(TIC80_SRC
	${TIC80_DIR}/net.c
	${TIC80_DIR}/system.c
	${TIC80_DIR}/ext/file_dialog.c
)

if(APPLE)
	set(TIC80_SRC ${TIC80_SRC} ${TIC80_DIR}/ext/file_dialog.m)
endif()

foreach(TIC80_OUTPUT ${TIC80_OUTPUTS})

	if(WIN32)
		set(TIC80_SRC ${TIC80_SRC} build/windows/tic80.rc)
		add_executable(${TIC80_OUTPUT} WIN32 ${TIC80_SRC})
	elseif(APPLE)
		add_executable(${TIC80_OUTPUT} MACOSX_BUNDLE ${TIC80_SRC} build/macosx/tic80.icns)
		set_source_files_properties(build/macosx/tic80.icns PROPERTIES MACOSX_PACKAGE_LOCATION RESOURCES)
		set_target_properties(${TIC80_OUTPUT} PROPERTIES MACOSX_BUNDLE_INFO_PLIST build/macosx/${TIC80_OUTPUT}.plist)
	else()
		add_executable(${TIC80_OUTPUT} ${TIC80_SRC})
	endif()

	target_include_directories(${TIC80_OUTPUT} PRIVATE include)
	target_include_directories(${TIC80_OUTPUT} PRIVATE 3rd-party/SDL2-2.0.8/include)
	target_include_directories(${TIC80_OUTPUT} PRIVATE 3rd-party/sdl-gpu/include)
	target_include_directories(${TIC80_OUTPUT} PRIVATE 3rd-party/SDL2_net-2.0.1)

	if(MINGW)
		target_link_libraries(${TIC80_OUTPUT} mingw32)
	endif()

	if(NOT EMSCRIPTEN)
		add_dependencies(${TIC80_OUTPUT} SDL2main SDL2-static)
		target_link_libraries(${TIC80_OUTPUT} SDL2-static SDL2main)
	endif()

	add_dependencies(${TIC80_OUTPUT} ${TIC80_OUTPUT}lib sdlnet sdlgpu)
	target_link_libraries(${TIC80_OUTPUT} ${TIC80_OUTPUT}lib sdlnet sdlgpu)

	if(LINUX)
		include(FindPkgConfig)
		if(NOT PKG_CONFIG_FOUND) 
			message(FATAL_ERROR "We need pkg-config to compile this project")
		endif()

		pkg_check_modules(GTK REQUIRED gtk+-3.0)

		target_include_directories(${TIC80_OUTPUT} PRIVATE ${GTK_INCLUDE_DIRS})
		target_link_libraries(${TIC80_OUTPUT} ${GTK_LIBRARIES})
	endif()

endforeach()