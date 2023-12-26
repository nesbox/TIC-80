################################
# QuickJS
################################

set(QUICKJS_DIR ${THIRDPARTY_DIR}/quickjs)

file(STRINGS ${QUICKJS_DIR}/VERSION CONFIG_VERSION)

add_library(quickjs STATIC
    ${QUICKJS_DIR}/quickjs.c 
    ${QUICKJS_DIR}/libregexp.c 
    ${QUICKJS_DIR}/libunicode.c
    ${QUICKJS_DIR}/cutils.c)

target_compile_definitions(quickjs PUBLIC CONFIG_VERSION="${CONFIG_VERSION}")
target_include_directories(quickjs INTERFACE ${QUICKJS_DIR})

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