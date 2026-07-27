// MIT License
// Copyright (c) 2026 Vadim Grigoruk @nesbox // grigoruk@gmail.com

#include "draw_cache.h"
#include "core.h"
#include "api.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>

#define RAM_A_SIZE          offsetof(tic_ram, input)
#define RAM_B_OFFSET        offsetof(tic_ram, flags)
#define RAM_B_SIZE          (offsetof(tic_ram, pcm) - offsetof(tic_ram, flags))

struct tic_draw_cache {
    bool is_recording;
    bool is_executing;
    bool has_invalidated;
    
    u8* curr_calls_buf;
    s32 curr_calls_pos;
    
    u8* prev_calls_buf;
    s32 prev_calls_size;
    s32 play_pos;
    
    // RAM mutation tracking
    u8* saved_ram_a;
    u8* saved_ram_b;
    u8* saved_vbank1;
    
    // Mouse tracking (to prevent cursor trails)
    tic80_mouse saved_mouse;
};

typedef enum {
    DRAW_CALL_CLS,
    DRAW_CALL_CLIP,
    DRAW_CALL_PIX,
    DRAW_CALL_RECT,
    DRAW_CALL_RECTB,
    DRAW_CALL_LINE,
    DRAW_CALL_CIRC,
    DRAW_CALL_CIRCB,
    DRAW_CALL_ELLI,
    DRAW_CALL_ELLIB,
    DRAW_CALL_PAINT,
    DRAW_CALL_TRI,
    DRAW_CALL_TRIB,
    DRAW_CALL_TTRI,
    DRAW_CALL_SPR,
    DRAW_CALL_MAP,
    DRAW_CALL_PRINT,
    DRAW_CALL_FONT
} DrawCallType;

typedef struct {
    u8 type;
    u8 color;
} DrawCallCls;

typedef struct {
    u8 type;
    s32 x, y, w, h;
} DrawCallClip;

typedef struct {
    u8 type;
    s32 x, y;
    u8 color;
    bool get;
} DrawCallPix;

typedef struct {
    u8 type;
    s32 x, y, w, h;
    u8 color;
} DrawCallRect;

typedef struct {
    u8 type;
    s32 x, y, w, h;
    u8 color;
} DrawCallRectb;

typedef struct {
    u8 type;
    float x0, y0, x1, y1;
    u8 color;
} DrawCallLine;

typedef struct {
    u8 type;
    s32 x, y, r;
    u8 color;
} DrawCallCirc;

typedef struct {
    u8 type;
    s32 x, y, r;
    u8 color;
} DrawCallCircb;

typedef struct {
    u8 type;
    s32 x, y, a, b;
    u8 color;
} DrawCallElli;

typedef struct {
    u8 type;
    s32 x, y, a, b;
    u8 color;
} DrawCallEllib;

typedef struct {
    u8 type;
    s32 x, y;
    u8 color;
    u8 bordercolor;
} DrawCallPaint;

typedef struct {
    u8 type;
    float x1, y1, x2, y2, x3, y3;
    u8 color;
} DrawCallTri;

typedef struct {
    u8 type;
    float x1, y1, x2, y2, x3, y3;
    u8 color;
} DrawCallTrib;

typedef struct {
    u8 type;
    float x1, y1, x2, y2, x3, y3;
    float u1, v1, u2, v2, u3, v3;
    s32 texsrc;
    u8 trans_count;
    u8 trans_colors[16];
    float z1, z2, z3;
    bool depth;
} DrawCallTtri;

typedef struct {
    u8 type;
    s32 index;
    s32 x, y, w, h;
    u8 trans_count;
    u8 trans_colors[16];
    s32 scale;
    s32 flip;
    s32 rotate;
} DrawCallSpr;

typedef struct {
    u8 type;
    s32 x, y, w, h, sx, sy;
    u8 count;
    u8 colors[16];
    s32 scale;
    u8 has_remap;
} DrawCallMap;

typedef struct {
    u8 type;
    s32 x, y;
    u8 color;
    bool fixed;
    s32 scale;
    bool alt;
    u32 len;
} DrawCallPrint;

typedef struct {
    u8 type;
    s32 x, y;
    u8 trans_count;
    u8 trans_colors[16];
    s32 w, h;
    bool fixed;
    s32 scale;
    bool alt;
    u32 len;
} DrawCallFont;

static s32 get_draw_call_size(u8 type, const u8* data)
{
    switch (type)
    {
        case DRAW_CALL_CLS: return sizeof(DrawCallCls);
        case DRAW_CALL_CLIP: return sizeof(DrawCallClip);
        case DRAW_CALL_PIX: return sizeof(DrawCallPix);
        case DRAW_CALL_RECT: return sizeof(DrawCallRect);
        case DRAW_CALL_RECTB: return sizeof(DrawCallRectb);
        case DRAW_CALL_LINE: return sizeof(DrawCallLine);
        case DRAW_CALL_CIRC: return sizeof(DrawCallCirc);
        case DRAW_CALL_CIRCB: return sizeof(DrawCallCircb);
        case DRAW_CALL_ELLI: return sizeof(DrawCallElli);
        case DRAW_CALL_ELLIB: return sizeof(DrawCallEllib);
        case DRAW_CALL_PAINT: return sizeof(DrawCallPaint);
        case DRAW_CALL_TRI: return sizeof(DrawCallTri);
        case DRAW_CALL_TRIB: return sizeof(DrawCallTrib);
        case DRAW_CALL_TTRI: return sizeof(DrawCallTtri);
        case DRAW_CALL_SPR: return sizeof(DrawCallSpr);
        case DRAW_CALL_MAP: {
            const DrawCallMap* c = (const DrawCallMap*)data;
            if (c->has_remap)
            {
                return sizeof(DrawCallMap) + c->w * c->h * sizeof(RemapResult);
            }
            return sizeof(DrawCallMap);
        }
        case DRAW_CALL_PRINT: {
            const DrawCallPrint* c = (const DrawCallPrint*)data;
            return sizeof(DrawCallPrint) + c->len + sizeof(s32);
        }
        case DRAW_CALL_FONT: {
            const DrawCallFont* c = (const DrawCallFont*)data;
            return sizeof(DrawCallFont) + c->len + sizeof(s32);
        }
        default: return 0;
    }
}

typedef struct {
    const RemapResult* remapped_tiles;
    s32 pos;
} PlaybackRemapContext;

static void playbackRemapCallback(void* data, s32 x, s32 y, RemapResult* result)
{
    PlaybackRemapContext* ctx = (PlaybackRemapContext*)data;
    *result = ctx->remapped_tiles[ctx->pos++];
}

static void execute_draw_call(tic_core* core, u8 type, const u8* data)
{
    switch (type)
    {
        case DRAW_CALL_CLS: {
            const DrawCallCls* c = (const DrawCallCls*)data;
            tic_api_cls(&core->memory, c->color);
            break;
        }
        case DRAW_CALL_CLIP: {
            const DrawCallClip* c = (const DrawCallClip*)data;
            tic_api_clip(&core->memory, c->x, c->y, c->w, c->h);
            break;
        }
        case DRAW_CALL_PIX: {
            const DrawCallPix* c = (const DrawCallPix*)data;
            tic_api_pix(&core->memory, c->x, c->y, c->color, c->get);
            break;
        }
        case DRAW_CALL_RECT: {
            const DrawCallRect* c = (const DrawCallRect*)data;
            tic_api_rect(&core->memory, c->x, c->y, c->w, c->h, c->color);
            break;
        }
        case DRAW_CALL_RECTB: {
            const DrawCallRectb* c = (const DrawCallRectb*)data;
            tic_api_rectb(&core->memory, c->x, c->y, c->w, c->h, c->color);
            break;
        }
        case DRAW_CALL_LINE: {
            const DrawCallLine* c = (const DrawCallLine*)data;
            tic_api_line(&core->memory, c->x0, c->y0, c->x1, c->y1, c->color);
            break;
        }
        case DRAW_CALL_CIRC: {
            const DrawCallCirc* c = (const DrawCallCirc*)data;
            tic_api_circ(&core->memory, c->x, c->y, c->r, c->color);
            break;
        }
        case DRAW_CALL_CIRCB: {
            const DrawCallCircb* c = (const DrawCallCircb*)data;
            tic_api_circb(&core->memory, c->x, c->y, c->r, c->color);
            break;
        }
        case DRAW_CALL_ELLI: {
            const DrawCallElli* c = (const DrawCallElli*)data;
            tic_api_elli(&core->memory, c->x, c->y, c->a, c->b, c->color);
            break;
        }
        case DRAW_CALL_ELLIB: {
            const DrawCallEllib* c = (const DrawCallEllib*)data;
            tic_api_ellib(&core->memory, c->x, c->y, c->a, c->b, c->color);
            break;
        }
        case DRAW_CALL_PAINT: {
            const DrawCallPaint* c = (const DrawCallPaint*)data;
            tic_api_paint(&core->memory, c->x, c->y, c->color, c->bordercolor);
            break;
        }
        case DRAW_CALL_TRI: {
            const DrawCallTri* c = (const DrawCallTri*)data;
            tic_api_tri(&core->memory, c->x1, c->y1, c->x2, c->y2, c->x3, c->y3, c->color);
            break;
        }
        case DRAW_CALL_TRIB: {
            const DrawCallTrib* c = (const DrawCallTrib*)data;
            tic_api_trib(&core->memory, c->x1, c->y1, c->x2, c->y2, c->x3, c->y3, c->color);
            break;
        }
        case DRAW_CALL_TTRI: {
            const DrawCallTtri* c = (const DrawCallTtri*)data;
            tic_api_ttri(&core->memory, c->x1, c->y1, c->x2, c->y2, c->x3, c->y3,
                         c->u1, c->v1, c->u2, c->v2, c->u3, c->v3,
                         (tic_texture_src)c->texsrc, (u8*)c->trans_colors, c->trans_count,
                         c->z1, c->z2, c->z3, c->depth);
            break;
        }
        case DRAW_CALL_SPR: {
            const DrawCallSpr* c = (const DrawCallSpr*)data;
            tic_api_spr(&core->memory, c->index, c->x, c->y, c->w, c->h,
                        (u8*)c->trans_colors, c->trans_count, c->scale, c->flip, c->rotate);
            break;
        }
        case DRAW_CALL_MAP: {
            const DrawCallMap* c = (const DrawCallMap*)data;
            if (c->has_remap)
            {
                s32 tiles_count = c->w * c->h;
                const RemapResult* remapped_tiles = (const RemapResult*)(data + sizeof(DrawCallMap));
                PlaybackRemapContext ctx = { remapped_tiles, 0 };
                tic_api_map(&core->memory, c->x, c->y, c->w, c->h, c->sx, c->sy,
                            (u8*)c->colors, c->count, c->scale, playbackRemapCallback, &ctx);
            }
            else
            {
                tic_api_map(&core->memory, c->x, c->y, c->w, c->h, c->sx, c->sy,
                            (u8*)c->colors, c->count, c->scale, NULL, NULL);
            }
            break;
        }
        case DRAW_CALL_PRINT: {
            const DrawCallPrint* c = (const DrawCallPrint*)data;
            char text_temp[512];
            u32 len = c->len < 511 ? c->len : 511;
            memcpy(text_temp, data + sizeof(DrawCallPrint), len);
            text_temp[len] = '\0';
            tic_api_print(&core->memory, text_temp, c->x, c->y, c->color, c->fixed, c->scale, c->alt);
            break;
        }
        case DRAW_CALL_FONT: {
            const DrawCallFont* c = (const DrawCallFont*)data;
            char text_temp[512];
            u32 len = c->len < 511 ? c->len : 511;
            memcpy(text_temp, data + sizeof(DrawCallFont), len);
            text_temp[len] = '\0';
            tic_api_font(&core->memory, text_temp, c->x, c->y, (u8*)c->trans_colors, c->trans_count,
                         c->w, c->h, c->fixed, c->scale, c->alt);
            break;
        }
    }
}

void tic_core_draw_cache_invalidate(tic_core* core)
{
    if (!core->draw_cache) return;
    if (core->draw_cache->is_recording && !core->draw_cache->has_invalidated)
    {
        core->draw_cache->has_invalidated = true;
        
        bool was_recording = core->draw_cache->is_recording;
        core->draw_cache->is_recording = false;
        core->draw_cache->is_executing = true;
        
        s32 pos = 0;
        while (pos < core->draw_cache->play_pos)
        {
            u8 type = core->draw_cache->prev_calls_buf[pos];
            s32 size = get_draw_call_size(type, core->draw_cache->prev_calls_buf + pos);
            execute_draw_call(core, type, core->draw_cache->prev_calls_buf + pos);
            pos += size;
        }
        
        core->draw_cache->is_executing = false;
        core->draw_cache->is_recording = was_recording;
    }
}

void tic_core_draw_cache_start(tic_core* core)
{
    if (!core->draw_cache) return;
    
    core->draw_cache->is_recording = true;
    core->draw_cache->curr_calls_pos = 0;
    core->draw_cache->play_pos = 0;
    core->draw_cache->has_invalidated = false;

    if (core->draw_cache->saved_ram_a && core->draw_cache->saved_ram_b && core->draw_cache->saved_vbank1)
    {
        bool match_a = memcmp(core->memory.ram, core->draw_cache->saved_ram_a, RAM_A_SIZE) == 0;
        bool match_b = memcmp(core->memory.ram->data + RAM_B_OFFSET, core->draw_cache->saved_ram_b, RAM_B_SIZE) == 0;
        
        tic_vram* v1 = core->state.vbank.id ? &core->memory.ram->vram : &core->state.vbank.mem;
        bool match_v1 = memcmp(v1, core->draw_cache->saved_vbank1, sizeof(tic_vram)) == 0;

        bool match_mouse = memcmp(&core->memory.ram->input.mouse, &core->draw_cache->saved_mouse, sizeof(tic80_mouse)) == 0;

        if (!match_a || !match_b || !match_v1 || !match_mouse)
        {
            tic_core_draw_cache_invalidate(core);
        }
    }
    else
    {
        tic_core_draw_cache_invalidate(core);
    }
}

void tic_core_draw_cache_end(tic_core* core)
{
    if (!core->draw_cache) return;
    
    core->draw_cache->is_recording = false;

    if (!core->draw_cache->saved_ram_a) core->draw_cache->saved_ram_a = malloc(RAM_A_SIZE);
    memcpy(core->draw_cache->saved_ram_a, core->memory.ram, RAM_A_SIZE);

    if (!core->draw_cache->saved_ram_b) core->draw_cache->saved_ram_b = malloc(RAM_B_SIZE);
    memcpy(core->draw_cache->saved_ram_b, core->memory.ram->data + RAM_B_OFFSET, RAM_B_SIZE);

    if (!core->draw_cache->saved_vbank1) core->draw_cache->saved_vbank1 = malloc(sizeof(tic_vram));
    tic_vram* v1 = core->state.vbank.id ? &core->memory.ram->vram : &core->state.vbank.mem;
    memcpy(core->draw_cache->saved_vbank1, v1, sizeof(tic_vram));

    memcpy(&core->draw_cache->saved_mouse, &core->memory.ram->input.mouse, sizeof(tic80_mouse));

    if (!core->draw_cache->has_invalidated && core->draw_cache->play_pos == core->draw_cache->prev_calls_size)
    {
        // Screen is identical
    }
    else
    {
        u8* temp = core->draw_cache->prev_calls_buf;
        core->draw_cache->prev_calls_buf = core->draw_cache->curr_calls_buf;
        core->draw_cache->curr_calls_buf = temp;
        
        core->draw_cache->prev_calls_size = core->draw_cache->curr_calls_pos;
    }
}

static bool handle_draw_call(tic_core* core, const void* data, s32 size)
{
    if (!core->draw_cache) return true;
    
    if (core->draw_cache->is_executing)
    {
        return true;
    }
    
    if (!core->draw_cache->is_recording)
    {
        return true;
    }
    
    if (core->draw_cache->curr_calls_pos + size > DRAW_CACHE_MAX_SIZE)
    {
        tic_core_draw_cache_invalidate(core);
        core->draw_cache->is_recording = false;
        return true;
    }
    
    memcpy(core->draw_cache->curr_calls_buf + core->draw_cache->curr_calls_pos, data, size);
    core->draw_cache->curr_calls_pos += size;
    
    if (!core->draw_cache->has_invalidated)
    {
        if (core->draw_cache->play_pos + size <= core->draw_cache->prev_calls_size)
        {
            if (memcmp(core->draw_cache->prev_calls_buf + core->draw_cache->play_pos, data, size) == 0)
            {
                core->draw_cache->play_pos += size;
                return false;
            }
            else
            {
                tic_core_draw_cache_invalidate(core);
            }
        }
        else
        {
            tic_core_draw_cache_invalidate(core);
        }
    }
    
    return true;
}

// Caching Wrappers for drawing APIs

static void cache_api_cls(tic_mem* tic, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallCls call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_CLS;
    call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_cls(tic, color);
    }
}

static void cache_api_clip(tic_mem* tic, s32 x, s32 y, s32 w, s32 h)
{
    tic_core* core = (tic_core*)tic;
    DrawCallClip call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_CLIP;
    call.x = x; call.y = y; call.w = w; call.h = h;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_clip(tic, x, y, w, h);
    }
}

static u8 cache_api_pix(tic_mem* tic, s32 x, s32 y, u8 color, bool get)
{
    tic_core* core = (tic_core*)tic;
    DrawCallPix call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_PIX;
    call.x = x; call.y = y; call.color = color; call.get = get;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        return tic_api_pix(tic, x, y, color, get);
    }
    return 0;
}

static void cache_api_rect(tic_mem* tic, s32 x, s32 y, s32 w, s32 h, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallRect call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_RECT;
    call.x = x; call.y = y; call.w = w; call.h = h; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_rect(tic, x, y, w, h, color);
    }
}

static void cache_api_rectb(tic_mem* tic, s32 x, s32 y, s32 w, s32 h, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallRectb call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_RECTB;
    call.x = x; call.y = y; call.w = w; call.h = h; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_rectb(tic, x, y, w, h, color);
    }
}

static void cache_api_line(tic_mem* tic, float x0, float y0, float x1, float y1, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallLine call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_LINE;
    call.x0 = x0; call.y0 = y0; call.x1 = x1; call.y1 = y1; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_line(tic, x0, y0, x1, y1, color);
    }
}

static void cache_api_circ(tic_mem* tic, s32 x, s32 y, s32 r, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallCirc call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_CIRC;
    call.x = x; call.y = y; call.r = r; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_circ(tic, x, y, r, color);
    }
}

static void cache_api_circb(tic_mem* tic, s32 x, s32 y, s32 r, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallCircb call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_CIRCB;
    call.x = x; call.y = y; call.r = r; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_circb(tic, x, y, r, color);
    }
}

static void cache_api_elli(tic_mem* tic, s32 x, s32 y, s32 a, s32 b, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallElli call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_ELLI;
    call.x = x; call.y = y; call.a = a; call.b = b; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_elli(tic, x, y, a, b, color);
    }
}

static void cache_api_ellib(tic_mem* tic, s32 x, s32 y, s32 a, s32 b, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallEllib call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_ELLIB;
    call.x = x; call.y = y; call.a = a; call.b = b; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_ellib(tic, x, y, a, b, color);
    }
}

static void cache_api_paint(tic_mem* tic, s32 x, s32 y, u8 color, u8 bordercolor)
{
    tic_core* core = (tic_core*)tic;
    DrawCallPaint call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_PAINT;
    call.x = x; call.y = y; call.color = color; call.bordercolor = bordercolor;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_paint(tic, x, y, color, bordercolor);
    }
}

static void cache_api_tri(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallTri call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_TRI;
    call.x1 = x1; call.y1 = y1; call.x2 = x2; call.y2 = y2; call.x3 = x3; call.y3 = y3; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_tri(tic, x1, y1, x2, y2, x3, y3, color);
    }
}

static void cache_api_trib(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3, u8 color)
{
    tic_core* core = (tic_core*)tic;
    DrawCallTrib call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_TRIB;
    call.x1 = x1; call.y1 = y1; call.x2 = x2; call.y2 = y2; call.x3 = x3; call.y3 = y3; call.color = color;
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_trib(tic, x1, y1, x2, y2, x3, y3, color);
    }
}

static void cache_api_ttri(tic_mem* tic, float x1, float y1, float x2, float y2, float x3, float y3,
                           float u1, float v1, float u2, float v2, float u3, float v3,
                           tic_texture_src texsrc, u8* colors, s32 count,
                           float z1, float z2, float z3, bool depth)
{
    tic_core* core = (tic_core*)tic;
    DrawCallTtri call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_TTRI;
    call.x1 = x1; call.y1 = y1; call.x2 = x2; call.y2 = y2; call.x3 = x3; call.y3 = y3;
    call.u1 = u1; call.v1 = v1; call.u2 = u2; call.v2 = v2; call.u3 = u3; call.v3 = v3;
    call.texsrc = texsrc;
    call.trans_count = count;
    call.z1 = z1; call.z2 = z2; call.z3 = z3;
    call.depth = depth;
    if (colors && count > 0)
    {
        memcpy(call.trans_colors, colors, count < 16 ? count : 16);
    }
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_ttri(tic, x1, y1, x2, y2, x3, y3, u1, v1, u2, v2, u3, v3, texsrc, colors, count, z1, z2, z3, depth);
    }
}

static void cache_api_spr(tic_mem* tic, s32 index, s32 x, s32 y, s32 w, s32 h, u8* trans_colors, u8 trans_count, s32 scale, tic_flip flip, tic_rotate rotate)
{
    tic_core* core = (tic_core*)tic;
    DrawCallSpr call;
    memset(&call, 0, sizeof(call));
    call.type = DRAW_CALL_SPR;
    call.index = index;
    call.x = x; call.y = y; call.w = w; call.h = h;
    call.trans_count = trans_count;
    call.scale = scale;
    call.flip = flip;
    call.rotate = rotate;
    if (trans_colors && trans_count > 0)
    {
        memcpy(call.trans_colors, trans_colors, trans_count < 16 ? trans_count : 16);
    }
    if (handle_draw_call(core, &call, sizeof(call)))
    {
        tic_api_spr(tic, index, x, y, w, h, trans_colors, trans_count, scale, flip, rotate);
    }
}

typedef struct {
    RemapFunc original_remap;
    void* original_data;
    RemapResult* record_buf;
    s32 record_pos;
} RemapInterceptorContext;

static void remapInterceptor(void* data, s32 x, s32 y, RemapResult* result)
{
    RemapInterceptorContext* ctx = (RemapInterceptorContext*)data;
    
    RemapResult user_result;
    memset(&user_result, 0, sizeof(RemapResult));
    user_result.index = result->index;
    user_result.flip = result->flip;
    user_result.rotate = result->rotate;
    
    ctx->original_remap(ctx->original_data, x, y, &user_result);
    
    result->index = user_result.index;
    result->flip = user_result.flip;
    result->rotate = user_result.rotate;
    
    ctx->record_buf[ctx->record_pos] = user_result;
    ctx->record_pos++;
}

static void cache_api_map(tic_mem* tic, s32 x, s32 y, s32 width, s32 height, s32 sx, s32 sy, u8* trans_colors, u8 trans_count, s32 scale, RemapFunc remap, void* data)
{
    tic_core* core = (tic_core*)tic;
    s32 tiles_count = width * height;
    
    if (remap != NULL)
    {
        if (core->draw_cache->is_executing)
        {
            tic_api_map(tic, x, y, width, height, sx, sy, trans_colors, trans_count, scale, remap, data);
            return;
        }
        
        RemapResult* remapped_tiles = malloc(tiles_count * sizeof(RemapResult));
        memset(remapped_tiles, 0, tiles_count * sizeof(RemapResult));
        
        RemapInterceptorContext ctx = { remap, data, remapped_tiles, 0 };
        
        tic_api_map(tic, x, y, width, height, sx, sy, trans_colors, trans_count, scale, remapInterceptor, &ctx);
        
        s32 total_size = sizeof(DrawCallMap) + tiles_count * sizeof(RemapResult);
        
        DrawCallMap header;
        memset(&header, 0, sizeof(header));
        header.type = DRAW_CALL_MAP;
        header.x = x; header.y = y; header.w = width; header.h = height; header.sx = sx; header.sy = sy;
        header.count = trans_count;
        header.scale = scale;
        header.has_remap = true;
        if (trans_colors && trans_count > 0)
        {
            memcpy(header.colors, trans_colors, trans_count < 16 ? trans_count : 16);
        }
        
        if (core->draw_cache->is_recording)
        {
            if (core->draw_cache->curr_calls_pos + total_size > DRAW_CACHE_MAX_SIZE)
            {
                tic_core_draw_cache_invalidate(core);
                core->draw_cache->is_recording = false;
                free(remapped_tiles);
                return;
            }
            
            u8* dest = core->draw_cache->curr_calls_buf + core->draw_cache->curr_calls_pos;
            memcpy(dest, &header, sizeof(DrawCallMap));
            memcpy(dest + sizeof(DrawCallMap), remapped_tiles, tiles_count * sizeof(RemapResult));
            
            if (!core->draw_cache->has_invalidated)
            {
                if (core->draw_cache->play_pos + total_size <= core->draw_cache->prev_calls_size &&
                    memcmp(core->draw_cache->prev_calls_buf + core->draw_cache->play_pos, dest, total_size) == 0)
                {
                    core->draw_cache->curr_calls_pos += total_size;
                    core->draw_cache->play_pos += total_size;
                }
                else
                {
                    tic_core_draw_cache_invalidate(core);
                }
            }
            else
            {
                core->draw_cache->curr_calls_pos += total_size;
            }
        }
        
        free(remapped_tiles);
    }
    else
    {
        DrawCallMap call;
        memset(&call, 0, sizeof(call));
        call.type = DRAW_CALL_MAP;
        call.x = x; call.y = y; call.w = width; call.h = height; call.sx = sx; call.sy = sy;
        call.count = trans_count;
        call.scale = scale;
        call.has_remap = false;
        if (trans_colors && trans_count > 0)
        {
            memcpy(call.colors, trans_colors, trans_count < 16 ? trans_count : 16);
        }
        if (handle_draw_call(core, &call, sizeof(call)))
        {
            tic_api_map(tic, x, y, width, height, sx, sy, trans_colors, trans_count, scale, remap, data);
        }
    }
}

static s32 cache_api_print(tic_mem* tic, const char* text, s32 x, s32 y, u8 color, bool fixed, s32 scale, bool alt)
{
    tic_core* core = (tic_core*)tic;
    if (core->draw_cache->is_executing)
    {
        return tic_api_print(tic, text, x, y, color, fixed, scale, alt);
    }
    if (!core->draw_cache->is_recording)
    {
        return tic_api_print(tic, text, x, y, color, fixed, scale, alt);
    }
    
    u32 len = strlen(text);
    DrawCallPrint header;
    memset(&header, 0, sizeof(header));
    header.type = DRAW_CALL_PRINT;
    header.x = x; header.y = y; header.color = color; header.fixed = fixed; header.scale = scale; header.alt = alt; header.len = len;
    s32 total_compare_size = sizeof(DrawCallPrint) + len;
    s32 total_record_size = total_compare_size + sizeof(s32);
    
    if (core->draw_cache->curr_calls_pos + total_record_size > DRAW_CACHE_MAX_SIZE)
    {
        tic_core_draw_cache_invalidate(core);
        core->draw_cache->is_recording = false;
        return tic_api_print(tic, text, x, y, color, fixed, scale, alt);
    }
    
    u8* write_ptr = core->draw_cache->curr_calls_buf + core->draw_cache->curr_calls_pos;
    memcpy(write_ptr, &header, sizeof(DrawCallPrint));
    memcpy(write_ptr + sizeof(DrawCallPrint), text, len);
    
    if (!core->draw_cache->has_invalidated)
    {
        if (core->draw_cache->play_pos + total_record_size <= core->draw_cache->prev_calls_size &&
            memcmp(core->draw_cache->prev_calls_buf + core->draw_cache->play_pos, write_ptr, total_compare_size) == 0)
        {
            s32 cached_width;
            memcpy(&cached_width, core->draw_cache->prev_calls_buf + core->draw_cache->play_pos + total_compare_size, sizeof(s32));
            memcpy(write_ptr + total_compare_size, &cached_width, sizeof(s32));
            core->draw_cache->curr_calls_pos += total_record_size;
            core->draw_cache->play_pos += total_record_size;
            return cached_width;
        }
        else
        {
            tic_core_draw_cache_invalidate(core);
        }
    }
    
    s32 result = tic_api_print(tic, text, x, y, color, fixed, scale, alt);
    memcpy(write_ptr + total_compare_size, &result, sizeof(s32));
    core->draw_cache->curr_calls_pos += total_record_size;
    return result;
}

static s32 cache_api_font(tic_mem* tic, const char* text, s32 x, s32 y, u8* trans_colors, u8 trans_count, s32 w, s32 h, bool fixed, s32 scale, bool alt)
{
    tic_core* core = (tic_core*)tic;
    if (core->draw_cache->is_executing)
    {
        return tic_api_font(tic, text, x, y, trans_colors, trans_count, w, h, fixed, scale, alt);
    }
    if (!core->draw_cache->is_recording)
    {
        return tic_api_font(tic, text, x, y, trans_colors, trans_count, w, h, fixed, scale, alt);
    }
    
    u32 len = strlen(text);
    DrawCallFont header;
    memset(&header, 0, sizeof(header));
    header.type = DRAW_CALL_FONT;
    header.x = x; header.y = y; header.trans_count = trans_count; header.w = w; header.h = h; header.fixed = fixed; header.scale = scale; header.alt = alt; header.len = len;
    if (trans_colors && trans_count > 0)
    {
        memcpy(header.trans_colors, trans_colors, trans_count < 16 ? trans_count : 16);
    }
    s32 total_compare_size = sizeof(DrawCallFont) + len;
    s32 total_record_size = total_compare_size + sizeof(s32);
    
    if (core->draw_cache->curr_calls_pos + total_record_size > DRAW_CACHE_MAX_SIZE)
    {
        tic_core_draw_cache_invalidate(core);
        core->draw_cache->is_recording = false;
        return tic_api_font(tic, text, x, y, trans_colors, trans_count, w, h, fixed, scale, alt);
    }
    
    u8* write_ptr = core->draw_cache->curr_calls_buf + core->draw_cache->curr_calls_pos;
    memcpy(write_ptr, &header, sizeof(DrawCallFont));
    memcpy(write_ptr + sizeof(DrawCallFont), text, len);
    
    if (!core->draw_cache->has_invalidated)
    {
        if (core->draw_cache->play_pos + total_record_size <= core->draw_cache->prev_calls_size &&
            memcmp(core->draw_cache->prev_calls_buf + core->draw_cache->play_pos, write_ptr, total_compare_size) == 0)
        {
            s32 cached_width;
            memcpy(&cached_width, core->draw_cache->prev_calls_buf + core->draw_cache->play_pos + total_compare_size, sizeof(s32));
            memcpy(write_ptr + total_compare_size, &cached_width, sizeof(s32));
            core->draw_cache->curr_calls_pos += total_record_size;
            core->draw_cache->play_pos += total_record_size;
            return cached_width;
        }
        else
        {
            tic_core_draw_cache_invalidate(core);
        }
    }
    
    s32 result = tic_api_font(tic, text, x, y, trans_colors, trans_count, w, h, fixed, scale, alt);
    memcpy(write_ptr + total_compare_size, &result, sizeof(s32));
    core->draw_cache->curr_calls_pos += total_record_size;
    return result;
}

void tic_core_draw_cache_init(tic_core* core)
{
    core->draw_cache = malloc(sizeof(struct tic_draw_cache));
    memset(core->draw_cache, 0, sizeof(struct tic_draw_cache));
    core->draw_cache->curr_calls_buf = (u8*)malloc(DRAW_CACHE_MAX_SIZE);
    core->draw_cache->prev_calls_buf = (u8*)malloc(DRAW_CACHE_MAX_SIZE);
    core->draw_cache->prev_calls_size = 0;
    core->draw_cache->saved_ram_a = NULL;
    core->draw_cache->saved_ram_b = NULL;
    core->draw_cache->saved_vbank1 = NULL;
}

void tic_core_draw_cache_free(tic_core* core)
{
    if (core->draw_cache)
    {
        if (core->draw_cache->curr_calls_buf) free(core->draw_cache->curr_calls_buf);
        if (core->draw_cache->prev_calls_buf) free(core->draw_cache->prev_calls_buf);
        if (core->draw_cache->saved_ram_a) free(core->draw_cache->saved_ram_a);
        if (core->draw_cache->saved_ram_b) free(core->draw_cache->saved_ram_b);
        if (core->draw_cache->saved_vbank1) free(core->draw_cache->saved_vbank1);
        free(core->draw_cache);
        core->draw_cache = NULL;
    }
}

void tic_core_draw_cache_hook_api(tic_core* core)
{
    core->api.cls = cache_api_cls;
    core->api.clip = cache_api_clip;
    core->api.pix = cache_api_pix;
    core->api.rect = cache_api_rect;
    core->api.rectb = cache_api_rectb;
    core->api.line = cache_api_line;
    core->api.circ = cache_api_circ;
    core->api.circb = cache_api_circb;
    core->api.elli = cache_api_elli;
    core->api.ellib = cache_api_ellib;
    core->api.tri = cache_api_tri;
    core->api.trib = cache_api_trib;
    core->api.ttri = cache_api_ttri;
    core->api.spr = cache_api_spr;
    core->api.map = cache_api_map;
    core->api.print = cache_api_print;
    core->api.font = cache_api_font;
    core->api.paint = cache_api_paint;
}

bool tic_core_draw_cache_has_invalidated(tic_core* core)
{
    return !core->draw_cache || core->draw_cache->has_invalidated;
}
