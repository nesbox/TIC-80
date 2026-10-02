#pragma once

#include "studio/system.h"
#include "sokol.h"      // the art texture the renderer draws with

#define CONTROLS_MAX_POINTERS   8
#define CONTROLS_MAX_QUADS      16

typedef enum
{
    controls_mode_none,
    controls_mode_gamepad,
    controls_mode_keyboard,
} ControlsMode;

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
    bool visible;                   // the layer decides, the module animates
    float dt;
    u8 alpha;                       // the theme's touch alpha
} ControlsInput;

typedef struct
{
    float x, y, w, h;               // where, in window pixels
    float u0, v0, u1, v1;           // what, in the texture's pixels
} ControlsQuad;

typedef struct
{
    bool visible;                   // the layout below means anything
    float x, y, w, h;               // the picture's rectangle
    ControlsQuad quads[CONTROLS_MAX_QUADS];
    s32 quadCount;
    tic80_gamepad gamepad;          // bits the controls are holding
    bool menu;                      // the menu control was tapped
} ControlsState;

void controls_init(const tic_cartridge* cart);
void controls_shutdown(void);
void controls_reset(void);
void controls_update(const ControlsInput* input);
const ControlsState* controls_state(void);
sg_view controls_view(void);
