################################
# QuickJS
################################

set(BUILD_WITH_JS_DEFAULT TRUE)

option(BUILD_WITH_JS "JS Enabled" ${BUILD_WITH_JS_DEFAULT})
message("BUILD_WITH_JS: ${BUILD_WITH_JS}")

if(BUILD_WITH_JS)

    set(QUICKJS_DIR ${THIRDPARTY_DIR}/quickjs)

    file(STRINGS ${QUICKJS_DIR}/VERSION CONFIG_VERSION)

    set(QUICKJS_SRC
        ${QUICKJS_DIR}/quickjs.c 
        ${QUICKJS_DIR}/libregexp.c 
        ${QUICKJS_DIR}/libunicode.c
        ${QUICKJS_DIR}/cutils.c
    )

    add_library(quickjs STATIC ${QUICKJS_SRC})
    target_compile_definitions(quickjs PRIVATE CONFIG_VERSION="${CONFIG_VERSION}")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(quickjs PRIVATE DUMP_LEAKS)
    endif()

    if(BAREMETALPI OR N3DS)
        target_compile_definitions(quickjs PRIVATE POOR_CLIB) 
    endif()

    if(LINUX)
        target_compile_definitions(quickjs PUBLIC _GNU_SOURCE _POSIX_C_SOURCE=200112)
        target_link_libraries(quickjs PUBLIC m dl pthread)
    endif()
    
    set(JS_SRC
        ${CMAKE_SOURCE_DIR}/src/api/js.c
        ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
    )

    add_library(js ${TIC_RUNTIME} ${JS_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(js PROPERTIES PREFIX "")
    endif()

    target_link_libraries(js PRIVATE runtime)

    target_link_libraries(js PRIVATE quickjs)
    target_include_directories(js 
        PRIVATE 
            ${QUICKJS_DIR}
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_definitions(js INTERFACE TIC_BUILD_WITH_JS=1)

endif()