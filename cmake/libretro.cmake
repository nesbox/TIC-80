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
        set_target_properties(tic80_libretro PROPERTIES SUFFIX "_partial.a")
        include(CheckCSourceCompiles)
        check_c_source_compiles(
"#ifndef __MSDOS__
#error \"Not DOS\"
#endif"
IS_DOS
)

        # Exact way to detect NGC/Wii depends on version of cmake files
        if("${CMAKE_SYSTEM_NAME}" STREQUAL "NintendoWii" OR "${CMAKE_SYSTEM_NAME}" STREQUAL "NintendoGameCube" OR GAMECUBE OR WII OR IS_DOS)
          add_custom_command(TARGET tic80_libretro
               POST_BUILD
                   COMMAND ${CMAKE_SOURCE_DIR}/build/libretro/merge_static.sh $(AR) ${CMAKE_BINARY_DIR}/lib/tic80_libretro${LIBRETRO_SUFFIX}.${LIBRETRO_EXTENSION} ${CMAKE_BINARY_DIR}/lib/tic80_libretro_partial.a ${CMAKE_BINARY_DIR}/lib/libtic80core.a ${CMAKE_BINARY_DIR}/lib/liblua.a ${CMAKE_BINARY_DIR}/lib/libblipbuf.a ${CMAKE_BINARY_DIR}/lib/libquickjs.a ${CMAKE_BINARY_DIR}/lib/libwren.a ${CMAKE_BINARY_DIR}/lib/libwasm.a ${CMAKE_BINARY_DIR}/lib/libjanet.a ${CMAKE_BINARY_DIR}/lib/libsquirrel.a ${CMAKE_BINARY_DIR}/lib/libscheme.a ${CMAKE_BINARY_DIR}/lib/libgiflib.a ${CMAKE_BINARY_DIR}/lib/liblpeg.a ${CMAKE_BINARY_DIR}/lib/libzlib.a ${MRUBY_LIB})
        else()
          add_custom_command(TARGET tic80_libretro
               POST_BUILD
                   COMMAND ${CMAKE_SOURCE_DIR}/build/libretro/merge_static.sh $(AR) ${CMAKE_BINARY_DIR}/lib/tic80_libretro${LIBRETRO_SUFFIX}.${LIBRETRO_EXTENSION} ${CMAKE_BINARY_DIR}/lib/tic80_libretro_partial.a ${CMAKE_BINARY_DIR}/lib/libtic80core.a ${CMAKE_BINARY_DIR}/lib/liblua.a ${CMAKE_BINARY_DIR}/lib/libblipbuf.a ${CMAKE_BINARY_DIR}/lib/libquickjs.a ${CMAKE_BINARY_DIR}/lib/libwren.a ${CMAKE_BINARY_DIR}/lib/libwasm.a ${CMAKE_BINARY_DIR}/lib/libsquirrel.a ${CMAKE_BINARY_DIR}/lib/libscheme.a ${CMAKE_BINARY_DIR}/lib/libjanet.a ${CMAKE_BINARY_DIR}/lib/libgiflib.a ${CMAKE_BINARY_DIR}/lib/liblpeg.a ${MRUBY_LIB})
        endif()
    else()
        add_library(tic80_libretro SHARED
        ${LIBRETRO_SRC}
        )
    endif()

    target_include_directories(tic80_libretro PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${TIC80CORE_DIR})

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

    target_link_libraries(tic80_libretro tic80core)
    set_target_properties(tic80_libretro PROPERTIES PREFIX "")
endif()