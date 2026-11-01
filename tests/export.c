/* The export command builder links without the studio: this file builds
 * export.c with the core alone, and no studio.c, editor or screen in sight.
 * The commands are pinned byte for byte; the PRO target builds the same source
 * with TIC80_PRO for the bank and alone arguments.
 *
 * Targets export-test / export-test-pro in cmake/tests.cmake;
 * `ctest -R export-test` runs both.
 */
#include "studio/export.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE (TICNAME_MAX + 64)

typedef struct
{
    s32 id;
    const char* command;
} Case;

static void testCommands(void)
{
#if defined(TIC80_PRO)
    const ExportState state =
        {
            .cartName = "game",
            .sfx = 3,
            .music = 2,
            .alone = true,
            .spriteBank = 2,
            .mapBank = 1,
            .musicBank = 3,
            .spriteVbank1 = true,
        };

    static const Case Cases[] =
        {
            {Export_win, "export win game-win alone=1"},
            {Export_linux, "export linux game-linux alone=1"},
            {Export_linuxarm, "export linuxarm game-linuxarm alone=1"},
            {Export_mac, "export mac game-macos alone=1"},
            {Export_macintel, "export macintel game-macintel alone=1"},
            {Export_html, "export html game-html"},
            {Export_binary, "export binary game"},
            {Export_map, "export map game-map bank=1"},
            {Export_tiles, "export tiles game-tiles bank=2"},
            {Export_sprites, "export sprites game-sprites bank=2"},
            {Export_mapimg, "export mapimg game-map bank=1"},
            {Export_screen, "export screen game-screen bank=1 vbank=1"},
            {Export_sfx, "export sfx game-sfx-03 id=3"},
            {Export_music, "export music game-music-02 id=2 bank=3"},
            {Export_help, "export help tic80-help"},
        };
#else
    const ExportState state =
        {
            .cartName = "game",
            .sfx = 3,
            .music = 2,
            .spriteVbank1 = true,
        };

    static const Case Cases[] =
        {
            {Export_win, "export win game-win"},
            {Export_linux, "export linux game-linux"},
            {Export_linuxarm, "export linuxarm game-linuxarm"},
            {Export_mac, "export mac game-macos"},
            {Export_macintel, "export macintel game-macintel"},
            {Export_html, "export html game-html"},
            {Export_binary, "export binary game"},
            {Export_map, "export map game-map"},
            {Export_tiles, "export tiles game-tiles"},
            {Export_sprites, "export sprites game-sprites"},
            {Export_mapimg, "export mapimg game-map"},
            {Export_screen, "export screen game-screen vbank=1"},
            {Export_sfx, "export sfx game-sfx-03 id=3"},
            {Export_music, "export music game-music-02 id=2"},
            {Export_help, "export help tic80-help"},
        };
#endif

    for(s32 i = 0; i < (s32)COUNT_OF(Cases); ++i)
    {
        char command[BUFFER_SIZE];

        assert(export_command(command, sizeof command, Cases[i].id, &state));
        assert(strcmp(command, Cases[i].command) == 0);
    }

    puts("commands ok");
}

static void testName(void)
{
    char command[BUFFER_SIZE];
    ExportState state = {.cartName = "invaders"};

    assert(export_command(command, sizeof command, Export_win, &state));
    assert(strcmp(command, "export win invaders-win") == 0);

    state.cartName = "";
    assert(export_command(command, sizeof command, Export_win, &state));
    assert(strcmp(command, "export win game-win") == 0);

    state.cartName = "my game";
    assert(export_command(command, sizeof command, Export_win, &state));
    assert(strcmp(command, "export win game-win") == 0);

    puts("name ok");
}

static void testToggles(void)
{
    char command[BUFFER_SIZE];
    const ExportState state = {.cartName = "game"};

    // The screen's palette bank, with the PRO bank argument when it exists.
    assert(export_command(command, sizeof command, Export_screen, &state));
#if defined(TIC80_PRO)
    assert(strcmp(command, "export screen game-screen bank=0 vbank=0") == 0);
#else
    assert(strcmp(command, "export screen game-screen vbank=0") == 0);
#endif

    puts("toggles ok");
}

static void testRefusal(void)
{
    char command[BUFFER_SIZE];
    const ExportState state = {.cartName = "game"};

    assert(!export_command(command, sizeof command, Export_Count, &state));
    assert(!export_command(command, sizeof command, -1, &state));
    assert(!export_command(command, 0, Export_win, &state));
    assert(!export_command(command, -1, Export_win, &state));

    char exact[sizeof "export win game-win"];
    assert(export_command(exact, sizeof exact, Export_win, &state));
    assert(strcmp(exact, "export win game-win") == 0);

    char shortBuffer[sizeof "export win game-win" - 1];
    assert(!export_command(shortBuffer, sizeof shortBuffer, Export_win, &state));

    // The base fits and an argument does not: the append's own refusal.
    char screen[sizeof "export screen game-screen"];
    assert(!export_command(screen, sizeof screen, Export_screen, &state));

    char shortArg[sizeof "export sfx game-sfx-00 id=0" - 1];
    assert(!export_command(shortArg, sizeof shortArg, Export_sfx, &state));

    puts("refusal ok");
}

int main(void)
{
    testCommands();
    testName();
    testToggles();
    testRefusal();

    return 0;
}
