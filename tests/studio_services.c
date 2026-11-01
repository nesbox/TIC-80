/* The studio's three services without the studio: a filesystem on its own
 * temporary root, a config written and read back, and a cart saved, reloaded,
 * auto-saved and loaded by hash — every notification recorded by the test. */
#include "studio/fs.h"
#include "studio/config.h"
#include "studio/rom.h"
#include "studio/paths.h"
#include "core/core.h"
#include "tools.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

static void testFilesystem(tic_fs* fs)
{
    const char* name = "fs-smoke.tic";
    static const u8 payload[] = {0xde, 0xad, 0xbe, 0xef};

    tic_fs_delfile(fs, name);

    assert(tic_fs_save(fs, name, payload, sizeof payload, false));
    assert(tic_fs_exists(fs, name));

    // Without overwrite the second save is refused, not silently allowed.
    assert(!tic_fs_save(fs, name, payload, sizeof payload, false));

    s32 size = 0;
    u8* data = tic_fs_load(fs, name, &size);
    assert(data);
    assert(size == (s32)sizeof payload);
    assert(memcmp(data, payload, sizeof payload) == 0);
    free(data);

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

static void onHashDone(void* data)
{
    *(bool*)data = true;
}

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

    // The saved name gets its extension, the identity takes it, the host hears.
    assert(rom_save_cart(&deps, "smoke") == CART_SAVE_OK);
    assert(host.saved == 1);
    assert(strcmp(cart.name, "smoke.tic") == 0);
    assert(strcmp(cart.path, tic_fs_path(fs, "smoke.tic")) == 0);

    tic_tool_poke4(tic->cart.bank0.tiles.data[3].data, 0, 0);
    rom_reload_cart(&deps);
    assert(host.loaded == 1);
    assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

    // The autosave goes to its own folder under the same name.
    assert(rom_autosave(&deps) == CART_SAVE_OK);
    assert(host.saved == 2);
    assert(tic_fs_exists(fs, "downloads/smoke.tic"));

    // A load by name reads the file into the machine and resets the identity.
    tic_tool_poke4(tic->cart.bank0.tiles.data[3].data, 0, 0);
    assert(rom_load_cart(&deps, "smoke.tic"));
    assert(host.loaded == 2);
    assert(strcmp(cart.name, "smoke.tic") == 0);
    assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

    // A cached cart loads by hash with no network at all.
    {
        s32 size = 0;
        u8* data = tic_fs_load(fs, "smoke.tic", &size);
        assert(data);
        assert(tic_fs_saveroot(fs, TIC_CACHE "hash-cache.tic", data, size, true));
        free(data);
    }

    tic_tool_poke4(tic->cart.bank0.tiles.data[3].data, 0, 0);
    bool hashDone = false;
    rom_load_by_hash(&deps, "hash-cart", "hash-cache", NULL, onHashDone, &hashDone);
    assert(hashDone);
    assert(host.loaded == 3);
    assert(strcmp(cart.name, "hash-cart") == 0);
    assert(strcmp(cart.path, tic_fs_path(fs, "hash-cart")) == 0);
    assert(tic_tool_peek4(tic->cart.bank0.tiles.data[3].data, 0) == 0xa);

    // The config's own cart is a save like any other, through the same callbacks.
    assert(rom_save_cart(&deps, CONFIG_TIC_PATH) == CART_SAVE_OK);
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
    tic_fs_delfile(fs, TIC_CACHE "hash-cache.tic");
    tic_fs_deldir(fs, "downloads");
    tic_fs_deldir(fs, TIC_CACHE);
    tic_fs_deldir(fs, TIC_LOCAL_VERSION);
    tic_fs_deldir(fs, TIC_LOCAL);
    tic_fs_deldir(fs, "");

    puts("services ok");
    return 0;
}
