################################
# Tests
################################

# Off by default: meant for a build directory of its own, configured with
# -DBUILD_TESTS=ON and run with ctest.
option(BUILD_TESTS "Build the tests" OFF)

if(BUILD_TESTS)

    enable_testing()

    # Every test here is assert-driven, and a Release configure defines NDEBUG,
    # which turns assert into a no-op: the test would pass without running at
    # all, whatever it says. NDEBUG comes off for the whole target, so a test
    # registered through this also gets its sources compiled with it off.
    function(tic80_add_test target)
        if(MSVC)
            target_compile_options(${target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${target} PRIVATE -UNDEBUG)
        endif()

        target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
        add_test(NAME ${target} COMMAND ${target} ${ARGN})
    endfunction()

    # The strip is tested against the core alone: linking tic80studio instead
    # would hide a call into studio.c, which is the one thing this test is for.
    # version.h is generated into the build directory, and the studio target is
    # what usually publishes that path.
    add_executable(toolbar-test
        ${CMAKE_SOURCE_DIR}/tests/toolbar.c
        ${CMAKE_SOURCE_DIR}/src/studio/toolbar.c
    )

    target_include_directories(toolbar-test PRIVATE ${CMAKE_BINARY_DIR})
    target_link_libraries(toolbar-test PRIVATE tic80core)
    tic80_add_test(toolbar-test)

    # The FFT module, driven offline by both tests. They build its sources
    # rather than linking the core, because the stubs variant below has to
    # recompile them with TIC80_FFT_UNSUPPORTED. FFT_Open reaches VQT_Open, so
    # the fft test needs the vqt sources as much as the vqt one does.
    set(TIC80_FFT_TEST_SRC
        ${CMAKE_SOURCE_DIR}/src/ext/fft.c
        ${CMAKE_SOURCE_DIR}/src/fftdata.c
        ${CMAKE_SOURCE_DIR}/src/vqtdata.c
        ${CMAKE_SOURCE_DIR}/src/ext/vqt.c
        ${CMAKE_SOURCE_DIR}/src/ext/vqt_kernel.c
        ${CMAKE_SOURCE_DIR}/src/ext/kiss_fft.c
        ${CMAKE_SOURCE_DIR}/src/ext/kiss_fftr.c
    )

    foreach(test fft vqt)
        add_executable(${test}-test ${CMAKE_SOURCE_DIR}/tests/${test}.c ${TIC80_FFT_TEST_SRC})
        tic80_add_test(${test}-test)

        # What include/tic80_config.h does on the targets without a capture
        # device; the API stubs it leaves behind are only reachable this way.
        add_executable(${test}-test-stubs ${CMAKE_SOURCE_DIR}/tests/${test}.c ${TIC80_FFT_TEST_SRC})
        target_compile_definitions(${test}-test-stubs PRIVATE TIC80_FFT_UNSUPPORTED)
        tic80_add_test(${test}-test-stubs)

        # miniaudio comes in through the module, so these want m and dl here
        # even though nothing in the test itself is doing the loading.
        if(LINUX)
            target_link_libraries(${test}-test PRIVATE m dl)
            target_link_libraries(${test}-test-stubs PRIVATE m dl)
        endif()
    endforeach()

    # The outline test dlopens a built script module, so it needs python as a
    # shared library; the path is baked into the test command below.
    if(UNIX AND BUILD_WITH_PYTHON AND NOT BUILD_STATIC)
        add_executable(python-outline-test ${CMAKE_SOURCE_DIR}/tests/python_outline.c)
        add_dependencies(python-outline-test python)

        if(LINUX)
            target_link_libraries(python-outline-test PRIVATE dl)
        endif()

        tic80_add_test(python-outline-test $<TARGET_FILE:python>)
    endif()

endif()
