/* The PlayStation and Switch readers, driven by reports rather than by pads.
 *
 * A platform backend can only be checked with the pad in hand, except for this
 * part of it: the report parsing is a pure function, so a table of reports and
 * the bits each must produce covers what the bench would otherwise be the only
 * way to see.
 *
 * The reports are built byte for byte the way SDL's drivers document the pads
 * sending them, one per family and per connection. What that cannot catch is a
 * layout SDL documents wrongly — only a pad on the bench can, which is what
 * the bench tasks are for. Until then these say the readers read what SDL
 * reads, which is the claim this change makes.
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
    sgamepad_hid_state state = {0};

    assert(sgamepad_hid_ps4_report(report, length, &state));
    return state;
}

static sgamepad_hid_state read_ps5(const uint8_t* report, size_t length)
{
    sgamepad_hid_state state = {0};

    assert(sgamepad_hid_ps5_report(report, length, &state));
    return state;
}

static sgamepad_hid_state read_switch(const uint8_t* report, size_t length)
{
    sgamepad_hid_state state = {0};

    assert(sgamepad_hid_switch_report(report, length, &state));
    return state;
}

// The buttons of one report, as a word the test compares against the ones the
// reader is expected to produce.
static unsigned buttons(sgamepad_hid_state state)
{
    // clang-format off
    return (state.up ? 1u : 0u) | (state.down ? 2u : 0u)
        | (state.left ? 4u : 0u) | (state.right ? 8u : 0u)
        | (state.a ? 16u : 0u) | (state.b ? 32u : 0u)
        | (state.x ? 64u : 0u) | (state.y ? 128u : 0u)
        | (state.back ? 256u : 0u) | (state.start ? 512u : 0u);
    // clang-format on
}

#define UP    1u
#define DOWN  2u
#define LEFT  4u
#define RIGHT 8u
#define A     16u
#define B     32u
#define X     64u
#define Y     128u
#define BACK  256u
#define START 512u

// ---------------------------------------------------------------- PlayStation

// A DualShock 4 state packet: the sticks, then the three button bytes. The
// reports below are built from this, one field at a time.
#define PS4_USB_REPORT_SIZE 64

static void ps4_state(uint8_t* report, uint8_t buttons, uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry)
{
    memset(report, 0, PS4_USB_REPORT_SIZE);

    report[0] = 0x01;    // the USB state report id
    report[1] = lx;      // the four stick axes, then three
    report[2] = ly;      // bytes of buttons — the hat and
    report[3] = rx;      // the face buttons in the first,
    report[4] = ry;      // the shoulders and start in the
    report[5] = buttons; // second, the rest in the third
    report[6] = 0x08;    // neither shoulder nor trigger is digital
}

// The face buttons, which sit in the high nibble of the first button byte.
static void testPS4FaceButtons(void)
{
    static const struct
    {
        uint8_t byte;
        unsigned expected;
    } Cases[] = {
        {0x20, A}, // the bottom one, `cross`
        {0x40, B}, // the right one, `circle`
        {0x10, X}, // the left one, `square`
        {0x80, Y}, // the top one, `triangle`
    };

    for(size_t i = 0; i < sizeof Cases / sizeof Cases[0]; i++)
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
    static const struct
    {
        uint8_t hat;
        unsigned expected;
    } Cases[] = {
        {0x00, UP},
        {0x01, UP | RIGHT},
        {0x02, RIGHT},
        {0x03, RIGHT | DOWN},
        {0x04, DOWN},
        {0x05, DOWN | LEFT},
        {0x06, LEFT},
        {0x07, UP | LEFT},
        {0x08, 0},
    };

    for(size_t i = 0; i < sizeof Cases / sizeof Cases[0]; i++)
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

    // Hard left and hard up: the pad sends up as the *smaller* value, and
    // both the machine and SDL read up as negative, so the axis comes back
    // the way it arrived.
    ps4_state(report, 0x08, 0x7F, 0x7F, 0x00, 0x00);

    const sgamepad_hid_state pushed = read_ps4(report, sizeof report);

    assert(pushed.right_x == -1.0f);
    assert(pushed.right_y < -0.99f);

    // Hard right and hard down, which is the larger value.
    ps4_state(report, 0x08, 0x7F, 0x7F, 0xFF, 0xFF);

    const sgamepad_hid_state other = read_ps4(report, sizeof report);

    assert(other.right_x > 0.99f);
    assert(other.right_y > 0.99f);

    puts("ps4 sticks ok");
}

// The left stick reads into the same four bits as the direction pad, which is
// how every other backend steers: the machine has one set of direction bits.
static void testPS4LeftStick(void)
{
    uint8_t report[PS4_USB_REPORT_SIZE];

    // Hard left, hard up — the pad sends up as the smaller value, so the
    // stick is read as the direction pad's left and up.
    ps4_state(report, 0x08, 0x00, 0x00, 0x7F, 0x7F);
    assert(buttons(read_ps4(report, sizeof report)) == (LEFT | UP));

    ps4_state(report, 0x08, 0xFF, 0xFF, 0x7F, 0x7F);
    assert(buttons(read_ps4(report, sizeof report)) == (RIGHT | DOWN));

    // A stick inside the threshold is no direction — which is why the resting
    // value every other case uses comes out as nothing.
    ps4_state(report, 0x08, 0x80, 0x80, 0x7F, 0x7F);

    const sgamepad_hid_state centered = read_ps4(report, sizeof report);

    assert(buttons(centered) == 0);
    assert(centered.right_x < 0.01f && centered.right_x > -0.01f);

    // The state report is not the enhanced state: a pad answering with this
    // one is a pad the keepalive would change rather than keep alive.
    assert(!centered.enhanced);

    // And the stick adds to the direction pad rather than replacing it: the
    // hat says nothing while the stick is hard left, and both come out.
    ps4_state(report, 0x08, 0x00, 0x80, 0x7F, 0x7F);
    assert(buttons(read_ps4(report, sizeof report)) == LEFT);

    // The hat on right, the stick on left: the machine reads both, since they
    // share its four bits.
    ps4_state(report, 0x02, 0x00, 0x80, 0x7F, 0x7F);
    assert(buttons(read_ps4(report, sizeof report)) == (LEFT | RIGHT));

    puts("ps4 left stick ok");
}

// The same packet over Bluetooth: a different report id, the report id again
// after the length byte, and a CRC32 over the lot.
static void ps4_bluetooth(uint8_t* report, uint8_t pressed)
{
    memset(report, 0, 78);

    report[0] = 0x11; // the Bluetooth state report id
    report[1] = 0xC0; // HID data is present in this one
    report[2] = 0x11; // the report id again

    // The state packet starts at 3 over Bluetooth — two later than the USB
    // report's, which has only the report id before it. So the sticks take
    // 3..6 and the buttons start at 7, two past where the USB report has them.
    // Both sticks are centered, so neither of them is a direction.
    report[3] = 0x80;
    report[4] = 0x80;
    report[5] = 0x80;
    report[6] = 0x7F;
    report[7] = pressed; // the face buttons and the hat
    report[8] = 0x08;    // neither shoulder nor trigger is digital

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
    sgamepad_hid_state state = {0};

    ps4_bluetooth(report, 0x20); // cross, and the hat saying up

    assert(sgamepad_hid_ps4_report(report, sizeof report, &state));
    assert(buttons(state) == (A | UP));

    // A pad sending this band is in its enhanced mode, which is the state the
    // keepalive is for: without this a Bluetooth DualShock 4 is never asked to
    // keep talking, and the stream stalls.
    assert(state.enhanced);

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

    report[0] = 0x01; // the USB state report id
    report[1] = lx;
    report[2] = ly;
    report[3] = rx;
    report[4] = ry;
    report[6] = 0;          // the counter
    report[8] = hat | face; // the buttons and the hat, one byte later than the DualShock 4
    report[9] = 0x01;       // a shoulder held, and nothing the machine reads
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
    ps5_state(report, 0x04, 0x00, 0x80, 0x80, 0xFF, 0xFF);

    const sgamepad_hid_state state = read_ps5(report, sizeof report);

    assert(buttons(state) == DOWN);
    assert(state.right_x > 0.99f);
    assert(state.right_y > 0.99f);

    puts("ps5 dualsense ok");
}

// A DualSense that nothing has brought into enhanced mode sends the simple
// layout under the same report id: the buttons sit where a DualShock 4 keeps
// them, not where the DualSense's own layout does. Read as the extended one,
// its trigger bytes come out as the hat and the face buttons.
static void testPS5SimpleReport(void)
{
    // The same bytes the DualShock 4's test uses, under the DualSense's reader.
    uint8_t report[PS4_USB_REPORT_SIZE];

    ps4_state(report, 0x20 | 0x08, 0x80, 0x80, 0x80, 0x80);
    assert(buttons(read_ps5(report, 10)) == A);

    ps4_state(report, 0x08, 0x80, 0x80, 0xFF, 0xFF);

    const sgamepad_hid_state state = read_ps5(report, 10);

    assert(buttons(state) == 0);
    assert(state.right_x > 0.99f);
    assert(state.right_y > 0.99f);

    // And through a Bluetooth report of the same layout, under the same id:
    // the state packet still starts at 1, and 78 bytes is what says the
    // layout is the simple one rather than the extended.
    uint8_t bluetooth[78];

    memset(bluetooth, 0, sizeof bluetooth);

    bluetooth[0] = 0x01;
    bluetooth[1] = 0x7F; // the left stick centered
    bluetooth[2] = 0x7F;
    bluetooth[3] = 0x7F; // and the right one
    bluetooth[4] = 0x7F;
    bluetooth[5] = 0x40 | 0x08; // circle, and the hat saying nothing

    const sgamepad_hid_state over_bt = read_ps5(bluetooth, sizeof bluetooth);

    assert(buttons(over_bt) == B);

    puts("ps5 simple report ok");
}

// Over Bluetooth the report id is 0x31 and the packet starts one byte later
// than the DualShock 4's, both after the report id and after the HID byte.
static void testPS5Bluetooth(void)
{
    uint8_t report[78];
    sgamepad_hid_state state = {0};

    memset(report, 0, sizeof report);

    // The state packet starts at 2 here — the report id, and the byte the
    // Bluetooth stack puts before it. So its four stick axes take 2..5, then
    // the counter, and the buttons, whose first byte lands at 9.
    report[0] = 0x31;
    report[1] = 0xC0;
    report[2] = 0x7F; // centering the sticks as the
    report[3] = 0x7F; // helpers above do, so no
    report[4] = 0x7F; // direction comes out of them
    report[5] = 0x7F;
    report[9] = 0x40 | 0x08; // circle, and the hat saying nothing
    report[10] = 0x01;       // the mute and function buttons

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

    const uint16_t sticks[4] = {lx, ly, rx, ry};

    for(size_t i = 0; i < 4; i++)
    {
        report[4 + i * 2] = (uint8_t)(sticks[i]);
        report[5 + i * 2] = (uint8_t)(sticks[i] >> 8);
    }
}

static void testSwitchFaceButtons(void)
{
    // The pad names its buttons the other way round from where they sit: `b`
    // is the bottom one and `a` the right one, and the machine reads positions.
    static const struct
    {
        uint8_t byte;
        unsigned expected;
    } Cases[] = {
        {0x01, A}, // the bottom one
        {0x02, B}, // the right one
        {0x04, X}, // the left one
        {0x08, Y}, // the top one
        {0x40, 0}, // a shoulder: read, and no bit
        {0x80, 0}, // the other one
    };

    for(size_t i = 0; i < sizeof Cases / sizeof Cases[0]; i++)
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

    // A stick held hard right and down, which the pad sends as the larger y.
    switch_state(report, 0x00, 0x00, 0x08, 0x8000, 0x8000, 0xFFFF, 0xFFFF);

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

    report[0] = 0x30; // the Switch full report, which is not read here

    sgamepad_hid_state state = {0};

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

// SDL's own crc32, written out here rather than called: the header's is a
// port of it, and a port checked against the thing it was ported from is only
// as good as the two staying apart. This is that second copy — from
// `vendor/sdl2/src/stdlib/SDL_crc32.c` — and it is what says the header's
// computes what the pads' packets carry.
static uint32_t sdl_crc32_for_byte(uint32_t r)
{
    for(int i = 0; i < 8; i++)
        r = (r & 1 ? 0 : 0xEDB88320u) ^ r >> 1;

    return r ^ 0xFF000000u;
}

static uint32_t sdl_crc32(uint32_t crc, const uint8_t* data, size_t length)
{
    for(size_t i = 0; i < length; i++)
        crc = sdl_crc32_for_byte((uint8_t)crc ^ data[i]) ^ crc >> 8;

    return crc;
}

static void testCrcIsSdls(void)
{
    uint8_t data[256];

    for(size_t i = 0; i < sizeof data; i++)
        data[i] = (uint8_t)(i * 7 + 3);

    // Every length, and then with the one-byte header the Bluetooth packets
    // sum the coming-in header over.
    for(size_t length = 0; length <= sizeof data; length++)
        assert(sgamepad_hid_crc32(0, data, length) == sdl_crc32(0, data, length));

    for(int header = 0; header < 256; header++)
    {
        const uint8_t byte = (uint8_t)header;

        assert(sgamepad_hid_crc32(sgamepad_hid_crc32(0, &byte, 1), data, 74) == sdl_crc32(sdl_crc32(0, &byte, 1), data, 74));
    }

    // And a value to pin, so a rewrite of the loop has something to be wrong
    // against: the standard crc32 of "123456789" from a zero state, which this
    // construction is — the byte-at-a-time form and the table-and-inversion
    // form agree on every input, this one included.
    static const uint8_t Check[10] = "123456789";

    assert(sgamepad_hid_crc32(0, Check, 9) == 0xCBF43926u);

    puts("crc32 is sdl's ok");
}

// The keepalive: the magic byte each pad's output report is recognised by,
// and no CRC — a packet whose sum adds up is a real effects report, which is
// a pad being changed rather than one being asked to keep talking. The pads
// themselves are what a wrong byte here would show, so the shape is what is
// pinned: this is what SDL sends, byte for byte.
static void testKeepalive(void)
{
    uint8_t report[78];

    sgamepad_hid_ps4_keepalive(report, sizeof report);

    assert(report[0] == 0x11);
    assert(report[1] == 0xC0);

    sgamepad_hid_ps5_keepalive(report, sizeof report);

    assert(report[0] == 0x31);
    assert(report[1] == 0x02);

    // Nothing else is set, and in particular no CRC: the last four bytes that
    // would carry one are zero.
    for(size_t i = 2; i < sizeof report; i++)
        assert(report[i] == 0);

    puts("keepalive ok");
}

int main(void)
{
    testPS4FaceButtons();
    testPS4Hat();
    testPS4BackAndStart();
    testPS4Sticks();
    testPS4LeftStick();
    testPS4Bluetooth();
    testPS5DualSense();
    testPS5SimpleReport();
    testPS5Bluetooth();
    testSwitchFaceButtons();
    testSwitchHatAndSticks();
    testUnknownReports();
    testFamilies();
    testCrcIsSdls();
    testKeepalive();

    puts("sgamepad_hid ok");
    return 0;
}
