/* The console's face, one request at a time: the module links with the core and
 * the modules it names, and the host in miniature below takes what the console
 * asks for and answers it — the part of the console no running app can show.
 *
 * Target console-test in cmake/tests.cmake; `ctest -R console` runs it.
 */
#include "studio/screens/console.h"
#include "studio/fs.h"
#include "core/core.h"
#include "tools.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The platform the console and the modules under it reach for: nothing opens,
// the keyboard types nothing and the clipboard is empty.
void tic_sys_open_path(const char* path)
{
}
bool tic_sys_keyboard_text(char* text)
{
    return false;
}
void tic_sys_clipboard_set(const char* text)
{
}
bool tic_sys_clipboard_has()
{
    return false;
}
char* tic_sys_clipboard_get()
{
    return NULL;
}
void tic_sys_clipboard_free(const char* text)
{
}

typedef struct
{
    tic_mem* tic;
    tic_fs* fs;
    Console* console;
    ConsoleView view;
    StudioConfig config;
    MouseState mouse[3];
    char* cmd;

} Host;

// A machine, a filesystem pointing at nothing, a config that prints nothing,
// and the console made out of them — everything a host gives a module.
static Host* hostNew(const char* cmd)
{
    Host* host = calloc(1, sizeof(Host));

    host->tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    host->fs = tic_fs_create("/tmp/", NULL);
    host->view.inFront = true;
    host->cmd = cmd ? strdup(cmd) : NULL;

    host->console = console_create(&(ConsoleDeps){
        .tic = host->tic,
        .fs = host->fs,
        .config = &host->config,
        .mouse = host->mouse,
        .view = &host->view,

        .args = {.cmd = host->cmd},

        .boot = false,
    });

    return host;
}

static void hostFree(Host* host)
{
    console_free(host->console);
    tic_core_close(host->tic);
    free(host->cmd);
    free(host);
}

// A frame of the console, and what it asked for: taken and left unanswered, so
// the caller answers for its kind, the way the studio's serving loop does.
static ConsoleAsk hostTick(Host* host)
{
    ConsoleAsk ask = {0};

    console_tick(host->console);

    if(!console_take(host->console, &ask))
        ask.kind = CONSOLE_ASK_NONE;

    return ask;
}

static ConsoleAsk hostTake(Host* host)
{
    ConsoleAsk ask = {0};

    assert(console_take(host->console, &ask));

    return ask;
}

static void hostAnswer(Host* host, ConsoleAnswer answer)
{
    console_answer(host->console, &answer);
}

// Whether the console has put `text` in its buffer. The buffer is a screen of
// rows whose first row is empty, so it is searched as flat bytes, not a string.
static bool said(Host* host, const char* text)
{
    enum
    {
        Row = TIC80_WIDTH / TIC_FONT_WIDTH,
        Rows = TIC80_HEIGHT / (TIC_FONT_HEIGHT + 1),
    };

    const s32 size = Row * Rows * 4; // the test's messages stay in the first screens
    const s32 len = (s32)strlen(text);

    for(s32 i = 0; i + len <= size; i++)
        if(memcmp(host->console->text + i, text, len) == 0)
            return true;

    return false;
}

// A cart on the filesystem the console reads: the test's own, so a load has
// something real to find.
static void putCart(Host* host, const char* name)
{
    // A cart that is not all zeroes, so there is something to save at all.
    tic_tool_poke4(host->tic->cart.bank0.tiles.data[0].data, 0, 42);

    u8 buffer[TIC_CART_SAVE_SIZE];
    s32 size = tic_cart_save(&host->tic->cart, buffer);

    assert(size > 0);
    assert(tic_fs_save(host->fs, name, buffer, size, true));
}

// The first frame loads the language's demo cart, so the cart's name is the
// first thing the console asks for; a move is what it reports and the host
// performs, and nothing of the command is over until the answer comes.
static void testMoves(void)
{
    Host* host = hostNew("run & resume & cls & exit");
    ConsoleAsk ask;

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    assert(ask.clear);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_RUN);
    assert(console_active(host->console));
    hostAnswer(host, (ConsoleAnswer){0});

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_RESUME);
    hostAnswer(host, (ConsoleAnswer){0});

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_NONE); // cls prints, asks for nothing

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_EXIT);
    hostAnswer(host, (ConsoleAnswer){0});

    hostFree(host);
}

// A command that would throw the cart away asks first, and the dialog is the
// host's to show: the console reports the rows and what a "yes" continues.
static void testConfirmNo(void)
{
    Host* host = hostNew("new");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    host->view.cartChanged = true;

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CONFIRM);
    assert(ask.rowsCount == 3);

    hostAnswer(host, (ConsoleAnswer){.yes = false});

    assert(console_active(host->console));
    assert(!said(host, "new cart has been created"));

    hostFree(host);
}

// Answered yes, the console's own flow continues — and what it needs from the
// host next is another ask, raised from inside the answer: the loaded cart's
// name, which only the answer can bring back.
static void testLoad(void)
{
    Host* host = hostNew("load smoke.tic");

    putCart(host, "smoke.tic");

    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    host->view.cartChanged = true;

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CONFIRM);
    hostAnswer(host, (ConsoleAnswer){.yes = true});

    ask = hostTake(host);
    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    assert(strcmp(ask.name, "smoke.tic") == 0);
    hostAnswer(host, (ConsoleAnswer){.name = "smoke.tic"});

    assert(said(host, "cart smoke.tic loaded!"));

    tic_fs_delfile(host->fs, "smoke.tic");

    hostFree(host);
}

// The cart the console saves is the host's to write; the answer carries what
// the console prints, and the host's other way in is its functions.
static void testSave(void)
{
    Host* host = hostNew("save smoke");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CART_SAVE);
    assert(strcmp(ask.name, "smoke") == 0);

    hostAnswer(host, (ConsoleAnswer){.save = CART_SAVE_OK, .name = "smoke.tic"});
    assert(said(host, "cart smoke.tic saved!"));

    console_trace(host->console, "traced", tic_color_white);
    assert(said(host, "traced"));

    console_error(host->console, "gone wrong");
    assert(said(host, "gone wrong"));

    console_done(host->console);
    assert(console_active(host->console));

    hostFree(host);
}

// The --cmd batch: when the launch keeps its commands and every one of them has
// run, the host is told to show the console again for the next run.
static void testReplay(void)
{
    Host* host = hostNew("run");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_RUN);
    hostAnswer(host, (ConsoleAnswer){0});

    host->console->args.keepcmd = true;
    assert(console_replay(host->console));

    host->console->args.keepcmd = false;
    assert(!console_replay(host->console));

    hostFree(host);
}

// A batch's own run must not count itself before the host has run the cart:
// runGame reads that count the moment it is asked, for the rewind --keepcmd
// does, and a command already counted there would rewind instead of running.
static void testRunCountsOnAnswer(void)
{
    Host* host = hostNew("cls & run");
    ConsoleAsk ask = hostTick(host);

    host->console->args.keepcmd = true;

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    ask = hostTick(host); // cls asks nothing, so it counts here
    assert(ask.kind == CONSOLE_ASK_NONE);
    assert(host->console->commands.current == 1);

    ask = hostTick(host); // run
    assert(ask.kind == CONSOLE_ASK_RUN);
    assert(host->console->commands.current == 1);
    assert(!console_replay(host->console)); // the host may start the cart

    hostAnswer(host, (ConsoleAnswer){0}); // ... and it counts now
    assert(host->console->commands.current == 2);
    assert(!host->console->counting);

    hostFree(host);
}

// The export's answer is the host's word for the file it wrote, and the
// console's message is the one that follows from it.
static void testExport(bool written)
{
    Host* host = hostNew("export sfx smoke.wav id=1");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_EXPORT_SFX);
    assert(strcmp(ask.name, "smoke.wav") == 0);
    assert(ask.index == 1);

    hostAnswer(host, (ConsoleAnswer){.ok = written});

    assert(written ? said(host, "smoke.wav exported :)") : said(host, "not exported"));

    hostFree(host);
}

int main(void)
{
    testMoves();
    testConfirmNo();
    testLoad();
    testSave();
    testReplay();
    testRunCountsOnAnswer();
    testExport(true);
    testExport(false);

    puts("moves ok / confirm ok / load ok / save ok / replay ok / run / export ok");

    return 0;
}
