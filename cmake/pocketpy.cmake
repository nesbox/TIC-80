################################
# pocketpy (Python)
################################

option(BUILD_WITH_PYTHON "Python Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_PYTHON: ${BUILD_WITH_PYTHON}")

if(BUILD_WITH_PYTHON)

    add_subdirectory(${THIRDPARTY_DIR}/pocketpy)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(pocketpy PRIVATE -Wno-psabi)
    endif()

    set(PYTHON_SRC 
        ${CMAKE_SOURCE_DIR}/src/api/python.c
        ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
    )

    add_library(python ${TIC_RUNTIME} ${PYTHON_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(python PROPERTIES PREFIX "")
    else()
        target_compile_definitions(python INTERFACE TIC_BUILD_WITH_PYTHON=1)
    endif()

    target_link_libraries(python PRIVATE runtime)

    target_include_directories(python 
        PRIVATE 
            ${THIRDPARTY_DIR}/pocketpy/include
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(python PRIVATE pocketpy)

    if(EMSCRIPTEN)
        # exceptions must be enabled for emscripten
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fexceptions")
    endif()


endif()
