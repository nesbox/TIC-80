/* The script API conformance runner: a player without a renderer.
 *
 * Runs every language tic_scripts() reports against its probe script
 * (tests/conformance/probes/<name><fileExtension>; lua.lua is the reference)
 * and substitutes the core's API table with the recording wrappers from
 * traps.c, so every call is recorded with the values it arrived with. The
 * records are diffed per probe section and printed as a markdown table —
 * ✅ where a language matches the reference, 🔴 with the count where not.
 * Adding a language to the suite is dropping a probe file in — nothing here
 * names one.
 *
 * Usage: api-conformance [probes dir] [--dump]
 * Exit: 0 — the table was printed, divergences and all; 2 — the measurement
 * broke (no reference, or a probe file that did not run).
 */
#include "core/core.h"
#include "script.h"
#include "traps.h"

#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LANGS   16      // MAX_SUPPORTED_LANGS, src/script.c
#define MAX_SECTIONS 512
#define TEXT_MAX    1024

typedef struct
{
    char** items;
    s32 count;
} Lines;

typedef struct
{
    const tic_script* script;
    char path[TEXT_MAX];
    bool hasFile;
    bool failed;
    char error[256];
    Lines lines;
} Lang;

// One probe: "#<api>/<variant>" and the observation lines under it.
typedef struct
{
    char marker[128];
    char api[32];
    s32 first;
    s32 count;
} Section;

typedef struct
{
    s32 lang;
    char api[32];
    char marker[128];
    char ref[TEXT_MAX];
    char got[TEXT_MAX];
} Divergence;

static Lang Langs[MAX_LANGS];
static s32 LangCount;

static Divergence* Divs;
static s32 DivCount;

// The api rows in the reference's probe order; the probes per api and the
// diverging-probe cells, addressed [api * LangCount + lang] — a new api
// appends one column, so the cap on neither is a number this file invents.
static const char** Apis;
static s32* ApiProbes;
static s32* ApiCells;
static s32 ApiCount;

static char* copyString(const char* text)
{
    char* copy = malloc(strlen(text) + 1);
    strcpy(copy, text);
    return copy;
}

static void linesPush(Lines* lines, const char* text)
{
    lines->items = realloc(lines->items, sizeof(char*) * (lines->count + 1));
    lines->items[lines->count++] = copyString(text);
}

// The language whose run is recording; the traps call api_record, and the
// runner is single-threaded.
static Lang* Current;

void api_record(const char* format, ...)
{
    if (!Current)
        return;

    char line[TEXT_MAX];
    va_list args;

    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);

    linesPush(&Current->lines, line);
}

static char* readFile(const char* path, s32* size)
{
    FILE* file = fopen(path, "rb");

    if (!file)
        return NULL;

    long length = -1;

    if (fseek(file, 0, SEEK_END) == 0)
        length = ftell(file);

    if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    char* data = malloc(length + 1);
    s32 read = fread(data, 1, length, file);
    data[read] = '\0';
    fclose(file);

    *size = read;
    return data;
}

// The trace text is a table line and the error a note under it: no newlines.
static void copySanitized(char* dst, s32 size, const char* src)
{
    s32 i = 0;

    for (; src[i] && i < size - 1; i++)
        dst[i] = isprint((unsigned char)src[i]) ? src[i] : ' ';

    dst[i] = '\0';
}

// The record carries the trace lines too: trace is an API call like the rest.
static void onTrace(void* data, const char* text, u8 color)
{
    (void)data;
    (void)text;
    (void)color;
}

static void onError(void* data, const char* info)
{
    Lang* lang = data;
    lang->failed = true;
    copySanitized(lang->error, sizeof lang->error, info);
}

static void onExit(void* data)
{
    (void)data;
}

static u64 counter(void* data)
{
    (void)data;
    return 0;
}

static u64 freq(void* data)
{
    (void)data;
    return 1;
}

// One headless tick: init runs the whole probe script, where the probes live.
static void runProbes(Lang* lang, const char* source, s32 size)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);

    if (size > (s32)sizeof(tic->cart.code.data))
    {
        lang->failed = true;
        snprintf(lang->error, sizeof lang->error, "the probe file is bigger than the code section");
    }
    else
    {
        memcpy(tic->cart.code.data, source, size);
        tic->cart.lang = lang->script->id;

        traps_install((tic_core*)tic);
        Current = lang;

        tic_tick_data data =
        {
            .error = onError,
            .trace = onTrace,
            .exit = onExit,
            .counter = counter,
            .freq = freq,
            .data = lang,
        };

        tic_core_tick_start(tic);
        tic_core_tick(tic, &data);
        tic_core_tick_end(tic);
        Current = NULL;
    }

    tic_core_close(tic);
}

// A probe opens its section by tracing "#api/variant"; trace is an API call,
// so the marker reaches the record like every other line.
static const char* markerOf(const char* line)
{
    static char marker[128];

    if (strncmp(line, "trace(\"#", 8) != 0)
        return NULL;

    const char* end = strchr(line + 8, '"');

    if (!end)
        return NULL;

    s32 len = end - (line + 7);

    if (len > (s32)sizeof marker - 1)
        len = sizeof marker - 1;

    memcpy(marker, line + 7, len);
    marker[len] = '\0';

    return marker;
}

static s32 parseSections(const Lines* lines, Section* sections, s32 max)
{
    s32 count = 0;

    for (s32 i = 0; i < lines->count; i++)
    {
        const char* marker = markerOf(lines->items[i]);

        if (!marker)
            continue;

        if (count >= max)
            break;

        Section* section = &sections[count++];
        snprintf(section->marker, sizeof section->marker, "%s", marker);

        const char* start = marker + 1;
        const char* slash = strchr(start, '/');
        s32 len = slash ? (s32)(slash - start) : (s32)strlen(start);

        if (len > (s32)sizeof section->api - 1)
            len = sizeof section->api - 1;

        memcpy(section->api, start, len);
        section->api[len] = '\0';

        s32 end = i + 1;
        while (end < lines->count && !markerOf(lines->items[end]))
            end++;

        section->first = i + 1;
        section->count = end - i - 1;
        i = end - 1;
    }

    return count;
}

static Section* findSection(Section* sections, s32 count, const char* marker)
{
    for (s32 i = 0; i < count; i++)
        if (strcmp(sections[i].marker, marker) == 0)
            return &sections[i];

    return NULL;
}

// "nil" four ways and the numbers: lua prints 14 significant digits, js 17.
static bool isNilToken(const char* token)
{
    return strcmp(token, "nil") == 0 || strcmp(token, "undefined") == 0
        || strcmp(token, "none") == 0 || strcmp(token, "null") == 0;
}

static bool tokenEqual(const char* a, const char* aEnd, const char* b, const char* bEnd)
{
    char ta[128], tb[128];
    s32 alen = aEnd - a;
    s32 blen = bEnd - b;

    if (alen < (s32)sizeof ta && blen < (s32)sizeof tb)
    {
        memcpy(ta, a, alen); ta[alen] = '\0';
        memcpy(tb, b, blen); tb[blen] = '\0';

        if (isNilToken(ta) && isNilToken(tb))
            return true;

        char* ea;
        char* eb;
        double na = strtod(ta, &ea);
        double nb = strtod(tb, &eb);

        if (ea != ta && !*ea && eb != tb && !*eb)
        {
            if (na == nb || (isnan(na) && isnan(nb)))   // equal, infinities and NaNs included
                return true;

            return fabs(na - nb) <= 1e-9 * fmax(1.0, fmax(fabs(na), fabs(nb)));
        }
    }

    return alen == blen && memcmp(a, b, alen) == 0;
}

static bool lineEqual(const char* ref, const char* got)
{
    while (*ref || *got)
    {
        while (isspace((unsigned char)*ref)) ref++;
        while (isspace((unsigned char)*got)) got++;

        if (!*ref || !*got)
            return !*ref && !*got;

        const char* refEnd = ref;
        while (*refEnd && !isspace((unsigned char)*refEnd)) refEnd++;

        const char* gotEnd = got;
        while (*gotEnd && !isspace((unsigned char)*gotEnd)) gotEnd++;

        if (!tokenEqual(ref, refEnd, got, gotEnd))
            return false;

        ref = refEnd;
        got = gotEnd;
    }

    return true;
}

static bool sectionEqual(const Lines* refLines, const Section* ref, const Lines* gotLines, const Section* got)
{
    if (ref->count != got->count)
        return false;

    for (s32 i = 0; i < ref->count; i++)
        if (!lineEqual(refLines->items[ref->first + i], gotLines->items[got->first + i]))
            return false;

    return true;
}

// A markdown table cell: a pipe in a trace line would end the column.
static void printCell(const char* text)
{
    for (; *text; text++)
    {
        if (*text == '|')
            putchar('\\');

        putchar(*text);
    }
}

static void joinLines(const Lines* lines, s32 first, s32 count, char* out, s32 size)
{
    out[0] = '\0';

    for (s32 i = 0; i < count; i++)
    {
        if (i)
            strncat(out, " / ", size - strlen(out) - 1);

        strncat(out, lines->items[first + i], size - strlen(out) - 1);
    }
}

static s32 apiIndex(const char* api)
{
    for (s32 i = 0; i < ApiCount; i++)
        if (strcmp(Apis[i], api) == 0)
            return i;

    Apis = realloc(Apis, sizeof(char*) * (ApiCount + 1));
    Apis[ApiCount] = copyString(api);

    ApiProbes = realloc(ApiProbes, sizeof(s32) * (ApiCount + 1));
    ApiProbes[ApiCount] = 0;

    ApiCells = realloc(ApiCells, sizeof(s32) * (ApiCount + 1) * LangCount);

    for (s32 l = 0; l < LangCount; l++)
        ApiCells[ApiCount * LangCount + l] = 0;

    return ApiCount++;
}

static void addDivergence(s32 lang, s32 api, const char* marker, const char* ref, const char* got)
{
    Divs = realloc(Divs, sizeof(Divergence) * (DivCount + 1));
    Divergence* div = &Divs[DivCount++];

    div->lang = lang;
    snprintf(div->api, sizeof div->api, "%s", marker + 1);
    {
        char* slash = strchr(div->api, '/');
        if (slash) *slash = '\0';
    }
    snprintf(div->marker, sizeof div->marker, "%s", marker);
    snprintf(div->ref, sizeof div->ref, "%s", ref);
    snprintf(div->got, sizeof div->got, "%s", got);

    ApiCells[api * LangCount + lang]++;
}

static bool langMeasured(const Lang* lang)
{
    return lang->hasFile && !lang->failed;
}

int main(int argc, char** argv)
{
    const char* dir = "tests/conformance/probes";
    bool dump = false;

    for (s32 i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--dump") == 0)
            dump = true;
        else
            dir = argv[i];
    }

    FOREACH_LANG(script)
    {
        if (LangCount >= MAX_LANGS)
            break;

        Lang* lang = &Langs[LangCount++];
        lang->script = script;
        snprintf(lang->path, sizeof lang->path, "%s/%s%s", dir, script->name, script->fileExtension);

        s32 size = 0;
        char* source = readFile(lang->path, &size);

        if (!source)
            continue;

        lang->hasFile = true;
        runProbes(lang, source, size);
        free(source);
    }

    if (LangCount == 0)
    {
        fprintf(stderr, "no languages compiled in: build with -DBUILD_STATIC=ON\n");
        return 2;
    }

    Lang* ref = NULL;

    for (s32 i = 0; i < LangCount; i++)
        if (strcmp(Langs[i].script->name, "lua") == 0)
            ref = &Langs[i];

    if (!ref || !langMeasured(ref))
    {
        fprintf(stderr, "no lua reference: %s\n",
            ref && ref->failed ? ref->error : "tests/conformance/probes/lua.lua is missing");
        return 2;
    }

    Section refSections[MAX_SECTIONS];
    s32 refCount = parseSections(&ref->lines, refSections, MAX_SECTIONS);

    if (refCount == 0)
    {
        fprintf(stderr, "the lua reference produced no probes: %s\n", ref->path);
        return 2;
    }

    for (s32 i = 0; i < refCount; i++)
    {
        // Two statements: apiIndex may realloc the table under ApiProbes.
        s32 api = apiIndex(refSections[i].api);
        ApiProbes[api]++;
    }

    for (s32 l = 0; l < LangCount; l++)
    {
        Lang* lang = &Langs[l];

        if (lang == ref || !langMeasured(lang))
            continue;

        Section sections[MAX_SECTIONS];
        s32 count = parseSections(&lang->lines, sections, MAX_SECTIONS);

        for (s32 i = 0; i < refCount; i++)
        {
            Section* refSection = &refSections[i];
            Section* gotSection = findSection(sections, count, refSection->marker);
            s32 api = apiIndex(refSection->api);

            if (gotSection && sectionEqual(&ref->lines, refSection, &lang->lines, gotSection))
                continue;

            char refText[TEXT_MAX];
            char gotText[TEXT_MAX] = "(no probe)";

            joinLines(&ref->lines, refSection->first, refSection->count, refText, sizeof refText);

            if (gotSection)
                joinLines(&lang->lines, gotSection->first, gotSection->count, gotText, sizeof gotText);

            addDivergence(l, api, refSection->marker, refText, gotText);
        }

        // A marker the reference never traced is a probe only this language ran.
        for (s32 i = 0; i < count; i++)
        {
            Section* section = &sections[i];

            if (findSection(refSections, refCount, section->marker))
                continue;

            char gotText[TEXT_MAX];
            joinLines(&lang->lines, section->first, section->count, gotText, sizeof gotText);
            addDivergence(l, apiIndex(section->api), section->marker, "(no probe)", gotText);
        }
    }

    // The table — markdown, so the CI job summary renders it: ✅ where a
    // language matches the reference, 🔴 with a count where it does not.
    {
        s32 probes = refCount;

        printf("API conformance — reference: lua, %d probes, %d apis\n\n", probes, ApiCount);
        printf("| api | probes |");

        for (s32 l = 0; l < LangCount; l++)
            if (langMeasured(&Langs[l]))
                printf(" %s |", Langs[l].script->name);

        printf("\n| --- | --- |");

        for (s32 l = 0; l < LangCount; l++)
            if (langMeasured(&Langs[l]))
                printf(" --- |");

        printf("\n");

        for (s32 i = 0; i < ApiCount; i++)
        {
            // No reference probes: the row is a section only some other
            // language traced, and the reference columns have nothing to say.
            if (ApiProbes[i])
                printf("| %s | %d |", Apis[i], ApiProbes[i]);
            else
                printf("| %s | - |", Apis[i]);

            for (s32 l = 0; l < LangCount; l++)
            {
                if (!langMeasured(&Langs[l]))
                    continue;

                if (&Langs[l] == ref)
                {
                    if (ApiProbes[i])
                        printf(" ✅ |");
                    else
                        printf(" - |");
                }
                else if (ApiCells[i * LangCount + l] == 0)
                    printf(" ✅ |");
                else
                    printf(" 🔴 %d |", ApiCells[i * LangCount + l]);
            }

            printf("\n");
        }

        printf("\n");

        for (s32 l = 0; l < LangCount; l++)
        {
            if (&Langs[l] == ref || !langMeasured(&Langs[l]))
                continue;

            s32 divergences = 0;

            for (s32 i = 0; i < ApiCount; i++)
                divergences += ApiCells[i * LangCount + l];

            // A count, not a ratio: a language-only section is a divergence the
            // reference's probe count does not know about.
            if (divergences)
                printf("- **%s: 🔴 %d divergences**\n", Langs[l].script->name, divergences);
            else
                printf("- **%s: ✅ no divergences**\n", Langs[l].script->name);
        }
    }

    // The divergences in full: one table per language.
    for (s32 l = 0; l < LangCount; l++)
    {
        bool header = false;

        for (s32 i = 0; i < DivCount; i++)
        {
            Divergence* div = &Divs[i];

            if (div->lang != l)
                continue;

            if (!header)
            {
                printf("\n**%s — divergences**\n\n| probe | lua | %s |\n| --- | --- | --- |\n",
                    Langs[l].script->name, Langs[l].script->name);
                header = true;
            }

            printf("| `");
            printCell(div->marker);
            printf("` | `");
            printCell(div->ref);
            printf("` | `");
            printCell(div->got);
            printf("` |\n");
        }
    }

    // --dump: every line a measured language traced, for reading a probe by eye.
    if (dump)
    {
        for (s32 l = 0; l < LangCount; l++)
        {
            if (!langMeasured(&Langs[l]))
                continue;

            printf("\n== %s ==\n", Langs[l].script->name);

            for (s32 i = 0; i < Langs[l].lines.count; i++)
                printf("%s\n", Langs[l].lines.items[i]);
        }
    }

    // And who was not measured at all.
    {
        bool any = false;

        for (s32 l = 0; l < LangCount; l++)
        {
            Lang* lang = &Langs[l];

            if (langMeasured(lang))
                continue;

            printf("%s%s (%s)", any ? ", " : "\nNot measured: ", lang->script->name,
                lang->failed ? lang->error : "no probe file");
            any = true;
        }

        if (any)
            printf("\n");
    }

    // A probe file that did not run is a broken measurement, whatever the
    // rest of the table says.
    for (s32 l = 0; l < LangCount; l++)
        if (Langs[l].hasFile && Langs[l].failed)
            return 2;

    return 0;
}
