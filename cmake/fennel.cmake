################################
# Fennel
################################

option(BUILD_WITH_FENNEL "Fennel Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_FENNEL: ${BUILD_WITH_FENNEL}")

if(BUILD_WITH_FENNEL)

    add_library(fennel ${TIC_RUNTIME} ${CMAKE_SOURCE_DIR}/src/api/fennel.c)

    if(NOT BUILD_STATIC)
        set_target_properties(fennel PROPERTIES PREFIX "")
    endif()

    target_link_libraries(fennel PRIVATE runtime luaapi)
    target_include_directories(fennel 
        PRIVATE 
        ${THIRDPARTY_DIR}/fennel
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_compile_definitions(fennel INTERFACE TIC_BUILD_WITH_FENNEL)

endif()