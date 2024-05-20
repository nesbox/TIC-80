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

        # Build a list of language libraries to link against.
        if(BUILD_WITH_FENNEL)
            set(LIBRETRO_FENNEL_LIB ${CMAKE_BINARY_DIR}/lib/libfennel.a)
        endif()
        if(BUILD_WITH_JANET)
            set(LIBRETRO_JANET_LIB ${CMAKE_BINARY_DIR}/lib/libjanet.a)
        endif()
        if(BUILD_WITH_LUA)
            set(LIBRETRO_LUA_LIB ${CMAKE_BINARY_DIR}/lib/liblua.a)
        endif()
        if(BUILD_WITH_MOON)
            set(LIBRETRO_MOON_LIB ${CMAKE_BINARY_DIR}/lib/libmoon.a)
        endif()
        if(BUILD_WITH_MRUBY)
            set(LIBRETRO_MRUBY_LIB ${CMAKE_BINARY_DIR}/lib/libmruby.a)
        endif()
        if(BUILD_WITH_JS)
            set(LIBRETRO_JS_LIB ${CMAKE_BINARY_DIR}/lib/libquickjs.a)
        endif()
        if(BUILD_WITH_SCHEME)
            set(LIBRETRO_SCHEME_LIB ${CMAKE_BINARY_DIR}/lib/libscheme.a)
        endif()
        if(BUILD_WITH_SQUIRREL)
            set(LIBRETRO_SQUIRREL_LIB ${CMAKE_BINARY_DIR}/lib/libsquirrel.a)
        endif()
        if(BUILD_WITH_WASM)
            set(LIBRETRO_WASM_LIB ${CMAKE_BINARY_DIR}/lib/libwasm.a)
        endif()
        if(BUILD_WITH_WREN)
            set(LIBRETRO_WREN_LIB ${CMAKE_BINARY_DIR}/lib/libwren.a)
        endif()
        set(LIBRETRO_LANG_LIBS ${LIBRETRO_FENNEL_LIB} ${LIBRETRO_JANET_LIB} ${LIBRETRO_LUA_LIB} ${LIBRETRO_MOON_LIB} ${LIBRETRO_MRUBY_LIB} ${LIBRETRO_JS_LIB} ${LIBRETRO_SCHEME_LIB} ${LIBRETRO_SQUIRREL_LIB} ${LIBRETRO_WASM_LIB} ${LIBRETRO_WREN_LIB})

        # Exact way to detect NGC/Wii depends on version of cmake files
        if("${CMAKE_SYSTEM_NAME}" STREQUAL "NintendoWii" OR "${CMAKE_SYSTEM_NAME}" STREQUAL "NintendoGameCube" OR GAMECUBE OR WII OR IS_DOS)
          add_custom_command(TARGET tic80_libretro
               POST_BUILD
                   COMMAND ${CMAKE_SOURCE_DIR}/build/libretro/merge_static.sh $(AR) ${CMAKE_BINARY_DIR}/lib/tic80_libretro${LIBRETRO_SUFFIX}.${LIBRETRO_EXTENSION} ${CMAKE_BINARY_DIR}/lib/tic80_libretro_partial.a ${CMAKE_BINARY_DIR}/lib/libtic80core.a ${CMAKE_BINARY_DIR}/lib/libblipbuf.a ${CMAKE_BINARY_DIR}/lib/libgiflib.a ${CMAKE_BINARY_DIR}/lib/liblpeg.a ${LIBRETRO_LANG_LIBS} ${CMAKE_BINARY_DIR}/lib/libzlib.a)
        else()
          add_custom_command(TARGET tic80_libretro
               POST_BUILD
                   COMMAND ${CMAKE_SOURCE_DIR}/build/libretro/merge_static.sh $(AR) ${CMAKE_BINARY_DIR}/lib/tic80_libretro${LIBRETRO_SUFFIX}.${LIBRETRO_EXTENSION} ${CMAKE_BINARY_DIR}/lib/tic80_libretro_partial.a ${CMAKE_BINARY_DIR}/lib/libtic80core.a ${CMAKE_BINARY_DIR}/lib/libblipbuf.a ${CMAKE_BINARY_DIR}/lib/libgiflib.a ${CMAKE_BINARY_DIR}/lib/liblpeg.a ${LIBRETRO_LANG_LIBS})
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

    target_compile_definitions(tic80_libretro PRIVATE
        __LIBRETRO__=TRUE
    )
    target_link_libraries(tic80_libretro tic80core)
    set_target_properties(tic80_libretro PROPERTIES PREFIX "")
endif()