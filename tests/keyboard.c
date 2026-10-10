/* The keyboard links without the studio: this file builds keyboard.c with the
 * core and nothing else, so a call into studio.c fails to link here rather than
 * appearing quietly in the app build.
 *
 * Target keyboard-test in cmake/tests.cmake; `ctest -R keyboard` runs it.
 */
#include "studio/keyboard.h"
#include <assert.h>
#include <stdio.h>

// The platform's own text input, which the module asks before its fallback: the
// studio's test is the one place that answers it.
bool tic_sys_keyboard_text(char* text)
{
    (void)text;
    return false;
}

// One key held this frame, with the previous frame's keys empty — which is what
// the machine reads as "just pressed".
static void press(tic_mem* tic, tic_key key)
{
    memset(&tic->ram->input.keyboard, 0, sizeof tic->ram->input.keyboard);
    tic->ram->input.keyboard.keys[0] = key;
}

static void alsoDown(tic_mem* tic, tic_key key)
{
    for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
        if(!tic->ram->input.keyboard.keys[i])
        {
            tic->ram->input.keyboard.keys[i] = key;
            return;
        }
}

static void testPressed(tic_mem* tic)
{
    press(tic, tic_key_a);

    assert(keyboard_pressed(tic, tic_key_a));
    assert(keyboard_pressed_once(tic, tic_key_a));
    assert(!keyboard_pressed(tic, tic_key_b));

    puts("pressed ok");
}

// The machine's own repeat: a held key fires again every few frames, and the
// once-check must not — that is the whole difference between the two, and what
// the four copies the module replaced relied on. The state that makes it a
// repeat lives in the core's tick, so the frames have to be really ticked.
static void testRepeat(tic_mem* tic)
{
    enum {Frames = 40};

    bool repeated = false;

    for(s32 i = 0; i < Frames; i++)
    {
        press(tic, tic_key_a);

        tic_core_tick_start(tic);
        tic_core_tick_io(tic);
        tic_core_tick_end(tic);

        if(i > 0 && keyboard_pressed(tic, tic_key_a))
        {
            // A repeat is not a fresh press.
            assert(!keyboard_pressed_once(tic, tic_key_a));
            repeated = true;
        }
    }

    assert(repeated);

    puts("repeat ok");
}

static void testEnter(tic_mem* tic)
{
    press(tic, tic_key_numpadenter);
    assert(keyboard_enter(tic, KEYBOARD_HOLD, KEYBOARD_PERIOD));
    assert(keyboard_enter(tic, -1, -1));

    press(tic, tic_key_b);
    assert(!keyboard_enter(tic, KEYBOARD_HOLD, KEYBOARD_PERIOD));

    puts("enter ok");
}

static void testText(tic_mem* tic)
{
    press(tic, tic_key_a);
    assert(keyboard_text(tic) == 'a');

    // Shift capitalises the key, caps lock turns that back around.
    alsoDown(tic, tic_key_shift);
    assert(keyboard_text(tic) == 'A');

    alsoDown(tic, tic_key_capslock);
    assert(keyboard_text(tic) == 'a');

    // A ctrl-based shortcut types nothing.
    alsoDown(tic, tic_key_ctrl);
    assert(keyboard_text(tic) == '\0');

    puts("text ok");
}

static void testClipboard(tic_mem* tic)
{
    press(tic, tic_key_c);
    assert(keyboard_clipboard(tic) == TIC_CLIPBOARD_NONE);

    alsoDown(tic, tic_key_ctrl);
    assert(keyboard_clipboard(tic) == TIC_CLIPBOARD_COPY);

    press(tic, tic_key_x);
    alsoDown(tic, tic_key_ctrl);
    assert(keyboard_clipboard(tic) == TIC_CLIPBOARD_CUT);

    // Shift+Insert pastes, the other spelling of Ctrl+V.
    press(tic, tic_key_insert);
    alsoDown(tic, tic_key_shift);
    assert(keyboard_clipboard(tic) == TIC_CLIPBOARD_PASTE);

    puts("clipboard ok");
}

int main(void)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);
    assert(tic);

    testPressed(tic);
    testRepeat(tic);
    testEnter(tic);
    testText(tic);
    testClipboard(tic);

    tic_core_close(tic);
    puts("keyboard ok");
    return 0;
}
