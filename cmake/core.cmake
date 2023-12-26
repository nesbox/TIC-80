################################
# TIC-80 core
################################

macro(MACRO_CORE SCRIPT DEFINE BUILD_DEPRECATED)

    set(TIC80CORE_DIR ${CMAKE_SOURCE_DIR}/src)
    set(TIC80CORE_SRC
        ${TIC80CORE_DIR}/core/core.c
        ${TIC80CORE_DIR}/core/languages.c
        ${TIC80CORE_DIR}/core/draw.c
        ${TIC80CORE_DIR}/core/io.c
        ${TIC80CORE_DIR}/core/sound.c
        ${TIC80CORE_DIR}/tic.c
        ${TIC80CORE_DIR}/cart.c
        ${TIC80CORE_DIR}/tools.c
        ${TIC80CORE_DIR}/zip.c
        ${TIC80CORE_DIR}/tilesheet.c
    )

    if(BUILD_WITH_LUA)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/lua.c)
    endif()

    if(BUILD_WITH_MOON)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/moonscript.c)
    endif()

    if(BUILD_WITH_FENNEL)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/fennel.c)
    endif()

    if(BUILD_WITH_JS)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/js.c)
    endif()

    if(BUILD_WITH_SCHEME)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/scheme.c)
    endif()

    if(BUILD_WITH_SQUIRREL)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/squirrel.c)
    endif()

    if(BUILD_WITH_PYTHON)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/python.c)
    endif()

    if(BUILD_WITH_WREN)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/wren.c)
    endif()

    if(BUILD_WITH_MRUBY)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/mruby.c)
    endif()

    if(BUILD_WITH_JANET)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/janet.c)
    endif()

    if(BUILD_WITH_WASM)
        list(APPEND TIC80CORE_SRC ${TIC80CORE_DIR}/api/wasm.c)
    endif()

    if(${BUILD_DEPRECATED})
        set(TIC80CORE_SRC ${TIC80CORE_SRC} ${TIC80CORE_DIR}/ext/gif.c)
    endif()

    add_library(tic80core${SCRIPT} STATIC ${TIC80CORE_SRC})

    if (FREEBSD)
        target_include_directories(tic80core${SCRIPT} PRIVATE ${SYSROOT_PATH}/usr/local/include)
        target_link_directories(tic80core${SCRIPT} PRIVATE ${SYSROOT_PATH}/usr/local/lib)
    endif()

    target_include_directories(tic80core${SCRIPT}
        PRIVATE
            ${THIRDPARTY_DIR}/moonscript
            ${THIRDPARTY_DIR}/fennel
            ${POCKETPY_DIR}/src
        PUBLIC
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src)

    target_link_libraries(tic80core${SCRIPT} blipbuf zlib)

    if(BUILD_WITH_LUA)
        target_link_libraries(tic80core${SCRIPT} lua)
    endif()

    if(BUILD_WITH_JS)
        target_link_libraries(tic80core${SCRIPT} quickjs)
    endif()

    if(BUILD_WITH_SCHEME)
        target_link_libraries(tic80core${SCRIPT} scheme)
    endif()

    if(BUILD_WITH_SQUIRREL)
        target_link_libraries(tic80core${SCRIPT} squirrel)
    endif()

    if(BUILD_WITH_PYTHON)
        target_link_libraries(tic80core${SCRIPT} python)
    endif()

    if(BUILD_WITH_WREN)
        target_link_libraries(tic80core${SCRIPT} wren)
    endif()

    if(BUILD_WITH_MRUBY)
        target_link_libraries(tic80core${SCRIPT} mruby)
    endif()

    if(BUILD_WITH_JANET)
        target_link_libraries(tic80core${SCRIPT} janet)
    endif()

    if(BUILD_WITH_WASM)
        target_link_libraries(tic80core${SCRIPT} wasm)
    endif()

    if(${BUILD_DEPRECATED})
        target_compile_definitions(tic80core${SCRIPT} PRIVATE BUILD_DEPRECATED)
        target_link_libraries(tic80core${SCRIPT} giflib)
    endif()

    if(LINUX)
        target_link_libraries(tic80core${SCRIPT} m)
    endif()

    target_compile_definitions(tic80core${SCRIPT} PUBLIC ${DEFINE})

endmacro()

MACRO_CORE("" "" TRUE)

if(BUILD_STUB)

    if(BUILD_WITH_LUA)
        MACRO_CORE(lua TIC_BUILD_WITH_LUA FALSE)
    endif()

    if(BUILD_WITH_MOON)
        MACRO_CORE(moon TIC_BUILD_WITH_MOON FALSE)
    endif()

    if(BUILD_WITH_FENNEL)
        MACRO_CORE(fennel TIC_BUILD_WITH_FENNEL FALSE)
    endif()

    if(BUILD_WITH_JS)
        MACRO_CORE(js TIC_BUILD_WITH_JS FALSE)
    endif()

    if(BUILD_WITH_SCHEME)
        MACRO_CORE(scheme TIC_BUILD_WITH_SCHEME FALSE)
    endif()

    if(BUILD_WITH_SQUIRREL)
        MACRO_CORE(squirrel TIC_BUILD_WITH_SQUIRREL FALSE)
    endif()

    if(BUILD_WITH_PYTHON)
        MACRO_CORE(python TIC_BUILD_WITH_PYTHON FALSE)
    endif()

    if(BUILD_WITH_WREN)
        MACRO_CORE(wren TIC_BUILD_WITH_WREN FALSE)
    endif()

    if(BUILD_WITH_MRUBY)
        MACRO_CORE(ruby TIC_BUILD_WITH_MRUBY FALSE)
    endif()

    if(BUILD_WITH_JANET)
        MACRO_CORE(janet TIC_BUILD_WITH_JANET FALSE)
    endif()

    if(BUILD_WITH_WASM)
        MACRO_CORE(wasm TIC_BUILD_WITH_WASM FALSE)
    endif()

endif()
