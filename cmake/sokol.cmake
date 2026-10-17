################################
# Sokol
################################

if(BUILD_SOKOL)

    # The studio offers the CRT option wherever the layer can draw it, and this
    # one always can.
    target_compile_definitions(tic80studio PUBLIC CRT_SHADER_SUPPORT)

    # The swap interval is fixed when sokol makes the window, which is before
    # the studio reads its config, so the option has nothing to say here and
    # the menu leaves it out. The field stays in the config for the layers
    # that can honor it.
    target_compile_definitions(tic80studio PUBLIC VSYNC_ALWAYS_ON)

    set(SOKOL_SRC ${CMAKE_SOURCE_DIR}/src/system/sokol/sokol_impl.c)

    add_library(sokol STATIC ${SOKOL_SRC})

    # sokol is implemented inside sokol_impl.c, and its warnings are not ours
    target_include_directories(sokol SYSTEM PUBLIC ${THIRDPARTY_DIR}/sokol)

    # The layer's own entry point: the console-only run must make no window.
    target_compile_definitions(sokol PUBLIC SOKOL_NO_ENTRY)

    if(APPLE)
        target_compile_definitions(sokol PUBLIC SOKOL_METAL)
    elseif(WIN32)
        target_compile_definitions(sokol PUBLIC SOKOL_D3D11 UNICODE)
    elseif(EMSCRIPTEN AND BUILD_SOKOL_WGPU)
        # The browser's other backend: sokol_app owns the WebGPU device and swapchain.
        target_compile_definitions(sokol PUBLIC SOKOL_WGPU)
    elseif(EMSCRIPTEN)
        target_compile_definitions(sokol PUBLIC SOKOL_GLES3)
    elseif(USE_GLES2)
        # USE_GLES2 reaches sokol's GLES 3 backend; see the ARM job.
        target_compile_definitions(sokol PUBLIC SOKOL_GLES3)
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
            # The gamepad backend reads the pads through GameController, which
            # needs no permission and knows the mainstream pads by itself.
            "-framework GameController"
        )

    elseif(WIN32)
        # xinput comes with the system, and the gamepad backend reads it: an
        # Xbox-family pad or anything that presents as one. The pads it does
        # not place are read from the raw HID reports they send, which is what
        # hid — the HID class calls — and setupapi — the device list — are for.
        # Both come with every Windows SDK, and neither is a redistributable.
        target_link_libraries(sokol PRIVATE d3d11 xinput hid setupapi)
    elseif(EMSCRIPTEN)

    else()
        # The link line sokol documents; GLES draws through EGL and GLESv2.
        if(USE_GLES2)
            target_link_libraries(sokol PRIVATE X11 GLESv2 EGL Xi Xcursor m dl asound pthread)
        else()
            target_link_libraries(sokol PRIVATE X11 GL Xi Xcursor m dl asound pthread)
        endif()

        target_compile_options(sokol PRIVATE -pthread)
    endif()

endif()

################################
# TIC-80 app (Sokol)
################################

if(BUILD_SOKOL)

    set(TIC80_SRC
        ${CMAKE_SOURCE_DIR}/src/system/sokol/main.c
        ${CMAKE_SOURCE_DIR}/src/system/sokol/render.c)

    # The on-screen controls are their own module, and only a build that wants
    # touch input has them: the layer draws the picture into the whole window
    # without them, and the layout, the clay it is laid out with and the
    # textures it draws are not compiled in at all.
    if(BUILD_TOUCH_INPUT)
        set(TIC80_SRC ${TIC80_SRC} ${CMAKE_SOURCE_DIR}/src/system/sokol/controls.c)
    endif()

    if(WIN32)

        configure_file("${PROJECT_SOURCE_DIR}/build/windows/tic80.rc.in" "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")
        set(TIC80_SRC ${TIC80_SRC} "${PROJECT_SOURCE_DIR}/build/windows/tic80.rc")

    endif()

    # No WIN32 keyword: it asks for the GUI subsystem, where --cli prints
    # nothing from a terminal.
    add_executable(${TIC80_TARGET} ${TIC80_SRC})

    # clay comes in the same way, through the controls the app draws
    target_include_directories(${TIC80_TARGET} SYSTEM PRIVATE ${THIRDPARTY_DIR}/clay)

    target_include_directories(${TIC80_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${THIRDPARTY_DIR}/sokol)

    target_link_libraries(${TIC80_TARGET} PRIVATE tic80studio sokol)

    if(BUILD_TOUCH_INPUT)
        # The same name the SDL layer's touch input is compiled under.
        target_compile_definitions(${TIC80_TARGET} PRIVATE TOUCH_INPUT_SUPPORT)
    endif()

    if(EMSCRIPTEN)

        # The same runtime shape the SDL web target asks for — memory growth,
        # fetch, the 4M stack a deep cart needs, the page's file hooks — with
        # WebGL2 in place of SDL's GL.
        set(TIC80_LINK_FLAGS "-s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s FETCH=1 -s STACK_SIZE=4194304 -s EXPORTED_FUNCTIONS=_main,_malloc,_free -s EXPORTED_RUNTIME_METHODS=UTF8ToString --pre-js ${CMAKE_SOURCE_DIR}/build/html/prejs.js -lidbfs.js")

        if(BUILD_SOKOL_WGPU)

            # WebGPU in place of WebGL2; the port carries the header, so it is on compile and link.
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --use-port=emdawnwebgpu")
            # Named because the port's C++ drops them from the default set; unnamed they die in callMain.
            set(TIC80_LINK_FLAGS "${TIC80_LINK_FLAGS} --use-port=emdawnwebgpu -sDEFAULT_TO_CXX -sEXPORTED_FUNCTIONS=_main,_malloc,_free,__emscripten_stack_alloc,_emscripten_stack_get_current,__emscripten_stack_restore")

        else()

            set(TIC80_LINK_FLAGS "${TIC80_LINK_FLAGS} -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2")

        endif()

        set_target_properties(${TIC80_TARGET} PROPERTIES LINK_FLAGS "${TIC80_LINK_FLAGS}")

    endif()

endif()
