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

    list(APPEND WREN_SRC ${CMAKE_SOURCE_DIR}/src/api/wren.c)
    list(APPEND WREN_SRC ${CMAKE_SOURCE_DIR}/src/api/parse_note.c)
    
    if(BUILD_STATIC)
        add_library(wren STATIC ${WREN_SRC})
        target_compile_definitions(wren PUBLIC TIC_BUILD_STATIC)
    else()
        add_library(wren SHARED ${WREN_SRC})
        set_target_properties(wren PROPERTIES PREFIX "")
        if(MINGW)
            target_link_options(wren PRIVATE -static)
        endif()
    endif()
    
    target_include_directories(wren 
        PRIVATE 
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_include_directories(wren PUBLIC ${THIRDPARTY_DIR}/wren/src/include)
    target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/optional)
    target_include_directories(wren PRIVATE ${THIRDPARTY_DIR}/wren/src/vm)
    target_compile_definitions(wren INTERFACE TIC_BUILD_WITH_WREN=1)

    if(BUILD_DEMO_CARTS)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/wrendemo.wren)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/wrenmark.wren)
    endif()

endif()