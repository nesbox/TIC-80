/* The studio's three services without the studio: a filesystem on its own
 * temporary root, a config written and read back, and a cart saved, reloaded,
 * auto-saved and loaded from a path and from the site — every notification
 * recorded by the test. */
#include "studio/fs.h"
#include "studio/config.h"
#include "studio/net.h"
#include "studio/rom.h"
#include "studio/paths.h"
#include "core/core.h"
#include "tools.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// The test's network: a request is held until the test answers it; no socket opens.
static net_get_callback NetCallback;
static void* NetCalldata;
static char NetUrl[TICNAME_MAX];

void tic_net_get(tic_net* net, const char* url, net_get_callback callback, void* calldata)
{
    (void)net;
    assert(!NetCallback);

    NetCallback = callback;
    NetCalldata = calldata;
    snprintf(NetUrl, sizeof NetUrl, "%s", url);
}

// The shelf is not driven here; a request through the primitive is a bug.
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

static void answerNetData(const void* data, s32 size)
{
    assert(NetCallback);

    net_get_callback callback = NetCallback;
    void* calldata = NetCalldata;
    NetCallback = NULL;

    net_get_data answer = {
        .type = net_get_done,
        .done = {.size = size, .data = (u8*)data},
        .calldata = calldata,
        .url = NetUrl,
    };

    callback(&answer);
}

static void answerNetJson(const char* json)
{
    answerNetData(json, (s32)strlen(json));
}

// The host the services notify; every callback lands here.
typedef struct
{
    s32 changed;
    s32 loaded;
    s32 saved;
} Host;

static void onChanged(void* host)
{
    ((Host*)host)->changed++;
}

static void onLoaded(void* host)
{
    ((Host*)host)->loaded++;
}

static void onSaved(void* host)
{
    ((Host*)host)->saved++;
}

// The platform's default key mapping: here the test is the platform.
void tic_sys_default_mapping(tic_mapping* mapping)
{
    memset(mapping, 0, sizeof(tic_mapping));
}

// The file manager the editor file system asks for; the test opens nothing.
void tic_sys_open_path(const char* path)
{
    (void)path;
}

typedef struct
{
    bool sawFile;
    bool done;
} EnumData;

static bool onEnumItem(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    (void)title;
    (void)hash;
    (void)id;
    (void)dir;

    if(strcmp(name, "fs-smoke.tic") == 0)
        ((EnumData*)data)->sawFile = true;

    return true;
}

static void onEnumDone(void* data)
{
    ((EnumData*)data)->done = true;
}

static void onSaveDone(const char* name, fs_result result, void* data)
{
    (void)name;
    *(bool*)data = result.code == FS_OK;
}

static bool saveFile(tic_fs* fs, const char* name, const void* data, s32 size)
{
    bool saved = false;
    tic_fs_save(fs, name, data, size, onSaveDone, &saved);
    return saved;
}

typedef struct
{
    u8 bytes[TICNAME_MAX];
    s32 size;
    fs_result result;
} ReadAnswer;

static void onReadDone(const char* name, const u8* bytes, s32 size, fs_result result, void* data)
{
    (void)name;

    ReadAnswer* read = data;
    read->result = result;
    read->size = MIN(size, (s32)sizeof read->bytes);
    memcpy(read->bytes, bytes, read->size);
}

static ReadAnswer readFile(tic_fs* fs, const char* name)
{
    ReadAnswer read = {0};
    tic_fs_load(fs, name, onReadDone, &read);
    return read;
}

static void testFilesystem(tic_fs* fs)
{
    const char* name = "fs-smoke.tic";
    static const u8 payload[] = {0xde, 0xad, 0xbe, 0xef};

    tic_fs_delfile(fs, name);

    assert(saveFile(fs, name, payload, sizeof payload));
    assert(tic_fs_exists(fs, name));

    // A save replaces what the name held.
    assert(saveFile(fs, name, payload, sizeof payload));

    ReadAnswer read = readFile(fs, name);
    assert(read.result.code == FS_OK);
    assert(read.size == (s32)sizeof payload);
    assert(memcmp(read.bytes, payload, sizeof payload) == 0);

    EnumData enumData = {0};
    tic_fs_enum(fs, onEnumItem, onEnumDone, &enumData);
    assert(enumData.done);
    assert(enumData.sawFile);

    char dir[TICNAME_MAX];
    tic_fs_dir(fs, dir);
    assert(dir[0] == '\0');

    tic_fs_changedir(fs, "downloads");
    tic_fs_dir(fs, dir);
    assert(strcmp(dir, "downloads") == 0);
    tic_fs_dirback(fs);
    tic_fs_dir(fs, dir);
    assert(dir[0] == '\0');

    tic_fs_delfile(fs, name);
    assert(!tic_fs_exists(fs, name));

    puts("fs ok");
}

static void testConfig(tic_mem* tic, tic_fs* fs)
{
    Host host = {0};

    tic_fs_delfile(fs, CONFIG_TIC_PATH);
    tic_fs_delfile(fs, TIC_LOCAL_VERSION "options.json");

    Config* first = calloc(1, sizeof(Config));
    assert(first);
    initConfig(first, tic, fs, onChanged, &host);

    // The defaults alone notify once: the embedded config cart is read
    // through the path a written one is.
    assert(host.changed == 1);

    first->data.options.volume = 9;
#if defined(BUILD_EDITORS)
    first->data.options.codeMinimap = true;
#endif
    first->saveOptions(first);

    // config.tic is a cart whose code carries the options JSON; a save takes
    // the machine's cart, so put a different UI_SCALE there first.
    strcpy(tic->cart.code.data, "{\"UI_SCALE\":7}");
    first->saveConfigCart(first);
    assert(host.changed == 2);
    assert(first->data.uiScale == 7);

    Config* second = calloc(1, sizeof(Config));
    assert(second);
    initConfig(second, tic, fs, onChanged, &host);

    // Once for the defaults, once for the config.tic the first one wrote.
    assert(host.changed == 4);
    assert(second->data.uiScale == 7);
    assert(second->data.options.volume == 9);
#if defined(BUILD_EDITORS)
    assert(second->data.options.codeMinimap);
#endif

    freeConfig(first);
    freeConfig(second);

    puts("config ok");
}

typedef struct
{
    bool done;
    fs_result result;
} LoadAnswer;

static void onLoadAnswer(fs_result result, void* data)
{
    LoadAnswer* answer = data;
    answer->done = true;
    answer->result = result;
}

typedef struct
{
    bool done;
    char name[TICNAME_MAX];
    fs_result result;
} SaveAnswer;

static void onSaveAnswer(const char* name, fs_result result, void* data)
{
    SaveAnswer* answer = data;
    answer->done = true;
    snprintf(answer->name, sizeof answer->name, "%s", name ? name : "");
    answer->result = result;
}

static const char SiteCartListing[] =
    "{\"folders\":[],\"files\":[{\"name\":\"Site Cart\",\"hash\":\"abc\",\"filename\":\"site.tic\",\"id\":1}]}";

static void testCart(tic_mem* tic, tic_fs* fs)
{
    Host host = {0};
    CartName cart = {0};

    tic_fs_delfile(fs, CONFIG_TIC_PATH);

    Config* config = calloc(1, sizeof(Config));
    assert(config);
    initConfig(config, tic, fs, onChanged, &host);
    assert(host.changed == 1);

    RomDeps deps = {
        .tic = tic,
        .fs = fs,
        .cart = &cart,
        .config = config,
        .host = &host,
        .loaded = onLoaded,
        .saved = onSaved,
    };

    // A cart with a byte the reload has to bring back.
    tic_tool_poke4(tic->cart.bank0.tiles.data[3].data, 0, 0xa);

    // The save serializes in the host and the filesystem writes it; the stored
    // name becomes the identity and the host hears when it has.
    SaveAnswer saved = {0};
    rom_save_cart(&deps, "smoke", onSaveAnswer, &saved);
    assert(saved.done);
    assert(saved.result.code == FS_OK);
    assert(strcmp(saved.name, "smoke.tic") == 0);
    assert(host.saved == 1);
    assert(strcmp(cart.name, "smoke.tic") == 0);
    assert(strcmp(cart.path, tic_fs_path(fs, "smoke.tic")) == 0);

    // A refused write answers its message and leaves the identity alone.
    {
        SaveAnswer refused = {0};
        rom_save_cart(&deps, "missing/smoke.tic", onSaveAnswer, &refused);
        assert(refused.done);
        assert(refused.result.code == FS_NOT_FOUND);
        assert(host.saved == 1);
        assert(strcmp(cart.name, "smoke.tic") == 0);
    }

    tic_tool_poke4(tic->cart.bank0.tiles.data[3].data, 0, 0);
    rom_reload_cart(&deps);
    assert(host.loaded == 1);
    assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

    // The autosave goes to its own folder through the same call.
    {
        SaveAnswer autosaved = {0};
        rom_autosave(&deps, onSaveAnswer, &autosaved);
        assert(autosaved.done);
        assert(autosaved.result.code == FS_OK);
        assert(strcmp(autosaved.name, "smoke.tic") == 0);
        assert(host.saved == 2);
        assert(tic_fs_exists(fs, "downloads/smoke.tic"));
        assert(strcmp(cart.path, tic_fs_path(fs, "downloads/smoke.tic")) == 0);
    }

    // A load by name reads the file into the machine and resets the identity.
    tic_tool_poke4(tic->cart.bank0.tiles.data[3].data, 0, 0);
    assert(rom_load_cart(&deps, "smoke.tic"));
    assert(host.loaded == 2);
    assert(strcmp(cart.name, "smoke.tic") == 0);
    assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

    // The site: list, hash lookup and fetch are the filesystem's; its name is the cart's.
    {
        u8* buffer = malloc(TIC_CART_SAVE_SIZE);
        assert(buffer);

        s32 size = tic_cart_save(&tic->cart, buffer);
        assert(size > 0);

        tic_fs_changedir(fs, TIC_HOST);

        s32 loaded = host.loaded;
        LoadAnswer answer = {0};

        rom_load_cart_async(&deps, "site.tic", onLoadAnswer, &answer);
        assert(!answer.done);
        assert(strcmp(NetUrl, "/json?fn=dir&path=") == 0);

        answerNetJson(SiteCartListing);
        assert(strcmp(NetUrl, "/cart/abc/site.tic") == 0);

        answerNetData(buffer, size);
        assert(answer.done);
        assert(answer.result.code == FS_OK);
        assert(host.loaded == loaded + 1);
        assert(strcmp(cart.name, "site.tic") == 0);
        assert(strcmp(cart.path, tic_fs_path(fs, "site.tic")) == 0);
        assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

        // The fetched cart is kept in the app's cache.
        {
            s32 cached = 0;
            void* data = tic_fs_loadroot(fs, TIC_CACHE "abc.tic", &cached);
            assert(data);
            assert(cached == size);
            free(data);
        }

        // A name the cached listing lacks is listed for before the refusal;
        // the cart stays.
        {
            s32 before = host.loaded;
            LoadAnswer missing = {0};

            rom_load_cart_async(&deps, "missing.tic", onLoadAnswer, &missing);
            assert(!missing.done);
            assert(NetCallback);
            assert(strcmp(NetUrl, "/json?fn=dir&path=") == 0);

            answerNetJson(SiteCartListing);
            assert(missing.done);
            assert(!NetCallback);
            assert(missing.result.code == FS_NOT_FOUND);
            assert(host.loaded == before);
            assert(strcmp(cart.name, "site.tic") == 0);
        }

        // A refused autosave answers its message and leaves the cart as it was.
        {
            SaveAnswer refused = {0};
            s32 saved = host.saved;

            rom_autosave(&deps, onSaveAnswer, &refused);
            assert(refused.done);
            assert(refused.result.code == FS_DENIED);
            assert(host.saved == saved);
            assert(strcmp(cart.name, "site.tic") == 0);
        }

        free(buffer);
        tic_fs_homedir(fs);
    }

    // A file that is not a cart is refused; the machine keeps its cart.
    {
        static const char Notes[] = "not a cartridge";
        assert(fs_write(tic_fs_path(fs, "notes.txt"), Notes, sizeof Notes));

        s32 loaded = host.loaded;
        assert(!rom_load_cart(&deps, "notes.txt"));
        assert(host.loaded == loaded);
        assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

        LoadAnswer answer = {0};
        rom_load_cart_async(&deps, "notes.txt", onLoadAnswer, &answer);
        assert(answer.done);
        assert(answer.result.code == FS_UNSUPPORTED);
        assert(host.loaded == loaded);
        assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

        tic_fs_delfile(fs, "notes.txt");
    }

    // A .png that is not a cart is refused too, not loaded as an empty cart.
    {
        static const char Garbage[] = "not a png cart";
        assert(fs_write(tic_fs_path(fs, "bad.png"), Garbage, sizeof Garbage));

        s32 loaded = host.loaded;
        assert(!rom_load_cart(&deps, "bad.png"));
        assert(host.loaded == loaded);

        LoadAnswer answer = {0};
        rom_load_cart_async(&deps, "bad.png", onLoadAnswer, &answer);
        assert(answer.done);
        assert(answer.result.code == FS_UNSUPPORTED);
        assert(host.loaded == loaded);
        assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

        tic_fs_delfile(fs, "bad.png");
    }

    // A drop under the site's cwd is still the process's own file.
    {
        u8* buffer = malloc(TIC_CART_SAVE_SIZE);
        assert(buffer);

        s32 size = tic_cart_save(&tic->cart, buffer);
        assert(size > 0);

        char dropDir[] = "/tmp/tic80-drop-XXXXXX";
        assert(mkdtemp(dropDir));

        char drop[TICNAME_MAX];
        snprintf(drop, sizeof drop, "%s/drop.tic", dropDir);
        assert(fs_write(drop, buffer, size));
        free(buffer);

        tic_tool_poke4(tic->cart.bank0.tiles.data[3].data, 0, 0x5);
        tic_fs_changedir(fs, TIC_HOST);

        s32 loaded = host.loaded;
        assert(rom_load_cart(&deps, drop));

        // The site provider held a listing for the dropped path's folder.
        assert(NetCallback);
        answerNetJson("{\"folders\":[],\"files\":[]}");

        assert(host.loaded == loaded + 1);
        assert(strcmp(cart.name, "drop.tic") == 0);
        assert(strcmp(cart.path, drop) == 0);
        assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

        tic_fs_homedir(fs);
        assert(remove(drop) == 0);
        assert(rmdir(dropDir) == 0);
    }

    // The config's own cart is a save like any other, through the same callbacks.
    {
        SaveAnswer configSaved = {0};
        rom_save_cart(&deps, CONFIG_TIC_PATH, onSaveAnswer, &configSaved);
        assert(configSaved.done);
        assert(configSaved.result.code == FS_OK);
    }
    assert(host.saved == 3);
    assert(host.changed == 2);

    freeConfig(config);

    puts("cart ok");
}

int main(void)
{
    // A root of this run's own: two runs never share one.
    char root[] = "/tmp/tic80-services-XXXXXX";
    assert(mkdtemp(root));

    tic_fs* fs = tic_fs_create(root, NULL);
    assert(fs);

    tic_fs_makedir(fs, TIC_LOCAL);
    tic_fs_makedir(fs, TIC_LOCAL_VERSION);
    tic_fs_makedir(fs, TIC_CACHE);
    tic_fs_makedir(fs, "downloads");

    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    testFilesystem(fs);
    testConfig(tic, fs);
    testCart(tic, fs);

    tic_core_close(tic);

    tic_fs_delfile(fs, "fs-smoke.tic");
    tic_fs_delfile(fs, "smoke.tic");
    tic_fs_delfile(fs, "downloads/smoke.tic");
    tic_fs_delfile(fs, CONFIG_TIC_PATH);
    tic_fs_delfile(fs, TIC_LOCAL_VERSION "options.json");
    tic_fs_delfile(fs, TIC_CACHE "abc.tic");
    tic_fs_deldir(fs, "downloads");
    tic_fs_deldir(fs, TIC_CACHE);
    tic_fs_deldir(fs, TIC_LOCAL_VERSION);
    tic_fs_deldir(fs, TIC_LOCAL);
    tic_fs_deldir(fs, "");

    puts("services ok");
    return 0;
}
