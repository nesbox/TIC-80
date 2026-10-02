#pragma once

#include "studio/system.h"
#include "sokol.h"      // the art textures the renderer draws with

// The keyboard grid carries one cell that is not a key: the one that brings
// the on-screen keyboard up, the way the SDL layer numbers it.
enum
{
    tic_key_board = tic_keys_count + 1,
    tic_touch_size,
};

#define CONTROLS_MAX_POINTERS   8
#define CONTROLS_MAX_QUADS      40

typedef enum
{
    controls_mode_none,
    controls_mode_gamepad,
    controls_mode_keyboard,
} ControlsMode;

// Which sheet a rectangle is cut out of.
typedef enum
{
    controls_tex_buttons,
    controls_tex_keyboard,
    controls_tex_keyboard_down,
    controls_tex_count,
} ControlsTexture;

typedef struct
{
    float x, y;
    bool  down;
} ControlsPointer;

// Everything the geometry needs about the world. No platform lives past this.
typedef struct
{
    float width, height;
    struct { float left, top, right, bottom; } insets;
    ControlsPointer pointers[CONTROLS_MAX_POINTERS];
    s32 pointerCount;
    ControlsMode mode;
    bool portrait;
    u8 gamepad;                     // what the machine sees of the first pad
    const bool* keys;               // the keys it sees, or NULL
    bool visible;                   // the layer decides, the module animates
    float dt;
    u8 alpha;                       // the theme's touch alpha
} ControlsInput;

typedef struct
{
    float x, y, w, h;               // where, in window pixels
    float u0, v0, u1, v1;           // what, in the texture's pixels
    u8 texture;
} ControlsQuad;

typedef struct
{
    bool visible;                   // the layout below means anything
    float x, y, w, h;               // the picture's rectangle
    ControlsQuad quads[CONTROLS_MAX_QUADS];
    s32 quadCount;
    tic80_gamepad gamepad;          // bits the gamepad controls are holding
    bool keys[tic_keys_count];      // keys the on-screen keyboard is holding
    bool menu;                      // the menu control was tapped
    u8 claimed;                     // bit per pointer a control took
} ControlsState;

void controls_init(const tic_cartridge* cart);
void controls_shutdown(void);
void controls_reset(void);
void controls_update(const ControlsInput* input);
const ControlsState* controls_state(void);
sg_view controls_view(ControlsTexture texture);
