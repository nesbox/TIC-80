################################
# QuickJS
################################

option(BUILD_WITH_JS "JS Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_JS: ${BUILD_WITH_JS}")

if(BUILD_WITH_JS AND PREFER_SYSTEM_LIBRARIES)
    find_path(quickjs_INLCUDE_DIR NAMES quickjs.h PATH_SUFFIXES quickjs)
    find_library(quickjs_LIBRARY NAMES quickjs PATH_SUFFIXES quickjs)
    if(quickjs_INLCUDE_DIR AND quickjs_LIBRARY)
        add_library(js STATIC
            ${CMAKE_SOURCE_DIR}/src/api/js.c
            ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
        )
        target_compile_definitions(js INTERFACE TIC_BUILD_WITH_JS)
        target_link_libraries(js PRIVATE runtime ${quickjs_LIBRARY})
        target_include_directories(js
            PUBLIC ${quickjs_INLCUDE_DIR}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src
        )
        message(STATUS "Use system library: quickjs")
        return()
    else()
        message(WARNING "System library quickjs not found")
    endif()
endif()

if(BUILD_WITH_JS)

    set(QUICKJS_DIR ${THIRDPARTY_DIR}/quickjs)

    file(STRINGS ${QUICKJS_DIR}/VERSION CONFIG_VERSION)

    set(QUICKJS_SRC
        ${QUICKJS_DIR}/quickjs.c
        ${QUICKJS_DIR}/libregexp.c
        ${QUICKJS_DIR}/libunicode.c
        ${QUICKJS_DIR}/cutils.c
        ${QUICKJS_DIR}/dtoa.c
    )

    add_library(quickjs STATIC ${QUICKJS_SRC})
    target_compile_definitions(quickjs PRIVATE CONFIG_VERSION="${CONFIG_VERSION}")

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(quickjs PRIVATE DUMP_LEAKS)
    endif()

    if(BAREMETALPI OR NINTENDO_3DS)
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
    else()
        target_compile_definitions(js INTERFACE TIC_BUILD_WITH_JS=1)
    endif()

    target_link_libraries(js PRIVATE runtime)

    target_link_libraries(js PRIVATE quickjs)
    target_include_directories(js
        PRIVATE
            ${QUICKJS_DIR}
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )


endif()