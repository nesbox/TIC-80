/* The drawing API benchmark: a player without a window, a VM or a renderer.
 *
 * Every scenario is a fixed `setup` plus a `body`; the body calls the drawing
 * function through core->api, the same table every script binding calls. The
 * setup puts VRAM into a known state outside the timed region before every
 * round, so repeated calls are measured from the same screen.
 *
 * Cost is process CPU time: single-threaded work, and the runner being shared
 * does not inflate it. A calibration pass sizes the round, a warmup round is
 * discarded, and five rounds produce the median and the min–max spread. One
 * instrumented call, also outside the timed region, says how many pixels the
 * scenario changed and hashes the screen; the hash is what says whether an
 * optimization changed the picture.
 *
 * Usage: api-bench [--compare <previous.txt>] [--help]
 *
 * The local before/after loop:
 *     api-bench > before.txt
 *     ... change the drawing code ...
 *     api-bench --compare before.txt
 *
 * The comparison reads the tool's own earlier output, matches rows by
 * api + scenario and prints before, now and the delta. Nothing is committed
 * as a baseline: the saved file is an ordinary local artifact.
 */
#include "core/core.h"
#include "api.h"
#include "tools.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#define BENCH_VERSION     1
#define TARGET_NS         50000000ull
#define ROUNDS            5
#define CALIBRATION_CALLS 64
#define CALIBRATION_NS    1000000ull
#define MAX_CALLS         50000000ull
#define MAX_SAVED         256
#define TEXT_MAX          1024
#define CELL_MAX          64

typedef struct
{
    const char* api;
    const char* name;
    void (*setup)(tic_core* core);
    void (*body)(tic_core* core);
    bool repeatable;
} Scenario;

typedef struct
{
    u64 calls;
    double medianNs;
    double lowNs;
    double highNs;
    u64 pixels;
    u64 hash;
} Measurement;

typedef struct
{
    char api[32];
    char scenario[64];
    double us;
    char screen[17];
} SavedRow;

static const char* buildName(void)
{
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

static bool debugBuild(void)
{
#if defined(NDEBUG)
    return false;
#else
    return true;
#endif
}

static u64 benchNowNs(void)
{
#if defined(_WIN32)
    FILETIME creation, exit, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user);

    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime;
    u.HighPart = user.dwHighDateTime;

    return (k.QuadPart + u.QuadPart) * 100ull; // 100 ns ticks
#else
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
#endif
}

static void fillRam(tic_core* core)
{
    tic_ram* ram = core->memory.ram;

    for(s32 t = 0; t < TIC_BANK_SPRITES; t++)
    {
        for(s32 i = 0; i < TIC_SPRITESIZE * TIC_SPRITESIZE; i++)
        {
            u8 color = (u8)(((i % TIC_SPRITESIZE) * 3 + (i / TIC_SPRITESIZE) * 5 + t) % 7 + 1);
            tic_tool_poke4(ram->tiles.data[t].data, (u32)i, color);
            tic_tool_poke4(ram->sprites.data[t].data, (u32)i, color);
        }
    }

    for(s32 i = 0; i < TIC_MAP_WIDTH * TIC_MAP_HEIGHT; i++)
        ram->map.data[i] = (u8)((i * 7) % TIC_BANK_SPRITES);
}

static void resetState(tic_core* core, u8 sentinel)
{
    tic_api_clip((tic_mem*)core, 0, 0, TIC80_WIDTH, TIC80_HEIGHT);

    core->memory.ram->vram.blit.segment = 2;

    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        tic_tool_poke4(core->memory.ram->vram.mapping, (u32)i, (u8)i);

    memset(core->memory.ram->vram.screen.data, (sentinel & 0xf) | (sentinel << 4), sizeof(tic_screen));
}

static void setupZero(tic_core* core)
{
    resetState(core, 0);
}

static void setupFifteen(tic_core* core)
{
    resetState(core, 15);
}

static void setupClipZero(tic_core* core)
{
    resetState(core, 0);
    tic_api_clip((tic_mem*)core, 20, 10, 60, 40);
}

static void setupClipFifteen(tic_core* core)
{
    resetState(core, 15);
    tic_api_clip((tic_mem*)core, 20, 10, 60, 40);
}

static void setupPaintFull(tic_core* core)
{
    resetState(core, 0);
    tic_api_rectb((tic_mem*)core, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 1);
}

static void setupPaintSmall(tic_core* core)
{
    resetState(core, 0);
    tic_api_rectb((tic_mem*)core, 50, 40, 100, 50, 1);
}

static void bodyCls(tic_core* core)
{
    core->api.cls((tic_mem*)core, 0);
}

static void bodyPix(tic_core* core)
{
    core->api.pix((tic_mem*)core, TIC80_WIDTH / 2, TIC80_HEIGHT / 2, 15, false);
}

static void bodyRect(tic_core* core)
{
    core->api.rect((tic_mem*)core, 20, 20, 100, 50, 15);
}

static void bodyRectFull(tic_core* core)
{
    core->api.rect((tic_mem*)core, 0, 0, TIC80_WIDTH, TIC80_HEIGHT, 15);
}

static void bodyRectb(tic_core* core)
{
    core->api.rectb((tic_mem*)core, 20, 20, 100, 50, 15);
}

static void bodyLineHorizontal(tic_core* core)
{
    core->api.line((tic_mem*)core, 10, TIC80_HEIGHT / 2, TIC80_WIDTH - 10, TIC80_HEIGHT / 2, 15);
}

static void bodyLineVertical(tic_core* core)
{
    core->api.line((tic_mem*)core, TIC80_WIDTH / 2, 10, TIC80_WIDTH / 2, TIC80_HEIGHT - 10, 15);
}

static void bodyLineDiagonal(tic_core* core)
{
    core->api.line((tic_mem*)core, 10, 10, TIC80_WIDTH - 10, TIC80_HEIGHT - 10, 15);
}

static void bodyTriSmall(tic_core* core)
{
    core->api.tri((tic_mem*)core, 110, 58, 130, 58, 120, 78, 15);
}

static void bodyTriFull(tic_core* core)
{
    core->api.tri((tic_mem*)core, 5, 5, TIC80_WIDTH - 5, 20, TIC80_WIDTH / 2, TIC80_HEIGHT - 5, 15);
}

static void bodyTrib(tic_core* core)
{
    core->api.trib((tic_mem*)core, 20, 20, TIC80_WIDTH - 20, 40, TIC80_WIDTH / 2, TIC80_HEIGHT - 20, 15);
}

static void bodyTtriTiles(tic_core* core)
{
    core->api.ttri((tic_mem*)core,
                   20, 20, TIC80_WIDTH - 20, 30, TIC80_WIDTH / 2, TIC80_HEIGHT - 10,
                   0, 0, 64, 0, 32, 64,
                   tic_tiles_texture, NULL, 0,
                   0, 0, 0, false);
}

static void bodyTtriMap(tic_core* core)
{
    core->api.ttri((tic_mem*)core,
                   20, 20, TIC80_WIDTH - 20, 30, TIC80_WIDTH / 2, TIC80_HEIGHT - 10,
                   0, 0, 64, 0, 32, 64,
                   tic_map_texture, NULL, 0,
                   0, 0, 0, false);
}

static void bodyCirc(tic_core* core)
{
    core->api.circ((tic_mem*)core, TIC80_WIDTH / 2, TIC80_HEIGHT / 2, 50, 15);
}

static void bodyCircb(tic_core* core)
{
    core->api.circb((tic_mem*)core, TIC80_WIDTH / 2, TIC80_HEIGHT / 2, 50, 15);
}

static void bodyElli(tic_core* core)
{
    core->api.elli((tic_mem*)core, TIC80_WIDTH / 2, TIC80_HEIGHT / 2, 60, 40, 15);
}

static void bodyEllib(tic_core* core)
{
    core->api.ellib((tic_mem*)core, TIC80_WIDTH / 2, TIC80_HEIGHT / 2, 60, 40, 15);
}

static void bodyRrect(tic_core* core)
{
    core->api.rrect((tic_mem*)core, 60, 38, 120, 60, 15, 12, 0);
}

static void bodyRrectb(tic_core* core)
{
    core->api.rrectb((tic_mem*)core, 60, 38, 120, 60, 15, 12, 0);
}

static void bodySprTile(tic_core* core)
{
    core->api.spr((tic_mem*)core, 0, 100, 50, 1, 1, NULL, 0, 1, tic_no_flip, tic_no_rotate);
}

static void bodySprBlock(tic_core* core)
{
    core->api.spr((tic_mem*)core, 0, 100, 50, 2, 2, NULL, 0, 1, tic_no_flip, tic_no_rotate);
}

static void bodySprScale(tic_core* core)
{
    core->api.spr((tic_mem*)core, 0, 100, 50, 1, 1, NULL, 0, 2, tic_no_flip, tic_no_rotate);
}

static void bodySprFlip(tic_core* core)
{
    core->api.spr((tic_mem*)core, 0, 100, 50, 1, 1, NULL, 0, 1, tic_horz_flip, tic_no_rotate);
}

static void bodySprRotate(tic_core* core)
{
    core->api.spr((tic_mem*)core, 0, 100, 50, 1, 1, NULL, 0, 1, tic_no_flip, tic_90_rotate);
}

static void bodySprChroma(tic_core* core)
{
    static u8 colors[] = {1};
    core->api.spr((tic_mem*)core, 0, 100, 50, 1, 1, colors, 1, 1, tic_no_flip, tic_no_rotate);
}

static void bodyMap(tic_core* core)
{
    core->api.map((tic_mem*)core, 0, 0, TIC80_WIDTH / TIC_SPRITESIZE, TIC80_HEIGHT / TIC_SPRITESIZE, 0, 0, NULL, 0, 1, NULL, NULL);
}

static void bodyMapScroll(tic_core* core)
{
    core->api.map((tic_mem*)core, 10, 5, TIC80_WIDTH / TIC_SPRITESIZE, TIC80_HEIGHT / TIC_SPRITESIZE, 0, 0, NULL, 0, 1, NULL, NULL);
}

static void bodyPrint(tic_core* core)
{
    core->api.print((tic_mem*)core, "Hello TIC-80 benchmark!", 10, 30, 15, false, 1, false);
}

static void bodyPrintFixed(tic_core* core)
{
    core->api.print((tic_mem*)core, "Hello TIC-80 benchmark!", 10, 30, 15, true, 2, false);
}

static void bodyPrintAlt(tic_core* core)
{
    core->api.print((tic_mem*)core, "Hello TIC-80 benchmark!", 10, 30, 15, false, 1, true);
}

static void bodyFont(tic_core* core)
{
    core->api.font((tic_mem*)core, "Custom font sample", 10, 30, NULL, 0, 6, 8, false, 1, false);
}

static void bodyPaintFull(tic_core* core)
{
    core->api.paint((tic_mem*)core, TIC80_WIDTH / 2, TIC80_HEIGHT / 2, 15, 1);
}

static void bodyPaintSmall(tic_core* core)
{
    core->api.paint((tic_mem*)core, 100, 65, 15, 1);
}

static void bodyClip(tic_core* core)
{
    core->api.clip((tic_mem*)core, 20, 10, 60, 40);
}

static const Scenario Scenarios[] =
    {
        {"cls", "full screen", setupFifteen, bodyCls, true},
        {"cls", "clip 60x40", setupClipFifteen, bodyCls, true},
        {"clip", "set 60x40", setupZero, bodyClip, true},
        {"pix", "single pixel", setupZero, bodyPix, true},
        {"rect", "100x50", setupZero, bodyRect, true},
        {"rect", "full screen", setupZero, bodyRectFull, true},
        {"rect", "full, clip 60x40", setupClipZero, bodyRectFull, true},
        {"rectb", "100x50", setupZero, bodyRectb, true},
        {"line", "horizontal 220px", setupZero, bodyLineHorizontal, true},
        {"line", "vertical 116px", setupZero, bodyLineVertical, true},
        {"line", "diagonal", setupZero, bodyLineDiagonal, true},
        {"tri", "small 20px", setupZero, bodyTriSmall, true},
        {"tri", "full screen", setupZero, bodyTriFull, true},
        {"trib", "large", setupZero, bodyTrib, true},
        {"ttri", "tiles", setupZero, bodyTtriTiles, true},
        {"ttri", "map", setupZero, bodyTtriMap, true},
        {"circ", "r=50", setupZero, bodyCirc, true},
        {"circb", "r=50", setupZero, bodyCircb, true},
        {"elli", "60x40", setupZero, bodyElli, true},
        {"ellib", "60x40", setupZero, bodyEllib, true},
        {"rrect", "120x60 r=12", setupZero, bodyRrect, true},
        {"rrectb", "120x60 r=12", setupZero, bodyRrectb, true},
        {"spr", "1x1", setupFifteen, bodySprTile, true},
        {"spr", "2x2 tiles", setupFifteen, bodySprBlock, true},
        {"spr", "scale 2", setupFifteen, bodySprScale, true},
        {"spr", "flip h", setupFifteen, bodySprFlip, true},
        {"spr", "rotate 90", setupFifteen, bodySprRotate, true},
        {"spr", "chroma 1", setupFifteen, bodySprChroma, true},
        {"map", "full screen", setupFifteen, bodyMap, true},
        {"map", "scroll 10,5", setupFifteen, bodyMapScroll, true},
        {"print", "23 chars", setupZero, bodyPrint, true},
        {"print", "fixed, scale 2", setupZero, bodyPrintFixed, true},
        {"print", "alt font", setupZero, bodyPrintAlt, true},
        {"font", "6x8", setupZero, bodyFont, true},
        {"paint", "full screen", setupPaintFull, bodyPaintFull, false},
        {"paint", "region 100x50", setupPaintSmall, bodyPaintSmall, false},
};

#define SCENARIO_COUNT ((s32)(sizeof(Scenarios) / sizeof(Scenarios[0])))

static u64 countChanged(const u8* before, const u8* after, size_t size)
{
    u64 changed = 0;

    for(size_t i = 0; i < size; i++)
    {
        u8 diff = before[i] ^ after[i];
        if(diff & 0x0f)
            changed++;
        if(diff & 0xf0)
            changed++;
    }

    return changed;
}

static u64 hashScreen(const u8* data, size_t size)
{
    u64 hash = 0xcbf29ce484222325ull;

    for(size_t i = 0; i < size; i++)
    {
        hash ^= data[i];
        hash *= 0x100000001b3ull;
    }

    return hash;
}

static void sortValues(double* values, s32 count)
{
    for(s32 i = 1; i < count; i++)
    {
        double value = values[i];
        s32 j = i - 1;

        while(j >= 0 && values[j] > value)
        {
            values[j + 1] = values[j];
            j--;
        }

        values[j + 1] = value;
    }
}

static void measureScenario(tic_core* core, const Scenario* scenario, Measurement* result)
{
    static u8 before[sizeof(tic_screen)];
    static u8 after[sizeof(tic_screen)];

    scenario->setup(core);
    memcpy(before, core->memory.ram->vram.screen.data, sizeof before);
    scenario->body(core);
    memcpy(after, core->memory.ram->vram.screen.data, sizeof after);

    result->pixels = countChanged(before, after, sizeof before);
    result->hash = hashScreen(after, sizeof after);

    u64 calls = 1;

    if(scenario->repeatable)
    {
        // A tiny call needs a longer probe than a timer can resolve in a
        // handful of iterations; grow it until the probe itself is measurable.
        u64 probe = CALIBRATION_CALLS;
        u64 elapsed = 0;

        for(;;)
        {
            scenario->setup(core);
            u64 start = benchNowNs();

            for(u64 i = 0; i < probe; i++)
                scenario->body(core);

            u64 end = benchNowNs();
            elapsed = end - start;

            if(elapsed >= CALIBRATION_NS || probe >= MAX_CALLS)
                break;

            probe *= 10;

            if(probe > MAX_CALLS)
                probe = MAX_CALLS;
        }

        // One probe can catch a scheduling visit; the shortest of three is
        // the closest to the work's own cost.
        u64 best = elapsed;

        for(s32 repeat = 0; repeat < 2; repeat++)
        {
            scenario->setup(core);
            u64 start = benchNowNs();

            for(u64 i = 0; i < probe; i++)
                scenario->body(core);

            u64 end = benchNowNs();

            if(end - start < best)
                best = end - start;
        }

        double perCall = (double)best / (double)probe;

        if(perCall < 1.0)
            perCall = 1.0;

        calls = (u64)((double)TARGET_NS / perCall);

        if(calls < 1)
            calls = 1;

        if(calls > MAX_CALLS)
            calls = MAX_CALLS;
    }

    result->calls = calls;

    double times[ROUNDS] = {0};

    for(s32 round = -1; round < ROUNDS; round++)
    {
        scenario->setup(core);
        u64 start = benchNowNs();

        for(u64 i = 0; i < calls; i++)
            scenario->body(core);

        u64 end = benchNowNs();

        if(round >= 0)
            times[round] = (double)(end - start) / (double)calls;
    }

    sortValues(times, ROUNDS);

    result->medianNs = times[ROUNDS / 2];
    result->lowNs = times[0];
    result->highNs = times[ROUNDS - 1];
}

static void printTable(const Measurement* results)
{
    printf("Drawing API benchmark — methodology %d, %s\n\n", BENCH_VERSION, buildName());

    if(debugBuild())
        printf("Debug build: the numbers are not representative.\n\n");

    printf("| api | scenario | calls/round | µs/call | px/call | ns/px | spread | screen |\n");
    printf("| --- | --- | --- | --- | --- | --- | --- | --- |\n");

    for(s32 i = 0; i < SCENARIO_COUNT; i++)
    {
        const Measurement* result = &results[i];
        double us = result->medianNs / 1000.0;
        double spread = result->medianNs > 0.0 ? (result->highNs - result->lowNs) / result->medianNs * 100.0 : 0.0;

        printf("| %s | %s | %llu | %.3f | %llu | ",
               Scenarios[i].api,
               Scenarios[i].name,
               (unsigned long long)result->calls,
               us,
               (unsigned long long)result->pixels);

        if(result->pixels)
            printf("%.2f", result->medianNs / (double)result->pixels);
        else
            printf("-");

        printf(" | %.1f%% | %016llx |\n", spread, (unsigned long long)result->hash);
    }
}

static char* readFile(const char* path, s32* size)
{
    FILE* file = fopen(path, "rb");

    if(!file)
        return NULL;

    long length = -1;

    if(fseek(file, 0, SEEK_END) == 0)
        length = ftell(file);

    if(length < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    char* data = malloc((size_t)length + 1);

    if(!data)
    {
        fclose(file);
        return NULL;
    }

    s32 read = (s32)fread(data, 1, (size_t)length, file);
    data[read] = '\0';
    fclose(file);

    *size = read;
    return data;
}

static void trim(char* text)
{
    s32 length = (s32)strlen(text);

    while(length > 0 && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';

    char* start = text;

    while(*start && isspace((unsigned char)*start))
        start++;

    if(start != text)
        memmove(text, start, strlen(start) + 1);
}

static s32 splitLine(char* line, char cells[][CELL_MAX], s32 max)
{
    s32 count = 0;
    char* pos = line;

    while(*pos && count < max)
    {
        char* bar = strchr(pos, '|');
        char* end = bar ? bar : pos + strlen(pos);
        s32 length = (s32)(end - pos);

        if(length >= CELL_MAX)
            length = CELL_MAX - 1;

        memcpy(cells[count], pos, (size_t)length);
        cells[count][length] = '\0';
        trim(cells[count]);
        count++;

        if(!bar)
            break;

        pos = bar + 1;
    }

    return count;
}

static s32 findColumn(char cells[][CELL_MAX], s32 count, const char* name)
{
    for(s32 i = 0; i < count; i++)
        if(strcmp(cells[i], name) == 0)
            return i;

    return -1;
}

static bool parseSaved(const char* text, SavedRow* rows, s32* count, s32* version)
{
    *count = 0;
    *version = -1;

    const char* header = strstr(text, "methodology ");

    if(!header || sscanf(header, "methodology %d", version) != 1)
        return false;

    char line[TEXT_MAX];
    bool inTable = false;
    bool haveColumns = false;
    s32 apiColumn = -1, scenarioColumn = -1, usColumn = -1, screenColumn = -1;
    const char* pos = text;

    while(*pos)
    {
        const char* end = strchr(pos, '\n');
        s32 length = end ? (s32)(end - pos) : (s32)strlen(pos);

        if(length >= (s32)sizeof line)
            length = (s32)sizeof line - 1;

        memcpy(line, pos, (size_t)length);
        line[length] = '\0';

        if(line[0] == '|')
        {
            char cells[16][CELL_MAX];
            s32 cellCount = splitLine(line, cells, (s32)(sizeof cells / sizeof cells[0]));

            if(!haveColumns)
            {
                apiColumn = findColumn(cells, cellCount, "api");
                scenarioColumn = findColumn(cells, cellCount, "scenario");
                usColumn = findColumn(cells, cellCount, "µs/call");
                screenColumn = findColumn(cells, cellCount, "screen");

                haveColumns = apiColumn >= 0 && scenarioColumn >= 0 && usColumn >= 0 && screenColumn >= 0;
                inTable = haveColumns;
            }
            else if(strstr(line, "---"))
            {
                // the separator under the header
            }
            else if(*count < MAX_SAVED && apiColumn < cellCount && scenarioColumn < cellCount && usColumn < cellCount && screenColumn < cellCount)
            {
                SavedRow* row = &rows[*count];

                snprintf(row->api, sizeof row->api, "%s", cells[apiColumn]);
                snprintf(row->scenario, sizeof row->scenario, "%s", cells[scenarioColumn]);
                row->us = strtod(cells[usColumn], NULL);
                snprintf(row->screen, sizeof row->screen, "%s", cells[screenColumn]);
                (*count)++;
            }
        }
        else if(inTable)
        {
            break; // the table ended at a non-table line
        }

        if(!end)
            break;

        pos = end + 1;
    }

    return haveColumns && *count > 0;
}

static void printComparison(const Measurement* results, const SavedRow* saved, s32 savedCount)
{
    printf("Drawing API benchmark — comparison, methodology %d, %s\n\n", BENCH_VERSION, buildName());
    printf("| api | scenario | before µs | now µs | Δ | screen |\n");
    printf("| --- | --- | --- | --- | --- | --- |\n");

    bool used[MAX_SAVED] = {false};

    for(s32 i = 0; i < SCENARIO_COUNT; i++)
    {
        const char* api = Scenarios[i].api;
        const char* name = Scenarios[i].name;
        double now = results[i].medianNs / 1000.0;
        char screen[17];
        snprintf(screen, sizeof screen, "%016llx", (unsigned long long)results[i].hash);

        s32 found = -1;

        for(s32 j = 0; j < savedCount; j++)
            if(strcmp(saved[j].api, api) == 0 && strcmp(saved[j].scenario, name) == 0)
            {
                found = j;
                break;
            }

        if(found < 0)
        {
            printf("| %s | %s | - | %.3f | new | - |\n", api, name, now);
            continue;
        }

        used[found] = true;
        double before = saved[found].us;
        char delta[32];

        if(before > 0.0)
            snprintf(delta, sizeof delta, "%+.1f%%", (now - before) / before * 100.0);
        else
            snprintf(delta, sizeof delta, "-");

        printf("| %s | %s | %.3f | %.3f | %s | %s |\n",
               api, name, before, now, delta,
               strcmp(saved[found].screen, screen) == 0 ? "=" : "✗");
    }

    for(s32 j = 0; j < savedCount; j++)
        if(!used[j])
            printf("\n- gone: %s / %s\n", saved[j].api, saved[j].scenario);
}

static void usage(void)
{
    printf("api-bench — the drawing API benchmark\n"
           "\n"
           "Usage: api-bench [--compare <previous.txt>] [--help]\n"
           "\n"
           "  --compare  read an earlier run of this tool and print the deltas\n"
           "  --help     this text\n"
           "\n"
           "Local before/after:\n"
           "  api-bench > before.txt\n"
           "  ... change the drawing code ...\n"
           "  api-bench --compare before.txt\n"
           "\n"
           "The saved file is a local artifact; nothing is committed as a baseline.\n");
}

int main(int argc, char** argv)
{
    const char* comparePath = NULL;

    for(s32 i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            usage();
            return 0;
        }
        else if(strcmp(argv[i], "--compare") == 0)
        {
            if(i + 1 >= argc)
            {
                fprintf(stderr, "--compare needs a file\n");
                return 2;
            }

            comparePath = argv[++i];
        }
        else
        {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);

    if(!tic)
    {
        fprintf(stderr, "could not create the core\n");
        return 2;
    }

    tic_core* core = (tic_core*)tic;

#if defined(BUILD_RENDER_CACHE)
    tic_core_draw_cache_set_enabled(tic, false);
#endif

    fillRam(core);

    Measurement results[SCENARIO_COUNT];

    for(s32 i = 0; i < SCENARIO_COUNT; i++)
        measureScenario(core, &Scenarios[i], &results[i]);

    tic_core_close(tic);

    if(comparePath)
    {
        s32 size = 0;
        char* text = readFile(comparePath, &size);

        if(!text)
        {
            fprintf(stderr, "cannot read %s\n", comparePath);
            return 2;
        }

        SavedRow saved[MAX_SAVED];
        s32 savedCount = 0;
        s32 version = -1;
        bool parsed = parseSaved(text, saved, &savedCount, &version);
        free(text);

        if(!parsed || version != BENCH_VERSION)
        {
            if(parsed)
                printf("the saved run uses methodology %d; this is %d — not comparable\n\n", version, BENCH_VERSION);
            else
                printf("the saved run carries no benchmark table — not comparable\n\n");

            printTable(results);
            return 0;
        }

        printComparison(results, saved, savedCount);
    }
    else
        printTable(results);

    return 0;
}
