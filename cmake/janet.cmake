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

    set(JANET_SRC 
        ${THIRDPARTY_DIR}/janet/build/c/janet.c
        ${CMAKE_SOURCE_DIR}/src/api/janet.c 
        ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
    )

    if(BUILD_STATIC)
        add_library(janet STATIC ${JANET_SRC})
        target_compile_definitions(janet PUBLIC TIC_BUILD_STATIC)
        target_link_libraries(janet PRIVATE tic80core)
    else()
        add_library(janet SHARED ${JANET_SRC})
        set_target_properties(janet PROPERTIES PREFIX "")
        if(MINGW)
            target_link_options(janet PRIVATE -static)
        endif()
    endif()

    target_include_directories(janet 
        PUBLIC 
            ${THIRDPARTY_DIR}/janet/src/include
            ${CMAKE_SOURCE_DIR}/build/janet
        PRIVATE 
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    target_compile_definitions(janet INTERFACE TIC_BUILD_WITH_JANET=1)

    if(BUILD_DEMO_CARTS)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/janetdemo.janet)
        list(APPEND DEMO_CARTS ${DEMO_CARTS_IN}/bunny/janetmark.janet)
    endif()

endif()