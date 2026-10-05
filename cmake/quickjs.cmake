################################
# QuickJS-ng
################################

option(BUILD_WITH_JS "JS Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_JS: ${BUILD_WITH_JS}")

if(BUILD_WITH_JS)

    if(PREFER_SYSTEM_LIBRARIES)
        find_path(quickjs_INCLUDE_DIR NAMES quickjs.h PATH_SUFFIXES quickjs-ng quickjs)
        find_library(quickjs_LIBRARY NAMES qjs quickjs-ng PATH_SUFFIXES quickjs-ng quickjs)
        if(quickjs_INCLUDE_DIR)
            file(STRINGS "${quickjs_INCLUDE_DIR}/quickjs.h" QUICKJS_NG_HEADER
                REGEX "^#define QUICKJS_NG[ \t]+1")
        endif()

        # The original QuickJS has a different C API and cannot be used here.
        if(QUICKJS_NG_HEADER AND quickjs_LIBRARY)
            add_library(quickjs UNKNOWN IMPORTED GLOBAL)
            set_target_properties(quickjs PROPERTIES
                IMPORTED_LOCATION "${quickjs_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${quickjs_INCLUDE_DIR}"
            )
            set(QUICKJS_DIR "${quickjs_INCLUDE_DIR}")
            message(STATUS "Use system library: quickjs-ng")
        else()
            message(WARNING "System library quickjs-ng not found; using bundled quickjs-ng")
        endif()
    endif()

    if(NOT TARGET quickjs)
        set(QUICKJS_DIR "${THIRDPARTY_DIR}/quickjs")

        set(QUICKJS_SRC
            ${QUICKJS_DIR}/quickjs.c
            ${QUICKJS_DIR}/libregexp.c
            ${QUICKJS_DIR}/libunicode.c
            ${QUICKJS_DIR}/dtoa.c
        )

        add_library(quickjs STATIC ${QUICKJS_SRC})
        # MSVC's default C mode cannot compile the C11 atomics used by QuickJS-ng.
        set_target_properties(quickjs PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
        target_compile_definitions(quickjs PRIVATE _GNU_SOURCE)
        target_include_directories(quickjs PUBLIC "${QUICKJS_DIR}")

        if(WIN32)
            target_compile_definitions(quickjs PRIVATE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
        endif()

        if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(quickjs PRIVATE -funsigned-char)
        endif()

        if(EMSCRIPTEN)
            target_compile_definitions(quickjs PRIVATE EMSCRIPTEN)
        endif()

        if(MSVC)
            include(CheckCCompilerFlag)
            check_c_compiler_flag(/experimental:c11atomics QUICKJS_HAS_C11_ATOMICS)
            if(QUICKJS_HAS_C11_ATOMICS)
                target_compile_options(quickjs PRIVATE /experimental:c11atomics)
            endif()
        endif()
    endif()

    get_target_property(QUICKJS_IMPORTED quickjs IMPORTED)
    if(QUICKJS_IMPORTED)
        set(QUICKJS_LINK_SCOPE INTERFACE)
    else()
        set(QUICKJS_LINK_SCOPE PUBLIC)
    endif()

    if(NOT WIN32)
        target_link_libraries(quickjs ${QUICKJS_LINK_SCOPE} m)
        # These console and bare-metal C libraries do not provide libdl.
        if(CMAKE_DL_LIBS AND NOT NINTENDO_SWITCH AND NOT NINTENDO_3DS AND NOT BAREMETALPI)
            target_link_libraries(quickjs ${QUICKJS_LINK_SCOPE} ${CMAKE_DL_LIBS})
        endif()
    endif()

    if(NOT EMSCRIPTEN)
        find_package(Threads)
        if(Threads_FOUND)
            target_link_libraries(quickjs ${QUICKJS_LINK_SCOPE} Threads::Threads)
        endif()
    endif()

    set(JS_SRC
        ${CMAKE_SOURCE_DIR}/src/api/js.c
        ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
    )

    add_library(js ${TIC_RUNTIME} ${JS_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(js PROPERTIES PREFIX "")
    else()
        target_compile_definitions(js INTERFACE TIC_BUILD_WITH_JS=1)
    endif()

    target_link_libraries(js PRIVATE runtime quickjs)
    target_include_directories(js
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )
endif()
