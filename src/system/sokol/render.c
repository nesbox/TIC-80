#include "render.h"

#include "sokol.h"
#include "blit.h"
#include "crt.h"
#include "controls.h"

#include <stdio.h>

// The CRT effect is drawn once, into a texture of its own at this many
// machine pixels to one, and the result is stretched into the picture's place.
// The mask and the scanlines then belong to the picture rather than to the
// window's pixels, and the effect costs the same whatever the window is.
#define CRT_SCALE 5

// The studio's picture, presented as one quad. The whole renderer is this:
// a 256x144 texture, a unit quad and the rectangle it goes into.
static struct
{
    sg_image    framebuffer;
    sg_view     view;
    sg_sampler  nearest;
    sg_buffer   quad;
    sg_pipeline pipeline;
    sg_pipeline crt;

    // The effect's own texture, a render target when it is drawn into and a
    // picture when it is drawn out. OpenGL keeps a texture's origin at the
    // bottom left and Metal and D3D at the top left, so what was drawn into
    // it is read from the row the backend means.
    sg_image    crt_image;
    sg_view     crt_target;
    sg_view     crt_texture;
    sg_sampler  linear;
    bool        crt_flipped;

    vs_params_t params;
} render;

// The largest rectangle of the framebuffer's shape that fits a box; with
// integer scaling the framebuffer keeps whole pixels.
static void fit_into(float bx, float by, float bw, float bh, bool integer,
    float* x, float* y, float* w, float* h)
{
    const int iw = (int)bw;
    const int ih = (int)bh;
    int dw, dh;

    if (iw * TIC80_FULLHEIGHT < ih * TIC80_FULLWIDTH)
    {
        dw = iw - (integer ? iw % TIC80_FULLWIDTH : 0);
        dh = TIC80_FULLHEIGHT * dw / TIC80_FULLWIDTH;
    }
    else
    {
        dh = ih - (integer ? ih % TIC80_FULLHEIGHT : 0);
        dw = TIC80_FULLWIDTH * dh / TIC80_FULLHEIGHT;
    }

    *x = bx + (bw - dw) * 0.5f;
    *y = by + (bh - dh) * 0.5f;
    *w = (float)dw;
    *h = (float)dh;
}

// Where the picture goes: the box the controls laid out while they are out,
// and the whole window when they are not.
void render_player_rect(const Studio* studio, float* x, float* y, float* w, float* h)
{
    const bool integer = studio_config(studio)->options.integerScale;
    const ControlsState* controls = controls_state();

    // The controls decide where the picture goes — they make room for
    // themselves and the place is eased — and the window is what is left of
    // the answer before they have run once.
    if (controls->known)
        fit_into(controls->x, controls->y, controls->w, controls->h, integer, x, y, w, h);
    else
        fit_into(0.0f, 0.0f, (float)sapp_width(), (float)sapp_height(), integer, x, y, w, h);
}

void render_init(void)
{
    render.framebuffer = sg_make_image(&(sg_image_desc){
        .width = TIC80_FULLWIDTH,
        .height = TIC80_FULLHEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.dynamic_update = true,
        .label = "tic80-framebuffer",
    });

    render.view = sg_make_view(&(sg_view_desc){
        .texture.image = render.framebuffer,
        .label = "tic80-framebuffer-view",
    });

    render.nearest = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "tic80-nearest",
    });

    // The effect's texture and the two ways of looking at it: an attachment to
    // draw it into, a picture to stretch out of it.
    render.crt_image = sg_make_image(&(sg_image_desc){
        .width = TIC80_FULLWIDTH * CRT_SCALE,
        .height = TIC80_FULLHEIGHT * CRT_SCALE,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.color_attachment = true,
        .label = "tic80-crt",
    });

    render.crt_target = sg_make_view(&(sg_view_desc){
        .color_attachment.image = render.crt_image,
        .label = "tic80-crt-target",
    });

    render.crt_texture = sg_make_view(&(sg_view_desc){
        .texture.image = render.crt_image,
        .label = "tic80-crt-texture",
    });

    render.crt_flipped = !sg_query_features().origin_top_left;

    // The effect is smooth by its nature: the stretch to the window is too.
    render.linear = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = "tic80-linear",
    });

    const float quad[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
    };

    render.quad = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(quad),
        .label = "tic80-quad",
    });

    render.pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(blit_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_blit_pos].format = SG_VERTEXFORMAT_FLOAT2,
            [ATTR_blit_uv].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .colors[0].blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .src_factor_alpha = SG_BLENDFACTOR_ONE,
            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "tic80-blit",
    });

    render.crt = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(crt_shader_desc(sg_query_backend())),
        .layout.attrs = {
            [ATTR_crt_pos].format = SG_VERTEXFORMAT_FLOAT2,
            [ATTR_crt_uv].format = SG_VERTEXFORMAT_FLOAT2,
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .label = "tic80-crt",
    });

    render.params.resolution[0] = (float)sapp_width();
    render.params.resolution[1] = (float)sapp_height();
}

// The controls are sprites of the same texture, drawn where the layout put them.
static void render_controls(void)
{
    const ControlsState* controls = controls_state();

    if (!controls->visible || !controls->quadCount)
        return;

    sg_apply_pipeline(render.pipeline);

    for (s32 i = 0; i < controls->quadCount; i++)
    {
        const ControlsQuad* quad = &controls->quads[i];

        // The gamepad sheet and the keyboard are two textures; a frame can
        // carry rectangles cut from both.
        if (i == 0 || controls->quads[i - 1].texture != quad->texture)
            sg_apply_bindings(&(sg_bindings){
                .vertex_buffers[0] = render.quad,
                .views[VIEW_tex] = controls_view((ControlsTexture)quad->texture),
                .samplers[SMP_smp] = render.nearest,
            });

        const vs_params_t params = {
            .rect_pos = { quad->x, quad->y },
            .rect_size = { quad->w, quad->h },
            .resolution = { render.params.resolution[0], render.params.resolution[1] },
            .uv_pos = { quad->u0, quad->v0 },
            .uv_size = { quad->u1 - quad->u0, quad->v1 - quad->v0 },
            .tex_size = { (float)TIC80_FULLWIDTH, (float)TIC80_FULLHEIGHT },
        };

        render.params = params;
        sg_apply_uniforms(UB_vs_params, &SG_RANGE(render.params));
        sg_draw(0, 4, 1);
    }
}

void render_shutdown(void)
{
    sg_destroy_pipeline(render.crt);
    sg_destroy_pipeline(render.pipeline);
    sg_destroy_buffer(render.quad);
    sg_destroy_sampler(render.linear);
    sg_destroy_sampler(render.nearest);
    sg_destroy_view(render.crt_texture);
    sg_destroy_view(render.crt_target);
    sg_destroy_image(render.crt_image);
    sg_destroy_view(render.view);
    sg_destroy_image(render.framebuffer);
}

void render_frame(const Studio* studio, const u32* framebuffer, bool dirty)
{
    if (dirty)
    {
        sg_update_image(render.framebuffer, &(sg_image_data){
            .mip_levels[0] = { .ptr = framebuffer, .size = TIC80_FULLWIDTH * TIC80_FULLHEIGHT * sizeof(u32) },
        });
    }

    float x, y, w, h;
    render_player_rect(studio, &x, &y, &w, &h);

    render.params.rect_pos[0] = x;
    render.params.rect_pos[1] = y;
    render.params.rect_size[0] = w;
    render.params.rect_size[1] = h;
    render.params.resolution[0] = (float)sapp_width();
    render.params.resolution[1] = (float)sapp_height();
    const bool crt = studio_config(studio)->options.crt;
    const float crt_w = (float)(TIC80_FULLWIDTH * CRT_SCALE);
    const float crt_h = (float)(TIC80_FULLHEIGHT * CRT_SCALE);

    // What is drawn is the effect's texture when there is an effect and the
    // machine's own picture when there is not.
    render.params.uv_pos[0] = 0.0f;
    render.params.uv_pos[1] = crt && render.crt_flipped ? crt_h : 0.0f;
    render.params.uv_size[0] = crt ? crt_w : (float)TIC80_FULLWIDTH;
    render.params.uv_size[1] = crt ? (render.crt_flipped ? -crt_h : crt_h) : (float)TIC80_FULLHEIGHT;
    render.params.tex_size[0] = crt ? crt_w : (float)TIC80_FULLWIDTH;
    render.params.tex_size[1] = crt ? crt_h : (float)TIC80_FULLHEIGHT;

    if (crt)
    {
        // The effect at its own size, the picture filling it whole.
        const crt_params_t params = {
            .rect_pos = { 0.0f, 0.0f },
            .rect_size = { crt_w, crt_h },
            .resolution = { crt_w, crt_h },
        };

        sg_begin_pass(&(sg_pass){
            .action = { .colors[0] = { .load_action = SG_LOADACTION_CLEAR } },
            .attachments = { .colors = { render.crt_target } },
        });

        sg_apply_pipeline(render.crt);
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = render.quad,
            .views[VIEW_tex] = render.view,
            .samplers[SMP_smp] = render.nearest,
        });
        sg_apply_uniforms(UB_crt_params, &SG_RANGE(params));
        sg_draw(0, 4, 1);
        sg_end_pass();
    }

    sg_begin_pass(&(sg_pass){
        .action = { .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.1f, 0.11f, 0.17f, 1.0f } } },
        .swapchain = sglue_swapchain(),
    });

    sg_apply_pipeline(render.pipeline);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = render.quad,
        .views[VIEW_tex] = crt ? render.crt_texture : render.view,
        .samplers[SMP_smp] = crt ? render.linear : render.nearest,
    });
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(render.params));

    sg_draw(0, 4, 1);

    render_controls();

    sg_end_pass();
    sg_commit();
}
