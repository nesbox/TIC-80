################################
# pocketpy (Python)
################################

set(BUILD_WITH_PYTHON_DEFAULT TRUE)

option(BUILD_WITH_PYTHON "Python Enabled" ${BUILD_WITH_PYTHON_DEFAULT})
message("BUILD_WITH_PYTHON: ${BUILD_WITH_PYTHON}")

if(BUILD_WITH_PYTHON)

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

    target_compile_definitions(pocketpy INTERFACE TIC_BUILD_WITH_PYTHON=1)
endif()
