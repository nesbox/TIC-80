################################
# Forth (Lugforth, sandboxed)
################################

# The Forth engine is Lugforth, an ANS Forth, compiled to WebAssembly and
# translated to C by wasm2c: a Forth cart runs natively, in a sandbox, as a
# Lua one does. The module and the wasm2c runtime are in vendor/lugforth,
# built in the Lugforth repository: see its README.md.
option(BUILD_WITH_FORTH "Forth Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_FORTH: ${BUILD_WITH_FORTH}")

if(BUILD_WITH_FORTH)

    set(LUGFORTH_DIR ${THIRDPARTY_DIR}/lugforth)

    # -------------------------------------------------------------------------
    # forthdemo.tic.dat — demo cartridge for 'new forth' command.
    # Source: demos/forthdemo.fth
    # Regenerate (after building prj2cart and bin2txt):
    #   prj2cart demos/forthdemo.fth /tmp/forthdemo.tic
    #   bin2txt  /tmp/forthdemo.tic build/assets/forthdemo.tic.dat -z
    # -------------------------------------------------------------------------

    add_library(forth ${TIC_RUNTIME}
        ${CMAKE_SOURCE_DIR}/src/api/forth.c
        ${LUGFORTH_DIR}/lugforth_w2c.c
        ${LUGFORTH_DIR}/wasm-rt-impl.c
        ${LUGFORTH_DIR}/wasm-rt-exceptions-impl.c
    )

    if(NOT BUILD_STATIC)
        set_target_properties(forth PROPERTIES PREFIX "")
    endif()

    target_compile_definitions(forth INTERFACE TIC_BUILD_WITH_FORTH=1)

    # Memory from malloc and a bounds check on each access: portable, where
    # guard pages need mmap and a signal handler.
    target_compile_definitions(forth PRIVATE
        WASM_RT_USE_MMAP=0
        WASM_RT_MEMCHECK_BOUNDS_CHECK=1
    )

    target_link_libraries(forth PRIVATE runtime)

    target_include_directories(forth
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    # The generated module and the runtime warn on their own terms.
    target_include_directories(forth SYSTEM PRIVATE ${LUGFORTH_DIR})

endif()
