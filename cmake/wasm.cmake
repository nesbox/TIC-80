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

    add_library(wasm STATIC ${WASM_SRC})
    target_include_directories(wasm PUBLIC ${THIRDPARTY_DIR}/wasm3/source)
    target_compile_definitions(wasm INTERFACE TIC_BUILD_WITH_WASM=1)

endif()