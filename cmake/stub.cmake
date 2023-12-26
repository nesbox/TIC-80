################################
# TIC-80 stub
################################

if(BUILD_STUB)

    macro(MACRO_STUB SCRIPT)

        set(TIC80_SRC
            src/system/sdl/main.c
            src/studio/screens/run.c
            src/studio/screens/menu.c
            src/studio/screens/mainmenu.c
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

        if(BUILD_SDLGPU)
            target_compile_definitions(tic80${SCRIPT} PRIVATE CRT_SHADER_SUPPORT)
        endif()

        if(MINGW)
            target_link_libraries(tic80${SCRIPT} mingw32)
            target_link_options(tic80${SCRIPT} PRIVATE -static -mconsole)
        endif()

        if(EMSCRIPTEN)
            set_target_properties(tic80${SCRIPT} PROPERTIES LINK_FLAGS "-s WASM=1 -s USE_SDL=2 -s ALLOW_MEMORY_GROWTH=1 -s FETCH=1 -s EXPORT_ES6=1 -s MODULARIZE=1 -s ENVIRONMENT=web -s EXPORT_NAME=createModule -s EXPORTED_RUNTIME_METHODS=['ENV'] -lidbfs.js")
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

    if(BUILD_WITH_LUA)
        MACRO_STUB(lua)
    endif()

    if(BUILD_WITH_MOON)
        MACRO_STUB(moon)
    endif()

    if(BUILD_WITH_FENNEL)
        MACRO_STUB(fennel)
    endif()

    if(BUILD_WITH_JS)
        MACRO_STUB(js)
    endif()

    if(BUILD_WITH_SCHEME)
        MACRO_STUB(scheme)
    endif()

    if(BUILD_WITH_SQUIRREL)
        MACRO_STUB(squirrel)
    endif()

    if(BUILD_WITH_PYTHON)
        MACRO_STUB(python)
    endif()

    if(BUILD_WITH_WREN)
        MACRO_STUB(wren)
    endif()

    if(BUILD_WITH_JANET)
        MACRO_STUB(janet)
    endif()

    if(BUILD_WITH_MRUBY)
        MACRO_STUB(ruby)
    endif()

    if(BUILD_WITH_WASM)
        MACRO_STUB(wasm)
    endif()

endif()