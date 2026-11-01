/* The launch's cart without the studio: an app image built with the format's
 * own writer, read back through the platform's two readers — the test is the
 * platform there — with a fake host that counts the word a load is.
 *
 * Target boot-test in cmake/tests.cmake; `ctest -R boot-test` runs it.
 */
#include "studio/boot.h"
#include "studio/embed.h"
#include "studio/fs.h"
#include "cart.h"
#include "tools.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum
{
    AppSize = 64,
    Tile = 0xa,
};

static tic_mem* Tic;
static s32 LoadedCalls;
static char ImagePath[TICNAME_MAX];

static void onLoaded(void* host)
{
    // In the order the spec names it: the cart and the flag are there first.
    assert(((Boot*)host)->cart);
    assert(tic_tool_peek4(Tic->cart.bank0.tiles.data[0].data, 0) == Tile);

    LoadedCalls++;
}

// The platform the launch asks: the app path is the file this test wrote, and
// a read is a real read of it.
const char* fs_apppath(void)
{
    return ImagePath;
}

void* fs_read(const char* path, s32* size)
{
    FILE* file = fopen(path, "rb");

    if(!file) return NULL;

    fseek(file, 0, SEEK_END);
    long len = ftell(file);
    fseek(file, 0, SEEK_SET);

    void* data = malloc(len);

    if(data && fread(data, len, 1, file) != 1)
    {
        free(data);
        data = NULL;
    }

    fclose(file);

    *size = (s32)len;

    return data;
}

static void writeImage(const void* data, s32 size)
{
    if(ImagePath[0])
        remove(ImagePath);

    char path[] = "/tmp/tic80-boot-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);

    ssize_t done = write(fd, data, size);
    close(fd);

    if(done != (ssize_t)size)
    {
        fprintf(stderr, "boot-test: image write failed\n");
        abort();
    }

    snprintf(ImagePath, sizeof ImagePath, "%s", path);
}

// The cart the image carries is the one that comes out of the machine.
static void testEmbeddedCart(void)
{
    u8 app[AppSize] = {0};

    tic_tool_poke4(Tic->cart.bank0.tiles.data[0].data, 0, Tile);

    // A palette byte of its own: a cart without one gets the legacy DB16
    // palette on load, and the round trip would not match byte for byte.
    Tic->cart.bank0.palette.vbank0.data[0] = 1;

    u8* ref = malloc(TIC_CART_SAVE_SIZE);
    assert(ref);
    s32 refSize = tic_cart_save(&Tic->cart, ref);

    s32 imageSize = AppSize;
    void* image = embed_cart(Tic, app, &imageSize);
    assert(image);
    writeImage(image, imageSize);
    free(image);

    // Out of the machine, so the load is what puts it back.
    tic_tool_poke4(Tic->cart.bank0.tiles.data[0].data, 0, 0);

    Boot boot = {0};
    LoadedCalls = 0;

    boot_init(&boot, &(BootDeps){.host = &boot, .tic = Tic, .loaded = onLoaded}, NULL);

    assert(boot.cart);
    assert(LoadedCalls == 1);

    // What came out is what went in, all of it.
    u8* got = malloc(TIC_CART_SAVE_SIZE);
    assert(got);
    assert(tic_cart_save(&Tic->cart, got) == refSize);
    assert(memcmp(ref, got, refSize) == 0);

    free(got);
    free(ref);
}

// An image with no cart, and one whose signature does not match, leave the
// machine, the flag and the host alone.
static void testNoCart(void)
{
    u8 app[AppSize] = {0};

    tic_tool_poke4(Tic->cart.bank0.tiles.data[0].data, 0, Tile);
    writeImage(app, AppSize);

    Boot boot = {0};
    LoadedCalls = 0;

    boot_init(&boot, &(BootDeps){.host = &boot, .tic = Tic, .loaded = onLoaded}, NULL);

    assert(!boot.cart);
    assert(LoadedCalls == 0);
    assert(tic_tool_peek4(Tic->cart.bank0.tiles.data[0].data, 0) == Tile);

    s32 imageSize = AppSize;
    void* image = embed_cart(Tic, app, &imageSize);
    assert(image);

    // The signature is there, but no longer the one the scan looks for.
    ((u8*)image)[AppSize] ^= 0xff;
    writeImage(image, imageSize);
    free(image);

    // Out of the machine, as above: only a load could put the tile back.
    tic_tool_poke4(Tic->cart.bank0.tiles.data[0].data, 0, 0);

    boot_init(&boot, &(BootDeps){.host = &boot, .tic = Tic, .loaded = onLoaded}, NULL);

    assert(!boot.cart);
    assert(LoadedCalls == 0);
    assert(tic_tool_peek4(Tic->cart.bank0.tiles.data[0].data, 0) == 0);
}

int main(void)
{
    Tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(Tic);

    testEmbeddedCart();
    testNoCart();

    tic_core_close(Tic);

    remove(ImagePath);

    puts("boot ok");
    return 0;
}
