################################
# SCHEME (S7)
################################

option(BUILD_WITH_SCHEME "Scheme Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_SCHEME: ${BUILD_WITH_SCHEME}")

if(BUILD_WITH_SCHEME)

    set(SCHEME_DIR ${THIRDPARTY_DIR}/s7)
    set(SCHEME_SRC ${SCHEME_DIR}/s7.c)

    list(APPEND SCHEME_SRC ${CMAKE_SOURCE_DIR}/src/api/scheme.c)

    add_library(scheme ${TIC_RUNTIME} ${SCHEME_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(scheme PROPERTIES PREFIX "")
    else()
        target_compile_definitions(scheme INTERFACE TIC_BUILD_WITH_SCHEME=1)
    endif()

    target_link_libraries(scheme PRIVATE runtime)

    set_target_properties(scheme PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(scheme
        PUBLIC ${SCHEME_DIR}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    if (NINTENDO_3DS)
        target_compile_definitions(scheme PRIVATE S7_N3DS)
    endif()

    if (BAREMETALPI)
        target_compile_definitions(scheme PRIVATE S7_BAREMETALPI)
    endif()


endif()