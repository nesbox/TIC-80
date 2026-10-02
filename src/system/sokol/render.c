#include "render.h"

#include "sokol.h"
#include "blit.h"
#include "controls.h"

// The studio's picture, presented as one quad. The whole renderer is this:
// a 256x144 texture, a unit quad and the rectangle it goes into.
static struct
{
    sg_image    framebuffer;
    sg_view     view;
    sg_sampler  nearest;
    sg_buffer   quad;
    sg_pipeline pipeline;
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

    if (controls->visible)
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
    sg_destroy_pipeline(render.pipeline);
    sg_destroy_buffer(render.quad);
    sg_destroy_sampler(render.nearest);
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
    render.params.uv_pos[0] = 0.0f;
    render.params.uv_pos[1] = 0.0f;
    render.params.uv_size[0] = (float)TIC80_FULLWIDTH;
    render.params.uv_size[1] = (float)TIC80_FULLHEIGHT;
    render.params.tex_size[0] = (float)TIC80_FULLWIDTH;
    render.params.tex_size[1] = (float)TIC80_FULLHEIGHT;

    sg_begin_pass(&(sg_pass){
        .action = { .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.1f, 0.11f, 0.17f, 1.0f } } },
        .swapchain = sglue_swapchain(),
    });

    sg_apply_pipeline(render.pipeline);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = render.quad,
        .views[VIEW_tex] = render.view,
        .samplers[SMP_smp] = render.nearest,
    });
    sg_apply_uniforms(UB_vs_params, &SG_RANGE(render.params));
    sg_draw(0, 4, 1);

    render_controls();

    sg_end_pass();
    sg_commit();
}
