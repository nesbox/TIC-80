################################
# SCHEME (S7)
################################

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