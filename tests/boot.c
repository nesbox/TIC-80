/* The format's two ends, driven into each other: boot_embedCart writes a cart
 * into an app image, boot_findCart reads it back out. */
#include "studio/boot.h"
#include "cart.h"
#include <assert.h>
#include <stdio.h>

// The app's own bytes, carrying a signature the scan has to pass over: the
// header behind it claims zero sizes, so it cannot add up to the image's.
enum {AppSize = 64, DecoyAt = 8};

static void testFindCart(tic_mem* tic)
{
    u8 app[AppSize] = {0};
    memcpy(app + DecoyAt, CART_SIG, STRLEN(CART_SIG));

    // What the cart serializes to: what comes back has to equal these bytes.
    // On the heap, because a cartridge is 1.4M and a stack is not always 8M.
    u8* ref = malloc(sizeof(tic_cartridge));
    s32 refSize = tic_cart_save(&tic->cart, ref);

    s32 imageSize = AppSize;
    void* image = boot_embedCart(tic, app, &imageSize);
    assert(image);
    assert(imageSize > AppSize);

    s32 foundSize = 0;
    u8* found = boot_findCart(image, imageSize, &foundSize);
    assert(found);
    assert(foundSize == refSize);
    assert(memcmp(found, ref, refSize) == 0);

    free(found);
    free(ref);
    free(image);
    puts("find ok");
}

static void testNoCart(void)
{
    u8 app[AppSize] = {0};
    s32 foundSize = 0;

    assert(!boot_findCart(app, AppSize, &foundSize));

    // A signature with nothing behind it is passed over, not read as a header.
    memcpy(app + DecoyAt, CART_SIG, STRLEN(CART_SIG));
    assert(!boot_findCart(app, AppSize, &foundSize));

    puts("no cart ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    // A cart with something in it: an all-zero one would not catch a payload
    // that came back mangled.
    for(s32 i = 0; i < 4; i++)
        tic_tool_poke4(tic->cart.bank0.tiles.data[i].data, 0, i + 1);

    testFindCart(tic);
    testNoCart();

    tic_core_close(tic);
    puts("boot ok");
    return 0;
}
