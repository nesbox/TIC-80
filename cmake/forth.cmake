################################
# Forth (pForth)
################################

option(BUILD_WITH_FORTH "Forth Enabled" ${BUILD_WITH_ALL})
message("BUILD_WITH_FORTH: ${BUILD_WITH_FORTH}")

if(BUILD_WITH_FORTH)

    set(PFORTH_DIR ${THIRDPARTY_DIR}/pforth/csrc)
    set(PFORTH_FTH_DIR ${THIRDPARTY_DIR}/pforth/fth)

    # -------------------------------------------------------------------------
    # pforth C sources — all files from sources.cmake, excluding the default
    # pfcustom.c (our forth.c serves as the replacement).
    # -------------------------------------------------------------------------
    set(PFORTH_KERNEL_SOURCES
        ${PFORTH_DIR}/pf_cglue.c
        ${PFORTH_DIR}/pf_clib.c
        ${PFORTH_DIR}/pf_core.c
        ${PFORTH_DIR}/pf_inner.c
        ${PFORTH_DIR}/pf_io.c
        ${PFORTH_DIR}/pf_io_none.c
        ${PFORTH_DIR}/pf_mem.c
        ${PFORTH_DIR}/pf_save.c
        ${PFORTH_DIR}/pf_text.c
        ${PFORTH_DIR}/pf_words.c
        ${PFORTH_DIR}/pfcompil.c
        ${PFORTH_DIR}/paging/pagedmem.c
        ${PFORTH_DIR}/paging/lockpage.c
        ${PFORTH_DIR}/paging/qadmpage.c
    )

    # -------------------------------------------------------------------------
    # pfdicdat.h — pforth standard dictionary, bootstrapped at configure time.
    #
    # vendor/pforth/csrc/pfdicdat.h is gitignored; it is generated here by
    # building pforth natively on the host.  This guarantees the dictionary
    # stays in sync with the pinned pforth submodule automatically — no
    # manual copy step needed.
    #
    # For WASM/Emscripten the dictionary must reflect a 32-bit cell size, so
    # pforth is built with -m32 (requires gcc-multilib on Linux hosts).
    #
    # To force regeneration after a pforth submodule update:
    #   rm vendor/pforth/csrc/pfdicdat.h  &&  cmake <build-dir>
    # -------------------------------------------------------------------------
    set(PFORTH_DICDAT ${PFORTH_DIR}/pfdicdat.h)

    # Always regenerate: a stale 32-bit pfdicdat.h in a 64-bit build (or vice
    # versa) causes a silent segfault at Forth VM startup.  Regeneration runs
    # at cmake configure time (not every build) so the cost is acceptable.
    file(REMOVE ${PFORTH_DICDAT})
    if(NOT EXISTS ${PFORTH_DICDAT})
        message(STATUS "Forth: bootstrapping pforth to generate pfdicdat.h...")
        set(_PFORTH_BOOTSTRAP_DIR "${CMAKE_BINARY_DIR}/pforth_bootstrap")

        if(CMAKE_CROSSCOMPILING)
            # Cross-compilation: CC may be set to the cross-compiler
            # (emcc, arm-none-eabi-gcc, …).  Always force the host native
            # gcc so pforth runs on the build machine.
            if(CMAKE_SIZEOF_VOID_P EQUAL 4)
                # 32-bit target (WASM, 3DS, RPI bare-metal, …): build a
                # 32-bit host pforth to produce a matching dictionary.
                # Requires gcc-multilib on Linux hosts.
                set(_PFORTH_EXTRA_ARGS
                    -DCMAKE_C_COMPILER=gcc
                    -DCMAKE_C_FLAGS=-m32
                    -DCMAKE_C_COMPILER_WORKS=TRUE
                    -DCMAKE_CXX_COMPILER_WORKS=TRUE)
            else()
                # 64-bit cross target (Switch ARM64, …): native host pforth.
                set(_PFORTH_EXTRA_ARGS
                    -DCMAKE_C_COMPILER=gcc
                    -DCMAKE_C_COMPILER_WORKS=TRUE
                    -DCMAKE_CXX_COMPILER_WORKS=TRUE)
            endif()
        else()
            # Native build: let cmake pick the host compiler automatically.
            set(_PFORTH_EXTRA_ARGS
                -DCMAKE_C_COMPILER_WORKS=TRUE
                -DCMAKE_CXX_COMPILER_WORKS=TRUE)
        endif()

        # Write a custom CMakeLists.txt for the bootstrap that uses the portable
        # stdio I/O module (getchar/putchar) instead of win32_console (_getch).
        # win32_console reads from the Windows console input buffer (CONIN$) via
        # _getch(), which blocks indefinitely on CI runners that have a console
        # but no keyboard input.  The stdio module reads from stdin (fd 0) which
        # respects the INPUT_FILE redirect to the null device.
        set(_PFORTH_BOOTSTRAP_SRC "${CMAKE_BINARY_DIR}/pforth_bootstrap_src")
        file(MAKE_DIRECTORY "${_PFORTH_BOOTSTRAP_SRC}")
        file(WRITE "${_PFORTH_BOOTSTRAP_SRC}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.6)
project(PForth)
message(STATUS "Configuring pforth bootstrap (stdio I/O)...")

if(NOT DEFINED PFORTH_VENDOR_DIR)
    message(FATAL_ERROR "PFORTH_VENDOR_DIR must be defined")
endif()

set(PFORTH_CSRC "${PFORTH_VENDOR_DIR}/csrc")
set(PFORTH_FTH  "${PFORTH_VENDOR_DIR}/fth")

file(STRINGS "${PFORTH_CSRC}/sources.cmake" _raw)
set(_c_srcs)
foreach(_f ${_raw})
    if(_f MATCHES "\\.c$")
        list(APPEND _c_srcs "${PFORTH_CSRC}/${_f}")
    endif()
endforeach()
list(APPEND _c_srcs
    "${PFORTH_CSRC}/stdio/pf_fileio_stdio.c"
    "${PFORTH_CSRC}/stdio/pf_io_stdio.c")

if(MSVC)
    add_compile_options(/W3 /wd4701 /wd4244 /wd4267 /wd4127 /wd4100 /wd4456)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-w)
endif()

add_library(PForth_lib STATIC ${_c_srcs})
target_compile_definitions(PForth_lib PRIVATE PF_SUPPORT_FP)
target_include_directories(PForth_lib PRIVATE "${PFORTH_CSRC}")

# Put pforth.exe directly in fth/ (no Debug/ subdir) on all generators.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY         "${PFORTH_FTH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${PFORTH_FTH}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${PFORTH_FTH}")

add_executable(pforth "${PFORTH_CSRC}/pf_main.c")
target_link_libraries(pforth PForth_lib)
target_include_directories(pforth PRIVATE "${PFORTH_CSRC}")
if(UNIX OR APPLE)
    target_link_libraries(pforth m)
endif()
]=])

        execute_process(
            COMMAND ${CMAKE_COMMAND}
                    -DPFORTH_VENDOR_DIR=${THIRDPARTY_DIR}/pforth
                    -DCMAKE_BUILD_TYPE=Release
                    ${_PFORTH_EXTRA_ARGS}
                    -S "${_PFORTH_BOOTSTRAP_SRC}" -B "${_PFORTH_BOOTSTRAP_DIR}"
            RESULT_VARIABLE _pforth_cfg_result
        )
        # Build in Release to avoid MSVC Debug overhead (runtime checks on every
        # function call make pforth ~10-20x slower, causing the 120s timeout).
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build "${_PFORTH_BOOTSTRAP_DIR}"
                    --target pforth --config Release
            RESULT_VARIABLE _pforth_build_result
            TIMEOUT 180
        )

        # pforth.exe is directly in PFORTH_FTH_DIR on all platforms/generators
        # because we force CMAKE_RUNTIME_OUTPUT_DIRECTORY* in the bootstrap.
        if(CMAKE_HOST_WIN32)
            set(_pforth_exe "${PFORTH_FTH_DIR}/pforth.exe")
            set(_pforth_null "NUL")
        else()
            set(_pforth_exe "${PFORTH_FTH_DIR}/pforth")
            set(_pforth_null "/dev/null")
        endif()

        # Build pforth.dic (initialise the full standard dictionary).
        # INPUT_FILE redirects stdin to the null device so pforth's getchar()
        # returns EOF immediately instead of waiting for interactive input.
        execute_process(
            COMMAND "${_pforth_exe}" -i "${PFORTH_FTH_DIR}/system.fth"
            WORKING_DIRECTORY "${PFORTH_FTH_DIR}"
            INPUT_FILE "${_pforth_null}"
            RESULT_VARIABLE _pforth_dic_result
            OUTPUT_QUIET
            ERROR_QUIET
            TIMEOUT 300
        )

        # Export the dictionary as a C header.
        execute_process(
            COMMAND "${_pforth_exe}" "${PFORTH_FTH_DIR}/mkdicdat.fth"
            WORKING_DIRECTORY "${PFORTH_FTH_DIR}"
            INPUT_FILE "${_pforth_null}"
            RESULT_VARIABLE _pforth_dicdat_result
            OUTPUT_QUIET
            ERROR_QUIET
            TIMEOUT 120
        )
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E rename
                    "${PFORTH_FTH_DIR}/pfdicdat.h"
                    "${PFORTH_DIR}/pfdicdat.h"
        )

        if(NOT EXISTS ${PFORTH_DICDAT})
            if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_CROSSCOMPILING)
                message(FATAL_ERROR
                    "Forth: could not generate 32-bit pfdicdat.h.\n"
                    "A 32-bit host gcc is required.  On Linux:\n"
                    "  sudo apt-get install gcc-multilib\n"
                    "Then delete the build directory and re-run cmake.")
            else()
                message(FATAL_ERROR
                    "Forth: failed to generate pfdicdat.h.\n"
                    "Try manually:\n"
                    "  cd vendor/pforth && cmake . && make pforth_dic_header")
            endif()
        endif()

        message(STATUS "Forth: pfdicdat.h generated successfully")
    endif()

    # -------------------------------------------------------------------------
    # forthdemo.tic.dat — demo cartridge for 'new forth' command.
    # Source: demos/forthdemo.fth
    # Regenerate (after building prj2cart and bin2txt):
    #   prj2cart demos/forthdemo.fth /tmp/forthdemo.tic
    #   bin2txt  /tmp/forthdemo.tic build/assets/forthdemo.tic.dat -z
    # -------------------------------------------------------------------------

    # -------------------------------------------------------------------------
    # TIC-80 forth library
    # -------------------------------------------------------------------------
    set(FORTH_SRC
        ${PFORTH_KERNEL_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/api/forth.c
        ${CMAKE_SOURCE_DIR}/src/api/parse_note.c
    )

    add_library(forth ${TIC_RUNTIME} ${FORTH_SRC})

    if(NOT BUILD_STATIC)
        set_target_properties(forth PROPERTIES PREFIX "")
    endif()

    target_compile_definitions(forth INTERFACE TIC_BUILD_WITH_FORTH=1)

    target_compile_definitions(forth PRIVATE
        PF_STATIC_DIC       # load the pre-compiled dictionary from pfdicdat.h
        PF_SUPPORT_FP       # enable floating-point word set
        PF_NO_FILEIO        # stub out file I/O (no filesystem in cartridges)
        PF_DEMAND_PAGING=0
    )


    target_link_libraries(forth PRIVATE runtime)

    target_include_directories(forth
        PRIVATE
            # pfdicdat.h lives here; must come before any other include that
            # could shadow it.
            ${PFORTH_DIR}
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
    )

    # Suppress warnings from pforth's own C files to avoid noise in TIC-80
    # build logs (pforth was not written to match TIC-80's warning flags).
    foreach(SRC ${PFORTH_KERNEL_SOURCES})
        set_source_files_properties(${SRC} PROPERTIES COMPILE_FLAGS
            "-w")
    endforeach()

endif(BUILD_WITH_FORTH)
