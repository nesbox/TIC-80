################################
# SCHEME (S7)
################################

set(BUILD_WITH_SCHEME_DEFAULT TRUE)

option(BUILD_WITH_SCHEME "Scheme Enabled" ${BUILD_WITH_SCHEME_DEFAULT})
message("BUILD_WITH_SCHEME: ${BUILD_WITH_SCHEME}")

if(BUILD_WITH_SCHEME)

    set(SCHEME_DIR ${THIRDPARTY_DIR}/s7)
    set(SCHEME_SRC
        ${SCHEME_DIR}/s7.c
    )

    add_library(scheme STATIC ${SCHEME_SRC})
    set_target_properties(scheme PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(scheme PUBLIC ${SCHEME_DIR})

    if (N3DS)
        target_compile_definitions(scheme PRIVATE S7_N3DS)
    endif()

    if (BAREMETALPI)
        target_compile_definitions(scheme PRIVATE S7_BAREMETALPI)
    endif()

    target_compile_definitions(scheme INTERFACE TIC_BUILD_WITH_SCHEME=1)

endif()