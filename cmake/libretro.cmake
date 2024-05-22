################################
# libretro renderer example
################################

if(BUILD_LIBRETRO)
    set(LIBRETRO_DIR ${TIC80CORE_DIR}/system/libretro)
    set(LIBRETRO_SRC
        ${LIBRETRO_DIR}/tic80_libretro.c
    )

    if (LIBRETRO_STATIC)
        add_library(tic80_libretro STATIC
            ${LIBRETRO_SRC}
        )
        if(EMSCRIPTEN)
            set(LIBRETRO_EXTENSION "bc")
        else()
            set(LIBRETRO_EXTENSION "a")
        endif()

        set_target_properties(tic80_libretro PROPERTIES SUFFIX "${LIBRETRO_SUFFIX}.${LIBRETRO_EXTENSION}")
    else()
        add_library(tic80_libretro SHARED
            ${LIBRETRO_SRC}
        )
    endif()

    target_include_directories(tic80_libretro PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${TIC80CORE_DIR}
    )

    if(MINGW)
        target_link_libraries(tic80_libretro mingw32)
    endif()

    if(ANDROID)
        set_target_properties(tic80_libretro PROPERTIES SUFFIX "_android.so")
    endif()

    # MSYS2 builds libretro to ./bin, despite it being a DLL. This forces it to ./lib.
    set_target_properties(tic80_libretro PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    )

    target_compile_definitions(tic80_libretro PRIVATE
        __LIBRETRO__=TRUE
    )
    target_link_libraries(tic80_libretro tic80core)
    set_target_properties(tic80_libretro PROPERTIES PREFIX "")
endif()