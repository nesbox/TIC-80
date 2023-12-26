################################
# Janet
################################

set(BUILD_WITH_JANET_DEFAULT TRUE)

option(BUILD_WITH_JANET "Janet Enabled" ${BUILD_WITH_JANET_DEFAULT})
message("BUILD_WITH_JANET: ${BUILD_WITH_JANET}")

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

    add_library(janet ${THIRDPARTY_DIR}/janet/build/c/janet.c)
    target_include_directories(janet PUBLIC ${THIRDPARTY_DIR}/janet/src/include)
    target_include_directories(janet PUBLIC ${CMAKE_SOURCE_DIR}/build/janet/)
    target_compile_definitions(janet INTERFACE TIC_BUILD_WITH_JANET=1)

endif()