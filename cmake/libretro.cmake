################################
# libretro renderer example
################################

if(BUILD_LIBRETRO)
    set(LIBRETRO_DIR ${TIC80CORE_DIR}/system/libretro)
    set(LIBRETRO_SRC
        ${LIBRETRO_DIR}/tic80_libretro.c
    )

    if (LIBRETRO_STATIC)
        # Collect sources from all dependencies to build a monolithic library.
        # This ensures that consoles and Emscripten get a single, complete archive.
        set(TIC80_MONOLITHIC_SRCS ${LIBRETRO_SRC})
        
        # List of all potential core component targets to collect sources from
        set(TIC80_CORE_OBJECT_DEPS 
            tic80core luaapi lua lpeg moon yue fennel wren squirrel pocketpy 
            quickjs mruby wasm scheme janet zip zlib png blipbuf giflib 
            wave argparse naett
        )

        set(TIC80_MONOLITHIC_INCLUDES "")
        set(TIC80_MONOLITHIC_DEFINES "")

        foreach(target ${TIC80_CORE_OBJECT_DEPS})
            if(TARGET ${target})
                # Collect sources
                get_target_property(target_sources ${target} SOURCES)
                get_target_property(target_dir ${target} SOURCE_DIR)
                if(target_sources)
                    foreach(src ${target_sources})
                        if(NOT IS_ABSOLUTE "${src}")
                            set(src "${target_dir}/${src}")
                        endif()
                        if(EXISTS "${src}")
                            list(APPEND TIC80_MONOLITHIC_SRCS "${src}")
                        endif()
                    endforeach()
                endif()

                # Collect ALL include directories (Private and Interface)
                get_target_property(includes ${target} INCLUDE_DIRECTORIES)
                if(includes)
                    list(APPEND TIC80_MONOLITHIC_INCLUDES ${includes})
                endif()
                get_target_property(iface_includes ${target} INTERFACE_INCLUDE_DIRECTORIES)
                if(iface_includes)
                    list(APPEND TIC80_MONOLITHIC_INCLUDES ${iface_includes})
                endif()

                # Collect ALL compile definitions (Private and Interface and Directory)
                get_target_property(defines ${target} COMPILE_DEFINITIONS)
                if(defines)
                    list(APPEND TIC80_MONOLITHIC_DEFINES ${defines})
                endif()
                get_target_property(iface_defines ${target} INTERFACE_COMPILE_DEFINITIONS)
                if(iface_defines)
                    list(APPEND TIC80_MONOLITHIC_DEFINES ${iface_defines})
                endif()
                get_directory_property(dir_defines DIRECTORY ${target_dir} COMPILE_DEFINITIONS)
                if(dir_defines)
                    list(APPEND TIC80_MONOLITHIC_DEFINES ${dir_defines})
                endif()
            endif()
        endforeach()

        # Remove duplicates from includes/defines to keep command lines manageable
        if(TIC80_MONOLITHIC_INCLUDES)
            list(REMOVE_DUPLICATES TIC80_MONOLITHIC_INCLUDES)
        endif()
        if(TIC80_MONOLITHIC_DEFINES)
            list(REMOVE_DUPLICATES TIC80_MONOLITHIC_DEFINES)
        endif()

        add_library(tic80_libretro STATIC
            ${TIC80_MONOLITHIC_SRCS}
        )

        target_include_directories(tic80_libretro PRIVATE ${TIC80_MONOLITHIC_INCLUDES})
        target_compile_definitions(tic80_libretro PRIVATE ${TIC80_MONOLITHIC_DEFINES})

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
    if(NOT LIBRETRO_STATIC)
        target_link_libraries(tic80_libretro tic80core)
    endif()
    set_target_properties(tic80_libretro PROPERTIES PREFIX "")
endif()
