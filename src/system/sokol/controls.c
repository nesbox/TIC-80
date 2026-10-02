#include "controls.h"
#include "tools.h"

#include "sokol.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The button art lives in the studio's own config cart: one row of tiles for
// the released state and the row under it for the pressed one.
#define ART_TILE        TIC_SPRITESIZE
#define ART_LEFT        (TIC80_MARGIN_LEFT + 8 * ART_TILE)
#define ART_TOP         TIC80_MARGIN_TOP

// One control tile is this share of the window's width, the way the SDL layer
// sizes the same controls.
#define TILE_PORTRAIT   7
#define TILE_LANDSCAPE  15

// How long the controls take to slide in and out.
#define APPEAR_TIME     0.18f

static struct
{
    sg_image        art;
    sg_view         artView;
    sg_sampler      nearest;
    Clay_Arena      arena;

    float           appear;         // 0 hidden, 1 fully out
    ControlsState   state;
} controls;

// CLAY_ID() only takes literals; the buttons are looked up by name at runtime.
static Clay_ElementId id_of(const char* name)
{
    return Clay_GetElementId((Clay_String){
        .isStaticallyAllocated = false,
        .length = (s32)strlen(name),
        .chars = name,
    });
}

static void on_clay_error(Clay_ErrorData error)
{
    printf("controls: clay error %d: %s\n", (s32)error.errorType, error.errorText.chars);
}

static void build_art(const tic_cartridge* cart)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);

    const tic_bank* bank = &cart->bank0;

    memcpy(tic->ram->vram.palette.data, &bank->palette.vbank0, sizeof(tic_palette));
    memcpy(tic->ram->tiles.data, &bank->tiles, sizeof(tic_tiles));

    tic_api_spr(tic, 0, 0, 0, TIC_SPRITESHEET_COLS, TIC_SPRITESHEET_COLS, NULL, 0, 1, tic_no_flip, tic_no_rotate);
    tic_core_blit(tic);

    // The art's background is the palette's first colour, and has to be a hole.
    const u32 key = tic_rgba(&bank->palette.vbank0.colors[0]);

    for (u32* pix = tic->product.screen, *end = pix + TIC80_FULLWIDTH * TIC80_FULLHEIGHT; pix != end; ++pix)
        if (*pix == key)
            *pix = 0;

    controls.art = sg_make_image(&(sg_image_desc){
        .width = TIC80_FULLWIDTH,
        .height = TIC80_FULLHEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.mip_levels[0] = { .ptr = tic->product.screen, .size = TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof(u32) },
        .label = "tic80-controls",
    });

    tic_core_close(tic);

    controls.artView = sg_make_view(&(sg_view_desc){
        .texture.image = controls.art,
        .label = "tic80-controls-view",
    });
}

void controls_init(const tic_cartridge* cart)
{
    build_art(cart);

    controls.nearest = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "tic80-controls-nearest",
    });

    const uint32_t size = Clay_MinMemorySize();
    controls.arena = Clay_CreateArenaWithCapacityAndMemory(size, malloc(size));

    Clay_Initialize(controls.arena, (Clay_Dimensions){ 0, 0 },
        (Clay_ErrorHandler){ on_clay_error, NULL });
}

void controls_shutdown(void)
{
    sg_destroy_sampler(controls.nearest);
    sg_destroy_view(controls.artView);
    sg_destroy_image(controls.art);
    free(controls.arena.memory);
}

sg_view controls_view(void)
{
    return controls.artView;
}

void controls_reset(void)
{
    controls.appear = 0.0f;
}

const ControlsState* controls_state(void)
{
    return &controls.state;
}

// One button of the pad: its box comes from the layout, its art from the sheet.
typedef struct
{
    Clay_ElementId id;
    tic_key         unused;
} ControlButton;

static const char* const ButtonNames[] = { "up", "down", "left", "right", "a", "b", "x", "y" };

static bool button_box(Clay_ElementId id, float* x, float* y, float* w, float* h)
{
    const Clay_ElementData data = Clay_GetElementData(id);

    if (!data.found)
        return false;

    *x = data.boundingBox.x;
    *y = data.boundingBox.y;
    *w = data.boundingBox.width;
    *h = data.boundingBox.height;
    return true;
}

static void add_button_quad(s32 index, float x, float y, float w, float h, bool pressed)
{
    ControlsState* state = &controls.state;

    if (state->quadCount >= CONTROLS_MAX_QUADS)
        return;

    ControlsQuad* quad = &state->quads[state->quadCount++];

    quad->x = x;
    quad->y = y;
    quad->w = w;
    quad->h = h;
    quad->u0 = ART_LEFT + index * ART_TILE;
    quad->v0 = ART_TOP + (pressed ? ART_TILE : 0);
    quad->u1 = quad->u0 + ART_TILE;
    quad->v1 = quad->v0 + ART_TILE;
}

static void layout_pad(float unit, bool left)
{
    const Clay_SizingAxis fixed = CLAY_SIZING_FIXED(unit);
    const char* const side = left ? "l_" : "r_";
    char name[16];

#define PAD_ID(base) (snprintf(name, sizeof name, "%s%s", side, base), id_of(name))

    CLAY(id_of(left ? "l_pad" : "r_pad"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(unit * 3), CLAY_SIZING_FIXED(unit * 3) },
            .childGap = 0,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    })
    {
        if (left)
        {
            CLAY(PAD_ID("up"), { .layout = { .sizing = { fixed, fixed } } }) {}
            CLAY(PAD_ID("middle"), {
                .layout = { .sizing = { CLAY_SIZING_FIXED(unit * 3), fixed }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER } },
            })
            {
                CLAY(PAD_ID("left"), { .layout = { .sizing = { fixed, fixed } } }) {}
                CLAY(PAD_ID("mspacer"), { .layout = { .sizing = { fixed, fixed } } }) {}
                CLAY(PAD_ID("right"), { .layout = { .sizing = { fixed, fixed } } }) {}
            }
            CLAY(PAD_ID("down"), { .layout = { .sizing = { fixed, fixed } } }) {}
        }
        else
        {
            CLAY(PAD_ID("y"), { .layout = { .sizing = { fixed, fixed } } }) {}
            CLAY(PAD_ID("middle2"), {
                .layout = { .sizing = { CLAY_SIZING_FIXED(unit * 3), fixed }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER } },
            })
            {
                CLAY(PAD_ID("x"), { .layout = { .sizing = { fixed, fixed } } }) {}
                CLAY(PAD_ID("mspacer2"), { .layout = { .sizing = { fixed, fixed } } }) {}
                CLAY(PAD_ID("b"), { .layout = { .sizing = { fixed, fixed } } }) {}
            }
            CLAY(PAD_ID("a"), { .layout = { .sizing = { fixed, fixed } } }) {}
        }
    }
}

void controls_update(const ControlsInput* input)
{
    ControlsState* state = &controls.state;


    const float target = input->visible ? 1.0f : 0.0f;
    const float step = input->dt > 0.0f ? input->dt / APPEAR_TIME : 1.0f;

    if (controls.appear < target)
        controls.appear = MIN(controls.appear + step, target);
    else if (controls.appear > target)
        controls.appear = MAX(controls.appear - step, target);

    state->quadCount = 0;
    state->gamepad.data = 0;
    state->menu = false;
    state->visible = controls.appear > 0.0f && input->mode != controls_mode_none;

    if (!state->visible)
        return;

    const float unit = (input->portrait ? input->width / TILE_PORTRAIT : input->width / TILE_LANDSCAPE) * controls.appear;

    Clay_SetLayoutDimensions((Clay_Dimensions){ input->width, input->height });
    Clay_BeginLayout();

    CLAY(id_of("root"), {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
            .padding = {
                (u16)input->insets.left, (u16)input->insets.right,
                (u16)input->insets.top, (u16)input->insets.bottom
            },
            .childGap = (u16)(unit / 3),
            // Portrait stacks the picture over the strip from the top; landscape
            // centres the pads against the picture.
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = input->portrait ? CLAY_ALIGN_Y_TOP : CLAY_ALIGN_Y_CENTER },
            .layoutDirection = input->portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
        },
    })
    {
        if (input->portrait)
        {
            // The picture takes what is left, the strip the bottom.
            // The picture takes what the strip leaves; the renderer fits the
            // framebuffer into it, so the layout has no opinion about 16:9.
            CLAY(id_of("player"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
            }) {}

            CLAY(id_of("strip"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(unit * 3 + unit / 4) },
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
                },
            })
            {
                layout_pad(unit, true);

                CLAY(id_of("gap"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) } } }) {}

                layout_pad(unit, false);
            }
        }
        else
        {
            layout_pad(unit, true);

            CLAY(id_of("player"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
            }) {}

            layout_pad(unit, false);
        }
    }

    Clay_EndLayout(input->dt);


    const Clay_ElementData player = Clay_GetElementData(id_of("player"));

    if (player.found)
    {
        state->x = player.boundingBox.x;
        state->y = player.boundingBox.y;
        state->w = player.boundingBox.width;
        state->h = player.boundingBox.height;
    }


    // The pointers are tested against the boxes the layout produced; one
    // pointer presses at most one control.
    const char* const names[] = { "l_up", "l_down", "l_left", "l_right", "r_a", "r_b", "r_x", "r_y" };

    for (s32 i = 0; i < COUNT_OF(names); i++)
    {
        float x, y, w, h;


        if (!button_box(id_of(names[i]), &x, &y, &w, &h))
            continue;

        bool pressed = false;

        for (s32 p = 0; p < input->pointerCount; p++)
        {
            const ControlsPointer* pointer = &input->pointers[p];

            if (pointer->down && pointer->x >= x && pointer->x < x + w && pointer->y >= y && pointer->y < y + h)
            {
                pressed = true;
                break;
            }
        }

        if (pressed)
        {
            static const tic80_gamepad bits[8] = {
                { .up = 1 }, { .down = 1 }, { .left = 1 }, { .right = 1 },
                { .a = 1 }, { .b = 1 }, { .x = 1 }, { .y = 1 },
            };
            state->gamepad.data |= bits[i].data;
        }

        add_button_quad(i, x, y, w, h, pressed);
    }

    float mx, my, mw, mh;

    if (button_box(id_of("menu"), &mx, &my, &mw, &mh))
    {
        for (s32 p = 0; p < input->pointerCount; p++)
        {
            const ControlsPointer* pointer = &input->pointers[p];

            if (pointer->down && pointer->x >= mx && pointer->x < mx + mw && pointer->y >= my && pointer->y < my + mh)
                state->menu = true;
        }
    }
}
