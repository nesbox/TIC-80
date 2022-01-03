// MIT License

// Copyright (c) 2020 Adrian "asie" Siekierka

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include <sys/stat.h>

#include <3ds.h>
#include <citro3d.h>

#include "studio/system.h"

#include "keyboard.h"
#include "utils.h"

// #define RENDER_CONSOLE_TOP
#define AUDIO_FREQ 44100
#define AUDIO_BLOCKS 4

typedef struct {
	float x, y, z;
	float u, v, opa;
} texture_vertex;

static struct
{
    Studio* studio;
    char* clipboard;

    struct
    {
        u32* tic_buf;
        C3D_Tex tic_tex, tic_scaler_tex;
        C3D_Mtx proj_top, proj_bottom, proj_scaler;
        C3D_RenderTarget *target_top, *target_bottom, *target_scaler;

        DVLB_s* shader_dvlb;
        shaderProgram_s shader_program;
        int shader_proj_mtx_loc;
        C3D_AttrInfo shader_attr;

        int scaled;
        bool on_bottom;
        bool wide_mode;

	texture_vertex *vbuf;
	int vbuf_pos;
	int vbuf_size;
    } render;

    struct {
        int x, y;
        int width, height;
    } screen_size;

    struct
    {
        int samples, buffer_size, curr_block;
        u8* buffer;
        ndspWaveBuf ndspBuf[AUDIO_BLOCKS];
    } audio;

    tic_n3ds_keyboard keyboard;

} platform;

// 256-wide
// 384-wide
// 400-wide
#define N3DS_SCALED_MODE_COUNT 3

static const char n3ds_shader_shbin[289] = {
	0x44, 0x56, 0x4c, 0x42, 0x01, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00,
	0x00, 0x44, 0x56, 0x4c, 0x50, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00,
	0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x08,
	0x00, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x4e, 0x01, 0xf0, 0x07, 0x4e, 0x02, 0x10, 0x20, 0x4e, 0x01, 0xf0,
	0x27, 0x4e, 0x03, 0x08, 0x02, 0x08, 0x04, 0x18, 0x02, 0x08, 0x05,
	0x28, 0x02, 0x08, 0x06, 0x38, 0x02, 0x08, 0x07, 0x10, 0x20, 0x4c,
	0x07, 0x10, 0x41, 0x4c, 0x00, 0x00, 0x00, 0x88, 0x6e, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xa1, 0x0a, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x4e, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68,
	0xc3, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0xc3, 0x06, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x62, 0xc3, 0x06, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x61, 0xc3, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6f, 0x03,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x56, 0x4c, 0x45, 0x02,
	0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x6c,
	0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x74, 0x00, 0x00, 0x00,
	0x0b, 0x00, 0x00, 0x00, 0x02, 0x00, 0x5f, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,
	0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x01, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x00,
	0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x13,
	0x00, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e,
	0x00, 0x00, 0x00
};

static const int n3ds_shader_shbin_length = 289;

static void n3ds_vbuf_init(void) {
    C3D_BufInfo *bufInfo;
    platform.render.vbuf_pos = 0;
    platform.render.vbuf_size = 6 * 160;
    platform.render.vbuf = linearAlloc(sizeof(texture_vertex) * platform.render.vbuf_size);

    bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, platform.render.vbuf, sizeof(texture_vertex), 2, 0x10);
}

static void n3ds_vbuf_exit(void) {
    linearFree(platform.render.vbuf);
    platform.render.vbuf_size = 0;
}

static int n3ds_vbuf_clear(void) {
    platform.render.vbuf_pos = 0;
}

static int n3ds_vbuf_append(float x, float y, float z, float u, float v, float opa) {
    int pos = platform.render.vbuf_pos++;
    platform.render.vbuf[pos].x = x;
    platform.render.vbuf[pos].y = y;
    platform.render.vbuf[pos].z = z;
    platform.render.vbuf[pos].u = u;
    platform.render.vbuf[pos].v = v;
    platform.render.vbuf[pos].opa = opa;
    return pos;
}

void tic_sys_clipboard_set(const char* text)
{
    if(platform.clipboard)
    {
        free(platform.clipboard);
        platform.clipboard = NULL;
    }

    platform.clipboard = strdup(text);
}

bool tic_sys_clipboard_has()
{
    return platform.clipboard != NULL;
}

char* tic_sys_clipboard_get()
{
    return platform.clipboard ? strdup(platform.clipboard) : NULL;
}

void tic_sys_clipboard_free(const char* text)
{
    free((void*) text);
}

u64 tic_sys_counter_get()
{
    return svcGetSystemTick();
}

u64 tic_sys_freq_get()
{
    return SYSCLOCK_ARM11;
}

void tic_sys_fullscreen_set(bool value)
{
}

bool tic_sys_fullscreen_get()
{
}

void tic_sys_message(const char* title, const char* message)
{
}

void tic_sys_title(const char* title)
{
}

void tic_sys_open_path(const char* path)
{
}

void tic_sys_preseed()
{
    srand(osGetTime());
    rand();
}

void tic_sys_update_config()
{

}

void tic_sys_default_mapping(tic_mapping* mapping)
{
    *mapping = (tic_mapping)
    {
        tic_key_up,
        tic_key_down,
        tic_key_left,
        tic_key_right,

        tic_key_z, // a
        tic_key_x, // b
        tic_key_a, // x
        tic_key_s, // y
    };
}

bool tic_sys_keyboard_text(char* text)
{
    return false;
}

static void update_screen_size(void) {
    int scr_width = platform.render.on_bottom ? 320 : 400;

    if (platform.render.scaled > 0) {
        float sw = platform.render.on_bottom ? 320.0f : (platform.render.scaled == 2 ? 400.0f : 384.0f);
        float sh = TIC80_FULLHEIGHT * sw / TIC80_FULLWIDTH;
        platform.screen_size.width = (int) (sw + 0.5f);
        platform.screen_size.height = (int) (sh + 0.5f);
    } else {
        platform.screen_size.width = TIC80_FULLWIDTH;
        platform.screen_size.height = TIC80_FULLHEIGHT;
    }

    platform.screen_size.x = (scr_width - platform.screen_size.width) >> 1;
    platform.screen_size.y = (240 - platform.screen_size.height) >> 1;
}

static void n3ds_draw_init(void)
{
    u8 consoleModelId = 3;
    C3D_TexEnv *texEnv;

    platform.render.wide_mode = false;
    CFGU_GetSystemModel(&consoleModelId);
    if (consoleModelId != 3) { // O2DS
        gfxSetWide(true);
        platform.render.wide_mode = true;
    }

    C3D_Init(0x10000);

    platform.render.target_top = C3D_RenderTargetCreate(240, platform.render.wide_mode ? 800 : 400, GPU_RB_RGB8, 0);
    platform.render.target_bottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGB8, 0);
    C3D_RenderTargetClear(platform.render.target_top, C3D_CLEAR_ALL, 0, 0);
    C3D_RenderTargetClear(platform.render.target_bottom, C3D_CLEAR_ALL, 0, 0);
    C3D_RenderTargetSetOutput(platform.render.target_top, GFX_TOP, GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
    C3D_RenderTargetSetOutput(platform.render.target_bottom, GFX_BOTTOM, GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));

    platform.render.tic_buf = linearAlloc(256 * 256 * 4);
    C3D_TexInitVRAM(&platform.render.tic_tex, 256, 256, GPU_RGBA8); // 256KB
    // maximum size: (256 x 144) * 3 => 768 x 432
    C3D_TexInitVRAM(&platform.render.tic_scaler_tex, 1024, 512, GPU_RGBA8); // 2MB

    platform.render.target_scaler = C3D_RenderTargetCreateFromTex(&platform.render.tic_scaler_tex, GPU_TEXFACE_2D, 0, 0);

    Mtx_OrthoTilt(&platform.render.proj_top, 0.0, 400.0, 0.0, 240.0, -1.0, 1.0, true);
    Mtx_OrthoTilt(&platform.render.proj_bottom, 0.0, 320.0, 0.0, 240.0, -1.0, 1.0, true);
    Mtx_Ortho(&platform.render.proj_scaler, 0.0, 1024.0, 512.0, 0.0, -1.0, 1.0, true);

    C3D_TexSetFilter(&platform.render.tic_tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetFilter(&platform.render.tic_scaler_tex, GPU_LINEAR, GPU_LINEAR);

    texEnv = C3D_GetTexEnv(0);
    C3D_TexEnvSrc(texEnv, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvOpRgb(texEnv, 0, 0, 0);
    C3D_TexEnvOpAlpha(texEnv, 0, 0, 0);
    C3D_TexEnvFunc(texEnv, C3D_Both, GPU_MODULATE);

    C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);

    platform.render.shader_dvlb = DVLB_ParseFile((u32 *) n3ds_shader_shbin, n3ds_shader_shbin_length);
    shaderProgramInit(&(platform.render.shader_program));
    shaderProgramSetVsh(&(platform.render.shader_program), &(platform.render.shader_dvlb->DVLE[0]));
    platform.render.shader_proj_mtx_loc = shaderInstanceGetUniformLocation(platform.render.shader_program.vertexShader, "projection");
    AttrInfo_Init(&(platform.render.shader_attr));

    AttrInfo_AddLoader(&(platform.render.shader_attr), 0, GPU_FLOAT, 3); // v0 = position
    AttrInfo_AddLoader(&(platform.render.shader_attr), 1, GPU_FLOAT, 3); // v1 = texcoord

    C3D_BindProgram(&(platform.render.shader_program));
    C3D_SetAttrInfo(&(platform.render.shader_attr));

    update_screen_size();

    n3ds_vbuf_init();
}

static void n3ds_draw_exit(void)
{
    n3ds_vbuf_exit();

    linearFree(platform.render.tic_buf);
    C3D_TexDelete(&platform.render.tic_scaler_tex);
    C3D_TexDelete(&platform.render.tic_tex);

    C3D_RenderTargetDelete(platform.render.target_top);
    C3D_RenderTargetDelete(platform.render.target_bottom);
    C3D_RenderTargetDelete(platform.render.target_scaler);

    C3D_Fini();
}

void n3ds_draw_texture(C3D_Tex *tex, int x, int y, int tx, int ty, int width, int height, int twidth, int theight,
int tgtheight,
float cmul) {
    float txmin, tymin, txmax, tymax;
    int pos;
    txmin = (float) tx / tex->width;
    tymax = (float) ty / tex->height;
    txmax = (float) (tx+twidth) / tex->width;
    tymin = (float) (ty+theight) / tex->height;

    C3D_TexBind(0, tex);

    pos = n3ds_vbuf_append((float) x, (float) tgtheight - y - height, 0.0f, (float) txmin, (float) tymin, cmul);
    n3ds_vbuf_append((float) x + width, (float) tgtheight - y - height, 0.0f, (float) txmax, (float) tymin, cmul);
    n3ds_vbuf_append((float) x + width, (float) tgtheight - y, 0.0f, (float) txmax, (float) tymax, cmul);

    n3ds_vbuf_append((float) x, (float) tgtheight - y - height, 0.0f, (float) txmin, (float) tymin, cmul);
    n3ds_vbuf_append((float) x, (float) tgtheight - y, 0.0f, (float) txmin, (float) tymax, cmul);
    n3ds_vbuf_append((float) x + width, (float) tgtheight - y, 0.0f, (float) txmax, (float) tymax, cmul);

    C3D_DrawArrays(GPU_TRIANGLES, pos, 6);
}

#define FRAME_PITCH (TIC80_FULLWIDTH * sizeof(u32))

static void n3ds_copy_frame(void)
{
    u32 *in = platform.studio->tic->screen;

    if (platform.render.scaled >= 1) {
        // pad border 1 pixel lower to minimize glitches in "linear" scaling mode
        memcpy(
            in + (FRAME_PITCH * TIC80_FULLHEIGHT),
            in + (FRAME_PITCH * (TIC80_FULLHEIGHT - 1)),
            FRAME_PITCH
        );
        GSPGPU_FlushDataCache(in, (TIC80_FULLHEIGHT + 1) * FRAME_PITCH);
    } else {
        GSPGPU_FlushDataCache(in, TIC80_FULLHEIGHT * FRAME_PITCH);
    }

    C3D_SyncDisplayTransfer(
        in, GX_BUFFER_DIM(256, 256),
        platform.render.tic_tex.data, GX_BUFFER_DIM(256, 256),
        (GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
    );
}

static inline void n3ds_clear_border(C3D_RenderTarget *target)
{
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, platform.studio->tic->screen[0] >> 8, 0);
}

static void n3ds_draw_screen(int scale_multiplier)
{
    n3ds_draw_texture((scale_multiplier > 1) ? &platform.render.tic_scaler_tex : &platform.render.tic_tex,
        platform.screen_size.x, platform.screen_size.y,
        0, 0,
        platform.screen_size.width, platform.screen_size.height,
        TIC80_FULLWIDTH * scale_multiplier, TIC80_FULLHEIGHT * scale_multiplier,
        240, 1.0f);
}

static void n3ds_draw_frame(void)
{
    int scale_multiplier = (platform.render.scaled > 0) ? 3 : 1;

    if (scale_multiplier > 1) {
        // prescale
        C3D_FrameDrawOn(platform.render.target_scaler);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, platform.render.shader_proj_mtx_loc, &platform.render.proj_scaler);

        n3ds_draw_texture(&platform.render.tic_tex,
            0, 0,
            0, 0,
            TIC80_FULLWIDTH * scale_multiplier, TIC80_FULLHEIGHT * scale_multiplier,
            TIC80_FULLWIDTH, TIC80_FULLHEIGHT,
            512, 1.0f);
    }

#ifndef RENDER_CONSOLE_TOP
    // draw top screen
    C3D_FrameDrawOn(platform.render.target_top);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, platform.render.shader_proj_mtx_loc, &platform.render.proj_top);
    n3ds_clear_border(platform.render.target_top);

    if (!platform.render.on_bottom) {
        n3ds_draw_screen(scale_multiplier);
    }
#endif

    // draw bottom screen
    C3D_FrameDrawOn(platform.render.target_bottom);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, platform.render.shader_proj_mtx_loc, &platform.render.proj_bottom);

    if (platform.render.on_bottom) {
        n3ds_clear_border(platform.render.target_bottom);
        n3ds_draw_screen(scale_multiplier);
    } else if (platform.keyboard.render_dirty) {
        n3ds_keyboard_draw(&platform.keyboard);
        platform.keyboard.render_dirty = false;
    }
}

void n3ds_sound_init(int sample_rate)
{
    float mix[12];
    int buffer_size;

    platform.audio.samples = platform.studio->tic->samples.size / (TIC_STEREO_CHANNELS * sizeof(u16));
    buffer_size = platform.audio.samples * (TIC_STEREO_CHANNELS * sizeof(u16));

    platform.audio.buffer = linearAlloc(buffer_size * AUDIO_BLOCKS);
    platform.audio.buffer_size = buffer_size;
    memset(platform.audio.buffer, 0, buffer_size * AUDIO_BLOCKS);

    ndspInit();
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, sample_rate);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    for(int i = 0; i < AUDIO_BLOCKS; i++)
    {
        platform.audio.ndspBuf[i].data_vaddr = &platform.audio.buffer[buffer_size * i];
        platform.audio.ndspBuf[i].nsamples = platform.audio.samples;

        ndspChnWaveBufAdd(0, &platform.audio.ndspBuf[i]);
    }

    platform.audio.curr_block = 0;
}

void n3ds_sound_exit(void)
{
    ndspExit();
    linearFree(platform.audio.buffer);
}

static void audio_update(void) {
    platform.studio->sound();

    ndspWaveBuf *wave_buf = &platform.audio.ndspBuf[platform.audio.curr_block];

    if (wave_buf->status == NDSP_WBUF_DONE) {
        u16 *audio_ptr = wave_buf->data_pcm16;
        memcpy(audio_ptr, platform.studio->tic->samples.buffer, platform.audio.buffer_size);
        DSP_FlushDataCache(audio_ptr, platform.audio.buffer_size);

        ndspChnWaveBufAdd(0, wave_buf);
        platform.audio.curr_block = (platform.audio.curr_block + 1) % AUDIO_BLOCKS;
    }
}

static void touch_update(void) {
	u32 key_down = hidKeysDown();
	u32 key_held = hidKeysHeld();
    touchPosition touch;
    tic80_mouse *mouse = &platform.studio->tic->ram->input.mouse;

    if (key_held & KEY_TOUCH)
    {
        hidTouchRead(&touch);
        if (
            touch.px >= platform.screen_size.x
            && touch.py >= platform.screen_size.y
            && touch.px < (platform.screen_size.x + platform.screen_size.width)
            && touch.py < (platform.screen_size.y + platform.screen_size.height)
        ) {
            int tic80_x = (int) ((touch.px - platform.screen_size.x) * TIC80_FULLWIDTH / platform.screen_size.width);
            int tic80_y = (int) ((touch.py - platform.screen_size.y) * TIC80_FULLHEIGHT / platform.screen_size.height);
            if (
                tic80_x >= 0
                && tic80_y >= 0
                && tic80_x < (TIC80_FULLWIDTH)
                && tic80_y < (TIC80_FULLHEIGHT)
            ) {
                mouse->x = tic80_x;
                mouse->y = tic80_y;
                mouse->left = true;
            }
        }
    }
}

static void keyboard_update(void) {
    hidScanInput();

    platform.studio->tic->ram->input.mouse.btns = 0;
    if (!platform.render.on_bottom) {
        n3ds_keyboard_update(&platform.keyboard, platform.studio->tic);
    } else {
        touch_update();
    }
    n3ds_gamepad_update(&platform.keyboard, platform.studio->tic);

    u32 kup = hidKeysUp();
    if (kup & KEY_SELECT) {
        platform.render.scaled = (platform.render.scaled + 1) % N3DS_SCALED_MODE_COUNT;
        update_screen_size();
    }
    if (kup & (KEY_L | KEY_R)) {
        platform.render.on_bottom = !platform.render.on_bottom;
        platform.keyboard.render_dirty = true;
        update_screen_size();
    }
}

int main(int argc, char **argv) {
    char *backup_argv[] = { "/3ds/tic80/tic80.3dsx", 0 };
    int argc_used = argc;
    char **argv_used = argv;

    if (argc_used <= 0 || argv_used[0] == NULL || argv_used[0][0] == '\0') {
        mkdir("/3ds/tic80", S_IRWXU | S_IRWXG | S_IRWXO);
        argc_used = 1;
        argv_used = backup_argv;
    }

    osSetSpeedupEnable(1);

    gfxInitDefault();
    gfxSet3D(false);
#ifdef RENDER_CONSOLE_TOP
    consoleInit(GFX_TOP, NULL);
#endif
    cfguInit();
    romfsInit();

    memset(&platform, 0, sizeof(platform));
   
    n3ds_draw_init();
    n3ds_keyboard_init(&platform.keyboard);

    platform.studio = studioInit(argc_used, (const char**)argv_used, AUDIO_FREQ, "./");
    platform.studio->tic->screen_format = TIC80_PIXEL_COLOR_ABGR8888;

    n3ds_sound_init(AUDIO_FREQ);

    while (aptMainLoop() && !platform.studio->quit) {
        u32 start_frame = C3D_FrameCounter(0);

        keyboard_update();

        platform.studio->tick();
        audio_update();

        n3ds_copy_frame();

        bool sync = (C3D_FrameCounter(0) == start_frame);
        C3D_FrameBegin(sync ? C3D_FRAME_SYNCDRAW : 0);
        n3ds_vbuf_clear();

        n3ds_draw_frame();

        C3D_FrameEnd(0);
    }

    n3ds_sound_exit();
    
    platform.studio->close();

    n3ds_keyboard_free(&platform.keyboard);
    n3ds_draw_exit();

    romfsExit();
    cfguExit();
    gfxExit();

    return 0;
};
