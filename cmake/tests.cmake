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

        # Every registration, so one target can build them all below.
        set_property(GLOBAL APPEND PROPERTY TIC80_TESTS ${target})
        add_test(NAME ${target} COMMAND ${target} ${ARGN})
    endfunction()

    if(BUILD_WITH_JS)
        add_executable(js-test ${CMAKE_SOURCE_DIR}/tests/js.c)
        target_link_libraries(js-test PRIVATE js tic80core)
        tic80_add_test(js-test)
    endif()

    if(BUILD_WITH_LUGFORTH)
        add_executable(lugforth-test ${CMAKE_SOURCE_DIR}/tests/lugforth.c)
        target_link_libraries(lugforth-test PRIVATE lugforth tic80core)
        tic80_add_test(lugforth-test)
    endif()

    # The menu bar is tested against the core alone: linking tic80studio instead
    # would hide a call into studio.c, which is the one thing this test is for.
    # The bar draws its icons and cursor through the ui module, so that builds
    # here too.
    add_executable(menubar-test
        ${CMAKE_SOURCE_DIR}/tests/menubar.c
        ${CMAKE_SOURCE_DIR}/src/studio/menubar.c
        ${CMAKE_SOURCE_DIR}/src/studio/ui.c
    )

    target_link_libraries(menubar-test PRIVATE tic80core)
    tic80_add_test(menubar-test)

    # The gamepad header is compiled with its test backend here: the slot
    # policy, the deadzone and the stick are exercised with no pad plugged in,
    # and on every platform — the platform backends above it need none of this.
    add_executable(sgamepad-test ${CMAKE_SOURCE_DIR}/tests/sgamepad.c)
    tic80_add_test(sgamepad-test)

    # The Windows readers are pure functions over reports, so what a pad can
    # only be asked for is asked here instead: a report in, its bits out. No
    # platform is touched, which is why this runs everywhere the rest do.
    add_executable(sgamepad-hid-test ${CMAKE_SOURCE_DIR}/tests/sgamepad_hid.c)
    tic80_add_test(sgamepad-hid-test)

    # The studio's sound is a module of its own, and links with the core alone:
    # nothing the test reaches goes near the studio, and its header names only
    # the core, so no build directory is needed here either.
    add_executable(sound-test
        ${CMAKE_SOURCE_DIR}/tests/sound.c
        ${CMAKE_SOURCE_DIR}/src/studio/sound.c
    )

    target_link_libraries(sound-test PRIVATE tic80core)
    tic80_add_test(sound-test)

    # The icon and the cursor are a module of their own, and link with the core
    # alone: the bar is one of their callers, not their owner.
    add_executable(ui-test
        ${CMAKE_SOURCE_DIR}/tests/ui.c
        ${CMAKE_SOURCE_DIR}/src/studio/ui.c
    )

    target_link_libraries(ui-test PRIVATE tic80core)
    tic80_add_test(ui-test)

    # The keyboard links with the core alone as well, and the platform call it
    # makes is the test's own to answer. That call is declared in system.h,
    # which wants the generated version.h, so this one keeps the build dir.
    add_executable(keyboard-test
        ${CMAKE_SOURCE_DIR}/tests/keyboard.c
        ${CMAKE_SOURCE_DIR}/src/studio/keyboard.c
    )

    target_include_directories(keyboard-test PRIVATE ${CMAKE_BINARY_DIR})
    target_link_libraries(keyboard-test PRIVATE tic80core)
    tic80_add_test(keyboard-test)

    # The pointer and the buttons, with the core alone as well: the machine's
    # pointer for the one, the state struct for the others.
    add_executable(mouse-test
        ${CMAKE_SOURCE_DIR}/tests/mouse.c
        ${CMAKE_SOURCE_DIR}/src/studio/mouse.c
    )

    target_link_libraries(mouse-test PRIVATE tic80core)
    tic80_add_test(mouse-test)

    # The splash links without the studio too: start.c and the module it plays
    # through, with the core and nothing else.
    add_executable(start-test
        ${CMAKE_SOURCE_DIR}/tests/start.c
        ${CMAKE_SOURCE_DIR}/src/studio/screens/start.c
        ${CMAKE_SOURCE_DIR}/src/studio/sound.c
    )

    target_include_directories(start-test PRIVATE ${CMAKE_BINARY_DIR})
    target_link_libraries(start-test PRIVATE tic80core)
    tic80_add_test(start-test)

    # The embedded cart format has two ends and this test drives one into the
    # other with the core alone, and with no build directory either: that is
    # only there for studio.h's version.h, which this module no longer needs.
    add_executable(embed-test
        ${CMAKE_SOURCE_DIR}/tests/embed.c
        ${CMAKE_SOURCE_DIR}/src/studio/embed.c
    )

    target_link_libraries(embed-test PRIVATE tic80core)
    tic80_add_test(embed-test)

    # The editor history is a byte buffer and the deltas between its commits:
    # the standard library is all it needs.
    add_executable(history-test
        ${CMAKE_SOURCE_DIR}/tests/history.c
        ${CMAKE_SOURCE_DIR}/src/ext/history.c
    )

    # The module allocates through these names so the test can fail one
    # allocation and count what stays alive.
    target_compile_definitions(history-test PRIVATE HISTORY_TEST_ALLOC)

    tic80_add_test(history-test)

    # The same module with a budget small enough that a few commits evict.
    add_executable(history-limit-test
        ${CMAKE_SOURCE_DIR}/tests/history_limit.c
        ${CMAKE_SOURCE_DIR}/src/ext/history.c
    )

    target_compile_definitions(history-limit-test PRIVATE HISTORY_MAX_BYTES=25)
    tic80_add_test(history-limit-test)

    # The console is the module whose face is a request the host answers, and
    # this test is its host: it builds the module with the core and the modules
    # it names — no studio anywhere — and every command it is given ends in an
    # ask the test takes and answers. Net is the test's own (tests/console.c):
    # a request is held until the test answers it, and no socket is opened.
    add_executable(console-test
        ${CMAKE_SOURCE_DIR}/tests/console.c
        ${CMAKE_SOURCE_DIR}/src/studio/screens/console.c
        ${CMAKE_SOURCE_DIR}/src/studio/keyboard.c
        ${CMAKE_SOURCE_DIR}/src/studio/mouse.c
        ${CMAKE_SOURCE_DIR}/src/studio/ui.c
        ${CMAKE_SOURCE_DIR}/src/studio/screens/start.c
        ${CMAKE_SOURCE_DIR}/src/studio/embed.c
        ${CMAKE_SOURCE_DIR}/src/studio/fs.c
        ${CMAKE_SOURCE_DIR}/src/ext/json.c
    )

    target_include_directories(console-test PRIVATE ${CMAKE_BINARY_DIR} ${THIRDPARTY_DIR}/jsmn)
    target_link_libraries(console-test PRIVATE tic80core zip)
    tic80_add_test(console-test)

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

    # The API conformance suite records every API call of every language
    # through traps over core->api, no window. It needs the static runtime:
    # only there does tic_scripts() list every binding instead of leaving
    # them to be loaded as modules at run time.
    if(BUILD_STATIC)
        add_executable(api-conformance
            ${CMAKE_SOURCE_DIR}/tests/conformance/runner.c
            ${CMAKE_SOURCE_DIR}/tests/conformance/traps.c)
        target_link_libraries(api-conformance PRIVATE tic80core)
        tic80_add_test(api-conformance ${CMAKE_SOURCE_DIR}/tests/conformance/probes)
    endif()

    # One target for the whole suite — what CI builds, so a test registered
    # above is built and run without a change to the workflow.
    get_property(tic80_tests GLOBAL PROPERTY TIC80_TESTS)
    add_custom_target(tic80-tests DEPENDS ${tic80_tests})

endif()
