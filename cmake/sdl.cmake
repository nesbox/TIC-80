################################
# SDL2
################################
if(BUILD_SDL AND NOT EMSCRIPTEN AND NOT RPI)

    if(WIN32)
        set(HAVE_LIBC TRUE)
    endif()

    if(ANDROID)
        include_directories(${ANDROID_NDK}/sources/android/cpufeatures)
        set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
    endif()

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)

    add_subdirectory(${THIRDPARTY_DIR}/sdl2)

endif()

################################
# SDL2 standalone cart player
################################

if(BUILD_SDL AND BUILD_PLAYER AND NOT RPI)

    add_executable(player-sdl WIN32 ${CMAKE_SOURCE_DIR}/src/system/sdl/player.c)

    if (FREEBSD)
        target_include_directories(player-sdl PRIVATE ${SYSROOT_PATH}/usr/local/include)
        target_link_directories(player-sdl PRIVATE ${SYSROOT_PATH}/usr/local/lib)
    endif()

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

        target_link_libraries(tic80)

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

    if (FREEBSD)
        target_include_directories(tic80 PRIVATE ${SYSROOT_PATH}/usr/local/include)
        target_link_directories(tic80 PRIVATE ${SYSROOT_PATH}/usr/local/lib)
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
