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

// The test's network, where net.c's own no-network shape would be: a request
// is held until the test answers it, so a command that waits on one is seen
// both waiting and answered. No socket is opened here either.
static net_get_callback netCallback;
static void* netCalldata;
static char netUrl[TICNAME_MAX];

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    (void)net;
    assert(!netCallback);

    netCallback = callback;
    netCalldata = calldata;
    snprintf(netUrl, sizeof netUrl, "%s", url);
}

static void answerNet(net_get_data answer)
{
    assert(netCallback);

    answer.calldata = netCalldata;
    answer.url = netUrl;

    netCallback(&answer);
}

// The request is over: the callback goes before it runs, so a request this
// answer starts — the export page after its stub — is the one left pending.
static void answerNetLast(net_get_data answer)
{
    assert(netCallback);

    net_get_callback callback = netCallback;
    netCallback = NULL;

    answer.calldata = netCalldata;
    answer.url = netUrl;

    callback(&answer);
}

static void answerNetProgress(s32 size, s32 total)
{
    answerNet((net_get_data){.type = net_get_progress, .progress = {.size = size, .total = total}});
}

static void answerNetDone(const void* data, s32 size)
{
    answerNetLast((net_get_data){.type = net_get_done, .done = {.size = size, .data = (u8*)data}});
}

static void answerNetError(void)
{
    answerNetLast((net_get_data){.type = net_get_error, .error = {.code = 404}});
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

enum
{
    ConsoleRow = TIC80_WIDTH / TIC_FONT_WIDTH,
    ConsoleRows = TIC80_HEIGHT / (TIC_FONT_HEIGHT + 1),
    ConsoleSpan = ConsoleRow * ConsoleRows * 4, // the test's messages stay in the first screens
};

// Whether the console has put `text` in its buffer. The buffer is a screen of
// rows whose first row is empty, so it is searched as flat bytes, not a string.
static bool said(Host* host, const char* text)
{
    const s32 len = (s32)strlen(text);

    for(s32 i = 0; i + len <= ConsoleSpan; i++)
        if(memcmp(host->console->text + i, text, len) == 0)
            return true;

    return false;
}

// The console's non-blank rows from the one holding `from`, trailing blanks
// trimmed: the screen the user sees there, without the banner above it.
static void screenFrom(Host* host, const char* from, char* out, size_t size)
{
    const char* buffer = host->console->text;
    const s32 len = (s32)strlen(from);
    s32 offset = -1;
    s32 last = -1;

    for(s32 i = 0; i + len <= ConsoleSpan; i++)
        if(memcmp(buffer + i, from, len) == 0)
        {
            offset = i - i % ConsoleRow;
            break;
        }

    assert(offset >= 0);

    for(s32 i = 0; i < ConsoleSpan; i += ConsoleRow)
        for(s32 n = 0; n < ConsoleRow; n++)
            if(buffer[i + n] && buffer[i + n] != ' ')
            {
                last = i;
                break;
            }

    assert(last >= offset);

    char* dst = out;

    for(const char* row = buffer + offset; row <= buffer + last; row += ConsoleRow)
    {
        s32 n = ConsoleRow;

        while(n && (row[n - 1] == '\0' || row[n - 1] == ' '))
            n--;

        assert(dst + n + 1 < out + size);

        memcpy(dst, row, n);
        dst += n;
        *dst++ = '\n';
    }

    *dst = '\0';
}

// The prompt is where the input starts: nothing is printed after it.
static bool promptAtInput(Host* host)
{
    return host->console->input.text > host->console->text &&
           host->console->input.text[-1] == '>';
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

    // The prompt follows the message, not the command: the load ends on the
    // host's answer, and nothing is printed after the input's `>`.
    char screen[1024];
    screenFrom(host, ">load smoke.tic", screen, sizeof screen);
    assert(strcmp(screen, ">load smoke.tic\n"
                          "cart smoke.tic loaded!\n"
                          "use RUN command to run it\n"
                          "\n"
                          ">\n") == 0);
    assert(promptAtInput(host));

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

    char screen[1024];
    screenFrom(host, ">save smoke", screen, sizeof screen);
    assert(strcmp(screen, ">save smoke\ncart smoke.tic saved!\n\n>\n") == 0);
    assert(promptAtInput(host));

    console_trace(host->console, "traced", tic_color_white);
    assert(said(host, "traced"));

    console_error(host->console, "gone wrong");
    assert(said(host, "gone wrong"));

    console_done(host->console);
    assert(console_active(host->console));

    hostFree(host);
}

// A config's demo cart is loaded through the same answer as `load`: the message
// and the prompt wait for the host's name, and neither comes before it.
static void testConfigDefault(void)
{
    Host* host = hostNew("config default");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CART_LOADED);

    char name[TICNAME_MAX];
    strcpy(name, ask.name);
    hostAnswer(host, (ConsoleAnswer){.name = name});

    assert(said(host, "loaded!"));

    // The demo's name can wrap the message, so the screen is checked by its
    // ends: the command first, the prompt alone at the end.
    char screen[1024];
    screenFrom(host, ">config default", screen, sizeof screen);
    assert(strncmp(screen, ">config default\ncart ", strlen(">config default\ncart ")) == 0);
    assert(strcmp(screen + strlen(screen) - strlen("\n>\n"), "\n>\n") == 0);
    assert(promptAtInput(host));

    hostFree(host);
}

// A native export's answer arrives on the network, after the command's own
// line is over: the result is one line under it, and then the prompt.
static void testExportNative(void)
{
    Host* host = hostNew("export linuxarm smoke");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    hostTick(host); // the command goes out and its request becomes the test's
    assert(netCallback);
    assert(strcmp(netUrl, "/export/" TIC_VERSION_DIR "/linuxarm") == 0);

    const u8 stub[] = "stub";
    answerNetDone(stub, sizeof stub - 1);

    char screen[1024];
    screenFrom(host, ">export linuxarm smoke", screen, sizeof screen);
    assert(strcmp(screen, ">export linuxarm smoke\nsmoke exported :)\n>\n") == 0);
    assert(promptAtInput(host));

    tic_fs_delfile(host->fs, "smoke");

    hostFree(host);
}

// The fetch reports every chunk; the console redraws one GET line instead of
// appending one per report, and the result takes its place.
static void testExportProgress(void)
{
    Host* host = hostNew("export linuxarm smoke");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    hostTick(host);
    assert(netCallback);

    answerNetProgress(10, 100);
    answerNetProgress(100, 100);

    assert(!said(host, "[10%]"));
    assert(said(host, "[100%]"));

    const u8 stub[] = "stub";
    answerNetDone(stub, sizeof stub - 1);

    char expected[TICNAME_MAX * 2];
    sprintf(expected, ">export linuxarm smoke\nGET %s [100%%]\nsmoke exported :)\n>\n", netUrl);

    char screen[1024];
    screenFrom(host, ">export linuxarm smoke", screen, sizeof screen);
    assert(strcmp(screen, expected) == 0);
    assert(promptAtInput(host));

    tic_fs_delfile(host->fs, "smoke");

    hostFree(host);
}

// An error after a progress report gets its own line, not the tail of the GET
// line, and the prompt still comes last.
static void testExportError(void)
{
    Host* host = hostNew("export linuxarm smoke");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    hostTick(host);
    assert(netCallback);

    answerNetProgress(50, 100);
    answerNetError();

    char expected[TICNAME_MAX * 2];
    sprintf(expected, ">export linuxarm smoke\nGET %s [50%%]\nfile downloading error :(\n>\n", netUrl);

    char screen[1024];
    screenFrom(host, ">export linuxarm smoke", screen, sizeof screen);
    assert(strcmp(screen, expected) == 0);
    assert(promptAtInput(host));

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

// An alias names the same command, so help on it shows that command's page.
static void testHelpAlias(void)
{
    Host* host = hostNew("help ls");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    hostTick(host);

    assert(said(host, "---=== COMMAND ===---"));
    assert(!said(host, "unknown topic"));

    hostFree(host);
}

int main(void)
{
    testMoves();
    testConfirmNo();
    testLoad();
    testSave();
    testConfigDefault();
    testExportNative();
    testExportProgress();
    testExportError();
    testReplay();
    testRunCountsOnAnswer();
    testExport(true);
    testExport(false);
    testHelpAlias();

    puts("moves ok / confirm ok / load ok / save ok / config ok / export ok / replay ok / run");

    return 0;
}
