/* The browser links without the studio: this file builds it with the menu
 * widget, the animation, the filesystem and the sound modules, a fake
 * filesystem that answers the listings and covers, and a fake host that
 * records the actions, so a call into studio.c fails to link here rather than
 * appearing quietly in the app build.
 *
 * Target surf-test in cmake/tests.cmake; `ctest -R surf-test` runs it.
 */
#include "studio/screens/surf.h"
#include "studio/config.h"
#include "studio/fs.h"
#include "studio/fs_internal.h"
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
static s32 AutoSaveCalls;
static s32 RunCalls;
static s32 ExitCalls;
static s32 ConfirmCalls;
static s32 CoverRequests;
static s32 OpenPageCalls;
static s32 OpenPageToken;

static char LoadedName[TICNAME_MAX];

static fs_done_cb LoadDone;
static void* LoadDoneData;

static SurfConfirm ConfirmAnswer;
static void* ConfirmAnswerData;

// The host load answers through the completion, held until the cart is "in".
static void fakeLoadCart(void* host, const char* name, fs_done_cb done, void* data)
{
    (void)host;
    LoadCartCalls++;
    snprintf(LoadedName, sizeof LoadedName, "%s", name);
    LoadDone = done;
    LoadDoneData = data;
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

// The site page the browser asks its host for: recorded, not opened.
static void fakeOpenItemPage(void* host, s32 token)
{
    (void)host;
    OpenPageCalls++;
    OpenPageToken = token;
}

// The fake filesystem's answers: the site and three local items at the root,
// the site's folders and carts, the shelf's carts under the site, one folder
// that fails to list.
static const char* RootNames[] = {TIC_HOST, "zzz", "bad", "local.tic"};
static const char* RootTitles[] = {NULL, NULL, NULL, NULL};
static const bool RootDirs[] = {true, true, true, false};
static const s32 RootTokens[] = {0, 0, 0, 0};

static const char* SiteNames[] = {"play", "devs", TIC_SHELF_DIR, "demo.tic", "b.tic", "c.tic"};
static const char* SiteTitles[] = {"Play", "Devs", "Shelf", "Demo.tic", "B.tic", "C.tic"};
static const bool SiteDirs[] = {true, true, true, false, false, false};
static const s32 SiteTokens[] = {0, 0, 0, 11, 12, 13};

static void fakeEmit(const char* const* names, const char* const* titles, const bool* dirs, const s32* tokens,
                     s32 count, fs_list_item_cb onItem, void* data)
{
    for(s32 i = 0; i < count; i++)
    {
        fs_item item = {names[i], titles ? titles[i] : NULL, dirs[i], -1, 0, tokens ? tokens[i] : 0};

        if(!onItem(&item, data))
            break;
    }
}

static void fakeList(tic_fs* fs, const char* path, fs_list_item_cb onItem, fs_list_done_cb onDone, void* data)
{
    (void)path;

    char dir[TICNAME_MAX];
    tic_fs_dir(fs, dir);

    if(strcmp(dir, "") == 0)
        fakeEmit(RootNames, RootTitles, RootDirs, RootTokens, COUNT_OF(RootNames), onItem, data);
    else if(strcmp(dir, TIC_HOST "/" TIC_SHELF_DIR) == 0)
    {
        static const char* ShelfNames[] = {"shelf-cart.tic"};
        static const bool ShelfDirs[] = {false};
        fakeEmit(ShelfNames, NULL, ShelfDirs, NULL, COUNT_OF(ShelfNames), onItem, data);
    }
    else if(strcmp(dir, TIC_HOST) == 0)
        fakeEmit(SiteNames, SiteTitles, SiteDirs, SiteTokens, COUNT_OF(SiteNames), onItem, data);
    else if(strcmp(dir, TIC_HOST "/play") == 0)
    {
        static const char* PlayNames[] = {"new", "recent"};
        static const char* PlayTitles[] = {"New", "Recent"};
        static const bool PlayDirs[] = {true, true};
        fakeEmit(PlayNames, PlayTitles, PlayDirs, NULL, COUNT_OF(PlayNames), onItem, data);
    }
    else if(strcmp(dir, "bad") == 0)
    {
        onDone(NULL, (fs_result){FS_OFFLINE, "no network"}, data);
        return;
    }
    else if(strcmp(dir, "zzz") != 0)
    {
        onDone(NULL, (fs_result){FS_NOT_FOUND, "not found"}, data);
        return;
    }

    onDone(NULL, (fs_result){FS_OK, NULL}, data);
}

static void fakeLoad(tic_fs* fs, const char* path, fs_load_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    onDone(NULL, NULL, 0, (fs_result){FS_NOT_FOUND, "not found"}, data);
}

static void fakeSave(tic_fs* fs, const char* path, const void* bytes, s32 size, fs_save_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;
    (void)bytes;
    (void)size;

    onDone(NULL, (fs_result){FS_DENIED, "access denied"}, data);
}

static void fakeDelete(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    onDone((fs_result){FS_DENIED, "access denied"}, data);
}

static void fakeStat(tic_fs* fs, const char* path, fs_stat_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    onDone(NULL, (fs_result){FS_NOT_FOUND, "not found"}, data);
}

static void fakeMkdir(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    onDone((fs_result){FS_UNSUPPORTED, "not supported"}, data);
}

static void fakeOpen(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    onDone((fs_result){FS_UNSUPPORTED, "not supported"}, data);
}

// A cover request the test holds, to see how many the screen keeps in flight.
typedef struct
{
    fs_cover_done_cb onDone;
    void* data;
} HeldCover;

static bool HoldCovers;
static HeldCover HeldCovers[8];
static s32 HeldCoverCount;

static void answerHeldCover(void)
{
    assert(HeldCoverCount > 0);

    HeldCover held = HeldCovers[0];
    memmove(HeldCovers, HeldCovers + 1, sizeof(HeldCover) * --HeldCoverCount);

    held.onDone(NULL, 0, (fs_result){FS_NOT_FOUND, "no cover"}, held.data);
}

static void fakeCover(tic_fs* fs, const char* path, fs_cover_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    CoverRequests++;

    if(HoldCovers)
    {
        assert(HeldCoverCount < COUNT_OF(HeldCovers));
        HeldCovers[HeldCoverCount++] = (HeldCover){onDone, data};
        return;
    }

    onDone(NULL, 0, (fs_result){FS_NOT_FOUND, "no cover"}, data);
}

static const fs_mount_ops FakeMount = {
    .list = fakeList,
    .load = fakeLoad,
    .save = fakeSave,
    .delete = fakeDelete,
    .stat = fakeStat,
    .mkdir = fakeMkdir,
    .openfolder = fakeOpen,
    .cover = fakeCover,
};

// The platform the screen and the filesystem ask: the test is all of it.

// The screen must never reach the net for a cart or a cover: the fake mount
// answers everything, so a request here is the screen's bug.
void tic_net_request(tic_net* net, net_method method, const char* url, const net_header* headers, s32 headersCount, const void* body, s32 bodySize, net_request_callback callback, void* calldata)
{
    (void)net;
    (void)method;
    (void)url;
    (void)headers;
    (void)headersCount;
    (void)body;
    (void)bodySize;
    (void)callback;
    (void)calldata;

    assert(false);
}

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    (void)net;
    (void)url;
    (void)callback;
    (void)calldata;

    assert(false);
}

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
    LoadCartCalls = AutoSaveCalls = 0;
    RunCalls = ExitCalls = ConfirmCalls = 0;
    CoverRequests = 0;
    OpenPageCalls = OpenPageToken = 0;
    HoldCovers = false;
    HeldCoverCount = 0;
    LoadedName[0] = '\0';
    LoadDone = NULL;
    LoadDoneData = NULL;
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
                          .config = &Cfg,

                          .loadCart = fakeLoadCart,
                          .autoSave = fakeAutoSave,
                          .cartChanged = fakeCartChanged,
                          .confirmLoadCart = fakeConfirmLoadCart,
                          .openItemPage = fakeOpenItemPage,
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

// A gamepad button, on the frame it is pressed and off after it.
static void pressButton(u8 button)
{
    Tic->ram->input.gamepads.data = button;
    frame();
    Tic->ram->input.gamepads.data = 0;
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

// The fake root's entries in the browser's order: the site folder, the test's
// own folder, the folder that fails to list, the test's cart.
enum
{
    RootSiteIndex = 0,
    RootSubIndex = 1,
    RootBadIndex = 2,
    RootCartIndex = 3,
    RootCount = 4,
};

// The site's entries in the browser's order: the step back, the site's
// folders (the shelf included) and its carts.
enum
{
    SiteBackIndex = 0,
    SitePlayIndex = 1,
    SiteDevsIndex = 2,
    SiteShelfIndex = 3,
    SiteDemoIndex = 4,
    SiteCount = 7,
};

// The screen's cover retry, five seconds: see COVER_RETRY_TICKS in surf.c.
enum
{
    CoverRetryTicks = TIC80_FRAMERATE * 5,
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

    assert(strcmp(dirName(), TIC_HOST) == 0);
}

// The fake root's listing is the browser's menu: the folders and the cart it
// answered are the items, with no net and no mount knowledge in the screen.
static void testListing(void)
{
    setup();

    assert(Screen->menu.count == RootCount);

    assert(strcmp(Screen->menu.items[RootSiteIndex].name, TIC_HOST) == 0);
    assert(Screen->menu.items[RootSiteIndex].dir);
    char siteLabel[TICNAME_MAX];
    sprintf(siteLabel, "[%s]", TIC_HOST);
    assert(strcmp(Screen->menu.items[RootSiteIndex].label, siteLabel) == 0);
    assert(strcmp(Screen->menu.items[RootSubIndex].name, "zzz") == 0);
    assert(Screen->menu.items[RootSubIndex].dir);
    assert(strcmp(Screen->menu.items[RootBadIndex].name, "bad") == 0);
    assert(Screen->menu.items[RootBadIndex].dir);
    assert(strcmp(Screen->menu.items[RootCartIndex].name, "local.tic") == 0);
    assert(!Screen->menu.items[RootCartIndex].dir);

    // A local item has no site token.
    assert(Screen->menu.items[RootCartIndex].token == 0);

    teardown();
    puts("listing ok");
}

// A site cart plays through the same host entry, the cover comes from the
// filesystem, and the retry asks again after its time.
static void testSiteCart(void)
{
    setup();

    Cart.banks[0].sfx.samples.data[2].note = 3;
    Cart.banks[0].sfx.samples.data[2].octave = 1;

    openSiteFolder();

    // The listing opens on "..", the step back to the root; the folders and
    // the carts follow.
    assert(Screen->menu.count == SiteCount);
    assert(strcmp(Screen->menu.items[SiteBackIndex].name, "..") == 0);
    assert(strcmp(Screen->menu.items[SiteDemoIndex].name, "demo.tic") == 0);

    // The server's name is the title plus the extension; the menu shows the title.
    assert(strcmp(Screen->menu.items[SiteDemoIndex].label, "Demo") == 0);

    // The mount's token travels with the item.
    assert(Screen->menu.items[SiteDemoIndex].token == 11);

    moveTo(SiteDemoIndex);
    assert(channelNote() == 3 + 12);

    // Y asks the host for the site's page of this cart.
    pressButton(1 << 7);
    assert(OpenPageCalls == 1);
    assert(OpenPageToken == 11);

    // A folder has no site page.
    press(tic_key_up);
    settle();
    pressButton(1 << 7);
    assert(OpenPageCalls == 1);

    press(tic_key_down);
    settle();

    // The cover is the filesystem's answer; a missing one is asked again later.
    assert(CoverRequests == 1);

    for(s32 i = 0; i <= CoverRetryTicks; i++)
        frame();

    assert(CoverRequests == 2);

    press(tic_key_return);
    settle();

    assert(LoadCartCalls == 1);
    assert(strcmp(LoadedName, "demo.tic") == 0);
    assert(RunCalls == 0);

    LoadDone((fs_result){FS_OK, NULL}, LoadDoneData);
    assert(RunCalls == 1);
    assert(AutoSaveCalls == 0);

    // The play movie ends where the host runs the cart; the browser is resumed
    // as the studio resumes it when the mode comes back.
    Screen->resume(Screen);
    settle();

    // The autosave runs through the host when the option is on.
    Cfg.data.options.autosave = true;
    press(tic_key_return);
    settle();
    assert(LoadCartCalls == 2);

    LoadDone((fs_result){FS_OK, NULL}, LoadDoneData);
    assert(RunCalls == 2);
    assert(AutoSaveCalls == 1);

    teardown();
    puts("site cart ok");
}

// A failed load leaves the listing in place, reports the result's message and
// does not run the cart.
static void testFailedLoad(void)
{
    setup();

    openSiteFolder();
    moveTo(SiteDemoIndex);

    press(tic_key_return);
    settle();

    assert(LoadCartCalls == 1);
    assert(RunCalls == 0);

    LoadDone((fs_result){FS_OFFLINE, "no network"}, LoadDoneData);
    settle();

    assert(RunCalls == 0);
    assert(strcmp(dirName(), TIC_HOST) == 0);
    assert(Screen->menu.count == SiteCount);
    assert(strcmp(Screen->message, "no network") == 0);

    // The next load clears the message and plays.
    press(tic_key_return);
    settle();
    assert(LoadCartCalls == 2);

    LoadDone((fs_result){FS_OK, NULL}, LoadDoneData);
    assert(RunCalls == 1);
    assert(Screen->message[0] == '\0');

    teardown();
    puts("failed load ok");
}

// A listing that succeeds clears the last failure the browser showed.
static void testFailedListing(void)
{
    setup();

    moveTo(RootBadIndex);
    press(tic_key_return);
    settle();

    assert(strcmp(dirName(), "bad") == 0);
    assert(Screen->menu.count == 1);
    assert(strcmp(Screen->menu.items[0].name, "..") == 0);
    assert(strcmp(Screen->message, "no network") == 0);

    press(tic_key_backspace);
    settle();

    assert(strcmp(dirName(), "") == 0);
    assert(Screen->menu.count == RootCount);
    assert(Screen->message[0] == '\0');

    teardown();
    puts("failed listing ok");
}

// The browser keeps at most two cover requests in flight.
static void testCoverCap(void)
{
    setup();
    HoldCovers = true;

    openSiteFolder();
    assert(CoverRequests == 0); // ".." is a folder

    moveTo(SiteDemoIndex); // demo
    assert(CoverRequests == 1);

    press(tic_key_down); // b
    settle();
    assert(CoverRequests == 2);

    press(tic_key_down); // c: the cap holds the third
    settle();
    assert(CoverRequests == 2);

    // One answer frees a slot; the selected item asks on the next frame.
    answerHeldCover();
    frame();
    assert(CoverRequests == 3);

    while(HeldCoverCount > 0)
        answerHeldCover();

    teardown();
    puts("cover cap ok");
}

// The site's folders are shown by their titles and entered by their names.
static void testSiteFolders(void)
{
    setup();
    openSiteFolder();

    assert(Screen->menu.count == SiteCount);
    assert(strcmp(Screen->menu.items[SitePlayIndex].name, "play") == 0);
    assert(Screen->menu.items[SitePlayIndex].dir);
    assert(strcmp(Screen->menu.items[SitePlayIndex].label, "[Play]") == 0);
    assert(strcmp(Screen->menu.items[SiteDevsIndex].name, "devs") == 0);
    assert(Screen->menu.items[SiteDevsIndex].dir);
    assert(strcmp(Screen->menu.items[SiteDevsIndex].label, "[Devs]") == 0);
    assert(strcmp(Screen->menu.items[SiteShelfIndex].name, TIC_SHELF_DIR) == 0);
    assert(Screen->menu.items[SiteShelfIndex].dir);
    assert(strcmp(Screen->menu.items[SiteShelfIndex].label, "[Shelf]") == 0);

    // Entering a folder takes its lowercase name.
    moveTo(SitePlayIndex);
    press(tic_key_return);
    settle();

    assert(strcmp(dirName(), TIC_HOST "/play") == 0);
    assert(Screen->menu.count == 3);
    assert(strcmp(Screen->menu.items[1].name, "new") == 0);
    assert(strcmp(Screen->menu.items[1].label, "[New]") == 0);
    assert(strcmp(Screen->menu.items[2].name, "recent") == 0);
    assert(strcmp(Screen->menu.items[2].label, "[Recent]") == 0);

    teardown();
    puts("site folders ok");
}

// The shelf is a folder inside the site, titled [Shelf]; its carts are keys,
// and a selection plays through the same host load.
static void testShelfFolder(void)
{
    setup();

    openSiteFolder();

    assert(strcmp(Screen->menu.items[SiteShelfIndex].name, TIC_SHELF_DIR) == 0);
    assert(Screen->menu.items[SiteShelfIndex].dir);
    assert(strcmp(Screen->menu.items[SiteShelfIndex].label, "[Shelf]") == 0);

    moveTo(SiteShelfIndex);
    press(tic_key_return);
    settle();

    assert(strcmp(dirName(), TIC_HOST "/" TIC_SHELF_DIR) == 0);
    assert(Screen->menu.count == 2);
    assert(strcmp(Screen->menu.items[1].name, "shelf-cart.tic") == 0);
    assert(strcmp(Screen->menu.items[1].label, "shelf-cart") == 0);

    press(tic_key_down);
    settle();
    press(tic_key_return);
    settle();

    assert(LoadCartCalls == 1);
    assert(strcmp(LoadedName, "shelf-cart.tic") == 0);
    assert(RunCalls == 0);

    LoadDone((fs_result){FS_OK, NULL}, LoadDoneData);
    assert(RunCalls == 1);

    teardown();
    puts("shelf folder ok");
}

static void testLocalCart(void)
{
    setup();

    moveTo(RootCartIndex);
    press(tic_key_return);
    settle();

    assert(LoadCartCalls == 1);
    assert(strcmp(LoadedName, "local.tic") == 0);
    assert(RunCalls == 0);

    LoadDone((fs_result){FS_OK, NULL}, LoadDoneData);
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
    assert(RunCalls == 0);

    LoadDone((fs_result){FS_OK, NULL}, LoadDoneData);
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
    assert(Screen->menu.count == 1);
    assert(strcmp(Screen->menu.items[0].name, "..") == 0);

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
    char root[] = "/tmp/tic80-surf-XXXXXX";
    assert(mkdtemp(root));

    Fs = tic_fs_create(root, NULL);
    assert(Fs);

    tic_fs_set_mount(Fs, &FakeMount);

    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testListing();
    testSiteFolders();
    testSiteCart();
    testFailedLoad();
    testFailedListing();
    testCoverCap();
    testShelfFolder();
    testLocalCart();
    testConfirm();
    testSubfolder();
    testExit();

    tic_core_close(Tic);

    tic_fs_deldir(Fs, "");

    puts("surf ok");
    return 0;
}
