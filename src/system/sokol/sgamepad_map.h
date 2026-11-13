// sgamepad_map.h -- the mapping table's element references, parsed and matched.
//
// A pad that neither XInput nor a HID family reader places is read through
// DirectInput, by the element indices of the standard joystick data format.
// SDL's game controller database says which index carries which role, and the
// table generated from it lives in sgamepad_map_db.h: a pad's vendor and
// product ids, and one reference per role.
//
// A reference is stored as a small integer rather than the string SDL writes:
// what kind of element it is, its index, and, for an axis, the half that
// counts. The parser here is the grammar those integers stand for, and the
// matcher turns a device's ids and product name CRC into its entry — a pad
// that shares its ids with another device is told apart by that CRC.
//
// Nothing here touches a platform: the parser is a pure function of its text
// and the matcher of the table, so the tests run wherever the studio builds.
//
//     Do this:
//         #define SGAMEPAD_MAP_IMPL
//     before you include this file in *one* C file to create the implementation.

#pragma once

#ifndef SGAMEPAD_MAP_INCLUDED
#define SGAMEPAD_MAP_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The roles the machine reads of a pad, in the order each generated entry
// carries them. The generator (tools/gamecontrollerdb.py) writes its rows in
// this order, so adding one here means regenerating the table.
typedef enum
{
    SGAMEPAD_MAP_ROLE_A,
    SGAMEPAD_MAP_ROLE_B,
    SGAMEPAD_MAP_ROLE_X,
    SGAMEPAD_MAP_ROLE_Y,
    SGAMEPAD_MAP_ROLE_BACK,
    SGAMEPAD_MAP_ROLE_START,
    SGAMEPAD_MAP_ROLE_DPUP,
    SGAMEPAD_MAP_ROLE_DPDOWN,
    SGAMEPAD_MAP_ROLE_DPLEFT,
    SGAMEPAD_MAP_ROLE_DPRIGHT,
    SGAMEPAD_MAP_ROLE_LEFTX,
    SGAMEPAD_MAP_ROLE_LEFTY,
    SGAMEPAD_MAP_ROLE_RIGHTX,
    SGAMEPAD_MAP_ROLE_RIGHTY,
    SGAMEPAD_MAP_ROLES,
} sgamepad_map_role;

// The elements a table reference may name: a DirectInput button, an axis, or
// a direction of a hat. An entry that names a role not at all holds
// SGAMEPAD_MAP_NONE.
//
//     bit 15..14  the kind
//     bit 13      the negative half or a reversed axis
//     bit 11..8   a hat's direction mask
//     bit  7..0   the element's index
enum
{
    SGAMEPAD_MAP_KIND_BUTTON = 0x4000,
    SGAMEPAD_MAP_KIND_AXIS = 0x8000,
    SGAMEPAD_MAP_KIND_HAT = 0xC000,
};

#define SGAMEPAD_MAP_NONE                 0x0000u
#define SGAMEPAD_MAP_NEGATIVE             0x2000u
#define SGAMEPAD_MAP_BUTTON(index)        (SGAMEPAD_MAP_KIND_BUTTON | (index))
#define SGAMEPAD_MAP_AXIS(index)          (SGAMEPAD_MAP_KIND_AXIS | (index))
#define SGAMEPAD_MAP_AXIS_NEGATIVE(index) (SGAMEPAD_MAP_KIND_AXIS | SGAMEPAD_MAP_NEGATIVE | (index))
#define SGAMEPAD_MAP_HAT(index, mask)     (SGAMEPAD_MAP_KIND_HAT | ((mask) << 8) | (index))

#define SGAMEPAD_MAP_KIND_OF(ref)     ((ref) & 0xC000u)
#define SGAMEPAD_MAP_INDEX_OF(ref)    ((int)((ref) & 0x00FFu))
#define SGAMEPAD_MAP_HAT_MASK_OF(ref) ((int)(((ref) >> 8) & 0x0Fu))
#define SGAMEPAD_MAP_IS_NEGATIVE(ref) (((ref) & SGAMEPAD_MAP_NEGATIVE) != 0)

// The elements the DirectInput table indexes: the standard joystick format's
// buttons and axes, and its four point-of-view hats.
#define SGAMEPAD_MAP_BUTTONS 128
#define SGAMEPAD_MAP_AXES    8
#define SGAMEPAD_MAP_HATS    4

typedef struct
{
    uint16_t vendor;
    uint16_t product;
    // The CRC of the device's product name for a pad that shares its ids with
    // another device, zero otherwise. A zero entry is the one any name matches.
    uint16_t crc;
    uint16_t ref[SGAMEPAD_MAP_ROLES];
} sgamepad_map_entry;

// One element reference as the table stores it: `b2`, `a0`, `-a1`, `h0.1`.
// Anything the grammar does not know reads as SGAMEPAD_MAP_NONE.
uint16_t sgamepad_map_parse(const char* text);

// The entry a pad's ids and product name CRC name, or NULL when the table has
// none. An entry whose CRC matches the name is the pad's own; an entry with a
// zero CRC is the fallback for a pad that no named entry claims. The
// DirectInput version is not part of the match: the driver's numbering of a
// pad's elements does not change with it.
const sgamepad_map_entry* sgamepad_map_find(uint16_t vendor, uint16_t product, uint16_t crc);

#endif /* SGAMEPAD_MAP_INCLUDED */

// ---------------------------------------------------------------------------
//  implementation
// ---------------------------------------------------------------------------
#if defined(SGAMEPAD_MAP_IMPL) && !defined(SGAMEPAD_MAP_IMPL_INCLUDED)

#ifndef SGAMEPAD_MAP_IMPL_INCLUDED
#define SGAMEPAD_MAP_IMPL_INCLUDED
#endif

#include "sgamepad_map_db.h"

// The number after a reference's letter: one to three digits, and then the
// end of the text. A longer one is not an index the table carries.
static bool _sgamepad_map_number(const char** text, int* value)
{
    int digits = 0;
    int number = 0;

    while(digits < 3 && (*text)[digits] >= '0' && (*text)[digits] <= '9')
    {
        number = number * 10 + ((*text)[digits] - '0');
        digits++;
    }

    if(digits == 0 || (*text)[digits] != 0)
        return false;

    *text += digits;
    *value = number;

    return true;
}

uint16_t sgamepad_map_parse(const char* text)
{
    int sign = 1;

    if(*text == '-')
    {
        sign = -1;
        text++;
    }

    const char kind = *text++;
    int index = 0;

    switch(kind)
    {
        case 'b':
            if(sign > 0 && _sgamepad_map_number(&text, &index) && index < SGAMEPAD_MAP_BUTTONS)
                return SGAMEPAD_MAP_BUTTON(index);
            break;

        case 'a':
            if(_sgamepad_map_number(&text, &index) && index < SGAMEPAD_MAP_AXES)
                return sign < 0 ? SGAMEPAD_MAP_AXIS_NEGATIVE(index) : SGAMEPAD_MAP_AXIS(index);
            break;

        case 'h':
            // A hat is one digit of index, a dot, and one digit of direction.
            if(sign > 0 && text[0] >= '0' && text[0] <= '9' && text[1] == '.' && text[2] >= '0' && text[2] <= '9' && text[3] == 0)
            {
                const int mask = text[2] - '0';

                index = text[0] - '0';

                if(index < SGAMEPAD_MAP_HATS && (mask == 1 || mask == 2 || mask == 4 || mask == 8))
                    return SGAMEPAD_MAP_HAT(index, mask);
            }
            break;

        default: break;
    }

    return SGAMEPAD_MAP_NONE;
}

const sgamepad_map_entry* sgamepad_map_find(uint16_t vendor, uint16_t product, uint16_t crc)
{
    const sgamepad_map_entry* fallback = NULL;

    for(int i = 0; i < SGAMEPAD_MAP_TABLE_SIZE; i++)
    {
        const sgamepad_map_entry* entry = &_sgamepad_map_table[i];

        if(entry->vendor != vendor || entry->product != product)
            continue;

        if(entry->crc == crc)
            return entry;

        if(entry->crc == 0)
            fallback = entry;
    }

    return fallback;
}

#endif /* SGAMEPAD_MAP_IMPL */
