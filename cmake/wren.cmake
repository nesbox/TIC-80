################################
# WREN
################################

set(BUILD_WITH_WREN_DEFAULT TRUE)

option(BUILD_WITH_WREN "WREN Enabled" ${BUILD_WITH_WREN_DEFAULT})
message("BUILD_WITH_WREN: ${BUILD_WITH_WREN}")

if(BUILD_WITH_WREN)

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
    target_include_directories(wren PUBLIC ${THIRDPARTY_DIR}/wren/src/include)
    target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/optional)
    target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/vm)
    target_compile_definitions(wren INTERFACE TIC_BUILD_WITH_WREN=1)

endif()