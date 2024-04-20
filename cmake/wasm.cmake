################################
# WASM
################################

set(BUILD_WITH_WASM_DEFAULT TRUE)

option(BUILD_WITH_WASM "Wasm Enabled" ${BUILD_WITH_WASM_DEFAULT})
message("BUILD_WITH_WASM: ${BUILD_WITH_WASM}")

if(BUILD_WITH_WASM)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dd_m3LogOutput=0")
    set(WASM_DIR ${THIRDPARTY_DIR}/wasm3/source)
    set(WASM_SRC
        ${WASM_DIR}/m3_bind.c
        ${WASM_DIR}/m3_code.c
        ${WASM_DIR}/m3_compile.c
        ${WASM_DIR}/m3_core.c
        ${WASM_DIR}/m3_parse.c
        ${WASM_DIR}/m3_env.c
        ${WASM_DIR}/m3_exec.c
        ${WASM_DIR}/m3_function.c
        ${WASM_DIR}/m3_info.c
        ${WASM_DIR}/m3_module.c
    )

    list(APPEND WASM_SRC ${CMAKE_SOURCE_DIR}/src/api/wasm.c)

    if(BUILD_STATIC)
        add_library(wasm STATIC ${WASM_SRC})
        target_compile_definitions(wasm PUBLIC TIC_BUILD_STATIC)
        target_link_libraries(wasm PRIVATE tic80core)
    else()
        add_library(wasm SHARED ${WASM_SRC})
        set_target_properties(wasm PROPERTIES PREFIX "")
        if(MINGW)
            target_link_options(wasm PRIVATE -static)
        endif()
    endif()

    target_include_directories(wasm 
        PUBLIC ${WASM_DIR}
        PRIVATE 
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_definitions(wasm INTERFACE TIC_BUILD_WITH_WASM=1)

    # we need to build these separately combining both the project
    # and the external WASM binary chunk since projects do not
    # include BINARY chunks

    if(BUILD_DEMO_CARTS)

        file(GLOB WASM_DEMOS
            ${DEMO_CARTS_IN}/wasm/*.wasmp
            ${DEMO_CARTS_IN}/bunny/wasmmark/*.wasmp
        )

        foreach(CART_FILE ${WASM_DEMOS})

            get_filename_component(CART_NAME ${CART_FILE} NAME_WE)
            get_filename_component(DIR ${CART_FILE} DIRECTORY)

            set(OUTNAME ${CMAKE_SOURCE_DIR}/build/assets/${CART_NAME}.tic.dat)
            set(WASM_BINARY ${DIR}/${CART_NAME}.wasm)
            set(OUTPRJ ${CMAKE_SOURCE_DIR}/build/${CART_NAME}.tic)
            list(APPEND DEMO_CARTS_OUT ${OUTNAME})
            add_custom_command(OUTPUT ${OUTNAME}
                COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/wasmp2cart ${CART_FILE} ${OUTPRJ} --binary ${WASM_BINARY} && ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bin2txt ${OUTPRJ} ${OUTNAME} -z
                DEPENDS bin2txt wasmp2cart ${CART_FILE} ${WASM_BINARY}
            )

        endforeach(CART_FILE)
    endif()

endif()