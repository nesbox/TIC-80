################################
# Forth (Lugforth, sandboxed)
################################

# The Forth of a Build all tree: Lugforth compiled to WebAssembly, then to C
# by wasm2c, so a Forth cart runs natively in a sandbox, as a Lua one does,
# and not as native code with the player's addresses as with pForth
# (cmake/forth.cmake). The module and the wasm2c runtime are in
# vendor/lugforth, built in the Lugforth repository: see its README.md.
option(BUILD_WITH_LUGFORTH "Forth Enabled (Lugforth, sandboxed)" ${BUILD_WITH_ALL})
message("BUILD_WITH_LUGFORTH: ${BUILD_WITH_LUGFORTH}")

if(BUILD_WITH_LUGFORTH AND BUILD_WITH_FORTH)
    message(FATAL_ERROR
        "BUILD_WITH_LUGFORTH and BUILD_WITH_FORTH are the same language (forth, .fth): "
        "turn one of them off.")
endif()

if(BUILD_WITH_LUGFORTH)

    set(LUGFORTH_DIR ${THIRDPARTY_DIR}/lugforth)

    add_library(lugforth ${TIC_RUNTIME}
        ${CMAKE_SOURCE_DIR}/src/api/lugforth.c
        ${LUGFORTH_DIR}/lugforth_w2c.c
        ${LUGFORTH_DIR}/wasm-rt-impl.c
        ${LUGFORTH_DIR}/wasm-rt-exceptions-impl.c
    )

    if(NOT BUILD_STATIC)
        set_target_properties(lugforth PROPERTIES PREFIX "")
    endif()

    # The script is the one forth.c exports: src/script.c knows it by this.
    target_compile_definitions(lugforth INTERFACE TIC_BUILD_WITH_FORTH=1)

    # Memory from malloc and a bounds check on each access: portable, where
    # guard pages need mmap and a signal handler.
    target_compile_definitions(lugforth PRIVATE
        WASM_RT_USE_MMAP=0
        WASM_RT_MEMCHECK_BOUNDS_CHECK=1
    )

    target_link_libraries(lugforth PRIVATE runtime)

    target_include_directories(lugforth
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    # The generated module and the runtime warn on their own terms.
    target_include_directories(lugforth SYSTEM PRIVATE ${LUGFORTH_DIR})

endif()
