/* The console's face, one request at a time: the module links with the core and
 * the modules it names, and the host in miniature below takes what the console
 * asks for and answers it — the part of the console no running app can show.
 *
 * Target console-test in cmake/tests.cmake; `ctest -R console` runs it.
 */
#include "studio/screens/console.h"
#include "studio/account.h"
#include "studio/fs.h"
#include "studio/fs_internal.h"
#include "core/core.h"
#include "tools.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// The platform the console and the modules under it reach for: nothing opens,
// the keyboard types nothing and the clipboard is empty.
void tic_sys_open_path(const char* path)
{
}
void tic_sys_open_url(const char* url)
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

// The shelf's requests go through the primitive: one held request, answered
// by the test, so a pairing or a shelf command is seen both waiting and
// answered.
static net_request_callback requestCallback;
static void* requestCalldata;
static net_method requestMethod;
static char requestUrl[TICNAME_MAX * 2];
static char requestAuthorization[128];

static bool requestHeld(void)
{
    return requestCallback != NULL;
}

void tic_net_request(tic_net* net, net_method method, const char* url, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    (void)net;
    (void)body;
    (void)bodySize;
    assert(!requestCallback);

    requestCallback = callback;
    requestCalldata = calldata;
    requestMethod = method;
    requestAuthorization[0] = '\0';
    snprintf(requestUrl, sizeof requestUrl, "%s", url);

    for(s32 i = 0; i < headersCount; i++)
        if(strcmp(headers[i].name, "Authorization") == 0)
            snprintf(requestAuthorization, sizeof requestAuthorization, "%s", headers[i].value);
}

static void answerRequestBytes(s32 status, const u8* body, s32 size)
{
    assert(requestCallback);

    net_request_callback callback = requestCallback;
    void* calldata = requestCalldata;
    requestCallback = NULL;

    net_request_data answer = {
        .type = net_request_done,
        .done = {.status = status, .size = size, .data = (u8*)body},
        .calldata = calldata,
        .url = requestUrl,
    };

    callback(&answer);
}

static void answerRequest(s32 status, const char* json)
{
    answerRequestBytes(status, (const u8*)json, (s32)strlen(json));
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
    tic_account* account;
    Console* console;
    ConsoleView view;
    StudioConfig config;
    MouseState mouse[3];
    char* cmd;

} Host;

// A machine, a filesystem at the root the test names, an account over it, a
// config that prints nothing, and the console made out of them.
static Host* hostNewAt(const char* root, const char* cmd)
{
    Host* host = calloc(1, sizeof(Host));

    host->tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    host->fs = tic_fs_create(root, NULL);
    host->account = tic_account_create(host->fs, NULL);
    host->view.inFront = true;
    host->cmd = cmd ? strdup(cmd) : NULL;

    host->console = console_create(&(ConsoleDeps){
        .tic = host->tic,
        .fs = host->fs,
        .account = host->account,
        .config = &host->config,
        .mouse = host->mouse,
        .view = &host->view,

        .args = {.cmd = host->cmd},

        .boot = false,
    });

    return host;
}

// A machine, a filesystem pointing at nothing, a config that prints nothing,
// and the console made out of them — everything a host gives a module.
static Host* hostNew(const char* cmd)
{
    return hostNewAt("/tmp/", cmd);
}

static void hostFree(Host* host)
{
    console_free(host->console);
    tic_account_close(host->account);
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

static void onSaved(const char* name, fs_result result, void* data)
{
    (void)name;
    *(bool*)data = result.code == FS_OK;
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

    bool saved = false;
    tic_fs_save(host->fs, name, buffer, size, onSaved, &saved);
    assert(saved);
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

    hostAnswer(host, (ConsoleAnswer){.result = {FS_OK, NULL}, .name = "smoke.tic"});
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

// The fill help is one topic: the API pages point at it, and the document
// carries it once instead of in every primitive's row.
static void testHelpFill(void)
{
    Host* host = hostNew("help fill");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    hostTick(host);

    assert(said(host, "Fill patterns:"));
    assert(said(host, "Ordered-dither"));
    assert(said(host, "0x0001"));
    assert(said(host, "0xEFFF"));
    assert(!said(host, "$FILL_PATTERNS$"));

    hostFree(host);

    host = hostNew("help rect");
    ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    hostTick(host);

    assert(said(host, "`help fill`"));
    assert(!said(host, "$FILL_PATTERNS$"));

    hostFree(host);
}

static void testExportHelp(void)
{
    Host* host = hostNew("export help smoke.md");
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    hostTick(host);

    s32 size = 0;
    char* raw = fs_read(tic_fs_path(host->fs, "smoke.md"), &size);
    assert(raw);

    char* text = malloc(size + 1);
    memcpy(text, raw, size);
    text[size] = '\0';
    free(raw);

    s32 sections = 0, paragraphs = 0, refs = 0, tables = 0;

    for(const char* at = text; (at = strstr(at, "## Fill patterns")); at++)
        sections++;
    for(const char* at = text; (at = strstr(at, "The color may pack a fill pattern")); at++)
        paragraphs++;
    for(const char* at = text; (at = strstr(at, "see [Fill patterns](#fill-patterns)")); at++)
        refs++;
    for(const char* at = text; (at = strstr(at, "Ordered-dither masks by coverage:")); at++)
        tables++;

    assert(sections == 1);
    assert(paragraphs == 1);
    assert(refs == 12);
    assert(tables == 1);
    assert(strstr(text, "masks by coverage:\n\n| Coverage | Mask |"));
    assert(strstr(text, "| 1/16 | `0x0001` |"));
    assert(strstr(text, "| 15/16 | `0xEFFF` |"));
    assert(!strstr(text, "$FILL_PATTERNS$"));

    free(text);
    tic_fs_delfile(host->fs, "smoke.md");

    hostFree(host);
}

// The console asks the current script where an error sits: this one places
// "line 8" and nothing else. A NULL hook is a language that cannot place them.
static s32 placedErrorLine(tic_mem* memory, const char* text)
{
    (void)memory;
    return strstr(text, "line 8") ? 8 : 0;
}

static const tic_script PlacedScript =
    {
        .id = 200,
        .name = "placed",
        .fileExtension = ".placed",
        .singleComment = "--",
        .errorLine = placedErrorLine,
};

static const tic_script UnplacedScript =
    {
        .id = 201,
        .name = "unplaced",
        .fileExtension = ".unplaced",
        .singleComment = "--",
};

// A pointer point in the console's own coordinates, as the studio ticks them.
static void mouseAt(Host* host, tic_point point, bool click)
{
    host->tic->ram->input.mouse.x = point.x + TIC80_OFFSET_LEFT;
    host->tic->ram->input.mouse.y = point.y + TIC80_OFFSET_TOP;
    host->mouse[tic_mouse_left].click = click;
    host->mouse[tic_mouse_left].start = point;
    host->mouse[tic_mouse_left].end = point;
}

// A placed error is a target: a click inside its block asks for the code
// editor at the line; a click outside, a drag and an unplaced error do not.
static void testErrorLink(void)
{
    Host* host = hostNew(NULL);

    tic_add_script(&PlacedScript);
    tic_add_script(&UnplacedScript);
    host->tic->cart.lang = PlacedScript.id;

    ConsoleAsk ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    // The first frame's demo load zeroes the cart, its language included: the
    // test's script is the one that answers the error.
    host->tic->cart.lang = PlacedScript.id;

    console_error(host->console, "line 8: boom");
    assert(host->console->errors.count == 1);
    assert(host->console->errors.line == 8);
    assert(console_error_line(host->console) == 8);

    ConsoleErrorLink link = host->console->errors.items[0];

    assert(link.line == 8);
    assert(link.size > 0);

    {
        s32 row = link.block / ConsoleRow;
        host->console->scroll.pos = row > 2 ? row - 2 : 0;

        tic_point inside =
            {
                (link.block % ConsoleRow) * TIC_FONT_WIDTH,
                (row - host->console->scroll.pos) * (TIC_FONT_HEIGHT + 1),
            };

        mouseAt(host, inside, true);
        ask = hostTick(host);
        assert(ask.kind == CONSOLE_ASK_CODE);
        assert(ask.line == 8);
        hostAnswer(host, (ConsoleAnswer){0});

        // The whole block is the target, its last cell included.
        {
            s32 last = link.block + link.size - 1;
            s32 endRow = last / ConsoleRow;
            tic_point end =
                {
                    (last % ConsoleRow) * TIC_FONT_WIDTH,
                    (endRow - host->console->scroll.pos) * (TIC_FONT_HEIGHT + 1),
                };

            mouseAt(host, end, true);
            ask = hostTick(host);
            assert(ask.kind == CONSOLE_ASK_CODE);
            assert(ask.line == 8);
            hostAnswer(host, (ConsoleAnswer){0});
        }

        // A click outside the block asks nothing.
        mouseAt(host, (tic_point){0, 0}, true);
        ask = hostTick(host);
        assert(ask.kind == CONSOLE_ASK_NONE);

        // A drag inside the block is a selection, not a click.
        host->mouse[tic_mouse_left].start = inside;
        host->mouse[tic_mouse_left].end = (tic_point){inside.x + 4, inside.y};
        host->mouse[tic_mouse_left].click = true;
        host->tic->ram->input.mouse.x = inside.x + 4 + TIC80_OFFSET_LEFT;
        host->tic->ram->input.mouse.y = inside.y + TIC80_OFFSET_TOP;
        ask = hostTick(host);
        assert(ask.kind == CONSOLE_ASK_NONE);

        // A drag from inside to outside is a selection, not a click.
        host->mouse[tic_mouse_left].start = inside;
        host->mouse[tic_mouse_left].end = (tic_point){0, 0};
        host->mouse[tic_mouse_left].click = true;
        host->tic->ram->input.mouse.x = TIC80_OFFSET_LEFT;
        host->tic->ram->input.mouse.y = TIC80_OFFSET_TOP;
        ask = hostTick(host);
        assert(ask.kind == CONSOLE_ASK_NONE);

        // A pending ask keeps the click away from the code editor.
        host->console->ask = (ConsoleAsk){.kind = CONSOLE_ASK_MENU};
        mouseAt(host, inside, true);
        ask = hostTick(host);
        assert(ask.kind == CONSOLE_ASK_MENU);
        hostAnswer(host, (ConsoleAnswer){0});

        // An ask being served does too.
        host->console->waiting = true;
        mouseAt(host, inside, true);
        ask = hostTick(host);
        assert(ask.kind == CONSOLE_ASK_NONE);
        host->console->waiting = false;

        // The menubar's pointer never jumps.
        host->view.pointerOwned = true;
        mouseAt(host, inside, true);
        ask = hostTick(host);
        assert(ask.kind == CONSOLE_ASK_NONE);
        host->view.pointerOwned = false;
    }

    // An error the binding cannot place clears the line and adds no link.
    console_error(host->console, "nothing here");
    assert(host->console->errors.count == 1);
    assert(host->console->errors.line == 0);
    assert(console_error_line(host->console) == 0);

    console_clear_errors(host->console);
    assert(host->console->errors.count == 0);

    hostFree(host);

    // A language without the hook offers no target at all.
    host = hostNew(NULL);
    host->tic->cart.lang = UnplacedScript.id;

    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});

    host->tic->cart.lang = UnplacedScript.id;

    console_error(host->console, "line 8: boom");
    assert(host->console->errors.count == 0);
    assert(console_error_line(host->console) == 0);

    mouseAt(host, (tic_point){10, 10}, true);
    ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_NONE);

    hostFree(host);
}

// The console's file commands over a fake filesystem: each operation is a
// scripted answer, held until the test pumps it (or answered inline).
typedef enum
{
    FakeList,
    FakeLoad,
    FakeStat,
    FakeDelete,
    FakeMkdir,
    FakeOpen,
    FakeSave,
} FakeKind;

typedef struct FakeOp
{
    struct FakeOp* next;
    FakeKind kind;
    char path[TICNAME_MAX];

    fs_list_item_cb onItem;
    fs_list_done_cb onListDone;
    fs_load_done_cb onLoadDone;
    fs_stat_done_cb onStatDone;
    fs_done_cb onDone;
    fs_save_done_cb onSaveDone;
    void* data;
} FakeOp;

static struct
{
    FakeOp* head;
    FakeOp* tail;
    s32 pending;
    bool inlineAnswers;

    fs_result result;
    fs_item item;
    char itemName[TICNAME_MAX];
    char loadName[TICNAME_MAX];
    const u8* bytes;
    s32 size;
    char stored[TICNAME_MAX];
    fs_item items[8];
    char names[8][TICNAME_MAX];
    s32 count;
    fs_usage usage;
    bool hasUsage;
} Fake;

static void fakeReset(void)
{
    assert(Fake.head == NULL);
    memset(&Fake, 0, sizeof Fake);
}

static void fakeList(fs_result result, const char* const* names, const bool* dirs, s32 count)
{
    Fake.result = result;
    Fake.count = count;

    for(s32 i = 0; i < count && i < (s32)COUNT_OF(Fake.items); i++)
    {
        snprintf(Fake.names[i], sizeof Fake.names[i], "%s", names[i]);
        Fake.items[i] = (fs_item){Fake.names[i], NULL, dirs[i], -1, 0, 0};
    }
}

static void fakeUsage(s32 count, s32 maxCount, s64 bytes, s64 maxBytes)
{
    Fake.hasUsage = true;
    Fake.usage = (fs_usage){true, count, maxCount, bytes, maxBytes};
}

static void fakeStatResult(fs_result result, const char* name, bool dir)
{
    Fake.result = result;
    snprintf(Fake.itemName, sizeof Fake.itemName, "%s", name);
    Fake.item = (fs_item){Fake.itemName, NULL, dir, -1, 0, 0};
}

static void fakeLoadResult(fs_result result, const char* name, const u8* bytes, s32 size)
{
    Fake.result = result;
    snprintf(Fake.loadName, sizeof Fake.loadName, "%s", name);
    Fake.bytes = bytes;
    Fake.size = size;
}

static void fakeDone(fs_result result)
{
    Fake.result = result;
}

static FakeOp* fakePush(FakeKind kind, const char* path, void* data)
{
    FakeOp* op = calloc(1, sizeof(FakeOp));
    assert(op);

    op->kind = kind;
    snprintf(op->path, sizeof op->path, "%s", path);
    op->data = data;

    if(Fake.tail)
        Fake.tail->next = op;
    else
        Fake.head = op;

    Fake.tail = op;
    Fake.pending++;

    return op;
}

static void fakeServe(FakeOp* op)
{
    fs_result result = Fake.result;

    switch(op->kind)
    {
        case FakeList:
            if(result.code == FS_OK)
                for(s32 i = 0; i < Fake.count; i++)
                    if(!op->onItem(&Fake.items[i], op->data))
                        break;

            op->onListDone(Fake.hasUsage ? &Fake.usage : NULL, result, op->data);
            break;

        case FakeLoad:
            op->onLoadDone(Fake.loadName, Fake.bytes, Fake.size, result, op->data);
            break;

        case FakeStat:
            op->onStatDone(result.code == FS_OK ? &Fake.item : NULL, result, op->data);
            break;

        case FakeDelete:
        case FakeMkdir:
        case FakeOpen:
            op->onDone(result, op->data);
            break;

        case FakeSave:
            op->onSaveDone(Fake.stored, result, op->data);
            break;
    }
}

static bool fakePump(void)
{
    FakeOp* op = Fake.head;

    if(!op)
        return false;

    Fake.head = op->next;

    if(!Fake.head)
        Fake.tail = NULL;

    Fake.pending--;
    fakeServe(op);

    free(op);
    return true;
}

static void fakeOpDone(FakeKind kind, void* data, fs_done_cb done)
{
    fakePush(kind, "", data)->onDone = done;

    if(Fake.inlineAnswers)
        fakePump();
}

static void fakeListOp(tic_fs* fs, const char* path, fs_list_item_cb onItem, fs_list_done_cb onDone, void* data)
{
    (void)fs;
    FakeOp* op = fakePush(FakeList, path, data);
    op->onItem = onItem;
    op->onListDone = onDone;

    if(Fake.inlineAnswers)
        fakePump();
}

static void fakeLoadOp(tic_fs* fs, const char* path, fs_load_done_cb onDone, void* data)
{
    (void)fs;
    fakePush(FakeLoad, path, data)->onLoadDone = onDone;

    if(Fake.inlineAnswers)
        fakePump();
}

static void fakeSaveOp(tic_fs* fs, const char* path, const void* bytes, s32 size, fs_save_done_cb onDone, void* data)
{
    (void)fs;
    (void)bytes;
    (void)size;
    fakePush(FakeSave, path, data)->onSaveDone = onDone;

    if(Fake.inlineAnswers)
        fakePump();
}

static void fakeDeleteOp(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    fakeOpDone(FakeDelete, data, onDone);
    (void)path;
}

static void fakeStatOp(tic_fs* fs, const char* path, fs_stat_done_cb onDone, void* data)
{
    (void)fs;
    fakePush(FakeStat, path, data)->onStatDone = onDone;

    if(Fake.inlineAnswers)
        fakePump();
}

static void fakeMkdirOp(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    fakeOpDone(FakeMkdir, data, onDone);
    (void)path;
}

static void fakeOpenOp(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    fakeOpDone(FakeOpen, data, onDone);
    (void)path;
}

static void fakeCoverOp(tic_fs* fs, const char* path, fs_cover_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;
    onDone(NULL, 0, (fs_result){FS_NOT_FOUND, "no cover"}, data);
}

static const fs_mount_ops FakeMount = {
    .list = fakeListOp,
    .load = fakeLoadOp,
    .save = fakeSaveOp,
    .delete = fakeDeleteOp,
    .stat = fakeStatOp,
    .mkdir = fakeMkdirOp,
    .openfolder = fakeOpenOp,
    .cover = fakeCoverOp,
};

static Host* fakeHost(const char* cmd, bool inlineAnswers)
{
    Host* host = hostNew(cmd);

    fakeReset();
    Fake.inlineAnswers = inlineAnswers;
    tic_fs_set_mount(host->fs, &FakeMount);

    return host;
}

// The first frame loads the language's demo cart and asks for its name.
static void firstFrame(Host* host)
{
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});
}

// `dir` prints the items, the mount's optional usage line and the prompt, the
// same code path for a local or a remote listing.
static void testFakeDir(void)
{
    static const char* names[] = {"sub", "cart.tic"};
    static const bool dirs[] = {true, false};

    {
        Host* host = fakeHost("dir", false);
        firstFrame(host);

        hostTick(host);
        assert(Fake.pending == 1);

        fakeList((fs_result){FS_OK, NULL}, names, dirs, 2);
        fakeUsage(2, 10, 100, 1000);
        assert(fakePump());

        assert(said(host, "[sub]"));
        assert(said(host, "cart.tic"));
        assert(said(host, "2/10 carts, 100 bytes/1000 bytes"));
        assert(promptAtInput(host));

        hostFree(host);
    }

    {
        // A mount with no usage numbers leaves the line out.
        Host* host = fakeHost("dir", false);
        firstFrame(host);

        hostTick(host);
        fakeList((fs_result){FS_OK, NULL}, names, dirs, 2);
        assert(fakePump());

        assert(said(host, "[sub]"));
        assert(said(host, "cart.tic"));
        assert(!said(host, "carts,"));
        assert(promptAtInput(host));

        hostFree(host);
    }
}

static void testFakeCd(void)
{
    Host* host = fakeHost("cd sub & cd nope & cd cart.tic", false);
    firstFrame(host);

    hostTick(host);
    fakeStatResult((fs_result){FS_OK, NULL}, "sub", true);
    assert(fakePump());
    assert(said(host, "sub>"));

    hostTick(host);
    fakeStatResult((fs_result){FS_NOT_FOUND, "not found"}, "nope", false);
    assert(fakePump());
    assert(said(host, "not found"));

    hostTick(host);
    fakeStatResult((fs_result){FS_OK, NULL}, "cart.tic", false);
    assert(fakePump());
    assert(said(host, "not a folder"));

    hostFree(host);
}

// A refused delete shows the mount's message and changes nothing.
static void testFakeDelRefused(void)
{
    Host* host = fakeHost("del cart.tic", false);
    firstFrame(host);

    ConsoleAsk ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CONFIRM);
    hostAnswer(host, (ConsoleAnswer){.yes = true});

    fakeStatResult((fs_result){FS_OK, NULL}, "cart.tic", false);
    assert(fakePump());
    assert(Fake.pending == 1);

    fakeDone((fs_result){FS_DENIED, "access denied"});
    assert(fakePump());

    assert(said(host, "access denied"));
    assert(promptAtInput(host));

    hostFree(host);
}

static void testFakeMkdir(void)
{
    Host* host = fakeHost("mkdir sub & mkdir sub", false);
    firstFrame(host);

    hostTick(host);
    fakeDone((fs_result){FS_OK, NULL});
    assert(fakePump());
    assert(said(host, "created [sub] folder :)"));

    hostTick(host);
    fakeDone((fs_result){FS_IO, "the folder exists"});
    assert(fakePump());
    assert(said(host, "the folder exists"));

    hostFree(host);
}

static void testFakeFolder(void)
{
    Host* host = fakeHost("folder", false);
    firstFrame(host);

    hostTick(host);
    fakeDone((fs_result){FS_UNSUPPORTED, "not supported on the site"});
    assert(fakePump());

    assert(said(host, "Storage path:"));
    assert(said(host, "not supported on the site"));

    hostFree(host);
}

// One outstanding filesystem operation per command: the next command waits for
// the answer, and a pending save keeps a file command from starting.
static void testFakePending(void)
{
    static const char* names[] = {"cart.tic"};
    static const bool dirs[] = {false};

    {
        Host* host = fakeHost("dir & dir", false);
        firstFrame(host);

        hostTick(host);
        assert(Fake.pending == 1);

        hostTick(host); // the second dir must wait for the first answer
        assert(Fake.pending == 1);

        fakeList((fs_result){FS_OK, NULL}, names, dirs, 1);
        assert(fakePump());
        assert(Fake.pending == 0);

        hostTick(host); // the first command is done; the second starts
        assert(Fake.pending == 1);

        fakeList((fs_result){FS_OK, NULL}, names, dirs, 1);
        assert(fakePump());
        assert(Fake.pending == 0);

        hostFree(host);
    }

    {
        Host* host = fakeHost("save smoke & dir", false);
        firstFrame(host);

        // The save checks its target through the filesystem first.
        hostTick(host);
        assert(Fake.pending == 1);
        fakeStatResult((fs_result){FS_NOT_FOUND, "not found"}, "smoke", false);
        assert(fakePump());
        assert(Fake.pending == 1);
        fakeStatResult((fs_result){FS_NOT_FOUND, "not found"}, "smoke.tic", false);
        assert(fakePump());

        ConsoleAsk save = hostTick(host);
        assert(save.kind == CONSOLE_ASK_CART_SAVE);
        assert(strcmp(save.name, "smoke") == 0);

        hostTick(host); // the pending save refuses the dir
        assert(Fake.pending == 0);

        hostAnswer(host, (ConsoleAnswer){.result = {FS_OK, NULL}, .name = "smoke.tic"});

        hostTick(host); // the save answered; now the dir starts
        assert(Fake.pending == 1);
        fakeList((fs_result){FS_OK, NULL}, names, dirs, 1);
        assert(fakePump());

        hostFree(host);
    }
}

// A local mount answers inline: the command completes inside its own tick.
static void testFakeInline(void)
{
    static const char* names[] = {"cart.tic"};
    static const bool dirs[] = {false};

    Host* host = fakeHost("dir", true);
    firstFrame(host);

    fakeList((fs_result){FS_OK, NULL}, names, dirs, 1);
    hostTick(host);

    assert(Fake.pending == 0);
    assert(said(host, "cart.tic"));
    assert(promptAtInput(host));

    hostFree(host);
}

// A load answers through the one call and asks for the resolved name; a local
// mount answers inside the command's tick.
static void testFakeLoad(void)
{
    Host* host = fakeHost("load cart.tic", true);
    firstFrame(host);

    static tic_cartridge cart;
    memset(&cart, 0, sizeof cart);
    tic_tool_poke4(cart.bank0.tiles.data[0].data, 0, 0x7);

    u8 buffer[TIC_CART_SAVE_SIZE];
    s32 size = tic_cart_save(&cart, buffer);
    assert(size > 0);

    fakeLoadResult((fs_result){FS_OK, NULL}, "cart.tic", buffer, size);
    ConsoleAsk ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    assert(strcmp(ask.name, "cart.tic") == 0);
    hostAnswer(host, (ConsoleAnswer){.name = "cart.tic"});

    assert(said(host, "cart cart.tic loaded!"));
    assert(promptAtInput(host));

    hostFree(host);
}

// A session that ended points at signing in, whatever the command.
static void testFakeSession(void)
{
    Host* host = fakeHost("dir", false);
    firstFrame(host);

    hostTick(host);
    fakeList((fs_result){FS_SESSION, "the session ended"}, NULL, NULL, 0);
    assert(fakePump());

    assert(said(host, "the session ended, please sign in"));

    hostFree(host);
}

// ---- the shelf in the console ----

#define SHELF_START_JSON       "{\"device_code\":\"dev-code\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://dev.tic80.com/link\",\"expires_in\":600,\"interval\":1}"
#define SHELF_START_PROD_JSON  "{\"device_code\":\"dev-code\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://tic80.com/link\",\"expires_in\":600,\"interval\":1}"
#define SHELF_START_QUERY_JSON "{\"device_code\":\"dev-code\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://tic80.com/link?x=1\",\"expires_in\":600,\"interval\":1}"
#define SHELF_START_FIT_JSON   "{\"device_code\":\"dev-code\",\"user_code\":\"ABCDEF\",\"verification_uri\":\"https://tic80.com/link\",\"expires_in\":600,\"interval\":1}"
#define SHELF_START_EDGE_JSON  "{\"device_code\":\"dev-code\",\"user_code\":\"ABCDEFG\",\"verification_uri\":\"https://tic80.com/link\",\"expires_in\":600,\"interval\":1}"
#define SHELF_APPROVED_JSON    "{\"status\":\"approved\",\"token\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"user\":\"member\"}"
#define SHELF_CARTS_JSON       "{\"carts\":[{\"key\":\"my-game\",\"title\":\"My Game\",\"author\":\"Me\",\"lang\":\"lua\",\"size\":123,\"created\":1,\"updated\":1794940745061}],\"count\":1,\"bytes\":123,\"max_count\":100,\"max_bytes\":33554432}"

static const char* consoleRoot(void)
{
    static char root[TICNAME_MAX];
    snprintf(root, sizeof root, "/tmp/tic80-console-XXXXXX");
    assert(mkdtemp(root));
    return root;
}

static void hostBoot(Host* host)
{
    ConsoleAsk ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    hostAnswer(host, (ConsoleAnswer){.name = ""});
}

// The site's root listing a cd into the shelf asks for; the parser adds the
// shelf row to an empty answer.
static void answerSiteRoot(Host* host)
{
    for(s32 i = 0; i < 8 && !netCallback; i++)
        hostTick(host);

    assert(netCallback);
    assert(strcmp(netUrl, "/json?fn=dir&path=") == 0);

    answerNetDone("{\"folders\":[],\"files\":[]}", (s32)strlen("{\"folders\":[],\"files\":[]}"));
}

// The command that enters the site and then its shelf folder.
static void shelfCommand(char* command, s32 size, const char* rest)
{
    snprintf(command, size, "cd /%s & cd shelf & %s", TIC_HOST, rest);
}

// The first frame of a session: the launch asks for the cart's name.
static void hostFreeEx(Host* host)
{
    tic_fs_deldir(host->fs, "");
    hostFree(host);
}

static void onPairingNoop(account_pairing_state state, const char* text, void* data)
{
    (void)state;
    (void)text;
    (void)data;
}

// signIn pairs through the fake transport before any command runs.
static void signIn(Host* host)
{
    assert(tic_account_login(host->account, onPairingNoop, NULL));
    assert(requestHeld());
    answerRequest(201, SHELF_START_JSON);

    tic_account_update(host->account, 1000);
    assert(requestHeld());
    answerRequest(200, SHELF_APPROVED_JSON);

    assert(tic_account_signedin(host->account));
}

// The link the console recorded is the one drawn, and it is under the cursor.
static void assertLoginLink(Host* host, const char* url)
{
    s32 length = (s32)strlen(url);

    assert(host->console->login.urlSize == length);
    assert(host->console->login.urlBlock >= 0);
    assert(memcmp(host->console->text + host->console->login.urlBlock, url, length) == 0);
    assert(said(host, url));
    assert(!said(host, "enter the code"));
}

// The login command prints the link with the code in it, polls, and reports
// the member.
static void testShelfLogin(void)
{
    Host* host = hostNewAt(consoleRoot(), "login");
    hostBoot(host);

    hostTick(host);
    assert(requestHeld());
    assert(requestMethod == net_method_post);
    assert(strcmp(requestUrl, "/api/v1/client/device") == 0);

    answerRequest(201, SHELF_START_JSON);
    assertLoginLink(host, "https://dev.tic80.com/link?code=ABCD-EFGH");
    assert(said(host, "ESC to cancel"));

    for(s32 i = 0; i < 120 && !requestHeld(); i++)
        hostTick(host);

    assert(requestHeld());
    assert(strcmp(requestUrl, "/api/v1/client/device/dev-code") == 0);

    answerRequest(200, SHELF_APPROVED_JSON);
    assert(said(host, "signed in as"));
    assert(said(host, "member"));

    hostFreeEx(host);
    puts("console shelf login ok");
}

// A link that does not fit after "open" moves to its own line, and a link that
// already has a query takes the code with `&`; the recorded block starts where
// the link is drawn.
static void testShelfLoginLinks(void)
{
    static const struct
    {
        const char* start;
        const char* url;
    } Cases[] = {
        {SHELF_START_FIT_JSON, "https://tic80.com/link?code=ABCDEF"},          // 34: after the space
        {SHELF_START_EDGE_JSON, "https://tic80.com/link?code=ABCDEFG"},        // 35: its own line
        {SHELF_START_PROD_JSON, "https://tic80.com/link?code=ABCD-EFGH"},      // 37: its own line
        {SHELF_START_QUERY_JSON, "https://tic80.com/link?x=1&code=ABCD-EFGH"}, // an existing query
    };

    for(s32 i = 0; i < COUNT_OF(Cases); i++)
    {
        Host* host = hostNewAt(consoleRoot(), "login");
        hostBoot(host);

        hostTick(host);
        assert(requestHeld());
        answerRequest(201, Cases[i].start);
        assertLoginLink(host, Cases[i].url);

        tic_account_cancel_login(host->account);
        hostFreeEx(host);
    }

    puts("console shelf links ok");
}

static void testShelfLoginDenied(void)
{
    Host* host = hostNewAt(consoleRoot(), "login");
    hostBoot(host);

    hostTick(host);
    answerRequest(201, SHELF_START_JSON);

    for(s32 i = 0; i < 120 && !requestHeld(); i++)
        hostTick(host);

    answerRequest(200, "{\"status\":\"denied\"}");
    assert(said(host, "the pairing was denied"));
    assert(!tic_account_signedin(host->account));

    hostFreeEx(host);
    puts("console shelf denied ok");
}

static void testShelfLogout(void)
{
    Host* host = hostNewAt(consoleRoot(), "logout");
    signIn(host);

    hostBoot(host);
    hostTick(host);

    assert(requestHeld());
    assert(requestMethod == net_method_delete);
    assert(strcmp(requestUrl, "/api/v1/client/token") == 0);
    assert(strncmp(requestAuthorization, "Bearer ", 7) == 0);
    assert(said(host, "signed out"));
    assert(!tic_account_signedin(host->account));

    answerRequest(200, "{\"ok\":true}");
    hostFreeEx(host);
    puts("console shelf logout ok");
}

// The listing shows the keys with the cart extension and the server's usage.
static void testShelfDir(void)
{
    char command[TICNAME_MAX];
    shelfCommand(command, sizeof command, "dir");

    Host* host = hostNewAt(consoleRoot(), command);
    signIn(host);

    hostBoot(host);
    answerSiteRoot(host);

    for(s32 i = 0; i < 8 && !requestHeld(); i++)
        hostTick(host);

    assert(requestHeld());
    assert(requestMethod == net_method_get);
    assert(strcmp(requestUrl, "/api/v1/client/shelf") == 0);
    assert(strcmp(requestAuthorization, "Bearer 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") == 0);

    answerRequest(200, SHELF_CARTS_JSON);

    assert(said(host, "my-game.tic"));
    assert(!said(host, "My Game"));
    assert(said(host, "1/100 carts"));
    assert(said(host, "123 bytes/32 MB"));

    hostFreeEx(host);
    puts("console shelf dir ok");
}

// The filesystem root lists the site only; the site's root names the mount
// [shelf] signed in and signed out alike.
static void testShelfRootListing(void)
{
    char site[TICNAME_MAX + 3];
    snprintf(site, sizeof site, "[%s]", TIC_HOST);

    for(s32 account = 0; account < 2; account++)
    {
        Host* host = hostNewAt(consoleRoot(), "dir");

        if(account)
            signIn(host);

        hostBoot(host);
        hostTick(host);

        assert(said(host, site));
        assert(!said(host, "[shelf]"));

        hostFreeEx(host);
    }

    for(s32 account = 0; account < 2; account++)
    {
        char command[TICNAME_MAX + 16];
        snprintf(command, sizeof command, "cd /%s & dir", TIC_HOST);

        Host* host = hostNewAt(consoleRoot(), command);

        if(account)
            signIn(host);

        hostBoot(host);
        answerSiteRoot(host);

        assert(said(host, "[shelf]"));

        hostFreeEx(host);
    }

    puts("console shelf root ok");
}

// Signed out, the site's root lists the shelf and the mount can be entered;
// its listing answers the sign-in hint.
static void testShelfRootSignedOut(void)
{
    char command[TICNAME_MAX];
    shelfCommand(command, sizeof command, "dir");

    Host* host = hostNewAt(consoleRoot(), command);
    hostBoot(host);
    answerSiteRoot(host);

    for(s32 i = 0; i < 8 && !said(host, "the shelf is not signed in"); i++)
        hostTick(host);

    assert(said(host, "the shelf is not signed in"));
    assert(said(host, "login"));
    assert(!requestHeld());

    char dir[TICNAME_MAX];
    tic_fs_dir(host->fs, dir);
    assert(strcmp(dir, TIC_HOST "/" TIC_SHELF_DIR) == 0);

    hostFreeEx(host);
    puts("console shelf signed out ok");
}

static void testShelfDelete(void)
{
    char command[TICNAME_MAX];
    shelfCommand(command, sizeof command, "del my-game");

    Host* host = hostNewAt(consoleRoot(), command);
    signIn(host);

    hostBoot(host);
    answerSiteRoot(host);

    ConsoleAsk ask = {0};

    for(s32 i = 0; i < 8 && ask.kind == CONSOLE_ASK_NONE; i++)
        ask = hostTick(host); // del asks for confirmation

    assert(ask.kind == CONSOLE_ASK_CONFIRM);
    hostAnswer(host, (ConsoleAnswer){.yes = true});

    for(s32 i = 0; i < 8 && !requestHeld(); i++)
        hostTick(host);

    assert(requestHeld());
    assert(requestMethod == net_method_delete);
    assert(strcmp(requestUrl, "/api/v1/client/shelf/my-game") == 0);

    answerRequest(200, "{\"count\":0,\"bytes\":0,\"max_count\":100,\"max_bytes\":33554432}");

    assert(said(host, "deleted"));
    assert(said(host, "0/100 carts"));

    hostFreeEx(host);
    puts("console shelf delete ok");
}

static void testShelfLoad(void)
{
    char command[TICNAME_MAX];
    shelfCommand(command, sizeof command, "load my-game.tic");

    Host* host = hostNewAt(consoleRoot(), command);
    signIn(host);

    hostBoot(host);
    answerSiteRoot(host);

    for(s32 i = 0; i < 8 && !requestHeld(); i++)
        hostTick(host);

    assert(requestHeld());
    assert(requestMethod == net_method_get);
    assert(strcmp(requestUrl, "/api/v1/client/shelf/my-game/cart.tic") == 0);

    u8 buffer[TIC_CART_SAVE_SIZE];
    s32 size = tic_cart_save(&host->tic->cart, buffer);
    assert(size > 0);
    answerRequestBytes(200, buffer, size);

    ConsoleAsk ask = hostTick(host);
    assert(ask.kind == CONSOLE_ASK_CART_LOADED);
    assert(strcmp(ask.name, "my-game.tic") == 0);

    hostFreeEx(host);
    puts("console shelf load ok");
}

static void testShelfLoadMissing(void)
{
    char command[TICNAME_MAX];
    shelfCommand(command, sizeof command, "load gone");

    Host* host = hostNewAt(consoleRoot(), command);
    signIn(host);

    hostBoot(host);
    answerSiteRoot(host);

    for(s32 i = 0; i < 8 && !requestHeld(); i++)
        hostTick(host);

    answerRequest(404, "{\"error\":\"No such cart.\"}");
    assert(said(host, "No such cart."));

    hostFreeEx(host);
    puts("console shelf load missing ok");
}

// The save ask is the host's, and it stays pending until the answer comes.
static void testShelfSave(void)
{
    char command[TICNAME_MAX];
    shelfCommand(command, sizeof command, "save my-cart & dir");

    Host* host = hostNewAt(consoleRoot(), command);
    signIn(host);

    hostBoot(host);
    answerSiteRoot(host);

    ConsoleAsk ask = {0};

    for(s32 i = 0; i < 8 && ask.kind == CONSOLE_ASK_NONE; i++)
        ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CART_SAVE);
    assert(ask.name && strcmp(ask.name, "my-cart") == 0);

    // The queued dir waits for the save's answer.
    hostTick(host);
    assert(!requestHeld());

    hostAnswer(host, (ConsoleAnswer){
                         .result = {FS_OK, "created on the shelf, 1/100 carts, 123 bytes/32 MB"},
                         .name = "my-cart",
                     });

    assert(said(host, "cart my-cart saved!"));
    assert(said(host, "created on the shelf"));
    assert(said(host, "1/100 carts"));

    hostFreeEx(host);
    puts("console shelf save ok");
}

// A 401 ends the session and the next listing points at login.
static void testShelfDirErrors(void)
{
    char command[TICNAME_MAX];
    shelfCommand(command, sizeof command, "dir");

    Host* host = hostNewAt(consoleRoot(), command);
    signIn(host);

    hostBoot(host);
    answerSiteRoot(host);

    for(s32 i = 0; i < 8 && !requestHeld(); i++)
        hostTick(host);

    answerRequest(401, "{\"error\":\"invalid or expired token\"}");

    assert(said(host, "the session ended, please sign in"));
    assert(!tic_account_signedin(host->account));

    hostFreeEx(host);
    puts("console shelf 401 ok");
}

// A missing file on the site keeps the stat's answer, as before: the delete
// never runs and its refusal does not replace the message.
static void testDelMissingSite(void)
{
    Host* host = hostNewAt(consoleRoot(), "del missing");
    tic_fs_changedir(host->fs, TIC_HOST);

    hostBoot(host);

    ConsoleAsk ask = {0};

    for(s32 i = 0; i < 4 && ask.kind == CONSOLE_ASK_NONE; i++)
        ask = hostTick(host);

    assert(ask.kind == CONSOLE_ASK_CONFIRM);
    hostAnswer(host, (ConsoleAnswer){.yes = true});

    for(s32 i = 0; i < 4 && !netCallback; i++)
        hostTick(host);

    assert(netCallback);
    answerNetDone("{\"folders\":[],\"files\":[]}", (s32)strlen("{\"folders\":[],\"files\":[]}"));

    assert(said(host, "not found"));
    assert(!said(host, "access denied"));

    hostFreeEx(host);
    puts("console del missing ok");
}

// The site enters by its absolute name too, from the root and with the
// account's state playing no part.
static void testSiteAbsoluteCd(void)
{
    char command[TICNAME_MAX + 8];
    snprintf(command, sizeof command, "cd /%s & dir", TIC_HOST);

    for(s32 account = 0; account < 2; account++)
    {
        Host* host = hostNewAt(consoleRoot(), command);

        if(account)
            signIn(host);

        hostBoot(host);

        for(s32 i = 0; i < 8 && !netCallback; i++)
            hostTick(host);

        char dir[TICNAME_MAX];
        tic_fs_dir(host->fs, dir);
        assert(strcmp(dir, TIC_HOST) == 0);
        assert(netCallback);
        assert(strcmp(netUrl, "/json?fn=dir&path=") == 0);

        answerNetDone("{\"folders\":[],\"files\":[]}", (s32)strlen("{\"folders\":[],\"files\":[]}"));

        hostFreeEx(host);
    }

    puts("console site absolute cd ok");
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
    testHelpFill();
    testExportHelp();
    testErrorLink();
    testShelfLogin();
    testShelfLoginLinks();
    testShelfLoginDenied();
    testShelfLogout();
    testShelfDir();
    testShelfRootListing();
    testShelfRootSignedOut();
    testShelfDelete();
    testShelfLoad();
    testShelfLoadMissing();
    testShelfSave();
    testShelfDirErrors();
    testDelMissingSite();
    testSiteAbsoluteCd();
    testFakeDir();
    testFakeCd();
    testFakeDelRefused();
    testFakeMkdir();
    testFakeFolder();
    testFakePending();
    testFakeInline();
    testFakeLoad();
    testFakeSession();

    puts("moves ok / confirm ok / load ok / save ok / config ok / export ok / replay ok / run / help fill / error link / fs ok");

    return 0;
}
