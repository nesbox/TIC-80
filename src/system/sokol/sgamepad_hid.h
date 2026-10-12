// sgamepad_hid.h -- the PlayStation and Switch report readers.
//
// The pads XInput does not report, read from the reports they send: Sony's
// DualShock 4 and DualSense, Nintendo's Switch Pro and Joy-Con, and anything
// presenting as one — which is how an 8BitDo in its S mode arrives.
//
// The decoding is ported from the vendors' own drivers rather than derived:
// SDL's `SDL_hidapi_ps4.c`, `SDL_hidapi_ps5.c` and `SDL_hidapi_switch.c`, which
// is where the report ids, the button bytes and the Bluetooth CRC come from.
// Only what the machine reads is taken — the buttons, the direction pad, the
// back and start of the layer, and the right stick it moves the mouse with.
// Triggers, rumble, LEDs, gyro and touchpad are left where they are.
//
// Nothing here touches a platform: a reader is `report + length -> state`, so
// the parsing runs and is tested wherever the studio builds.
//
//     Report the bytes as the HID layer delivers them: the first byte is the
//     report id, which is what the platform prepends to an input report.
//
//     The state is in the layer's own terms: the bits are positional — the
//     bottom face button is `a` on every family — the right stick is already
//     in the machine's axes (x right, y down), and what is left is the
//     deadzone the layer applies, not the reader.

#pragma once

#ifndef SGAMEPAD_HID_INCLUDED
#define SGAMEPAD_HID_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    SGAMEPAD_HID_NONE = 0,
    SGAMEPAD_HID_PS4,           // DualShock 4, over USB or Bluetooth
    SGAMEPAD_HID_PS5,           // DualSense and DualSense Edge
    SGAMEPAD_HID_SWITCH,        // Switch Pro, Joy-Con, and pads presenting as one
} sgamepad_hid_family;

typedef struct
{
    // The direction pad, and the left stick pushed past the threshold below:
    // the machine has one set of direction bits, and every other backend
    // reads both into it.
    bool    up, down, left, right;
    bool    a, b, x, y;
    bool    back, start;

    // The right stick, -1..1 in the machine's axes, centered on zero, with the
    // layer's own deadzone still to be applied.
    float   right_x, right_y;

    // The pad is sending the report it sends once something has asked it to,
    // rather than the simple one it starts with. Only such a pad wants the
    // Bluetooth keepalive: the packet that keeps it streaming is the same one
    // that asks a simple pad to switch, so asking twice is asking for a pad
    // nobody meant to change.
    bool    enhanced;
} sgamepad_hid_state;

// Which reader reads this device. The vendor and product ids are the ones
// SDL's drivers claim, so a third-party pad presenting as a Sony or a Nintendo
// one is caught here too.
sgamepad_hid_family sgamepad_hid_family_of(uint16_t vendor, uint16_t product);

// A report the reader does not know — another report id, a length that cannot
// hold what it must — is left alone: false, and `state` is not touched.
bool sgamepad_hid_ps4_report(const uint8_t* report, size_t length, sgamepad_hid_state* state);
bool sgamepad_hid_ps5_report(const uint8_t* report, size_t length, sgamepad_hid_state* state);
bool sgamepad_hid_switch_report(const uint8_t* report, size_t length, sgamepad_hid_state* state);

// The host writes one of these to a PlayStation pad on Bluetooth to keep it
// sending: without it such a pad works on a cable and stalls on a couch. The
// Bluetooth tickle SDL sends, magic byte and all, and *without* a CRC — the
// sum is what turns it into a real effects report, which is not what asking a
// pad to keep talking should do to it. Both are 78 bytes, the report the
// platform writes: `report` is that whole report, id included, and `length` is
// its size.
//
// Send them only to a pad that reports `enhanced`: that is the state SDL
// tickles, and a pad in the simple one is streaming without being asked.
void sgamepad_hid_ps4_keepalive(uint8_t* report, size_t length);
void sgamepad_hid_ps5_keepalive(uint8_t* report, size_t length);

// What SDL's crc32 does, byte for byte: the reflected polynomial, and no final
// inversion. The packets carry what this sums over them.
uint32_t sgamepad_hid_crc32(uint32_t crc, const uint8_t* data, size_t length);

#endif /* SGAMEPAD_HID_INCLUDED */

// ---------------------------------------------------------------------------
//  implementation
// ---------------------------------------------------------------------------
#if defined(SGAMEPAD_HID_IMPL) && !defined(SGAMEPAD_HID_IMPL_INCLUDED)

#ifndef SGAMEPAD_HID_IMPL_INCLUDED
#define SGAMEPAD_HID_IMPL_INCLUDED
#endif

#include <string.h>

// The vendor and product ids of the pads the readers know, as SDL's drivers
// and its usb_ids.h spell them.
#define _SGAMEPAD_HID_SONY          0x054C
#define _SGAMEPAD_HID_NINTENDO      0x057E

#define _SGAMEPAD_HID_SONY_DS4      0x05C4
#define _SGAMEPAD_HID_SONY_DS4_SLIM 0x09CC
#define _SGAMEPAD_HID_SONY_DS4_PRO  0x05C5  // the officially licensed third-party pad
#define _SGAMEPAD_HID_SONY_DS5      0x0CE6
#define _SGAMEPAD_HID_SONY_DS5_EDGE 0x0DF2

#define _SGAMEPAD_HID_SWITCH_PRO    0x2009
#define _SGAMEPAD_HID_SWITCH_LEFT   0x2006
#define _SGAMEPAD_HID_SWITCH_RIGHT  0x2007
#define _SGAMEPAD_HID_SWITCH_PAIR   0x2008
#define _SGAMEPAD_HID_SWITCH_GRIP   0x200E

// The report ids. The PlayStation state reports are `1` over USB and `0x11`
// over Bluetooth for the DualShock 4 — SDL accepts the whole `0x11`..`0x19`
// band — and `1` and `0x31` for the DualSense.
#define _SGAMEPAD_HID_PS4_USB_STATE     0x01
#define _SGAMEPAD_HID_PS4_BT_STATE_1    0x11
#define _SGAMEPAD_HID_PS4_BT_STATE_9    0x19
#define _SGAMEPAD_HID_PS5_STATE         0x01
#define _SGAMEPAD_HID_PS5_BT_STATE      0x31
#define _SGAMEPAD_HID_SWITCH_SIMPLE     0x3F

// What the CRC32 of a packet coming in is summed with: the HIDP header is part
// of the sum. `0xA1` is the one for a packet coming in, and nothing here sends
// one, so nothing here needs the `0xA2` that goes with a packet going out.
#define _SGAMEPAD_HID_CRC_IN             0xA1

// The size of a PlayStation report with the CRC at its end.
#define _SGAMEPAD_HID_PS_REPORT_SIZE     78

// A DualSense sends its state report in one of two layouts, and the size is
// what says which: the simple one carries the buttons where a DualShock 4
// keeps them, and the extended one is the DualSense's own. SDL splits on the
// size the same way — the simple report is 10 bytes on its own, and 78 when
// the Bluetooth stack has added its header and the CRC.
#define _SGAMEPAD_HID_PS5_SIMPLE_SIZE    10

// The wires carry the pads' sticks as 8-bit values centered on 0x80 and as
// 16-bit ones centered on 0x8000, and both families send y growing *upwards* —
// 0 is the stick pushed down, the largest value pushed up. The machine reads y
// the other way, so the readers negate it, which is what SDL's mapping for
// these pads leaves to the driver: it does not invert the axis, so what a cart
// gets is what the pad sent, and that is up-positive.
#define _SGAMEPAD_HID_STICK8_CENTER      0x80
#define _SGAMEPAD_HID_STICK16_CENTER     0x8000

// A 16-bit stick is read in the share of its travel SDL's own writers give it
//: half the range reads as full, which is where the calibrated extents settle.
#define _SGAMEPAD_HID_STICK16_RANGE      0x4000

// The direction pad both families send as a nibble, where this is nothing held
// and the rest count clockwise from up.
#define _SGAMEPAD_HID_HAT_NONE        0x08

// The share of the left stick's travel that counts as a direction — the share
// the layer's own deadzone treats as nothing, since the two answer the same
// question about the same stick.
#define _SGAMEPAD_HID_THRESHOLD       0.5f

// Where the three button bytes sit in a DualShock 4's state packet, straight
// after the four stick axes. The DualSense spends two more bytes on triggers
// and a counter before its own, which is why the two are separate.
#define _SGAMEPAD_HID_PS4_BUTTONS        4

// The DualSense spends more of its report on the same buttons.
#define _SGAMEPAD_HID_PS5_BUTTONS_AND_HAT 7

// Where the Bluetooth report puts what the USB one starts with: one byte
// saying whether HID data is present, and the report id before it.
#define _SGAMEPAD_HID_PS4_BT_DATA        3
#define _SGAMEPAD_HID_PS5_BT_DATA        2

// The reflected polynomial summed a byte at a time, which is what SDL's own
// crc32 does — the packets carry the sum this makes, not a library's.
static uint32_t _sgamepad_hid_crc32_byte(uint32_t r)
{
    for (int i = 0; i < 8; i++)
        r = (r & 1 ? 0 : 0xEDB88320u) ^ r >> 1;

    return r ^ 0xFF000000u;
}

uint32_t sgamepad_hid_crc32(uint32_t crc, const uint8_t* data, size_t length)
{
    for (size_t i = 0; i < length; i++)
        crc = _sgamepad_hid_crc32_byte((uint8_t)crc ^ data[i]) ^ crc >> 8;

    return crc;
}

// The sum a Bluetooth report carries at its end over the packet before it,
// with the header of the one coming in.
static bool _sgamepad_hid_crc_ok(const uint8_t* report, size_t length)
{
    const uint8_t header = _SGAMEPAD_HID_CRC_IN;
    const uint32_t packet = (uint32_t)report[length - 4]
        | (uint32_t)report[length - 3] << 8
        | (uint32_t)report[length - 2] << 16
        | (uint32_t)report[length - 1] << 24;

    return sgamepad_hid_crc32(sgamepad_hid_crc32(0, &header, 1), report, length - 4) == packet;
}

// An 8-bit stick axis as the machine reads it; -1..1, y away from the pad.
static float _sgamepad_hid_stick8(uint8_t value)
{
    return (value - _SGAMEPAD_HID_STICK8_CENTER) / 128.0f;
}

// The same for a 16-bit one, which arrives little-endian and centered on a
// value that is its own negative: read as a signed offset, not a magnitude.
static float _sgamepad_hid_stick16(const uint8_t* bytes)
{
    const uint16_t raw = (uint16_t)(bytes[0] | bytes[1] << 8);
    const int32_t centered = (int32_t)raw - _SGAMEPAD_HID_STICK16_CENTER;
    const float value = centered / (float)_SGAMEPAD_HID_STICK16_RANGE;

    return value > 1.0f ? 1.0f : (value < -1.0f ? -1.0f : value);
}

// The direction a nibble names, where nothing held is its own value and the
// rest count clockwise from up; the diagonals set both of their bits.
static void _sgamepad_hid_hat(uint8_t hat, sgamepad_hid_state* state)
{
    if (hat == _SGAMEPAD_HID_HAT_NONE)
        return;

    state->up = state->up || hat == 7 || hat == 0 || hat == 1;
    state->right = state->right || hat == 1 || hat == 2 || hat == 3;
    state->down = state->down || hat == 3 || hat == 4 || hat == 5;
    state->left = state->left || hat == 5 || hat == 6 || hat == 7;
}

// The left stick, read into the same four bits as the direction pad: the
// machine has one set, and a pad steered with the stick steers like one
// steered with the pad.
static void _sgamepad_hid_left_stick(float x, float y, sgamepad_hid_state* state)
{
    state->left = state->left || x < -_SGAMEPAD_HID_THRESHOLD;
    state->right = state->right || x > _SGAMEPAD_HID_THRESHOLD;
    state->up = state->up || y < -_SGAMEPAD_HID_THRESHOLD;
    state->down = state->down || y > _SGAMEPAD_HID_THRESHOLD;
}

// Both PlayStation families pack the same three bytes the same way.
static void _sgamepad_hid_ps_buttons(const uint8_t* buttons, sgamepad_hid_state* state)
{
    // The low nibble is a direction, or which of the eight the pad holds.
    _sgamepad_hid_hat(buttons[0] & 0x0F, state);

    // The high nibble is the face buttons, and the id that reads as `x` on a
    // PlayStation pad is the one where the machine's `y` sits.
    state->x = (buttons[0] & 0x10) != 0;
    state->a = (buttons[0] & 0x20) != 0;
    state->b = (buttons[0] & 0x40) != 0;
    state->y = (buttons[0] & 0x80) != 0;

    state->back = (buttons[1] & 0x10) != 0;
    state->start = (buttons[1] & 0x20) != 0;
}

sgamepad_hid_family sgamepad_hid_family_of(uint16_t vendor, uint16_t product)
{
    if (vendor == _SGAMEPAD_HID_SONY)
    {
        switch (product)
        {
        case _SGAMEPAD_HID_SONY_DS4:
        case _SGAMEPAD_HID_SONY_DS4_SLIM:
        case _SGAMEPAD_HID_SONY_DS4_PRO:
            return SGAMEPAD_HID_PS4;

        case _SGAMEPAD_HID_SONY_DS5:
        case _SGAMEPAD_HID_SONY_DS5_EDGE:
            return SGAMEPAD_HID_PS5;

        default:
            break;
        }
    }
    else if (vendor == _SGAMEPAD_HID_NINTENDO)
    {
        switch (product)
        {
        case _SGAMEPAD_HID_SWITCH_PRO:
        case _SGAMEPAD_HID_SWITCH_LEFT:
        case _SGAMEPAD_HID_SWITCH_RIGHT:
        case _SGAMEPAD_HID_SWITCH_PAIR:
        case _SGAMEPAD_HID_SWITCH_GRIP:
            return SGAMEPAD_HID_SWITCH;

        default:
            break;
        }
    }

    return SGAMEPAD_HID_NONE;
}

bool sgamepad_hid_ps4_report(const uint8_t* report, size_t length, sgamepad_hid_state* state)
{
    // A state packet holds the sticks, the buttons, the triggers and the
    // timestamp before anything the reader is after.
    if (length < _SGAMEPAD_HID_PS4_BUTTONS + 3)
        return false;

    const uint8_t* packet;

    if (report[0] == _SGAMEPAD_HID_PS4_USB_STATE)
    {
        packet = report + 1;
    }
    else if (report[0] >= _SGAMEPAD_HID_PS4_BT_STATE_1 && report[0] <= _SGAMEPAD_HID_PS4_BT_STATE_9)
    {
        // Over Bluetooth the packet carries a CRC over its whole length, and
        // says in the byte before the report whether it holds HID data at all.
        if (length != _SGAMEPAD_HID_PS_REPORT_SIZE || !(report[1] & 0x80) || !_sgamepad_hid_crc_ok(report, length))
            return false;

        packet = report + _SGAMEPAD_HID_PS4_BT_DATA;
    }
    else
    {
        return false;
    }

    state->right_x = _sgamepad_hid_stick8(packet[2]);
    state->right_y = -_sgamepad_hid_stick8(packet[3]);

    _sgamepad_hid_left_stick(_sgamepad_hid_stick8(packet[0]), -_sgamepad_hid_stick8(packet[1]), state);
    _sgamepad_hid_ps_buttons(packet + _SGAMEPAD_HID_PS4_BUTTONS, state);

    return true;
}

bool sgamepad_hid_ps5_report(const uint8_t* report, size_t length, sgamepad_hid_state* state)
{
    const uint8_t* packet;
    size_t buttons;

    if (report[0] == _SGAMEPAD_HID_PS5_STATE)
    {
        // Two layouts, and the size is what tells them apart: the simple one
        // is what a DualSense sends until something brings it into enhanced
        // mode. Read as the extended one, its trigger bytes come out as the
        // hat and the face buttons, which is a pad that reads as held up with
        // nothing else ever pressed.
        const bool simple = length == _SGAMEPAD_HID_PS5_SIMPLE_SIZE
            || length == _SGAMEPAD_HID_PS_REPORT_SIZE;

        if (length < (simple ? _SGAMEPAD_HID_PS4_BUTTONS + 3 : _SGAMEPAD_HID_PS5_BUTTONS_AND_HAT + 3))
            return false;

        packet = report + 1;
        buttons = simple ? _SGAMEPAD_HID_PS4_BUTTONS : _SGAMEPAD_HID_PS5_BUTTONS_AND_HAT;

        // Over a cable the extended layout is the one the pad sends once it is
        // in enhanced mode; the simple one is the state it starts in.
        state->enhanced = !simple;
    }
    else if (report[0] == _SGAMEPAD_HID_PS5_BT_STATE)
    {
        if (length < _SGAMEPAD_HID_PS5_BUTTONS_AND_HAT + 3 || !_sgamepad_hid_crc_ok(report, length))
            return false;

        // This report exists only in enhanced mode: a pad in the simple one
        // sends the state report, whatever the wire.
        packet = report + _SGAMEPAD_HID_PS5_BT_DATA;
        buttons = _SGAMEPAD_HID_PS5_BUTTONS_AND_HAT;
        state->enhanced = true;
    }
    else
    {
        return false;
    }

    state->right_x = _sgamepad_hid_stick8(packet[2]);
    state->right_y = -_sgamepad_hid_stick8(packet[3]);

    _sgamepad_hid_left_stick(_sgamepad_hid_stick8(packet[0]), -_sgamepad_hid_stick8(packet[1]), state);

    // Past the hat byte the DualSense carries one more byte of buttons than
    // the DualShock 4; what the machine reads of them is the same three.
    _sgamepad_hid_ps_buttons(packet + buttons, state);

    return true;
}

bool sgamepad_hid_switch_report(const uint8_t* report, size_t length, sgamepad_hid_state* state)
{
    // The simple report is the only one read: the buttons, the hat and the
    // sticks, with no mode command and no calibration to ask for.
    if (report[0] != _SGAMEPAD_HID_SWITCH_SIMPLE || length < 12)
        return false;

    const uint8_t* packet = report + 1;

    // The pad names its buttons by the labels printed on them, which is the
    // other way round from where they sit: the one it calls `b` is the bottom
    // one and the one it calls `a` is on the right. The machine reads
    // positions, where the bottom button is `a` — so the label is not what is
    // read here, and the two that swap are the pad's `a` and `b`.
    state->a = (packet[0] & 0x01) != 0;
    state->b = (packet[0] & 0x02) != 0;
    state->y = (packet[0] & 0x04) != 0;
    state->x = (packet[0] & 0x08) != 0;

    state->back = (packet[1] & 0x01) != 0;
    state->start = (packet[1] & 0x02) != 0;

    _sgamepad_hid_hat(packet[2], state);

    _sgamepad_hid_left_stick(_sgamepad_hid_stick16(packet + 3), -_sgamepad_hid_stick16(packet + 5), state);

    // The two button bytes and the hat, then the sticks: four 16-bit axes,
    // the right pair last.
    state->right_x = _sgamepad_hid_stick16(packet + 7);
    state->right_y = -_sgamepad_hid_stick16(packet + 9);

    return true;
}

void sgamepad_hid_ps4_keepalive(uint8_t* report, size_t length)
{
    if (length < _SGAMEPAD_HID_PS_REPORT_SIZE)
        return;

    memset(report, 0, _SGAMEPAD_HID_PS_REPORT_SIZE);

    // SDL's tickle: the Bluetooth effects report with the magic byte, and no
    // CRC — a packet that asks the pad to keep talking and changes nothing.
    report[0] = _SGAMEPAD_HID_PS4_BT_STATE_1;
    report[1] = 0xC0;
}

void sgamepad_hid_ps5_keepalive(uint8_t* report, size_t length)
{
    if (length < _SGAMEPAD_HID_PS_REPORT_SIZE)
        return;

    memset(report, 0, _SGAMEPAD_HID_PS_REPORT_SIZE);

    // The DualSense's effects report with its magic byte and no CRC: the sum
    // is what makes this a real effects report, and a real one leaves the pad
    // changed. Sent as the pad's tickle rather than as an effect.
    report[0] = _SGAMEPAD_HID_PS5_BT_STATE;
    report[1] = 0x02;
}

#endif /* SGAMEPAD_HID_IMPL */
