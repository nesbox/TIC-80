################################
# GRAVITY
################################

option(BUILD_WITH_GRAVITY "Gravity Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_GRAVITY: ${BUILD_WITH_GRAVITY}")

if(BUILD_WITH_GRAVITY)

    # set some directories
    set(GRAVITY_MODULE_ROOT ${THIRDPARTY_DIR}/gravity)
    set(GRAVITY_MODULE_SRC ${GRAVITY_MODULE_ROOT}/src)
    set(GRAVITY_BUILD_ROOT ${CMAKE_CURRENT_BINARY_DIR}/vendor/gravity)
    set(GRAVITY_BUILD_SRC ${GRAVITY_BUILD_ROOT}/src)
    set(PATCH_DIR ${CMAKE_SOURCE_DIR}/patches)
    set(GRAVITY_PATCH ${PATCH_DIR}/gravity-patch.patch)

    # clean existing dir
    file(REMOVE_RECURSE "${GRAVITY_BUILD_SRC}")

    # copy all required source into the build dir
    file(GLOB_RECURSE GRAVITY_FILES
            RELATIVE ${GRAVITY_MODULE_SRC}
            ${GRAVITY_MODULE_SRC}/*
    )
    foreach(rel IN LISTS GRAVITY_FILES)
        if(NOT IS_DIRECTORY "${GRAVITY_MODULE_SRC}/${rel}")
            get_filename_component(rel_dir "${rel}" DIRECTORY)

            file(MAKE_DIRECTORY "${GRAVITY_BUILD_SRC}/${rel_dir}")

            configure_file(
                    "${GRAVITY_MODULE_SRC}/${rel}"
                    "${GRAVITY_BUILD_SRC}/${rel}"
                    COPYONLY
            )
        endif()
    endforeach()

    # find the repository root
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --show-toplevel
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_ROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
    )

    # make the generated source path relative to the repository root.
    file(RELATIVE_PATH GRAVITY_RELATIVE_DEST
            "${GIT_ROOT}"
            "${GRAVITY_BUILD_ROOT}"
    )

    # apply the patch
    execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply
            --verbose
            --ignore-whitespace
            --directory=${GRAVITY_RELATIVE_DEST}
            "${GRAVITY_PATCH}"
            WORKING_DIRECTORY "${GIT_ROOT}"
            RESULT_VARIABLE PATCH_RESULT
            OUTPUT_VARIABLE PATCH_OUTPUT
            ERROR_VARIABLE PATCH_ERROR
    )

    # fatal error if the patch didn't apply
    if(NOT PATCH_RESULT EQUAL 0)
        message(STATUS "${PATCH_RESULT}")
        message(STATUS "${PATCH_OUTPUT}")
        message(STATUS "${PATCH_ERROR}")
        message(FATAL_ERROR "Gravity patch failed")
    endif()

    # make a list of files to compile
    set(GRAVITY_SRC
            ${GRAVITY_BUILD_SRC}/compiler/gravity_ast.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_codegen.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_compiler.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_ircode.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_lexer.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_optimizer.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_parser.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_semacheck1.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_semacheck2.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_symboltable.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_token.c
            ${GRAVITY_BUILD_SRC}/compiler/gravity_visitor.c
            ${GRAVITY_BUILD_SRC}/optionals/gravity_opt_json.c
            ${GRAVITY_BUILD_SRC}/optionals/gravity_opt_math.c
            ${GRAVITY_BUILD_SRC}/runtime/gravity_core.c
            ${GRAVITY_BUILD_SRC}/runtime/gravity_vm.c
            ${GRAVITY_BUILD_SRC}/shared/gravity_hash.c
            ${GRAVITY_BUILD_SRC}/shared/gravity_memory.c
            ${GRAVITY_BUILD_SRC}/shared/gravity_value.c
            ${GRAVITY_BUILD_SRC}/utils/gravity_debug.c
            ${GRAVITY_BUILD_SRC}/utils/gravity_json.c
            ${GRAVITY_BUILD_SRC}/utils/gravity_utils.c
    )

    # add our own TIC-80 bindings
    list(APPEND GRAVITY_SRC ${CMAKE_SOURCE_DIR}/src/api/gravity.c)
    list(APPEND GRAVITY_SRC ${CMAKE_SOURCE_DIR}/src/api/parse_note.c)

    add_library(gravity ${TIC_RUNTIME} ${GRAVITY_SRC})
    target_compile_definitions(gravity PRIVATE TIC_BUILD_WITH_GRAVITY=1)
    
    if(NOT BUILD_STATIC)
        set_target_properties(gravity PROPERTIES PREFIX "")
    else()
        target_compile_definitions(gravity INTERFACE TIC_BUILD_WITH_GRAVITY=1)
    endif()

    target_link_libraries(gravity PRIVATE runtime)

    if(WIN32)
        target_link_libraries(gravity PRIVATE shlwapi)
    endif()

    set_target_properties(gravity PROPERTIES LINKER_LANGUAGE CXX)

    target_include_directories(gravity
        PUBLIC
            ${GRAVITY_BUILD_SRC}/compiler
            ${GRAVITY_BUILD_SRC}/optionals
            ${GRAVITY_BUILD_SRC}/runtime
            ${GRAVITY_BUILD_SRC}/shared
            ${GRAVITY_BUILD_SRC}/utils
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )


endif()
