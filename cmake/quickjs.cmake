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
        ${CMAKE_SOURCE_DIR}/src/api/js.c
        ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
    )

    if(BUILD_STATIC)
        add_library(js STATIC ${QUICKJS_SRC})
        target_compile_definitions(js PUBLIC TIC_BUILD_STATIC)
        target_link_libraries(js PRIVATE tic80core)
    else()
        add_library(js SHARED ${QUICKJS_SRC})
        set_target_properties(js PROPERTIES PREFIX "")
        if(MINGW)
            target_link_options(js PRIVATE -static)
        endif()
    endif()

    target_compile_definitions(js PUBLIC CONFIG_VERSION="${CONFIG_VERSION}")
    target_include_directories(js 
        PUBLIC ${QUICKJS_DIR}
        PRIVATE 
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(js PRIVATE DUMP_LEAKS)
    endif()

    if(BAREMETALPI OR N3DS)
        target_compile_definitions(js PRIVATE POOR_CLIB) 
    endif()

    if(LINUX)
        target_compile_definitions(js PUBLIC _GNU_SOURCE _POSIX_C_SOURCE=200112)
        target_link_libraries(js PUBLIC m dl pthread)
    endif()

    target_compile_definitions(js INTERFACE TIC_BUILD_WITH_JS=1)

    if(BUILD_DEMO_CARTS)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/jsdemo.js)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/jsmark.js)
    endif()

endif()