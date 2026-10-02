################################
# Sokol
################################

if(BUILD_SOKOL)

    # The studio offers the CRT option wherever the layer can draw it, and this
    # one always can.
    target_compile_definitions(tic80studio PUBLIC CRT_SHADER_SUPPORT)

    set(SOKOL_SRC ${CMAKE_SOURCE_DIR}/src/system/sokol/sokol_impl.c)

    add_library(sokol STATIC ${SOKOL_SRC})

    target_include_directories(sokol PUBLIC ${THIRDPARTY_DIR}/sokol)

    if(APPLE)
        target_compile_definitions(sokol PUBLIC SOKOL_METAL)
    elseif(WIN32)
        target_compile_definitions(sokol PUBLIC SOKOL_D3D11 UNICODE)
    elseif(EMSCRIPTEN)
        target_compile_definitions(sokol PUBLIC SOKOL_GLES3)
        # The page needs its data folder mounted from IndexedDB before the
        # studio starts, so the layer provides its own entry point there.
        target_compile_definitions(sokol PUBLIC SOKOL_NO_ENTRY)
    else()
        target_compile_definitions(sokol PUBLIC SOKOL_GLCORE)
    endif()

    if(APPLE)

        # sokol_app's macOS backend is Objective-C.
        target_compile_options(sokol PRIVATE -x objective-c)
        target_link_libraries(sokol PRIVATE
            "-framework Cocoa"
            "-framework IOKit"
            "-framework QuartzCore"
            "-framework Metal"
            "-framework MetalKit"
            "-framework AudioToolbox"
        )

    elseif(WIN32)
        target_link_libraries(sokol PRIVATE d3d11)
    elseif(EMSCRIPTEN)

    else()
        target_link_libraries(sokol PRIVATE X11 GL Xi Xcursor m dl asound)
    endif()

endif()

################################
# TIC-80 app (Sokol)
################################

if(BUILD_SOKOL)

    set(TIC80_SRC
        ${CMAKE_SOURCE_DIR}/src/system/sokol/main.c
        ${CMAKE_SOURCE_DIR}/src/system/sokol/render.c
        ${CMAKE_SOURCE_DIR}/src/system/sokol/controls.c)

    if(WIN32)

        configure_file("${PROJECT_SOURCE_DIR}/build/windows/tic80.rc.in" "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")
        set(TIC80_SRC ${TIC80_SRC} "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")

        add_executable(${TIC80_TARGET} WIN32 ${TIC80_SRC})

    else()
        add_executable(${TIC80_TARGET} ${TIC80_SRC})
    endif()

    target_include_directories(${TIC80_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${THIRDPARTY_DIR}/sokol
        ${THIRDPARTY_DIR}/clay)

    target_link_libraries(${TIC80_TARGET} PRIVATE tic80studio sokol)

    if(EMSCRIPTEN)

        # The same runtime shape the SDL web target asks for — memory growth,
        # fetch, the 4M stack a deep cart needs, the page's file hooks — with
        # WebGL2 in place of SDL's GL.
        set_target_properties(${TIC80_TARGET} PROPERTIES LINK_FLAGS "-s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s FETCH=1 -s STACK_SIZE=4194304 -s EXPORTED_FUNCTIONS=_main,_malloc,_free -s EXPORTED_RUNTIME_METHODS=UTF8ToString -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 --pre-js ${CMAKE_SOURCE_DIR}/build/html/prejs.js -lidbfs.js")

    endif()

endif()
