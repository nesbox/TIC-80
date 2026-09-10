################################
# MiniScript
################################

option(BUILD_WITH_MINISCRIPT "MiniScript Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_MINISCRIPT: ${BUILD_WITH_MINISCRIPT}")

if(BUILD_WITH_MINISCRIPT)
    set(MS2_DIR ${THIRDPARTY_DIR}/miniscript2)
    file(GLOB MS2_CORE_SOURCES CONFIGURE_DEPENDS
        ${MS2_DIR}/cpp/core/*.cpp
        ${MS2_DIR}/cpp/core/*.c
    )
    list(FILTER MS2_CORE_SOURCES EXCLUDE REGEX "core/test_.*")
    list(FILTER MS2_CORE_SOURCES EXCLUDE REGEX "core/debug_.*")
    list(FILTER MS2_CORE_SOURCES EXCLUDE REGEX "core/keyboard\\.cpp$")
    list(APPEND MS2_CORE_SOURCES "${THIRDPARTY_DIR}/miniscript2_extras/keyboard.cpp")

    file(GLOB MS2_GEN_SOURCES CONFIGURE_DEPENDS
        ${MS2_DIR}/generated/*.g.cpp
    )
    # App.g.cpp is MS2's standalone command-line app entry point (defines main())
    list(FILTER MS2_GEN_SOURCES EXCLUDE REGEX "generated/App\\.g\\.cpp$")
    # we also don't need ShellIntrinsics (and on some platforms it can cause build issues)
    list(FILTER MS2_GEN_SOURCES EXCLUDE REGEX "generated/ShellIntrinsics\\.g\\.cpp$")
    # these ones just kinda trip up the build
    list(FILTER MS2_GEN_SOURCES EXCLUDE REGEX "generated/VMVis\\.g\\.cpp$")
    list(FILTER MS2_GEN_SOURCES EXCLUDE REGEX "generated/UnitTests\\.g\\.cpp$")
    list(FILTER MS2_GEN_SOURCES EXCLUDE REGEX "generated/Assembler\\.g\\.cpp$")

    # MiniScript 2 compiles as C++14, not C++20
    # but src/api/miniscript.cpp has to compile as C++20
    add_library(libminiscript OBJECT ${MS2_CORE_SOURCES} ${MS2_GEN_SOURCES})
    set_target_properties(libminiscript PROPERTIES
        LINKER_LANGUAGE CXX
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON
    )
    if(NINTENDO_3DS)
        target_compile_options(libminiscript PRIVATE -ftls-model=initial-exec)
    endif()

    add_library(miniscript ${TIC_RUNTIME} src/api/miniscript.cpp src/api/parse_note.c)
    target_link_libraries(miniscript PRIVATE libminiscript)

    if(NOT BUILD_STATIC)
        set_target_properties(miniscript PROPERTIES PREFIX "")
    else()
        target_compile_definitions(miniscript INTERFACE TIC_BUILD_WITH_MINISCRIPT)
    endif()

    set_target_properties(miniscript PROPERTIES
        LINKER_LANGUAGE CXX
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    target_link_libraries(miniscript PRIVATE runtime)

    target_include_directories(libminiscript
        PRIVATE
            ${MS2_DIR}/cpp
            ${MS2_DIR}/cpp/core
            ${MS2_DIR}/generated
    )
    target_include_directories(miniscript
        PRIVATE
            ${MS2_DIR}/cpp
            ${MS2_DIR}/cpp/core
            ${MS2_DIR}/generated
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_BINARY_DIR}
    )

endif()
