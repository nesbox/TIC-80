/* The gamepad header with its test backend: the devices are set here, so the
 * slot policy, the deadzone and the stick run with no pad plugged in and on
 * every platform — the CI's Linux included.
 *
 * Target sgamepad-test in cmake/tests.cmake; `ctest -R sgamepad` runs it.
 */
#define SGAMEPAD_IMPL
#define SGAMEPAD_TEST
#include "system/sokol/sgamepad.h"

#include <assert.h>
#include <stdio.h>

// The deadzone the layer is set up with, and the values the tests push the
// stick to: 0.2 is inside it, 0.8 and 1.0 are past it.
#define Deadzone    0.5f
#define Inside      0.2f
#define Outside     0.8f

static void setup(void)
{
    sgamepad_setup(&(sgamepad_desc){ .deadzone = Deadzone });
}

static sgamepad_state pressed_a(void)
{
    sgamepad_state state = { 0 };
    state.a = true;
    return state;
}

static sgamepad_state pressed_b(void)
{
    sgamepad_state state = { 0 };
    state.b = true;
    return state;
}

static sgamepad_state stick(float x, float y)
{
    sgamepad_state state = { 0 };
    state.right_x = x;
    state.right_y = y;
    return state;
}

static bool empty(sgamepad_state state)
{
    return state.data == 0 && !state.back && !state.start
        && state.right_x == 0.0f && state.right_y == 0.0f;
}

// The pad the platform numbers second, used first, takes the first slot; the
// first one, used after it, takes the next.
static void testFirstUsedTakesTheFirstSlot(void)
{
    setup();

    sgamepad_test_device(1, true, pressed_a());
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 1);
    assert(sgamepad_gamepad_state(0).a);
    assert(empty(sgamepad_gamepad_state(1)));

    sgamepad_test_device(0, true, pressed_b());
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 2);

    // The pad that took the first slot keeps it, whatever the platform's
    // numbering says.
    assert(sgamepad_gamepad_state(0).a && !sgamepad_gamepad_state(0).b);
    assert(sgamepad_gamepad_state(1).b && !sgamepad_gamepad_state(1).a);

    puts("first used takes the first slot ok");
}

// A pad that goes away frees its slot and the cart stops seeing its bits.
static void testThePadThatGoesAwayFreesItsSlot(void)
{
    setup();

    sgamepad_test_device(2, true, pressed_a());
    sgamepad_record_state();

    assert(sgamepad_gamepad_state(0).a);

    sgamepad_test_device(2, false, pressed_a());
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 0);
    assert(empty(sgamepad_gamepad_state(0)));

    // The slot it freed is the first one the next pad takes.
    sgamepad_test_device(0, true, pressed_b());
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 1);
    assert(sgamepad_gamepad_state(0).b);

    puts("a pad that goes away frees its slot ok");
}

// A connected pad keeps its slot — pressed or not — and the next pad goes
// after it.
static void testAPadThatStaysKeepsItsSlot(void)
{
    setup();

    sgamepad_test_device(0, true, pressed_a());
    sgamepad_test_device(1, true, pressed_b());
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 2);
    assert(sgamepad_gamepad_state(0).a && sgamepad_gamepad_state(1).b);

    // Both let go: the slots stay theirs, they only stop reporting.
    sgamepad_test_device(0, true, (sgamepad_state){ 0 });
    sgamepad_test_device(1, true, (sgamepad_state){ 0 });
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 2);
    assert(empty(sgamepad_gamepad_state(0)) && empty(sgamepad_gamepad_state(1)));

    // The second pad pressed again lands in its own slot, not the first.
    sgamepad_test_device(1, true, pressed_b());
    sgamepad_record_state();

    assert(sgamepad_gamepad_state(1).b && empty(sgamepad_gamepad_state(0)));

    puts("a pad that stays keeps its slot ok");
}

// A device the backend cannot place is one it never reports; one it reports
// but nobody has touched yet takes no slot either — a pad is in a slot once
// something on it moves.
static void testAnUntouchedDeviceTakesNoSlot(void)
{
    setup();

    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 0);
    assert(empty(sgamepad_gamepad_state(0)));

    sgamepad_test_device(3, true, (sgamepad_state){ 0 });
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 0);
    assert(empty(sgamepad_gamepad_state(0)));

    puts("an untouched device takes no slot ok");
}

// The deadzone: what is inside it is nothing — and a stick inside it is not
// the use that takes a slot, either.
static void testTheDeadzone(void)
{
    setup();

    sgamepad_test_device(0, true, stick(Inside, -Inside));
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 0);
    assert(empty(sgamepad_gamepad_state(0)));

    sgamepad_test_device(0, true, stick(Outside, -Outside));
    sgamepad_record_state();

    assert(sgamepad_num_gamepads() == 1);
    assert(sgamepad_gamepad_state(0).right_x == Outside);
    assert(sgamepad_gamepad_state(0).right_y == -Outside);

    puts("the deadzone ok");
}

// The stick travels with the pad it belongs to, and reads back to zero when it
// is let go.
static void testTheStickTravelsWithThePad(void)
{
    setup();

    sgamepad_test_device(1, true, stick(1.0f, 0.0f));
    sgamepad_record_state();

    assert(sgamepad_gamepad_state(0).right_x == 1.0f);
    assert(sgamepad_gamepad_state(0).right_y == 0.0f);

    sgamepad_test_device(1, true, pressed_a());
    sgamepad_record_state();

    assert(sgamepad_gamepad_state(0).a);
    assert(sgamepad_gamepad_state(0).right_x == 0.0f);

    puts("the stick travels with the pad ok");
}

// The slots the layer does not carry read back as nothing rather than as
// whatever an index past the end would have found.
static void testTheSlotsPastTheEnd(void)
{
    setup();

    sgamepad_test_device(0, true, pressed_a());
    sgamepad_record_state();

    assert(empty(sgamepad_gamepad_state(-1)));
    assert(empty(sgamepad_gamepad_state(SGAMEPAD_MAX_GAMEPADS)));
    assert(empty(sgamepad_gamepad_state(100)));

    puts("the slots past the end ok");
}

int main(void)
{
    testFirstUsedTakesTheFirstSlot();
    testThePadThatGoesAwayFreesItsSlot();
    testAPadThatStaysKeepsItsSlot();
    testAnUntouchedDeviceTakesNoSlot();
    testTheDeadzone();
    testTheStickTravelsWithThePad();
    testTheSlotsPastTheEnd();

    puts("sgamepad ok");
    return 0;
}
