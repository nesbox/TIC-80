################################
# pocketpy (Python)
################################

add_subdirectory(${THIRDPARTY_DIR}/pocketpy)
target_compile_definitions(pocketpy PRIVATE PK_ENABLE_OS=0)
include_directories(${THIRDPARTY_DIR}/pocketpy/include)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(pocketpy PRIVATE -Wno-psabi)
endif()

# alias pocketpy to python for next steps
set_target_properties(pocketpy PROPERTIES OUTPUT_NAME python)
add_library(python ALIAS pocketpy)

if(EMSCRIPTEN)
    # exceptions must be enabled for emscripten
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fexceptions")
endif()