################################
# MoonScript
################################

option(BUILD_WITH_MOON "Moon Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_MOON: ${BUILD_WITH_MOON}")

if(BUILD_WITH_MOON)

    set(LPEG_DIR ${THIRDPARTY_DIR}/lpeg)
    set(LPEG_SRC
        ${LPEG_DIR}/lpcap.c
        ${LPEG_DIR}/lpcode.c
        ${LPEG_DIR}/lpprint.c
        ${LPEG_DIR}/lptree.c
        ${LPEG_DIR}/lpvm.c
    )

    add_library(lpeg STATIC ${LPEG_SRC})
    target_include_directories(lpeg PRIVATE ${LUA_DIR})

    add_library(moon ${TIC_RUNTIME} ${CMAKE_SOURCE_DIR}/src/api/moonscript.c)

    if(NOT BUILD_STATIC)
        set_target_properties(moon PROPERTIES PREFIX "")
    endif()

    target_link_libraries(moon PRIVATE lpeg runtime luaapi)

    target_include_directories(moon
            PRIVATE 
                ${THIRDPARTY_DIR}/moonscript
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_definitions(moon INTERFACE TIC_BUILD_WITH_MOON)

endif()
