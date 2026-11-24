#include "keylayout.h"

#include <string.h>

// What each character position prints, once the OS has said. Unknown entries
// fall back to the position unless another position's ASCII label claims it.
static tic_key keys[tic_keys_count];
static bool known[tic_keys_count];

#if defined(__EMSCRIPTEN__) || defined(__APPLE__) || defined(_WIN32) || \
    (defined(__linux__) && !defined(__ANDROID__)) || defined(__FreeBSD__)

static tic_key char_key(uint32_t ch)
{
    if(ch >= 'A' && ch <= 'Z')
        ch += 'a' - 'A';

    if(ch >= 'a' && ch <= 'z')
        return (tic_key)(tic_key_a + ch - 'a');

    if(ch >= '0' && ch <= '9')
        return (tic_key)(tic_key_0 + ch - '0');

    switch(ch)
    {
        case ' ': return tic_key_space;
        case '-': return tic_key_minus;
        case '=': return tic_key_equals;
        case '[': return tic_key_leftbracket;
        case ']': return tic_key_rightbracket;
        case '\\': return tic_key_backslash;
        case ';': return tic_key_semicolon;
        case '\'': return tic_key_apostrophe;
        case '`': return tic_key_grave;
        case ',': return tic_key_comma;
        case '.': return tic_key_period;
        case '/': return tic_key_slash;
        default: return tic_key_unknown;
    }
}

static void set_key(tic_key position, uint32_t ch)
{
    keys[position] = char_key(ch);
    known[position] = true;

    // A non-ASCII character keeps the letter positions only: a non-Latin layout
    // still needs the WASD keys carts read, while the rest send nothing.
    if(!(ch >= 0x20 && ch <= 0x7e) && position >= tic_key_a && position <= tic_key_z)
        known[position] = false;
}

#endif

// A position left at its own key must not share that key with an ASCII label
// another position prints; the label wins, as it does in SDL.
static void resolve(void)
{
    bool claimed[tic_keys_count] = {0};

    for(s32 i = tic_key_a; i <= tic_key_space; i++)
        if(known[i] && keys[i] != tic_key_unknown)
            claimed[keys[i]] = true;

    for(s32 i = tic_key_a; i <= tic_key_space; i++)
        if(!known[i] && claimed[i])
        {
            keys[i] = tic_key_unknown;
            known[i] = true;
        }
}

tic_key tic_sys_layout_key(tic_key position)
{
    return position < tic_keys_count && known[position] ? keys[position] : position;
}

// The three positions SDL's detect_keyboard_layout() tells the layouts apart by.
static tic_layout classify(void)
{
    const tic_key q = tic_sys_layout_key(tic_key_q);
    const tic_key w = tic_sys_layout_key(tic_key_w);
    const tic_key y = tic_sys_layout_key(tic_key_y);

    if(q == tic_key_a && w == tic_key_z && y == tic_key_y)
        return tic_layout_azerty;

    if(q == tic_key_q && w == tic_key_w && y == tic_key_z)
        return tic_layout_qwertz;

    if(q == tic_key_q && w == tic_key_z && y == tic_key_y)
        return tic_layout_qzerty;

    if(q == tic_key_x && w == tic_key_v && y == tic_key_k)
        return tic_layout_de_neo;

    if(q == tic_key_j && w == tic_key_d && y == tic_key_p)
        return tic_layout_de_bone;

    return tic_layout_qwerty;
}

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>

static const struct
{
    tic_key key;
    const char* code;
} Positions[] =
    {
        {tic_key_a, "KeyA"},
        {tic_key_b, "KeyB"},
        {tic_key_c, "KeyC"},
        {tic_key_d, "KeyD"},
        {tic_key_e, "KeyE"},
        {tic_key_f, "KeyF"},
        {tic_key_g, "KeyG"},
        {tic_key_h, "KeyH"},
        {tic_key_i, "KeyI"},
        {tic_key_j, "KeyJ"},
        {tic_key_k, "KeyK"},
        {tic_key_l, "KeyL"},
        {tic_key_m, "KeyM"},
        {tic_key_n, "KeyN"},
        {tic_key_o, "KeyO"},
        {tic_key_p, "KeyP"},
        {tic_key_q, "KeyQ"},
        {tic_key_r, "KeyR"},
        {tic_key_s, "KeyS"},
        {tic_key_t, "KeyT"},
        {tic_key_u, "KeyU"},
        {tic_key_v, "KeyV"},
        {tic_key_w, "KeyW"},
        {tic_key_x, "KeyX"},
        {tic_key_y, "KeyY"},
        {tic_key_z, "KeyZ"},
        {tic_key_0, "Digit0"},
        {tic_key_1, "Digit1"},
        {tic_key_2, "Digit2"},
        {tic_key_3, "Digit3"},
        {tic_key_4, "Digit4"},
        {tic_key_5, "Digit5"},
        {tic_key_6, "Digit6"},
        {tic_key_7, "Digit7"},
        {tic_key_8, "Digit8"},
        {tic_key_9, "Digit9"},
        {tic_key_minus, "Minus"},
        {tic_key_equals, "Equal"},
        {tic_key_leftbracket, "BracketLeft"},
        {tic_key_rightbracket, "BracketRight"},
        {tic_key_backslash, "Backslash"},
        {tic_key_semicolon, "Semicolon"},
        {tic_key_apostrophe, "Quote"},
        {tic_key_grave, "Backquote"},
        {tic_key_comma, "Comma"},
        {tic_key_period, "Period"},
        {tic_key_slash, "Slash"},
        {tic_key_space, "Space"},
};

static void query(void)
{
    // The browser knows both the physical code and the key the layout prints.
    // A capture listener runs first; Shift and Alt change the key, so skip them.
    // clang-format off
    EM_ASM({
        if(!Module.tic80Keys)
        {
            Module.tic80Keys = {};
            window.addEventListener('keydown', function(e)
            {
                if(e.key && e.key.length === 1 && !e.shiftKey && !e.altKey)
                    Module.tic80Keys[e.code] = e.key;
            }, true);
        }
    });
    // clang-format on

    for(s32 i = 0; i < COUNT_OF(Positions); i++)
    {
        // clang-format off
        const int ch = EM_ASM_INT({
            const s = Module.tic80Keys[UTF8ToString($0)];
            return s ? s.charCodeAt(0) : 0;
        }, Positions[i].code);
        // clang-format on

        if(ch)
            set_key(Positions[i].key, (uint32_t)ch);
    }
}

#elif defined(__APPLE__)

#include <Carbon/Carbon.h>

static const struct
{
    tic_key key;
    CGKeyCode code;
} Positions[] =
    {
        {tic_key_a, 0x00},
        {tic_key_b, 0x0B},
        {tic_key_c, 0x08},
        {tic_key_d, 0x02},
        {tic_key_e, 0x0E},
        {tic_key_f, 0x03},
        {tic_key_g, 0x05},
        {tic_key_h, 0x04},
        {tic_key_i, 0x22},
        {tic_key_j, 0x26},
        {tic_key_k, 0x28},
        {tic_key_l, 0x25},
        {tic_key_m, 0x2E},
        {tic_key_n, 0x2D},
        {tic_key_o, 0x1F},
        {tic_key_p, 0x23},
        {tic_key_q, 0x0C},
        {tic_key_r, 0x0F},
        {tic_key_s, 0x01},
        {tic_key_t, 0x11},
        {tic_key_u, 0x20},
        {tic_key_v, 0x09},
        {tic_key_w, 0x0D},
        {tic_key_x, 0x07},
        {tic_key_y, 0x10},
        {tic_key_z, 0x06},
        {tic_key_0, 0x1D},
        {tic_key_1, 0x12},
        {tic_key_2, 0x13},
        {tic_key_3, 0x14},
        {tic_key_4, 0x15},
        {tic_key_5, 0x17},
        {tic_key_6, 0x16},
        {tic_key_7, 0x1A},
        {tic_key_8, 0x1C},
        {tic_key_9, 0x19},
        {tic_key_minus, 0x1B},
        {tic_key_equals, 0x18},
        {tic_key_leftbracket, 0x21},
        {tic_key_rightbracket, 0x1E},
        {tic_key_backslash, 0x2A},
        {tic_key_semicolon, 0x29},
        {tic_key_apostrophe, 0x27},
        {tic_key_grave, 0x32},
        {tic_key_comma, 0x2B},
        {tic_key_period, 0x2F},
        {tic_key_slash, 0x2C},
        {tic_key_space, 0x31},
};

static void query(void)
{
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();

    if(!source)
        return;

    CFDataRef data = TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData);
    const UCKeyboardLayout* layout = data ? (const UCKeyboardLayout*)CFDataGetBytePtr(data) : NULL;

    if(layout)
    {
        const UInt32 keyboard = LMGetKbdType();

        for(s32 i = 0; i < COUNT_OF(Positions); i++)
        {
            UniChar chars[8];
            UniCharCount len = 0;
            UInt32 dead = 0;

            if(UCKeyTranslate(layout, Positions[i].code, kUCKeyActionDown, 0, keyboard,
                              kUCKeyTranslateNoDeadKeysMask, &dead, COUNT_OF(chars), &len, chars) == noErr &&
               len > 0)
                set_key(Positions[i].key, chars[0]);
        }
    }

    CFRelease(source);
}

#elif defined(_WIN32)

#include <windows.h>

static const struct
{
    tic_key key;
    UINT scan;
} Positions[] =
    {
        {tic_key_a, 0x01E},
        {tic_key_b, 0x030},
        {tic_key_c, 0x02E},
        {tic_key_d, 0x020},
        {tic_key_e, 0x012},
        {tic_key_f, 0x021},
        {tic_key_g, 0x022},
        {tic_key_h, 0x023},
        {tic_key_i, 0x017},
        {tic_key_j, 0x024},
        {tic_key_k, 0x025},
        {tic_key_l, 0x026},
        {tic_key_m, 0x032},
        {tic_key_n, 0x031},
        {tic_key_o, 0x018},
        {tic_key_p, 0x019},
        {tic_key_q, 0x010},
        {tic_key_r, 0x013},
        {tic_key_s, 0x01F},
        {tic_key_t, 0x014},
        {tic_key_u, 0x016},
        {tic_key_v, 0x02F},
        {tic_key_w, 0x011},
        {tic_key_x, 0x02D},
        {tic_key_y, 0x015},
        {tic_key_z, 0x02C},
        {tic_key_0, 0x00B},
        {tic_key_1, 0x002},
        {tic_key_2, 0x003},
        {tic_key_3, 0x004},
        {tic_key_4, 0x005},
        {tic_key_5, 0x006},
        {tic_key_6, 0x007},
        {tic_key_7, 0x008},
        {tic_key_8, 0x009},
        {tic_key_9, 0x00A},
        {tic_key_minus, 0x00C},
        {tic_key_equals, 0x00D},
        {tic_key_leftbracket, 0x01A},
        {tic_key_rightbracket, 0x01B},
        {tic_key_backslash, 0x02B},
        {tic_key_semicolon, 0x027},
        {tic_key_apostrophe, 0x028},
        {tic_key_grave, 0x029},
        {tic_key_comma, 0x033},
        {tic_key_period, 0x034},
        {tic_key_slash, 0x035},
        {tic_key_space, 0x039},
};

static void query(void)
{
    const HKL layout = GetKeyboardLayout(0);

    for(s32 i = 0; i < COUNT_OF(Positions); i++)
    {
        const UINT vk = MapVirtualKeyEx(Positions[i].scan, MAPVK_VSC_TO_VK, layout);
        const uint32_t ch = MapVirtualKeyEx(vk, MAPVK_VK_TO_CHAR, layout) & 0x7FFF;

        if(ch)
            set_key(Positions[i].key, ch);
    }
}

#elif (defined(__linux__) && !defined(__ANDROID__)) || defined(__FreeBSD__)

#include <X11/XKBlib.h>
#include <X11/Xlib.h>

static const struct
{
    tic_key key;
    const char* name;
} Positions[] =
    {
        {tic_key_a, "AC01"},
        {tic_key_b, "AB05"},
        {tic_key_c, "AB03"},
        {tic_key_d, "AC03"},
        {tic_key_e, "AD03"},
        {tic_key_f, "AC04"},
        {tic_key_g, "AC05"},
        {tic_key_h, "AC06"},
        {tic_key_i, "AD08"},
        {tic_key_j, "AC07"},
        {tic_key_k, "AC08"},
        {tic_key_l, "AC09"},
        {tic_key_m, "AB07"},
        {tic_key_n, "AB06"},
        {tic_key_o, "AD09"},
        {tic_key_p, "AD10"},
        {tic_key_q, "AD01"},
        {tic_key_r, "AD04"},
        {tic_key_s, "AC02"},
        {tic_key_t, "AD05"},
        {tic_key_u, "AD07"},
        {tic_key_v, "AB04"},
        {tic_key_w, "AD02"},
        {tic_key_x, "AB02"},
        {tic_key_y, "AD06"},
        {tic_key_z, "AB01"},
        {tic_key_0, "AE10"},
        {tic_key_1, "AE01"},
        {tic_key_2, "AE02"},
        {tic_key_3, "AE03"},
        {tic_key_4, "AE04"},
        {tic_key_5, "AE05"},
        {tic_key_6, "AE06"},
        {tic_key_7, "AE07"},
        {tic_key_8, "AE08"},
        {tic_key_9, "AE09"},
        {tic_key_minus, "AE11"},
        {tic_key_equals, "AE12"},
        {tic_key_leftbracket, "AD11"},
        {tic_key_rightbracket, "AD12"},
        {tic_key_backslash, "BKSL"},
        {tic_key_semicolon, "AC10"},
        {tic_key_apostrophe, "AC11"},
        {tic_key_grave, "TLDE"},
        {tic_key_comma, "AB08"},
        {tic_key_period, "AB09"},
        {tic_key_slash, "AB10"},
        {tic_key_space, "SPCE"},
};

static Display* display;
static XkbDescPtr map;
static KeyCode codes[COUNT_OF(Positions)];
static bool opened;

static bool open_display(void)
{
    if(opened)
        return map != NULL;

    opened = true;
    display = XOpenDisplay(NULL);

    if(!display)
        return false;

    map = XkbGetMap(display, XkbAllClientInfoMask, XkbUseCoreKbd);

    if(!map || XkbGetNames(display, XkbKeyNamesMask, map) != Success)
    {
        if(map)
            XkbFreeKeyboard(map, 0, True);
        map = NULL;
        return false;
    }

    for(s32 code = map->min_key_code; code <= map->max_key_code; code++)
        for(s32 i = 0; i < COUNT_OF(Positions); i++)
            if(strncmp(map->names->keys[code].name, Positions[i].name, XkbKeyNameLength) == 0)
                codes[i] = (KeyCode)code;

    return true;
}

static void query(void)
{
    if(!open_display())
        return;

    if(XkbGetUpdatedMap(display, XkbAllClientInfoMask, map) != Success)
        return;

    XkbStateRec state;

    if(XkbGetState(display, XkbUseCoreKbd, &state) != Success)
        return;

    for(s32 i = 0; i < COUNT_OF(Positions); i++)
    {
        if(!codes[i])
            continue;

        // A key may have fewer groups than the active one; SDL clamps the same way.
        int group = state.group;
        const int groups = XkbKeyNumGroups(map, codes[i]);

        if(group >= groups)
            group = groups ? groups - 1 : 0;

        const KeySym sym = XkbKeySymEntry(map, codes[i], 0, group);

        if(sym > 0)
            set_key(Positions[i].key, (uint32_t)sym);
    }
}

#else

// Android reports its own key codes; this layer leaves them to the Keymap.
static void query(void)
{
}

#endif

tic_layout tic_sys_layout_refresh(void)
{
    memset(keys, 0, sizeof keys);
    memset(known, 0, sizeof known);

    query();
    resolve();

    return classify();
}
