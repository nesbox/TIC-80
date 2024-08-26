################################
# WREN
################################

option(BUILD_WITH_WREN "WREN Enabled" ${BUILD_WITH_ALL})
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

    list(APPEND WREN_SRC ${CMAKE_SOURCE_DIR}/src/api/wren.c)
    list(APPEND WREN_SRC ${CMAKE_SOURCE_DIR}/src/api/parse_note.c)

    add_library(wren ${TIC_RUNTIME} ${WREN_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(wren PROPERTIES PREFIX "")
    else()
        target_compile_definitions(wren INTERFACE TIC_BUILD_WITH_WREN=1)
    endif()

    target_link_libraries(wren PRIVATE runtime)

    target_include_directories(wren
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_include_directories(wren PUBLIC ${THIRDPARTY_DIR}/wren/src/include)
    target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/optional)
    target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/vm)

endif()