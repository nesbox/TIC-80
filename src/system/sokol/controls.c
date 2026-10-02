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

// The on-screen keyboard is the config cart's map, once as it is and once
// pressed, with the legends drawn on by the studio's own font.
#define KBD_COLS        22
#define KBD_ROWS        17
#define KBD_WIDTH       (KBD_COLS * TIC_SPRITESIZE)
#define KBD_HEIGHT      (KBD_ROWS * TIC_SPRITESIZE)

// One control tile is this share of the window's width, the way the SDL layer
// sizes the same controls.
#define TILE_PORTRAIT   7
#define TILE_LANDSCAPE  15

// How long the controls take to slide in and out.
#define APPEAR_TIME     0.18f

static const tic_key KbdLayout[] =
{
    #include "../kbdlayout.inl"
};

static struct
{
    sg_image        art[controls_tex_count];
    sg_view         view[controls_tex_count];
    sg_sampler      nearest;
    Clay_Arena      arena;

    float           appear;         // 0 hidden, 1 fully out
    ControlsState   state;
} controls;

// CLAY_ID() only takes literals; the controls are looked up by name at runtime.
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

// The same legends the SDL layer draws on the keyboard.
static void draw_keyboard_labels(tic_mem* tic, s32 shift)
{
    typedef struct { const char* text; s32 x; s32 y; bool alt; const char* shift; } Label;

    static const Label Labels[] =
    {
        #include "../kbdlabels.inl"
    };

    for (s32 i = 0; i < COUNT_OF(Labels); i++)
    {
        const Label* label = Labels + i;

        if (label->text)
            tic_api_print(tic, label->text, label->x, label->y + shift, tic_color_grey, true, 1, label->alt);

        if (label->shift)
            tic_api_print(tic, label->shift, label->x + 6, label->y + shift + 2, tic_color_light_grey, true, 1, label->alt);
    }
}

static sg_image make_art_image(const u32* pixels, const char* label)
{
    return sg_make_image(&(sg_image_desc){
        .width = TIC80_FULLWIDTH,
        .height = TIC80_FULLHEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .data.mip_levels[0] = { .ptr = pixels, .size = TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof(u32) },
        .label = label,
    });
}

static void build_buttons(const tic_cartridge* cart)
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

    controls.art[controls_tex_buttons] = make_art_image(tic->product.screen, "tic80-buttons");

    tic_core_close(tic);
}

static void build_keyboard(const tic_cartridge* cart, bool down, ControlsTexture cell)
{
    tic_mem* tic = tic_core_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_RGBA8888);

    const tic_bank* bank = &cart->bank0;

    memcpy(tic->ram->vram.palette.data, &bank->palette.vbank0, sizeof(tic_palette));
    memcpy(tic->ram->map.data, &bank->map, sizeof(tic_map));
    memcpy(tic->ram->tiles.data, &bank->tiles, sizeof(tic_tiles) * TIC_SPRITE_BANKS);

    tic_api_cls(tic, 0);

    // The map carries the keyboard twice: released and pressed.
    tic_api_map(tic, down ? KBD_COLS : 0, 0, KBD_COLS, KBD_ROWS, 0, 0, NULL, 0, 1, NULL, NULL);
    draw_keyboard_labels(tic, down ? 2 : 0);
    tic_core_blit(tic);

    controls.art[cell] = make_art_image(tic->product.screen, down ? "tic80-keyboard-down" : "tic80-keyboard");

    tic_core_close(tic);
}

void controls_init(const tic_cartridge* cart)
{
    build_buttons(cart);
    build_keyboard(cart, false, controls_tex_keyboard);
    build_keyboard(cart, true, controls_tex_keyboard_down);

    for (s32 i = 0; i < controls_tex_count; i++)
        controls.view[i] = sg_make_view(&(sg_view_desc){
            .texture.image = controls.art[i],
            .label = "tic80-controls-view",
        });

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

    for (s32 i = 0; i < controls_tex_count; i++)
    {
        sg_destroy_view(controls.view[i]);
        sg_destroy_image(controls.art[i]);
    }

    free(controls.arena.memory);
}

sg_view controls_view(ControlsTexture texture)
{
    return controls.view[texture];
}

void controls_reset(void)
{
    controls.appear = 0.0f;
}

const ControlsState* controls_state(void)
{
    return &controls.state;
}

static void add_quad(ControlsTexture texture, float x, float y, float w, float h,
    float u0, float v0, float u1, float v1)
{
    ControlsState* state = &controls.state;

    if (state->quadCount >= CONTROLS_MAX_QUADS)
        return;

    ControlsQuad* quad = &state->quads[state->quadCount++];

    *quad = (ControlsQuad){ x, y, w, h, u0, v0, u1, v1, (u8)texture };
}

static bool element_box(const char* name, float* x, float* y, float* w, float* h)
{
    const Clay_ElementData data = Clay_GetElementData(id_of(name));

    if (!data.found)
        return false;

    *x = data.boundingBox.x;
    *y = data.boundingBox.y;
    *w = data.boundingBox.width;
    *h = data.boundingBox.height;
    return true;
}

static bool point_in(const ControlsInput* input, float x, float y, float w, float h)
{
    for (s32 p = 0; p < input->pointerCount; p++)
    {
        const ControlsPointer* pointer = &input->pointers[p];

        if (pointer->down && pointer->x >= x && pointer->x < x + w && pointer->y >= y && pointer->y < y + h)
            return true;
    }

    return false;
}

// One button of the pad: its box comes from the layout, its art from the sheet.
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

#undef PAD_ID
}

// The direction pad is one region, the way a real one is: where the thumb
// lands inside it decides which sides are held, and the middle cell divides
// into thirds so a diagonal comes from one finger.
static void dpad_held(const ControlsInput* input, float x, float y, float w, float h, bool held[4])
{
    for (s32 p = 0; p < input->pointerCount; p++)
    {
        const ControlsPointer* pointer = &input->pointers[p];

        if (!pointer->down || pointer->x < x || pointer->x >= x + w || pointer->y < y || pointer->y >= y + h)
            continue;

        const float unit = w / 3.0f;
        const s32 col = (s32)((pointer->x - x) / unit);
        const s32 row = (s32)((pointer->y - y) / unit);

        if (row == 0) held[0] = true; else if (row == 2) held[1] = true;
        if (col == 0) held[2] = true; else if (col == 2) held[3] = true;

        if (row == 1 && col == 1)
        {
            const float sub = unit / 3.0f;
            const s32 sx = (s32)((pointer->x - x - unit) / sub);
            const s32 sy = (s32)((pointer->y - y - unit) / sub);

            if (sy == 0) held[0] = true; else if (sy == 2) held[1] = true;
            if (sx == 0) held[2] = true; else if (sx == 2) held[3] = true;
        }
    }
}

static void gamepad_quads(const ControlsInput* input)
{
    ControlsState* state = &controls.state;

    enum { Up, Down, Left, Right };

    static const struct { const char* name; tic80_gamepad bit; } Pad[] = {
        { "l_up",    { .up = 1 } },
        { "l_down",  { .down = 1 } },
        { "l_left",  { .left = 1 } },
        { "l_right", { .right = 1 } },
    };

    static const struct { const char* name; tic80_gamepad bit; } Buttons[] = {
        { "r_a", { .a = 1 } },
        { "r_b", { .b = 1 } },
        { "r_x", { .x = 1 } },
        { "r_y", { .y = 1 } },
    };

    float px, py, pw, ph;
    bool held[4] = { false, false, false, false };

    if (element_box("l_pad", &px, &py, &pw, &ph))
        dpad_held(input, px, py, pw, ph, held);

    for (s32 i = 0; i < COUNT_OF(Pad); i++)
    {
        float x, y, w, h;

        if (!element_box(Pad[i].name, &x, &y, &w, &h))
            continue;

        if (held[i])
            state->gamepad.data |= Pad[i].bit.data;

        // The pressed state is the row of tiles under the released one.
        const float v = ART_TOP + (held[i] ? ART_TILE : 0);
        const float u = ART_LEFT + i * ART_TILE;

        add_quad(controls_tex_buttons, x, y, w, h, u, v, u + ART_TILE, v + ART_TILE);
    }

    for (s32 i = 0; i < COUNT_OF(Buttons); i++)
    {
        float x, y, w, h;

        if (!element_box(Buttons[i].name, &x, &y, &w, &h))
            continue;

        // A button is pressed by a finger of its own: two of them at once is
        // two fingers, which is what a phone has.
        const bool pressed = point_in(input, x, y, w, h);

        if (pressed)
            state->gamepad.data |= Buttons[i].bit.data;

        const float v = ART_TOP + (pressed ? ART_TILE : 0);
        const float u = ART_LEFT + (i + 4) * ART_TILE;

        add_quad(controls_tex_buttons, x, y, w, h, u, v, u + ART_TILE, v + ART_TILE);
    }
}

static void keyboard_quads(const ControlsInput* input, float x, float y, float w, float h)
{
    ControlsState* state = &controls.state;

    for (s32 p = 0; p < input->pointerCount; p++)
    {
        const ControlsPointer* pointer = &input->pointers[p];

        if (!pointer->down || pointer->x < x || pointer->x >= x + w || pointer->y < y || pointer->y >= y + h)
            continue;

        const s32 col = (s32)((pointer->x - x) * KBD_COLS / w);
        const s32 row = (s32)((pointer->y - y) * KBD_ROWS / h);
        const tic_key key = KbdLayout[row * KBD_COLS + col];

        if (key != tic_key_unknown && key < tic_keys_count)
            state->keys[key] = true;
    }

    add_quad(controls_tex_keyboard, x, y, w, h,
        TIC80_OFFSET_LEFT, TIC80_OFFSET_TOP,
        TIC80_OFFSET_LEFT + KBD_WIDTH, TIC80_OFFSET_TOP + KBD_HEIGHT);

    // Every cell of a held key is drawn pressed, the way the SDL layer draws it.
    for (s32 i = 0; i < COUNT_OF(KbdLayout); i++)
    {
        const tic_key key = KbdLayout[i];

        if (key == tic_key_unknown || key >= tic_keys_count || !state->keys[key])
            continue;

        const float cw = w / KBD_COLS;
        const float ch = h / KBD_ROWS;
        const float cx = x + (i % KBD_COLS) * cw;
        const float cy = y + (i / KBD_COLS) * ch;
        const float u = TIC80_OFFSET_LEFT + (i % KBD_COLS) * ART_TILE;
        const float v = TIC80_OFFSET_TOP + (i / KBD_COLS) * ART_TILE;

        add_quad(controls_tex_keyboard_down, cx, cy, cw, ch, u, v, u + ART_TILE, v + ART_TILE);
    }
}

// The menu control is the keyboard's own ESC key, so a phone can leave a game.
static void menu_quad(const ControlsInput* input)
{
    float x, y, w, h;

    if (!element_box("menu", &x, &y, &w, &h))
        return;

    if (point_in(input, x, y, w, h))
        controls.state.menu = true;

    add_quad(controls_tex_keyboard, x, y, w, h,
        TIC80_OFFSET_LEFT, TIC80_OFFSET_TOP,
        TIC80_OFFSET_LEFT + ART_TILE, TIC80_OFFSET_TOP + ART_TILE);
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
    memset(state->keys, 0, sizeof state->keys);
    state->menu = false;

    // The on-screen keyboard is for a keyboard cart and for the studio's own
    // editors, where the player writes code; it only fits upright.
    const bool keyboard = input->mode == controls_mode_keyboard && input->portrait;

    state->visible = controls.appear > 0.0f
        && (input->mode == controls_mode_gamepad || keyboard);

    if (!state->visible)
        return;

    const float unit = (input->portrait ? input->width / TILE_PORTRAIT : input->width / TILE_LANDSCAPE) * controls.appear;
    const float kbd = input->width / (KBD_WIDTH / (float)KBD_HEIGHT) * controls.appear;

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
        // The picture takes what the controls leave; the renderer fits the
        // framebuffer into it, so the layout has no opinion about 16:9.
        if (input->portrait)
        {
            CLAY(id_of("player"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
            }) {}

            if (keyboard)
            {
                CLAY(id_of("kbd"), {
                    .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(kbd) } },
                }) {}
            }
            else
            {
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
        }
        else
        {
            layout_pad(unit, true);

            CLAY(id_of("player"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } },
            }) {}

            layout_pad(unit, false);
        }

        CLAY(id_of("menu"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(unit), CLAY_SIZING_FIXED(unit) } },
            .floating = {
                .attachTo = CLAY_ATTACH_TO_PARENT,
                .attachPoints = { .element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP },
                .offset = { -unit, unit / 2 },
            },
        }) {}
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

    if (keyboard)
    {
        float x, y, w, h;

        if (element_box("kbd", &x, &y, &w, &h))
            keyboard_quads(input, x, y, w, h);
    }
    else
        gamepad_quads(input);

    menu_quad(input);

}
