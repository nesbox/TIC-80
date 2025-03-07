################################
# Janet
################################

option(BUILD_WITH_JANET "Janet Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_JANET: ${BUILD_WITH_JANET}")

if(BUILD_WITH_JANET AND PREFER_SYSTEM_LIBRARIES)
    find_path(janet_INCLUDE_DIR NAMES janet.h)
    find_library(janet_LIBRARY NAMES janet)
    if(janet_INCLUDE_DIR AND janet_LIBRARY)
        add_library(janet STATIC
            ${CMAKE_SOURCE_DIR}/src/api/janet.c
            ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
        )
        target_compile_definitions(janet INTERFACE TIC_BUILD_WITH_JANET)
        target_link_libraries(janet PRIVATE runtime ${janet_LIBRARY})
        target_include_directories(janet
            PUBLIC ${janet_INCLUDE_DIR}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/src
        )
        message(STATUS "Use sytem library: janet")
        return()
    else()
        message(WARNING "System library janet not found")
    endif()
endif()

if(BUILD_WITH_JANET)

    if(MINGW)
        find_program(GIT git)
        get_filename_component(GIT_DIR ${GIT} DIRECTORY)
        find_program(GITBASH bash PATHS "${GIT_DIR}/../bin" NO_DEFAULT_PATH)

        if(NOT GITBASH)
            message(FATAL_ERROR "Git Bash not found!")
        endif()

        add_custom_command(
            OUTPUT ${THIRDPARTY_DIR}/janet/build/c/janet.c
            COMMAND ${GITBASH} -c "CC=gcc mingw32-make build/c/janet.c"
            WORKING_DIRECTORY ${THIRDPARTY_DIR}/janet/
        )
    elseif(WIN32)
        add_custom_command(
            OUTPUT ${THIRDPARTY_DIR}/janet/build/c/janet.c
            COMMAND ./build_win.bat
            WORKING_DIRECTORY ${THIRDPARTY_DIR}/janet/
        )
    else()
        add_custom_command(
            OUTPUT ${THIRDPARTY_DIR}/janet/build/c/janet.c
            COMMAND make build/c/janet.c
            WORKING_DIRECTORY ${THIRDPARTY_DIR}/janet/
        )
    endif()

    set(JANET_SRC
        ${THIRDPARTY_DIR}/janet/build/c/janet.c
        ${CMAKE_SOURCE_DIR}/src/api/janet.c
        ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
    )

    add_library(janet ${TIC_RUNTIME} ${JANET_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(janet PROPERTIES PREFIX "")
    else()
        target_compile_definitions(janet INTERFACE TIC_BUILD_WITH_JANET=1)
    endif()

    target_link_libraries(janet PRIVATE runtime)

    target_include_directories(janet
        PRIVATE
            ${THIRDPARTY_DIR}/janet/src/include
            ${CMAKE_SOURCE_DIR}/build/janet
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )


endif()