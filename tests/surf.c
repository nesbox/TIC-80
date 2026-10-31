/* The browser links without the studio: this file builds it with the menu
 * widget, the animation, the filesystem and the sound modules, a fake net that
 * holds its requests, a filesystem on a temporary root and a fake host that
 * records the actions, so a call into studio.c fails to link here rather than
 * appearing quietly in the app build.
 *
 * Target surf-test in cmake/tests.cmake; `ctest -R surf-test` runs it.
 */
#include "studio/screens/surf.h"
#include "studio/config.h"
#include "studio/fs.h"
#include "studio/net.h"
#include "studio/paths.h"
#include "studio/system.h"
#include "cart.h"
#include "tools.h"
#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static tic_mem* Tic;
static tic_fs* Fs;
static Config Cfg;
static tic_cartridge Cart;
static Surf* Screen;

static bool Active;
static bool CartChanged;

static s32 LoadCartCalls;
static s32 LoadByHashCalls;
static s32 AutoSaveCalls;
static s32 RunCalls;
static s32 ExitCalls;
static s32 ConfirmCalls;

static char LoadedName[TICNAME_MAX];
static char LoadedHash[TICNAME_MAX];

static fs_done_callback HashDone;
static void* HashDoneData;

static SurfConfirm ConfirmAnswer;
static void* ConfirmAnswerData;

// The test's network: the covers are answered at once with an error, so the
// listing is the only request the test holds. No socket is opened.
static net_get_callback NetCallback;
static void* NetCalldata;
static char NetUrl[TICNAME_MAX];

static void fakeLoadCart(void* host, const char* name)
{
    (void)host;
    LoadCartCalls++;
    snprintf(LoadedName, sizeof LoadedName, "%s", name);
}

static void fakeLoadByHash(void* host, const char* name, const char* hash, const char* section, fs_done_callback done, void* data)
{
    (void)host;
    (void)section;

    LoadByHashCalls++;
    snprintf(LoadedName, sizeof LoadedName, "%s", name);
    snprintf(LoadedHash, sizeof LoadedHash, "%s", hash);
    HashDone = done;
    HashDoneData = data;
}

static void fakeAutoSave(void* host)
{
    (void)host;
    AutoSaveCalls++;
}

static bool fakeCartChanged(void* host)
{
    (void)host;
    return CartChanged;
}

static void fakeConfirmLoadCart(void* host, SurfConfirm callback, void* data)
{
    (void)host;
    ConfirmCalls++;
    ConfirmAnswer = callback;
    ConfirmAnswerData = data;
}

static void fakeRun(void* host)
{
    (void)host;
    RunCalls++;
}

static void fakeExit(void* host)
{
    (void)host;
    ExitCalls++;
    Active = false;
}

static bool fakeIsActive(void* host)
{
    (void)host;
    return Active;
}

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    (void)net;

    if(strncmp(url, "/cart/", 6) == 0)
    {
        net_get_data answer = {.type = net_get_error, .error = {.code = 404}, .calldata = calldata, .url = url};
        callback(&answer);
        return;
    }

    assert(!NetCallback);

    NetCallback = callback;
    NetCalldata = calldata;
    snprintf(NetUrl, sizeof NetUrl, "%s", url);
}

// The platform the screen and the filesystem ask: the test is all of it.
void tic_sys_open_url(const char* url)
{
    (void)url;
}

void tic_sys_open_path(const char* path)
{
    (void)path;
}

bool tic_sys_keyboard_text(char* text)
{
    (void)text;
    return false;
}

static bool netPending(void)
{
    return NetCallback != NULL;
}

static void answerNet(const char* json)
{
    assert(NetCallback);

    net_get_callback callback = NetCallback;
    NetCallback = NULL;

    net_get_data answer = {
        .type = net_get_done,
        .done = {.size = (s32)strlen(json), .data = (u8*)json},
        .calldata = NetCalldata,
        .url = NetUrl,
    };

    callback(&answer);
}

static const char SiteListing[] =
    "{\"folders\":[{\"name\":\"games\"}],"
    "\"files\":[{\"name\":\"Demo\",\"hash\":\"abc\",\"filename\":\"demo.tic\",\"id\":7}]}";

static const char EmptyListing[] = "{\"folders\":[],\"files\":[]}";

static const char* dirName(void)
{
    static char dir[TICNAME_MAX];
    tic_fs_dir(Fs, dir);
    return dir;
}

static void settle(void);

// A root of the test's own: the mode the screen is left in is the host's
// answer to `isActive`, so the exit flips it.
static void setup(void)
{
    LoadCartCalls = LoadByHashCalls = AutoSaveCalls = 0;
    RunCalls = ExitCalls = ConfirmCalls = 0;
    LoadedName[0] = LoadedHash[0] = '\0';
    HashDone = NULL;
    HashDoneData = NULL;
    ConfirmAnswer = NULL;
    ConfirmAnswerData = NULL;

    Active = true;
    CartChanged = false;

    memset(&Cart, 0, sizeof Cart);
    Cfg = (Config){.cart = &Cart};

    for(s32 i = 0; i < 8 && !tic_fs_isroot(Fs); i++)
        tic_fs_dirback(Fs);

    Screen = calloc(1, sizeof(Surf));
    assert(Screen);

    surf_init(Screen, &(SurfDeps){
                          .host = NULL,
                          .tic = Tic,
                          .fs = Fs,
                          .net = (tic_net*)&Cfg,
                          .config = &Cfg,

                          .loadCart = fakeLoadCart,
                          .loadByHash = fakeLoadByHash,
                          .autoSave = fakeAutoSave,
                          .cartChanged = fakeCartChanged,
                          .confirmLoadCart = fakeConfirmLoadCart,
                          .run = fakeRun,
                          .exit = fakeExit,
                          .isActive = fakeIsActive,
                      });

    settle();
}

static void teardown(void)
{
    surf_free(Screen);
    Screen = NULL;
}

// The browser's own frame, in the studio's order.
static void frame(void)
{
    tic_core_tick_start(Tic);
    Screen->tick(Screen);
    tic_core_tick_end(Tic);
}

static void keys(tic_key key)
{
    memset(&Tic->ram->input.keyboard, 0, sizeof Tic->ram->input.keyboard);

    if(key != tic_key_unknown)
        Tic->ram->input.keyboard.keys[0] = key;
}

static void press(tic_key key)
{
    keys(key);
    frame();
    keys(tic_key_unknown);
}

// Enough frames for the longest animations, and the listing behind them.
static void settle(void)
{
    keys(tic_key_unknown);

    for(s32 i = 0; i < 40; i++)
        frame();
}

static void moveTo(s32 index)
{
    for(s32 i = 0; i < index; i++)
    {
        press(tic_key_down);
        settle();
    }
}

// The root's entries in the browser's order: the folders first, each group by
// name. The screen's cache folder starts with a dot and the listing skips
// those, so the site folder is first, then the test's own folder and cart.
enum
{
    RootSiteIndex = 0,
    RootSubIndex = 1,
    RootCartIndex = 2,
};

static s32 channelNote(void)
{
    return ((tic_core*)Tic)->state.sfx.channels[0].note;
}

// The second bank the browser draws into: the core keeps it out of the
// machine's vram while bank 0 is in front.
static u8* menuBank(void)
{
    return ((tic_core*)Tic)->state.vbank.mem.screen.data;
}

static void fillBank(u8 value)
{
    memset(menuBank(), value, sizeof(tic_screen));
}

static bool bankIs(u8 value)
{
    const u8* bank = menuBank();

    for(s32 i = 0; i < (s32)sizeof(tic_screen); i++)
        if(bank[i] != value)
            return false;

    return true;
}

static void openSiteFolder(void)
{
    moveTo(RootSiteIndex);
    press(tic_key_return);
    settle();

    assert(netPending());
    assert(strcmp(dirName(), TIC_HOST) == 0);
}

static void testSiteListing(void)
{
    setup();

    Cart.banks[0].sfx.samples.data[2].note = 3;
    Cart.banks[0].sfx.samples.data[2].octave = 1;

    openSiteFolder();

    answerNet(SiteListing);
    settle();

    // A move plays the config cart's system sound. The listing opens on "..",
    // the step back to the root.
    press(tic_key_down);
    assert(channelNote() == 3 + 12);
    settle();
    press(tic_key_up);
    settle();

    // The answered folder is the entry after "..": opening it asks the site
    // for that folder.
    press(tic_key_down);
    settle();
    press(tic_key_return);
    settle();
    assert(netPending());
    assert(strstr(NetUrl, "path=games"));

    answerNet(EmptyListing);
    settle();

    // Back to the site root, which lists again, and the cursor returns to the
    // folder it left.
    press(tic_key_backspace);
    settle();
    assert(netPending());
    assert(strcmp(dirName(), TIC_HOST) == 0);

    answerNet(SiteListing);
    settle();

    // The answered cart loads by hash, and the host's answer runs it.
    press(tic_key_down);
    settle();
    press(tic_key_return);
    settle();

    assert(LoadByHashCalls == 1);
    assert(strcmp(LoadedName, "demo.tic") == 0);
    assert(strcmp(LoadedHash, "abc") == 0);
    assert(RunCalls == 0);

    HashDone(HashDoneData);
    assert(RunCalls == 1);
    assert(AutoSaveCalls == 0);

    // The play movie ends where the host runs the cart; the browser is resumed
    // as the studio resumes it when the mode comes back.
    Screen->resume(Screen);
    settle();

    // The autosave writes the loaded cart when the option is on.
    Cfg.data.options.autosave = true;
    press(tic_key_return);
    settle();
    assert(LoadByHashCalls == 2);

    HashDone(HashDoneData);
    assert(RunCalls == 2);
    assert(AutoSaveCalls == 1);

    // The autosave made the downloads folder at the root; the tests after this
    // one count the root's entries. The screen is still in the site folder, so
    // the path is the root's.
    tic_fs_deldir(Fs, "/downloads");

    teardown();
    puts("site listing ok");
}

static void testLocalCart(void)
{
    setup();

    moveTo(RootCartIndex);
    press(tic_key_return);
    settle();

    assert(LoadCartCalls == 1);
    assert(strcmp(LoadedName, "local.tic") == 0);
    assert(RunCalls == 1);

    teardown();
    puts("local cart ok");
}

static void testConfirm(void)
{
    setup();

    CartChanged = true;

    moveTo(RootCartIndex);
    press(tic_key_return);
    settle();

    // The host is asked, and nothing loads until it answers.
    assert(ConfirmCalls == 1);
    assert(LoadCartCalls == 0);
    assert(RunCalls == 0);

    ConfirmAnswer(NULL, false, ConfirmAnswerData);
    assert(LoadCartCalls == 0);
    assert(RunCalls == 0);

    // Answered no, the browser waits for the host to resume it, as the studio
    // does when the mode comes back.
    Screen->resume(Screen);
    settle();

    press(tic_key_return);
    settle();
    assert(ConfirmCalls == 2);

    ConfirmAnswer(NULL, true, ConfirmAnswerData);
    assert(LoadCartCalls == 1);
    assert(RunCalls == 1);

    teardown();
    puts("confirm ok");
}

static void testSubfolder(void)
{
    setup();

    moveTo(RootSubIndex);
    press(tic_key_return);
    settle();
    assert(strcmp(dirName(), "zzz") == 0);

    press(tic_key_backspace);
    settle();
    assert(strcmp(dirName(), "") == 0);

    teardown();
    puts("subfolder ok");
}

static void testExit(void)
{
    setup();

    // The escape key leaves from the root whatever the list holds.
    press(tic_key_escape);
    settle();
    assert(ExitCalls == 1);

    teardown();

    // The back key leaves too, and the mode check after the exit sees the new
    // mode: the frame is not drawn.
    setup();

    fillBank(0xAA);
    press(tic_key_backspace);
    settle();

    assert(ExitCalls == 1);
    assert(bankIs(0xAA));

    teardown();
    puts("exit ok");
}

int main(void)
{
    // A root of this run's own: two runs never share one.
    char root[] = "/tmp/tic80-surf-XXXXXX";
    assert(mkdtemp(root));

    Fs = tic_fs_create(root, NULL);
    assert(Fs);

    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    // A cart of the test's own and a folder to walk into; the screen makes the
    // cache folder where it always does.
    {
        u8* buffer = malloc(TIC_CART_SAVE_SIZE);
        tic_cartridge cart = {0};

        s32 size = tic_cart_save(&cart, buffer);
        assert(tic_fs_save(Fs, "local.tic", buffer, size, false));

        free(buffer);
        tic_fs_makedir(Fs, "zzz");
    }

    testSiteListing();
    testLocalCart();
    testConfirm();
    testSubfolder();
    testExit();

    tic_core_close(Tic);

    tic_fs_delfile(Fs, "local.tic");
    tic_fs_deldir(Fs, "zzz");
    tic_fs_deldir(Fs, "/downloads");
    tic_fs_deldir(Fs, TIC_CACHE);
    tic_fs_deldir(Fs, TIC_LOCAL);
    tic_fs_deldir(Fs, "");

    puts("surf ok");
    return 0;
}
