#!/usr/bin/env python3
"""Generate the gamepad mapping tables from SDL's gamecontrollerdb.

The database lists every pad SDL knows how to place, keyed by a GUID and
spelled with element references like `b2`, `-a1` or `h0.4`. What the machine
reads of a pad is small, so each table is cut to those roles and converted to
the integers the platform's backend indexes its elements by: DirectInput on
Windows, evdev on Linux, IOHID on macOS. The three databases have their own
element numbering, so each platform gets its own table from its own block.

A pad whose entry carries a `crc:` is one of several devices sharing a vendor
and product id, told apart by a CRC of the device name; the entry keeps it,
and each backend computes the same CRC from the name its platform reports.
The bus a GUID carries is kept too: the same ids over USB and Bluetooth can
be numbered differently.

Not carried: an axis SDL builds from two half axes (`+lefty:+a2,-lefty:-a1`),
which one reference per role cannot spell. The Razer Serval is the one pad in
the DirectInput block that has one; the vertical half of its left stick reads
as nothing, while its buttons, direction pad and horizontal half work.

Run from the repository root:

    tools/gamecontrollerdb.py

The outputs are committed, and the SDL submodule is read only here: bumping it
does not regenerate the tables, so a new vendor entry cannot change the table a
release carries without this script being run and its diff reviewed.
"""

import re
import sys
import textwrap

SOURCE = "vendor/sdl2/src/joystick/SDL_gamecontrollerdb.h"

# The blocks of the database, and where each table goes: the platform, the
# preprocessor line that opens its block, and the header the table is written
# to. The element numbering the references use is the one the matching SDL
# backend numbers its elements by.
BLOCKS = [
    (
        "windows",
        r"#ifdef SDL_JOYSTICK_DINPUT",
        "src/system/sokol/sgamepad_map_windows.h",
        "the DirectInput block",
        "DirectInput element indices of the standard joystick format",
    ),
    (
        "linux",
        r"#if defined\(SDL_JOYSTICK_LINUX\) \|\| defined\(__OpenBSD__\)",
        "src/system/sokol/sgamepad_map_linux.h",
        "the evdev block",
        "Linux evdev indices: buttons rank the device's key codes in SDL's order, axes rank its absolute axes, hats are its digital hat pairs",
    ),
    (
        "macos",
        r"#if defined\(__MACOSX__\)",
        "src/system/sokol/sgamepad_map_macos.h",
        "the IOKit block",
        "IOHID element indices: axes rank the Generic Desktop axes by usage, buttons rank the button and D-pad usages, hats are its hat switches",
    ),
]

# The roles the machine reads, in the order of each entry's references. The
# names live in sgamepad_map.h, and the order there is this one.
ROLES = [
    "a",
    "b",
    "x",
    "y",
    "back",
    "start",
    "dpup",
    "dpdown",
    "dpleft",
    "dpright",
    "leftx",
    "lefty",
    "rightx",
    "righty",
]

# The elements a table may index; past these the backend cannot read them.
BUTTONS = 128
AXES = 16
HATS = 4

ENTRY = re.compile(r'^\s*"([^"]+)"', re.M)
CRC = re.compile(r"(?:^|,)crc:([0-9a-fA-F]{1,4})(?:,|$)")
GUID = re.compile(r"[0-9a-fA-F]{32}")


def parse_reference(text):
    """One SDL element reference, as (kind, index, sign, mask), or None.

    The forms are `bN`, `aN`, `-aN`, `+aN`, `aN~`, `hH.M`. SDL spells a
    reversed axis with a trailing `~` and a half axis with a sign before the
    `a`; both become a sign here, which is the form the backends store.
    """
    match = re.fullmatch(r"b(\d+)", text)

    if match:
        index = int(match.group(1))
        return ("button", index, 1, 0) if index < BUTTONS else None

    match = re.fullmatch(r"([+-]?)a(\d+)(~?)", text)

    if match:
        index = int(match.group(2))

        if index >= AXES:
            return None

        negative = (match.group(1) == "-") != (match.group(3) == "~")
        return ("axis", index, -1 if negative else 1, 0)

    match = re.fullmatch(r"h(\d)\.(\d)", text)

    if match:
        index = int(match.group(1))
        mask = int(match.group(2))

        return ("hat", index, 1, mask) if index < HATS and mask in (1, 2, 4, 8) else None

    return None


def emit_reference(reference):
    """The reference as the C the table carries, or SGAMEPAD_MAP_NONE."""
    if reference is None:
        return "SGAMEPAD_MAP_NONE"

    kind, index, sign, mask = reference

    if kind == "button":
        return f"SGAMEPAD_MAP_BUTTON({index})"

    if kind == "axis":
        return f"SGAMEPAD_MAP_AXIS_NEGATIVE({index})" if sign < 0 else f"SGAMEPAD_MAP_AXIS({index})"

    return f"SGAMEPAD_MAP_HAT({index}, {mask})"


def parse_entries(source, pattern):
    """One block of the database, as ((bus, vendor, product, crc), refs)."""
    block = re.search(pattern + r"\n(.*?)\n#endif", source, re.S)

    if block is None:
        sys.exit(f"{SOURCE}: no {pattern} block")

    table = {}
    skipped = 0

    for line in ENTRY.findall(block.group(1)):
        fields = line.split(",", 2)

        if len(fields) != 3:
            continue

        guid, _, mapping = fields

        # The standard GUID form: bus, crc16, vendor, zero, product, zero,
        # version, driver. Matching leaves the version out; the bus stays,
        # because the same ids can be numbered differently over another wire.
        # An entry keyed by the device's name instead has no ids to match on.
        if not GUID.fullmatch(guid):
            skipped += 1
            continue

        data = bytes.fromhex(guid)

        if data[6:8] != b"\0\0" or data[10:12] != b"\0\0":
            skipped += 1
            continue

        bus = int.from_bytes(data[0:2], "little")
        vendor = int.from_bytes(data[4:6], "little")
        product = int.from_bytes(data[8:10], "little")

        # The wildcard entry names no device at all, and a match torn from it
        # would apply one pad's layout to every other.
        if vendor == 0 and product == 0:
            continue

        references = {role: None for role in ROLES}

        for element in mapping.split(","):
            role, _, reference = element.partition(":")

            if role in references:
                references[role] = parse_reference(reference)

        # The CRC of the device name, for the pads that share an id with
        # another device; zero when the entry does not name one.
        crc_match = CRC.search(mapping)
        crc = int(crc_match.group(1), 16) if crc_match else 0

        # The same pad can be written twice — the two entries differ only in
        # the button-label hint — and the last one is kept: it is the variant
        # that reads the buttons by position, which is what TIC-80's machine
        # and its Switch reader expect. SDL's own default keeps the other, so
        # this is a choice, not a port.
        table[(bus, vendor, product, crc)] = [references[role] for role in ROLES]

    return table, skipped


def render(platform, output, block, numbering, table):
    """One generated header, as text."""
    lines = [
        f"// {output.rsplit('/', 1)[-1]} -- the pads SDL's game controller database places, cut to the",
        "// roles the machine reads. Generated from vendor/sdl2/src/joystick/SDL_gamecontrollerdb.h;",
        "// do not edit.",
        "//",
        "// Regenerate from the repository root:",
        "//     tools/gamecontrollerdb.py",
        "//",
    ]

    lines += [f"// {line}" for line in textwrap.wrap(f"The table is {block}'s, and its references are {numbering}.", 92)]

    lines += [
        "// A pad is matched by the bus, vendor and product id its GUID carries and the CRC of",
        "// the device name where the database gives one, without the driver version.",
        "//",
        "// The table is committed and regenerated by hand, like the shaders: a submodule bump",
        "// does not change it, and a release carries the table its diff was reviewed with.",
        "//",
        "// The table is SDL's, zlib licensed:",
        "//     Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>",
        "//     https://github.com/libsdl-org/SDL",
        "// See THIRD_PARTY_LICENSES.md and LICENSES/Zlib.txt.",
        "//",
        "// Each entry is a pad's bus, vendor and product id, the CRC of its device name where",
        "// the database gives one (zero otherwise), and one reference per role, in the order of",
        "// sgamepad_map.h's roles: a, b, x, y, back, start, dpup, dpdown, dpleft, dpright,",
        "// leftx, lefty, rightx, righty.",
        "",
        f"#define SGAMEPAD_MAP_{platform.upper()}_TABLE_SIZE {len(table)}",
        "",
        f"static const sgamepad_map_entry _sgamepad_map_{platform}_table[SGAMEPAD_MAP_{platform.upper()}_TABLE_SIZE] = {{",
    ]

    for key in sorted(table):
        bus, vendor, product, crc = key
        refs = ", ".join(emit_reference(reference) for reference in table[key])

        lines.append(f"    {{0x{bus:04X}, 0x{vendor:04X}, 0x{product:04X}, 0x{crc:04X}, {{{refs}}}}},")

    lines.append("};")
    lines.append("")

    return "\n".join(lines)


def main():
    source = open(SOURCE, encoding="utf-8-sig").read()

    for platform, pattern, output, block, numbering in BLOCKS:
        table, skipped = parse_entries(source, pattern)

        if not table:
            sys.exit(f"{SOURCE}: no entries in {block}")

        if skipped:
            print(f"{output}: {skipped} entries name their pad by name rather than ids: skipped", file=sys.stderr)

        with open(output, "w", encoding="utf-8", newline="\n") as file:
            file.write(render(platform, output, block, numbering, table))

        print(f"{output}: {len(table)} pads")


if __name__ == "__main__":
    main()
