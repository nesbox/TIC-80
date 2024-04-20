################################
# pocketpy (Python)
################################

set(BUILD_WITH_PYTHON_DEFAULT TRUE)

option(BUILD_WITH_PYTHON "Python Enabled" ${BUILD_WITH_PYTHON_DEFAULT})
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

    if(BUILD_STATIC)
        add_library(python STATIC ${PYTHON_SRC})
        target_compile_definitions(python PUBLIC TIC_BUILD_STATIC)
    else()
        add_library(python SHARED ${PYTHON_SRC})
        set_target_properties(python PROPERTIES PREFIX "")
        if(MINGW)
            target_link_options(python PRIVATE -static)
        endif()
    endif()

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

    target_compile_definitions(python INTERFACE TIC_BUILD_WITH_PYTHON=1)

    if(BUILD_DEMO_CARTS)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/pythondemo.py)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/pythonmark.py)
    endif()

endif()
