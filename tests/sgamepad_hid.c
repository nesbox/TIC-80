/* The PlayStation and Switch readers, driven by reports rather than by pads.
 *
 * A platform backend can only be checked with the pad in hand, except for this
 * part of it: the report parsing is a pure function, so a table of reports and
 * the bits each must produce covers what the bench would otherwise be the only
 * way to see. The reports are the ones the design names — the Switch ones from
 * the owner's 8BitDo, the PlayStation ones from the virtual DualShock 4 of the
 * bench, over both connections.
 *
 * Target sgamepad-hid-test in cmake/tests.cmake; `ctest -R sgamepad-hid` runs it.
 */
#define SGAMEPAD_HID_IMPL
#include "system/sokol/sgamepad_hid.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static sgamepad_hid_state read_ps4(const uint8_t* report, size_t length)
{
    sgamepad_hid_state state = { 0 };

    assert(sgamepad_hid_ps4_report(report, length, &state));
    return state;
}

static sgamepad_hid_state read_ps5(const uint8_t* report, size_t length)
{
    sgamepad_hid_state state = { 0 };

    assert(sgamepad_hid_ps5_report(report, length, &state));
    return state;
}

static sgamepad_hid_state read_switch(const uint8_t* report, size_t length)
{
    sgamepad_hid_state state = { 0 };

    assert(sgamepad_hid_switch_report(report, length, &state));
    return state;
}

// The buttons of one report, as a word the test compares against the ones the
// reader is expected to produce.
static unsigned buttons(sgamepad_hid_state state)
{
    return (state.up ? 1u : 0u) | (state.down ? 2u : 0u)
        | (state.left ? 4u : 0u) | (state.right ? 8u : 0u)
        | (state.a ? 16u : 0u) | (state.b ? 32u : 0u)
        | (state.x ? 64u : 0u) | (state.y ? 128u : 0u)
        | (state.back ? 256u : 0u) | (state.start ? 512u : 0u);
}

#define UP 1u
#define DOWN 2u
#define LEFT 4u
#define RIGHT 8u
#define A 16u
#define B 32u
#define X 64u
#define Y 128u
#define BACK 256u
#define START 512u

// ---------------------------------------------------------------- PlayStation

// A DualShock 4 state packet: the sticks, then the three button bytes. The
// reports below are built from this, one field at a time.
#define PS4_USB_REPORT_SIZE 64

static void ps4_state(uint8_t* report, uint8_t buttons, uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry)
{
    memset(report, 0, PS4_USB_REPORT_SIZE);

    report[0] = 0x01;                       // the USB state report id
    report[1] = lx;                         // the four stick axes, then three
    report[2] = ly;                         // bytes of buttons — the hat and
    report[3] = rx;                         // the face buttons in the first,
    report[4] = ry;                         // the shoulders and start in the
    report[5] = buttons;                    // second, the rest in the third
    report[6] = 0x08;                       // neither shoulder nor trigger is digital
}

// The face buttons, which sit in the high nibble of the first button byte.
static void testPS4FaceButtons(void)
{
    static const struct { uint8_t byte; unsigned expected; } Cases[] = {
        { 0x20, A },                        // the bottom one, `cross`
        { 0x40, B },                        // the right one, `circle`
        { 0x10, X },                        // the left one, `square`
        { 0x80, Y },                        // the top one, `triangle`
    };

    for (size_t i = 0; i < sizeof Cases / sizeof Cases[0]; i++)
    {
        uint8_t report[PS4_USB_REPORT_SIZE];

        // The hat nibble says nothing is held, so only the face bit is read.
        ps4_state(report, Cases[i].byte | 0x08, 0x80, 0x80, 0x80, 0x80);

        const sgamepad_hid_state state = read_ps4(report, sizeof report);

        assert(buttons(state) == Cases[i].expected);
    }

    puts("ps4 face buttons ok");
}

// The direction pad is the low nibble: a direction, or the neighbouring two on
// the diagonals, and `8` for nothing.
static void testPS4Hat(void)
{
    static const struct { uint8_t hat; unsigned expected; } Cases[] = {
        { 0x00, UP },
        { 0x01, UP | RIGHT },
        { 0x02, RIGHT },
        { 0x03, RIGHT | DOWN },
        { 0x04, DOWN },
        { 0x05, DOWN | LEFT },
        { 0x06, LEFT },
        { 0x07, UP | LEFT },
        { 0x08, 0 },
    };

    for (size_t i = 0; i < sizeof Cases / sizeof Cases[0]; i++)
    {
        uint8_t report[PS4_USB_REPORT_SIZE];

        ps4_state(report, Cases[i].hat, 0x80, 0x80, 0x80, 0x80);

        const sgamepad_hid_state state = read_ps4(report, sizeof report);

        assert(buttons(state) == Cases[i].expected);
    }

    puts("ps4 hat ok");
}

// Back and start are the share and options buttons, in the second byte.
static void testPS4BackAndStart(void)
{
    uint8_t report[PS4_USB_REPORT_SIZE];

    ps4_state(report, 0x08, 0x80, 0x80, 0x80, 0x80);
    report[6] = 0x10;
    assert(buttons(read_ps4(report, sizeof report)) == BACK);

    ps4_state(report, 0x08, 0x80, 0x80, 0x80, 0x80);
    report[6] = 0x20;
    assert(buttons(read_ps4(report, sizeof report)) == START);

    // A shoulder and a stick pressed in are no bit of the machine's: they are
    // read, and read as nothing, rather than as a button nobody pressed.
    ps4_state(report, 0x08, 0x80, 0x80, 0x80, 0x80);
    report[6] = 0x01;
    assert(buttons(read_ps4(report, sizeof report)) == 0);

    puts("ps4 back and start ok");
}

// The sticks: an 8-bit axis centered on 0x80, with y read the machine's way —
// down the screen — where the pad sends it up.
static void testPS4Sticks(void)
{
    uint8_t report[PS4_USB_REPORT_SIZE];

    ps4_state(report, 0x08, 0x80, 0x80, 0x80, 0x80);

    const sgamepad_hid_state centered = read_ps4(report, sizeof report);

    assert(centered.right_x == 0.0f && centered.right_y == 0.0f);

    // Hard left, and hard up on the pad — which the machine reads as y away
    // from the top of the screen, so negative.
    ps4_state(report, 0x08, 0x80, 0x80, 0x00, 0xFF);

    const sgamepad_hid_state pushed = read_ps4(report, sizeof report);

    assert(pushed.right_x == -1.0f);
    assert(pushed.right_y < -0.99f);

    // Hard right, and hard down.
    ps4_state(report, 0x08, 0x80, 0x80, 0xFF, 0x00);

    const sgamepad_hid_state other = read_ps4(report, sizeof report);

    assert(other.right_x > 0.99f);
    assert(other.right_y > 0.99f);

    puts("ps4 sticks ok");
}

// The same packet over Bluetooth: a different report id, the report id again
// after the length byte, and a CRC32 over the lot.
static void ps4_bluetooth(uint8_t* report, uint8_t pressed)
{
    memset(report, 0, 78);

    report[0] = 0x11;                       // the Bluetooth state report id
    report[1] = 0xC0;                       // HID data is present in this one
    report[2] = 0x11;                       // the report id again

    // The state packet starts at 3 over Bluetooth — two later than the USB
    // report's, which has only the report id before it. So the sticks take
    // 4..6 and the buttons start at 7, two past where the USB report has them.
    report[4] = 0x80;
    report[5] = 0x80;
    report[6] = 0x80;
    report[7] = pressed;                    // the face buttons and the hat
    report[8] = 0x08;                       // neither shoulder nor trigger is digital

    const uint8_t header = 0xA1;
    const uint32_t crc = sgamepad_hid_crc32(sgamepad_hid_crc32(0, &header, 1), report, 74);

    report[74] = (uint8_t)(crc);
    report[75] = (uint8_t)(crc >> 8);
    report[76] = (uint8_t)(crc >> 16);
    report[77] = (uint8_t)(crc >> 24);
}

static void testPS4Bluetooth(void)
{
    uint8_t report[78];
    sgamepad_hid_state state = { 0 };

    ps4_bluetooth(report, 0x20);            // cross, and the hat saying up

    assert(sgamepad_hid_ps4_report(report, sizeof report, &state));
    assert(buttons(state) == (A | UP));

    // A report whose CRC does not add up is not one the pad sent: it is
    // dropped rather than read as buttons nobody pressed.
    report[40] ^= 0xFF;
    memset(&state, 0, sizeof state);

    assert(!sgamepad_hid_ps4_report(report, sizeof report, &state));
    assert(buttons(state) == 0);

    puts("ps4 bluetooth ok");
}

// --------------------------------------------------------------- DualSense

#define PS5_USB_REPORT_SIZE 64

static void ps5_state(uint8_t* report, uint8_t hat, uint8_t face, uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry)
{
    memset(report, 0, PS5_USB_REPORT_SIZE);

    report[0] = 0x01;                       // the USB state report id
    report[1] = lx;
    report[2] = ly;
    report[3] = rx;
    report[4] = ry;
    report[6] = 0;                          // the counter
    report[8] = hat | face;                 // the buttons and the hat, one byte later than the DualShock 4
    report[9] = 0x01;                       // a shoulder held, and nothing the machine reads
}

static void testPS5DualSense(void)
{
    // The same nibbles, at the offsets the DualSense uses.
    uint8_t report[PS5_USB_REPORT_SIZE];

    ps5_state(report, 0x08, 0x20, 0x80, 0x80, 0x80, 0x80);
    assert(buttons(read_ps5(report, sizeof report)) == A);

    ps5_state(report, 0x08, 0x40, 0x80, 0x80, 0x80, 0x80);
    assert(buttons(read_ps5(report, sizeof report)) == B);

    ps5_state(report, 0x08, 0x80, 0x80, 0x80, 0x80, 0x80);
    assert(buttons(read_ps5(report, sizeof report)) == Y);

    // Down on the hat, and the right stick hard right and hard down.
    ps5_state(report, 0x04, 0x00, 0x80, 0x80, 0xFF, 0x00);

    const sgamepad_hid_state state = read_ps5(report, sizeof report);

    assert(buttons(state) == DOWN);
    assert(state.right_x > 0.99f);
    assert(state.right_y > 0.99f);

    puts("ps5 dualsense ok");
}

// Over Bluetooth the report id is 0x31 and the packet starts one byte later
// than the DualShock 4's, both after the report id and after the HID byte.
static void testPS5Bluetooth(void)
{
    uint8_t report[78];
    sgamepad_hid_state state = { 0 };

    memset(report, 0, sizeof report);

    // The state packet starts at 2 here — the report id, and the byte the
    // Bluetooth stack puts before it. Its sticks take 2..5, then the counter
    // and the buttons, whose first byte lands at 9.
    report[0] = 0x31;
    report[1] = 0xC0;
    report[9] = 0x40 | 0x08;                // circle, and the hat saying nothing
    report[10] = 0x01;                      // the mute and function buttons

    const uint8_t header = 0xA1;
    const uint32_t crc = sgamepad_hid_crc32(sgamepad_hid_crc32(0, &header, 1), report, 74);

    report[74] = (uint8_t)(crc);
    report[75] = (uint8_t)(crc >> 8);
    report[76] = (uint8_t)(crc >> 16);
    report[77] = (uint8_t)(crc >> 24);

    assert(sgamepad_hid_ps5_report(report, sizeof report, &state));
    assert(buttons(state) == B);

    // And a CRC that does not add up is dropped.
    report[30] ^= 0x5A;
    memset(&state, 0, sizeof state);

    assert(!sgamepad_hid_ps5_report(report, sizeof report, &state));

    puts("ps5 bluetooth ok");
}

// ------------------------------------------------------------------- Switch

// The simple report: two button bytes, the hat, then the sticks as 16-bit
// values centered on 0x8000.
// The left and right sticks are written together, little-endian as the wire
// carries them, so a case names what both are doing.
static void switch_state(uint8_t* report, uint8_t low, uint8_t high, uint8_t hat, uint16_t lx, uint16_t ly, uint16_t rx, uint16_t ry)
{
    memset(report, 0, 12);

    report[0] = 0x3F;
    report[1] = low;
    report[2] = high;
    report[3] = hat;

    const uint16_t sticks[4] = { lx, ly, rx, ry };

    for (size_t i = 0; i < 4; i++)
    {
        report[4 + i * 2] = (uint8_t)(sticks[i]);
        report[5 + i * 2] = (uint8_t)(sticks[i] >> 8);
    }
}

static void testSwitchFaceButtons(void)
{
    // The pad names its buttons the other way round from where they sit: `b`
    // is the bottom one and `a` the right one, and the machine reads positions.
    static const struct { uint8_t byte; unsigned expected; } Cases[] = {
        { 0x01, A },                        // the bottom one, which the pad names `b`
        { 0x02, B },                        // the right one, which the pad names `a`
        { 0x04, Y },                        // the left one, which the pad names `y`
        { 0x08, X },                        // the top one, which the pad names `x`
        { 0x40, 0 },                        // a shoulder: read, and no bit
        { 0x80, 0 },                        // the other one
    };

    for (size_t i = 0; i < sizeof Cases / sizeof Cases[0]; i++)
    {
        uint8_t report[12];

        switch_state(report, Cases[i].byte, 0x00, 0x08, 0x8000, 0x8000, 0x8000, 0x8000);

        assert(buttons(read_switch(report, sizeof report)) == Cases[i].expected);
    }

    puts("switch face buttons ok");
}

static void testSwitchHatAndSticks(void)
{
    uint8_t report[12];

    // The minus and plus buttons, which the machine reads as back and start,
    // sharing their byte with the stick-presses the layer does not read.
    switch_state(report, 0x00, 0x01, 0x08, 0x8000, 0x8000, 0x8000, 0x8000);
    assert(buttons(read_switch(report, sizeof report)) == BACK);

    switch_state(report, 0x00, 0x02, 0x08, 0x8000, 0x8000, 0x8000, 0x8000);
    assert(buttons(read_switch(report, sizeof report)) == START);

    // A stick pressed in shares that byte, and reads as nothing either.
    switch_state(report, 0x00, 0x04, 0x08, 0x8000, 0x8000, 0x8000, 0x8000);
    assert(buttons(read_switch(report, sizeof report)) == 0);

    // The hat: a direction, both of its bits on the diagonals, and nothing.
    switch_state(report, 0x00, 0x00, 0x01, 0x8000, 0x8000, 0x8000, 0x8000);
    assert(buttons(read_switch(report, sizeof report)) == (UP | RIGHT));

    switch_state(report, 0x00, 0x00, 0x08, 0x8000, 0x8000, 0x8000, 0x8000);

    const sgamepad_hid_state centered = read_switch(report, sizeof report);

    assert(buttons(centered) == 0);
    assert(centered.right_x == 0.0f && centered.right_y == 0.0f);

    // A stick held hard right and down, where the pad's y grows upwards.
    switch_state(report, 0x00, 0x00, 0x08, 0x8000, 0x8000, 0xFFFF, 0x0000);

    const sgamepad_hid_state pushed = read_switch(report, sizeof report);

    assert(pushed.right_x > 0.99f);
    assert(pushed.right_y > 0.99f);

    puts("switch hat and sticks ok");
}

// A report of another id, or one too short to hold what it must, is left
// alone: an unclaimed pad is better than one read as buttons nobody pressed.
static void testUnknownReports(void)
{
    uint8_t report[78];

    memset(report, 0, sizeof report);

    report[0] = 0x30;                       // the Switch full report, which is not read here

    sgamepad_hid_state state = { 0 };

    assert(!sgamepad_hid_switch_report(report, 12, &state));
    assert(!sgamepad_hid_switch_report(report, 3, &state));
    assert(!sgamepad_hid_ps4_report(report, sizeof report, &state));
    assert(!sgamepad_hid_ps5_report(report, sizeof report, &state));
    assert(!sgamepad_hid_ps4_report(report, 2, &state));
    assert(!sgamepad_hid_ps5_report(report, 2, &state));

    // A Bluetooth report the pad sent carries its id, but one whose CRC is not
    // there is not read either.
    report[0] = 0x31;

    assert(!sgamepad_hid_ps5_report(report, sizeof report, &state));

    puts("unknown reports ok");
}

// The families, by the vendor and product ids SDL's drivers claim.
static void testFamilies(void)
{
    assert(sgamepad_hid_family_of(0x054C, 0x05C4) == SGAMEPAD_HID_PS4);
    assert(sgamepad_hid_family_of(0x054C, 0x09CC) == SGAMEPAD_HID_PS4);
    assert(sgamepad_hid_family_of(0x054C, 0x0CE6) == SGAMEPAD_HID_PS5);
    assert(sgamepad_hid_family_of(0x054C, 0x0DF2) == SGAMEPAD_HID_PS5);
    assert(sgamepad_hid_family_of(0x057E, 0x2009) == SGAMEPAD_HID_SWITCH);
    assert(sgamepad_hid_family_of(0x057E, 0x2007) == SGAMEPAD_HID_SWITCH);

    // A wheel, a keyboard, and XInput's own pads are none of these readers'.
    assert(sgamepad_hid_family_of(0x045E, 0x028E) == SGAMEPAD_HID_NONE);
    assert(sgamepad_hid_family_of(0x054C, 0x0268) == SGAMEPAD_HID_NONE);
    assert(sgamepad_hid_family_of(0x046D, 0xC219) == SGAMEPAD_HID_NONE);

    puts("families ok");
}

// The keepalive the pad needs over Bluetooth: the magic byte in the report the
// pad listens on, and — for the DualSense, which checks — a CRC that adds up.
static void testKeepalive(void)
{
    uint8_t report[78];

    sgamepad_hid_ps4_keepalive(report, sizeof report);

    assert(report[0] == 0x11);
    assert(report[1] == 0xC0);

    sgamepad_hid_ps5_keepalive(report, sizeof report);

    // What the DualSense is asked to listen for, and the sum of everything
    // before the CRC it carries.
    assert(report[0] == 0x31);
    assert(report[1] == 0x02);

    const uint8_t header = 0xA2;
    const uint32_t crc = sgamepad_hid_crc32(sgamepad_hid_crc32(0, &header, 1), report, 74);

    assert(report[74] == (crc & 0xFF));
    assert(report[75] == ((crc >> 8) & 0xFF));
    assert(report[76] == ((crc >> 16) & 0xFF));
    assert(report[77] == ((crc >> 24) & 0xFF));

    puts("keepalive ok");
}

int main(void)
{
    testPS4FaceButtons();
    testPS4Hat();
    testPS4BackAndStart();
    testPS4Sticks();
    testPS4Bluetooth();
    testPS5DualSense();
    testPS5Bluetooth();
    testSwitchFaceButtons();
    testSwitchHatAndSticks();
    testUnknownReports();
    testFamilies();
    testKeepalive();

    puts("sgamepad_hid ok");
    return 0;
}
