cmake_minimum_required(VERSION 3.3)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 80)
set(VERSION_REVISION 0)
set(VERSION_STATUS "-dev")

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
            COMMAND ${GIT_EXECUTABLE} rev-list HEAD --count
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE VERSION_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

    endif()
endif()

project(TIC-80 VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION} LANGUAGES C CXX)
message("Building for target : ${CMAKE_SYSTEM_NAME}")

message("PROJECT_VERSION: ${PROJECT_VERSION}${VERSION_STATUS}")

configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")

if(ANDROID OR EMSCRIPTEN)
    set(BUILD_SOKOL_DEFAULT OFF)
    set(BUILD_LIBRETRO_DEFAULT OFF)
    set(BUILD_DEMO_CARTS_DEFAULT OFF)
    set(BUILD_PLAYER_DEFAULT OFF)
else()
    set(BUILD_SOKOL_DEFAULT ON)
    set(BUILD_LIBRETRO_DEFAULT ON)
    set(BUILD_DEMO_CARTS_DEFAULT ON)
    set(BUILD_PLAYER_DEFAULT ON)
endif()

option(BUILD_SDL "SDL Enabled" ON)
option(BUILD_SOKOL "Sokol Enabled" ${BUILD_SOKOL_DEFAULT})
option(BUILD_LIBRETRO "libretro Enabled" ${BUILD_LIBRETRO_DEFAULT})
option(BUILD_DEMO_CARTS "Demo Carts Enabled" ${BUILD_DEMO_CARTS_DEFAULT})
option(BUILD_PRO "Build PRO version" FALSE)
option(BUILD_PLAYER "Build standalone players" ${BUILD_PLAYER_DEFAULT})

if (BAREMETALPI)

    set(BUILD_SDL off)
    set(BUILD_DEMO_CARTS OFF)

    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR ARM)

    if(MINGW OR CYGWIN OR WIN32)
        set(UTIL_SEARCH_CMD where)
    elseif(UNIX OR APPLE)
        set(UTIL_SEARCH_CMD which)
    endif()

    set(TOOLCHAIN_PREFIX arm-none-eabi-)

    execute_process(
      COMMAND ${UTIL_SEARCH_CMD} ${TOOLCHAIN_PREFIX}gcc
      OUTPUT_VARIABLE BINUTILS_PATH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message("Crosscompiler path is ${BINUTILS_PATH}")
    get_filename_component(ARM_TOOLCHAIN_DIR ${BINUTILS_PATH} DIRECTORY)

    # Without that flag CMake is not able to pass test compilation check
    if (${CMAKE_VERSION} VERSION_EQUAL "3.6.0" OR ${CMAKE_VERSION} VERSION_GREATER "3.6")
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    else()
        set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-E")
    endif()

    # removed squirrel language as it doesn't seem to compile under arm. Needs investigation.
    # Ideally should use the CFLAGS defined in circle build files, not hardwire them here too.
    # For RPI2
    #set(CMAKE_C_FLAGS " -DTIC_BUILD_WITH_FENNEL -DTIC_BUILD_WITH_MOON -DTIC_BUILD_WITH_JS -DTIC_BUILD_WITH_WREN -DTIC_BUILD_WITH_LUA -DLUA_32BITS -std=c99 -march=armv7-a+neon-vfpv4  -D AARCH=32 -D __circle__ -D BAREMETALPI  --specs=nosys.specs -O3 -mabi=aapcs -marm  -mfloat-abi=hard -mfpu=neon-vfpv4  -D__DYNAMIC_REENT__")
    # For RPI3
    # investigate -funsafe-math-optimizations and -march=armv8-a+crc -mcpu=cortex-a53
    set(CMAKE_C_FLAGS " -DTIC_BUILD_WITH_FENNEL -DTIC_BUILD_WITH_MOON -DTIC_BUILD_WITH_JS -DTIC_BUILD_WITH_WREN -DTIC_BUILD_WITH_LUA -DLUA_32BITS -std=c99 -march=armv8-a  -D AARCH=32 -mtune=cortex-a53  -D __circle__ -D BAREMETALPI  --specs=nosys.specs -O3 -marm -mfloat-abi=hard -mfpu=neon-fp-armv8 -funsafe-math-optimizations -D__DYNAMIC_REENT__")

    set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}gcc)
    set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

    set(CMAKE_OBJCOPY ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "objcopy tool")
    set(CMAKE_SIZE_UTIL ${ARM_TOOLCHAIN_DIR}/${TOOLCHAIN_PREFIX}size CACHE INTERNAL "size tool")

    set(CMAKE_SYSROOT ${ARM_TOOLCHAIN_DIR}/../arm-none-eabi)
    set(CMAKE_FIND_ROOT_PATH ${BINUTILS_PATH})
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

endif()

if (DISABLE_NETWORKING)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDISABLE_NETWORKING")
endif()

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN AND NOT ANDROID)
    set(LINUX TRUE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if (NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

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
target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/include)
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
target_include_directories(squirrel PRIVATE ${SQUIRREL_DIR}/include)
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
# TIC-80 core
################################

set(TIC80CORE_DIR ${CMAKE_SOURCE_DIR}/src)
set(TIC80CORE_SRC
    ${TIC80CORE_DIR}/tic80.c
    ${TIC80CORE_DIR}/tic.c 
    ${TIC80CORE_DIR}/tools.c 
    ${TIC80CORE_DIR}/jsapi.c 
    ${TIC80CORE_DIR}/luaapi.c 
    ${TIC80CORE_DIR}/wrenapi.c 
    ${TIC80CORE_DIR}/squirrelapi.c
    ${TIC80CORE_DIR}/ext/gif.c
    ${THIRDPARTY_DIR}/blip-buf/blip_buf.c # TODO: link it as lib?
    ${THIRDPARTY_DIR}/duktape/src/duktape.c # TODO: link it as lib?
)

add_library(tic80core STATIC ${TIC80CORE_SRC})

target_include_directories(tic80core 
    PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${THIRDPARTY_DIR}/blip-buf
        ${THIRDPARTY_DIR}/duktape/src
        ${THIRDPARTY_DIR}/wren/src/include
        ${THIRDPARTY_DIR}/squirrel/include
        ${THIRDPARTY_DIR}/moonscript
        ${THIRDPARTY_DIR}/fennel
    PUBLIC 
        ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(tic80core lua lpeg wren squirrel giflib)

if(LINUX)
    target_link_libraries(tic80core m)
endif()

################################
# SDL2
################################
if(BUILD_SDL AND NOT EMSCRIPTEN)

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

if(BUILD_SDL AND BUILD_PLAYER)

    add_executable(player-sdl WIN32 ${CMAKE_SOURCE_DIR}/src/player/sdl.c)

    target_include_directories(player-sdl PRIVATE 
        ${THIRDPARTY_DIR}/sdl2/include 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_SOURCE_DIR}/src)

    if(MINGW)
        target_link_libraries(player-sdl mingw32)
    endif()

    target_link_libraries(player-sdl tic80core SDL2-static SDL2main)
endif()

################################
# Sokol
################################

if(BUILD_SOKOL)
set(SOKOL_LIB_SRC ${CMAKE_SOURCE_DIR}/src/system/sokol_gfx.c)

if(APPLE)
    set(SOKOL_LIB_SRC ${SOKOL_LIB_SRC} ${CMAKE_SOURCE_DIR}/src/system/sokol_impl.m)
else()
    set(SOKOL_LIB_SRC ${SOKOL_LIB_SRC} ${CMAKE_SOURCE_DIR}/src/system/sokol_impl.c)
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

    add_executable(player-sokol WIN32 ${CMAKE_SOURCE_DIR}/src/player/sokol.c)

    if(MINGW)
        target_link_libraries(player-sokol mingw32)
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
                   COMMAND ${CMAKE_SOURCE_DIR}/build/libretro/merge_static.sh $(AR) ${CMAKE_BINARY_DIR}/lib/tic80_libretro${LIBRETRO_SUFFIX}.a ${CMAKE_BINARY_DIR}/lib/tic80_libretro_partial.a ${CMAKE_BINARY_DIR}/lib/libtic80core.a ${CMAKE_BINARY_DIR}/lib/liblua.a ${CMAKE_BINARY_DIR}/lib/libwren.a ${CMAKE_BINARY_DIR}/lib/libsquirrel.a ${CMAKE_BINARY_DIR}/lib/libgiflib.a ${CMAKE_BINARY_DIR}/lib/liblpeg.a)
    else()
        add_library(tic80_libretro SHARED
        ${LIBRETRO_SRC}
        )
    endif()

    target_include_directories(tic80_libretro PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
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
# SDL GPU
################################

if(BUILD_SDL)

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

target_include_directories(sdlgpu PRIVATE ${THIRDPARTY_DIR}/sdl-gpu/include)
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
# CURL
################################

if (NOT DISABLE_NETWORKING)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")

if(WIN32)
    set(CURL_STATIC_CRT ON CACHE BOOL "")
endif()

set(CMAKE_USE_OPENSSL OFF CACHE BOOL "" )
set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "")
set(HTTP_ONLY ON CACHE BOOL "")
set(BUILD_CURL_EXE OFF CACHE BOOL "")
set(CURL_CA_BUNDLE "none" CACHE STRING "")
set(CURL_CA_PATH "none" CACHE STRING "")

add_subdirectory(${THIRDPARTY_DIR}/curl)
endif()

################################
# ZLIB
################################

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

################################
# ZIP
################################

set(CMAKE_DISABLE_TESTING ON CACHE BOOL "" FORCE)
add_subdirectory(${THIRDPARTY_DIR}/zip)

################################
# bin2txt
################################

if(BUILD_DEMO_CARTS)
    set(BIN2TXT_DIR ${CMAKE_SOURCE_DIR}/build/tools/bin2txt)
    set(BIN2TXT_SRC 
        ${BIN2TXT_DIR}/bin2txt.c
    )

    add_executable(bin2txt ${BIN2TXT_SRC})

    target_link_libraries(bin2txt zlib)

    file(GLOB DEMO_CARTS ${CMAKE_SOURCE_DIR}/demos/*.tic )

    list(APPEND DEMO_CARTS 
        ${CMAKE_SOURCE_DIR}/config.tic
        ${CMAKE_SOURCE_DIR}/build/html/embed.html
    )

    set(DEMO_CARTS_OUT)

    foreach(CART_FILE ${DEMO_CARTS})

        get_filename_component(cart_name ${CART_FILE} NAME)

        set(OUTNAME ${CMAKE_SOURCE_DIR}/build/assets/${cart_name}.dat)

        list(APPEND DEMO_CARTS_OUT ${OUTNAME})

        add_custom_command(OUTPUT ${OUTNAME}
            COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2txt ${CART_FILE} ${OUTNAME} -z
            DEPENDS bin2txt ${CART_FILE}
        )

    endforeach(CART_FILE)

    add_custom_target(generate_demo_carts DEPENDS ${DEMO_CARTS_OUT})

endif()

################################
# TIC-80 lib
################################

set(TIC80LIB_DIR ${CMAKE_SOURCE_DIR}/src)
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
    ${TIC80LIB_DIR}/net.c
)

set(TIC80_OUTPUT tic80)

add_library(${TIC80_OUTPUT}lib STATIC ${TIC80LIB_SRC} ${DEMO_CARTS_OUT})

if(WIN32)
    target_include_directories(${TIC80_OUTPUT}lib PRIVATE ${THIRDPARTY_DIR}/dirent/include)
endif()

target_include_directories(${TIC80_OUTPUT}lib PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${THIRDPARTY_DIR}/curl/include)

target_link_libraries(${TIC80_OUTPUT}lib tic80core zlib libcurl zip)

if(BUILD_PRO)
    target_compile_definitions(tic80lib PRIVATE TIC80_PRO)
endif()

################################
# TIC-80 app
################################

if(BUILD_SDL)

    set(TIC80_SRC ${CMAKE_SOURCE_DIR}/src/ext/file_dialog.c)

    if(APPLE)
        set(TIC80_SRC ${TIC80_SRC} ${CMAKE_SOURCE_DIR}/src/ext/file_dialog.m)
    endif()

    set(TIC80_SRC ${TIC80_SRC} src/system/sdlgpu.c)

    if(WIN32)
        
        configure_file("${PROJECT_SOURCE_DIR}/build/windows/tic80.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/windows/tic80.rc")
        set(TIC80_SRC ${TIC80_SRC} build/windows/tic80.rc)

        # Debug build is a console app.
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        else()
            set(SYSTEM_TYPE WIN32)
        endif()

        add_executable(${TIC80_OUTPUT} ${SYSTEM_TYPE} ${TIC80_SRC})

    elseif(ANDROID)

        set(TIC80_SRC ${TIC80_SRC} ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c)

        add_library(${TIC80_OUTPUT} SHARED ${TIC80_SRC})

        target_link_libraries(${TIC80_OUTPUT} hidapi)

    else()
        add_executable(${TIC80_OUTPUT} ${TIC80_SRC})
    endif()

    target_include_directories(${TIC80_OUTPUT} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${THIRDPARTY_DIR}/sdl2/include
        ${THIRDPARTY_DIR}/sdl-gpu/include)

    if(MINGW)
        target_link_libraries(${TIC80_OUTPUT} mingw32)
    endif()

    if(EMSCRIPTEN)
        set_target_properties(${TIC80_OUTPUT} PROPERTIES LINK_FLAGS "-s WASM=1 -s USE_SDL=2 -s TOTAL_MEMORY=67108864 --pre-js ${CMAKE_SOURCE_DIR}/build/html/prejs.js")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_SDL=2")
    elseif(NOT ANDROID)
        target_link_libraries(${TIC80_OUTPUT} SDL2main)
    endif()

    target_link_libraries(${TIC80_OUTPUT} ${TIC80_OUTPUT}lib sdlgpu)

    if(LINUX)
        include(FindPkgConfig)
        if(NOT PKG_CONFIG_FOUND) 
            message(FATAL_ERROR "We need pkg-config to compile this project")
        endif()

        pkg_check_modules(GTK REQUIRED gtk+-3.0)

        target_include_directories(${TIC80_OUTPUT} PRIVATE ${GTK_INCLUDE_DIRS})
        target_link_libraries(${TIC80_OUTPUT} ${GTK_LIBRARIES})
    endif()

endif()

################################
# TIC-80 app (Sokol)
################################

if(BUILD_SOKOL)

    set(TIC80_SRC ${CMAKE_SOURCE_DIR}/src/ext/file_dialog.c)

    if(APPLE)
        set(TIC80_SRC ${TIC80_SRC} ${CMAKE_SOURCE_DIR}/src/ext/file_dialog.m)
    endif()

    set(TIC80_SRC ${TIC80_SRC} ${CMAKE_SOURCE_DIR}/src/system/sokol.c)

    if(WIN32)

        set(TIC80_SRC ${TIC80_SRC} build/windows/tic80.rc)
        
        add_executable(${TIC80_OUTPUT}-sokol WIN32 ${TIC80_SRC})        
    else()
        add_executable(${TIC80_OUTPUT}-sokol ${TIC80_SRC})
    endif()

    target_include_directories(${TIC80_OUTPUT}-sokol PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${THIRDPARTY_DIR}/sdl2/include
        ${THIRDPARTY_DIR}/sokol)

    if(MINGW)
        target_link_libraries(${TIC80_OUTPUT}-sokol mingw32)
    endif()

    target_link_libraries(${TIC80_OUTPUT}-sokol ${TIC80_OUTPUT}lib sokol)

    if(LINUX)
        include(FindPkgConfig)
        if(NOT PKG_CONFIG_FOUND) 
            message(FATAL_ERROR "We need pkg-config to compile this project")
        endif()

        pkg_check_modules(GTK REQUIRED gtk+-3.0)

        target_include_directories(${TIC80_OUTPUT}-sokol PRIVATE ${GTK_INCLUDE_DIRS})
        target_link_libraries(${TIC80_OUTPUT}-sokol ${GTK_LIBRARIES})
    endif()

endif()
