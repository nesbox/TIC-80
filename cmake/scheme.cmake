################################
# SCHEME (S7)
################################

set(BUILD_WITH_SCHEME_DEFAULT TRUE)

option(BUILD_WITH_SCHEME "Scheme Enabled" ${BUILD_WITH_SCHEME_DEFAULT})
message("BUILD_WITH_SCHEME: ${BUILD_WITH_SCHEME}")

if(BUILD_WITH_SCHEME)

    set(SCHEME_DIR ${THIRDPARTY_DIR}/s7)
    set(SCHEME_SRC ${SCHEME_DIR}/s7.c)

    list(APPEND SCHEME_SRC ${CMAKE_SOURCE_DIR}/src/api/scheme.c)

    if(BUILD_STATIC)
        add_library(scheme STATIC ${SCHEME_SRC})
        target_compile_definitions(scheme PUBLIC TIC_BUILD_STATIC)
        target_link_libraries(scheme PRIVATE tic80core)
    else()
        add_library(scheme SHARED ${SCHEME_SRC})
        set_target_properties(scheme PROPERTIES PREFIX "")
        if(MINGW)
            target_link_options(scheme PRIVATE -static)
        endif()
    endif()

    set_target_properties(scheme PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(scheme 
        PUBLIC ${SCHEME_DIR}
        PRIVATE 
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    if (N3DS)
        target_compile_definitions(scheme PRIVATE S7_N3DS)
    endif()

    if (BAREMETALPI)
        target_compile_definitions(scheme PRIVATE S7_BAREMETALPI)
    endif()

    target_compile_definitions(scheme INTERFACE TIC_BUILD_WITH_SCHEME=1)

    if(BUILD_DEMO_CARTS)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/schemedemo.scm)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/schememark.scm)
    endif()

endif()