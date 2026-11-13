/* The mapping table's parser and matcher, driven by references and ids rather
 * than by a pad.
 *
 * A pad on the bench is the only way to see whether its entry is right, but
 * the grammar of an element reference and the lookup of an entry are pure
 * functions: a table of references and the values each must produce covers
 * what no pad can be asked about the table the generator wrote.
 *
 * Target sgamepad-map-test in cmake/tests.cmake; `ctest -R sgamepad-map` runs it.
 */
#define SGAMEPAD_MAP_IMPL
#include "system/sokol/sgamepad_map.h"
#include "system/sokol/sgamepad_map_windows.h"

#include <assert.h>
#include <stdio.h>

// The Windows table is plain data, so the matcher is asked about it on every
// platform the tests run on: the Linux and macOS tables hold the numbering of
// their own element lists, and the lookup is the same for all three.
static const sgamepad_map_entry* find_pad(uint16_t vendor, uint16_t product, uint16_t crc)
{
    return sgamepad_map_find(_sgamepad_map_windows_table, SGAMEPAD_MAP_WINDOWS_TABLE_SIZE, 3, vendor, product, crc);
}

// A button reference resolves to the backend's button index.
static void testButtonReference(void)
{
    const uint16_t ref = sgamepad_map_parse("b2");

    assert(SGAMEPAD_MAP_KIND_OF(ref) == SGAMEPAD_MAP_KIND_BUTTON);
    assert(SGAMEPAD_MAP_INDEX_OF(ref) == 2);
    assert(!SGAMEPAD_MAP_IS_NEGATIVE(ref));

    assert(SGAMEPAD_MAP_INDEX_OF(sgamepad_map_parse("b0")) == 0);
    assert(SGAMEPAD_MAP_INDEX_OF(sgamepad_map_parse("b127")) == SGAMEPAD_MAP_BUTTONS - 1);

    puts("a button reference ok");
}

// An axis reference is the axis' index, and the minus of a half axis or the
// tilde of a reversed one is the same sign once it is stored.
static void testAxisReference(void)
{
    const uint16_t right = sgamepad_map_parse("a3");
    const uint16_t inverted = sgamepad_map_parse("-a1");

    assert(SGAMEPAD_MAP_KIND_OF(right) == SGAMEPAD_MAP_KIND_AXIS);
    assert(SGAMEPAD_MAP_INDEX_OF(right) == 3);
    assert(!SGAMEPAD_MAP_IS_NEGATIVE(right));

    assert(SGAMEPAD_MAP_KIND_OF(inverted) == SGAMEPAD_MAP_KIND_AXIS);
    assert(SGAMEPAD_MAP_INDEX_OF(inverted) == 1);
    assert(SGAMEPAD_MAP_IS_NEGATIVE(inverted));

    puts("an axis reference ok");
}

// A hat reference names its hat and the direction of it that counts.
static void testHatReference(void)
{
    const uint16_t up = sgamepad_map_parse("h0.1");
    const uint16_t left = sgamepad_map_parse("h3.8");

    assert(SGAMEPAD_MAP_KIND_OF(up) == SGAMEPAD_MAP_KIND_HAT);
    assert(SGAMEPAD_MAP_INDEX_OF(up) == 0);
    assert(SGAMEPAD_MAP_HAT_MASK_OF(up) == 1);

    assert(SGAMEPAD_MAP_INDEX_OF(left) == 3);
    assert(SGAMEPAD_MAP_HAT_MASK_OF(left) == 8);

    puts("a hat reference ok");
}

// Anything the grammar does not know is no reference at all, not a reference
// to element zero.
static void testUnknownReferences(void)
{
    static const char* const Cases[] = {
        "",      // nothing at all
        "b",     // no index
        "a",     // no index
        "b2x",   // trailing text
        "s2",    // no such kind
        "+a1",   // the generator stores a half axis as a sign, not a plus
        "-b2",   // a button has no half
        "h0.3",  // not a direction
        "h0.1x", // trailing text
        "h4.1",  // past the hats the table indexes
        "a16",   // past the axes
        "b128",  // past the buttons
    };

    for(size_t i = 0; i < sizeof Cases / sizeof Cases[0]; i++)
        assert(sgamepad_map_parse(Cases[i]) == SGAMEPAD_MAP_NONE);

    puts("unknown references ok");
}

// The entry a pad's ids name, and the encoding the generated table is in.
static void testMatcher(void)
{
    // The Saitek P990. SDL's database writes the id twice, with two
    // DirectInput versions and a different back and start; the version is
    // not part of the match, so the later entry is the one kept — SDL itself
    // would prefer the entry whose version the device reports, which a table
    // without versions cannot.
    const sgamepad_map_entry* pad = find_pad(0x06A3, 0x040B, 0);

    assert(pad != NULL);
    assert(pad->vendor == 0x06A3 && pad->product == 0x040B);

    // The table carries what the generator made of SDL's references: the face
    // buttons by index, the direction pad as a hat, the sticks as axes.
    assert(pad->ref[SGAMEPAD_MAP_ROLE_A] == SGAMEPAD_MAP_BUTTON(1));
    assert(pad->ref[SGAMEPAD_MAP_ROLE_BACK] == SGAMEPAD_MAP_BUTTON(8));
    assert(pad->ref[SGAMEPAD_MAP_ROLE_DPUP] == SGAMEPAD_MAP_HAT(0, 1));
    assert(pad->ref[SGAMEPAD_MAP_ROLE_DPLEFT] == SGAMEPAD_MAP_HAT(0, 8));
    assert(pad->ref[SGAMEPAD_MAP_ROLE_LEFTX] == SGAMEPAD_MAP_AXIS(0));
    assert(pad->ref[SGAMEPAD_MAP_ROLE_RIGHTX] == SGAMEPAD_MAP_AXIS(3));

    // A wheel or a pad in a vendor mode: the ids name nothing, so the device
    // is left alone rather than read as buttons nobody pressed.
    assert(find_pad(0xFFFF, 0xFFFF, 0) == NULL);

    puts("the matcher ok");
}

// The CRC the database's `crc:` fields carry, over the device names SDL
// computed them from.
static void testNameCRC(void)
{
    assert(sgamepad_map_crc16(0, "Defender Joystick Cobra R4") == 0xC77A);
    assert(sgamepad_map_crc16(0, "SPEEDLINK STRIKE Gamepad") == 0x5811);
    assert(sgamepad_map_crc16(0, "SplitFish Game Controller") == 0x5628);

    // Feeding a second string continues the sum, which is how macOS hashes
    // the manufacturer and the product name into one.
    const uint16_t space = sgamepad_map_crc16(sgamepad_map_crc16(0, "Manufacturer"), " ");
    assert(space == sgamepad_map_crc16(0, "Manufacturer "));

    puts("the name CRC ok");
}

// A vendor and product id two devices share: the CRC of the product name is
// what tells them apart, and a name no entry names falls back to the entry
// written without one.
static void testProductNameCRC(void)
{
    // 0x054C:0x0268 is the DualShock 3, and the SplitFish Game Controller
    // shares the id. The name CRC picks each one's own entry.
    const sgamepad_map_entry* ds3 = find_pad(0x054C, 0x0268, 0);
    const sgamepad_map_entry* splitfish = find_pad(0x054C, 0x0268, 0x5628);

    assert(ds3 != NULL && ds3->crc == 0);
    assert(ds3->ref[SGAMEPAD_MAP_ROLE_A] == SGAMEPAD_MAP_BUTTON(2));
    assert(ds3->ref[SGAMEPAD_MAP_ROLE_BACK] == SGAMEPAD_MAP_BUTTON(9));

    assert(splitfish != NULL && splitfish->crc == 0x5628);
    assert(splitfish->ref[SGAMEPAD_MAP_ROLE_A] == SGAMEPAD_MAP_BUTTON(0));

    // A name neither entry was written for is the DualShock 3's generic one.
    assert(find_pad(0x054C, 0x0268, 0x1234) == ds3);

    // 0x0079:0x0006 names three pads, every one of them by its CRC, so an
    // unknown name there names no pad at all.
    assert(find_pad(0x0079, 0x0006, 0x20F0)->ref[SGAMEPAD_MAP_ROLE_A] == SGAMEPAD_MAP_BUTTON(0));
    assert(find_pad(0x0079, 0x0006, 0x0001) == NULL);

    puts("the product name CRC ok");
}

int main(void)
{
    testButtonReference();
    testAxisReference();
    testHatReference();
    testUnknownReferences();
    testMatcher();
    testNameCRC();
    testProductNameCRC();

    puts("sgamepad map ok");
    return 0;
}
