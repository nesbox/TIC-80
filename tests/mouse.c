/* The pointer and the buttons link without the studio: this file builds mouse.c
 * with the core and nothing else, so a call into studio.c fails to link here
 * rather than appearing quietly in the app build.
 *
 * Target mouse-test in cmake/tests.cmake; `ctest -R mouse` runs it.
 */
#include "studio/mouse.h"
#include <assert.h>
#include <stdio.h>

static const tic_rect Rect = {.x = 10, .y = 20, .w = 30, .h = 40};

// Where the machine says the pointer is: relative mode reports it straight,
// with no letterbox to subtract.
static void putPointer(tic_mem* tic, s32 x, s32 y)
{
    tic->ram->input.mouse.relative = true;
    tic->ram->input.mouse.rx = x;
    tic->ram->input.mouse.ry = y;
}

static void testOver(tic_mem* tic)
{
    putPointer(tic, Rect.x, Rect.y);
    assert(mouse_over(tic, &Rect));

    // The rect is half open: its right and bottom edges are outside it.
    putPointer(tic, Rect.x + Rect.w, Rect.y);
    assert(!mouse_over(tic, &Rect));

    putPointer(tic, Rect.x, Rect.y + Rect.h);
    assert(!mouse_over(tic, &Rect));

    puts("over ok");
}

static void testClick(void)
{
    MouseState state = {.click = true, .start = {.x = 15, .y = 25}, .end = {.x = 15, .y = 25}};

    // A click inside is consumed, so a widget below it never sees it too.
    assert(mouse_click(&state, &Rect));
    assert(!mouse_click(&state, &Rect));

    // One that started or ended outside is not this rect's to take.
    state = (MouseState){.click = true, .start = {.x = 15, .y = 25}, .end = {.x = 5, .y = 25}};
    assert(!mouse_click(&state, &Rect));
    assert(state.click);

    puts("click ok");
}

static void testDblClick(void)
{
    MouseState state = {.dbl = {.click = true}, .start = {.x = 15, .y = 25}, .end = {.x = 15, .y = 25}};

    assert(mouse_dblclick(&state, &Rect));
    assert(!mouse_dblclick(&state, &Rect));

    puts("double click ok");
}

static void testDown(void)
{
    MouseState state = {.down = true, .start = {.x = 15, .y = 25}};

    assert(mouse_down(&state, &Rect));

    state.start = (tic_point){.x = 5, .y = 25};
    assert(!mouse_down(&state, &Rect));

    puts("down ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    testOver(tic);
    testClick();
    testDblClick();
    testDown();

    tic_core_close(tic);
    puts("mouse ok");
    return 0;
}
