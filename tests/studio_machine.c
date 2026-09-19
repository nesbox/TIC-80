/* The studio mode machine — unit tests.
 *
 * Built by cmake with -DBUILD_TESTS=ON, once per shipped configuration
 * (cmake/studio.cmake):
 *
 *   studio_machine_editors   BUILD_EDITORS BUILD_SURF SURF_MENU
 *   studio_machine_surf                BUILD_SURF SURF_MENU
 *   studio_machine_plain
 *
 * Each binary carries the variant it was built for as TEST_VARIANT_*, so what
 * is checked here is a literal and not a re-read of the very macros the module
 * under test reads: a build flag that stops meaning what the machine believes
 * it means fails here rather than in the field.
 *
 * Without cmake:
 *   cc -Isrc -Iinclude -DBUILD_EDITORS -DBUILD_SURF -DTEST_VARIANT_EDITORS \
 *      tests/studio_machine.c src/studio/machine.c -o machine-test && ./machine-test
 */
#include "studio/machine.h"

#include <stdio.h>
#include <string.h>

static int failed;
static int checks;

static void check(bool ok, const char* what)
{
    ++checks;

    if(!ok)
    {
        ++failed;
        printf("FAIL %s\n", what);
    }
}

static void check_mode(bool ok, const char* what, EditorMode got, EditorMode want)
{
    ++checks;

    if(!ok)
    {
        ++failed;
        printf("FAIL %s: got %s, want %s\n", what, sm_mode_name(got), sm_mode_name(want));
    }
}

#if defined(TEST_VARIANT_EDITORS)
enum { WANT_EDITORS = true, WANT_SURF = true };
#elif defined(TEST_VARIANT_SURF)
enum { WANT_EDITORS = false, WANT_SURF = true };
#elif defined(TEST_VARIANT_PLAIN)
enum { WANT_EDITORS = false, WANT_SURF = false };
#else
#error "the test target has to name its variant (TEST_VARIANT_*)"
#endif

// What the build carries is the machine's only input from the build system,
// and the three variants are the ones cmake/studio.cmake ships.
static void test_features(void)
{
    SmFeatures features = sm_build_features();

    check(features.has_editors == WANT_EDITORS, "has_editors follows the build");
    check(features.has_surf == WANT_SURF, "has_surf follows the build");
}

// The toolbar draws the tabs in this order and Ctrl+PgUp/PgDn walks it: the
// order is behaviour, not a detail of where the list is stored.
static void test_editor_ring(void)
{
    static const EditorMode Want[] =
    {
        TIC_CODE_MODE,
        TIC_SPRITE_MODE,
        TIC_MAP_MODE,
        TIC_SFX_MODE,
        TIC_MUSIC_MODE,
    };

    const EditorMode* ring = sm_editor_ring();

    check(SM_EDITOR_RING_COUNT == COUNT_OF(Want), "the count and the ring are the same length");

    for(size_t i = 0; i < COUNT_OF(Want); ++i)
        check_mode(ring[i] == Want[i], "the ring keeps its order", ring[i], Want[i]);
}

// Names are what the transition trace (TIC80_TRACE_MODES) and the failure
// messages above print, so every mode needs one and no two may share it.
static void test_mode_names(void)
{
    for(int mode = 0; mode < TIC_MODES_COUNT; ++mode)
    {
        const char* name = sm_mode_name((EditorMode)mode);

        check(name && *name && *name != '?', "every mode has a name");

        for(int other = mode + 1; other < TIC_MODES_COUNT; ++other)
            check(strcmp(name, sm_mode_name((EditorMode)other)) != 0, "no two modes share a name");
    }

    // The count is not a mode, and neither is anything outside the enum.
    check(strcmp(sm_mode_name(TIC_MODES_COUNT), "?") == 0, "the count is not a mode");
    check(strcmp(sm_mode_name((EditorMode)-1), "?") == 0, "nothing below the first mode");
}

int main(void)
{
    test_features();
    test_editor_ring();
    test_mode_names();

    printf("%s: %d checks, %d failed\n",
        failed ? "FAILED" : "ok", checks, failed);

    return failed ? 1 : 0;
}
