#include "sokol.h"

static struct 
{
	sg_draw_state upscale_draw_state;
	sg_pass upscale_pass;
	sg_draw_state draw_state;
	int fb_width;
	int fb_height;
	int fb_aspect_scale_x;
	int fb_aspect_scale_y;
} sokol_gfx;

#if defined(SOKOL_GLCORE33)
static const char* gfx_vs_src =
    "#version 330\n"
    "in vec2 in_pos;\n"
    "in vec2 in_uv;\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(in_pos*2.0-1.0, 0.5, 1.0);\n"
    "  uv = in_uv;\n"
    "}\n";
static const char* gfx_fs_src =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = texture(tex, uv);\n"
    "}\n";
#elif defined(SOKOL_GLES2)
static const char* gfx_vs_src =
    "attribute vec2 in_pos;\n"
    "attribute vec2 in_uv;\n"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_Position = vec4(in_pos*2.0-1.0, 0.5, 1.0);\n"
    "  uv = in_uv;\n"
    "}\n";
static const char* gfx_fs_src =
    "precision mediump float;\n"
    "uniform sampler2D tex;"
    "varying vec2 uv;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(tex, uv);\n"
    "}\n";
#elif defined(SOKOL_METAL)
static const char* gfx_vs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct vs_in {\n"
    "  float2 pos [[attribute(0)]];\n"
    "  float2 uv [[attribute(1)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float2 uv;\n"
    "};\n"
    "vertex vs_out _main(vs_in in [[stage_in]]) {\n"
    "  vs_out out;\n"
    "  out.pos = float4(in.pos*2.0-1.0, 0.5, 1.0);\n"
    "  out.uv = in.uv;\n"
    "  return out;\n"
    "}\n";
static const char* gfx_fs_src =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct fs_in {\n"
    "  float2 uv;\n"
    "};\n"
    "fragment float4 _main(fs_in in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
    "  return tex.sample(smp, in.uv);\n"
    "}\n";
#elif defined(SOKOL_D3D11)
static const char* gfx_vs_src =
    "struct vs_in {\n"
    "  float2 pos: POS;\n"
    "  float2 uv:  UV;\n"
    "};\n"
    "struct vs_out {\n"
    "  float2 uv: TEXCOORD0;\n"
    "  float4 pos: SV_Position;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  vs_out outp;\n"
    "  outp.pos = float4(inp.pos*2.0-1.0, 0.5, 1.0);\n"
    "  outp.uv = inp.uv;\n"
    "  return outp;\n"
    "}\n";
static const char* gfx_fs_src =
    "Texture2D<float4> tex: register(t0);\n"
    "sampler smp: register(s0);\n"
    "float4 main(float2 uv: TEXCOORD0): SV_Target0 {\n"
    "  return tex.Sample(smp, uv);\n"
    "}\n";
#endif

void sokol_gfx_init(int w, int h, int sx, int sy) {
    sokol_gfx.fb_width = w;
    sokol_gfx.fb_height = h;
    sokol_gfx.fb_aspect_scale_x = sx;
    sokol_gfx.fb_aspect_scale_y = sy;
    sg_setup(&(sg_desc){
        .mtl_device = sapp_metal_get_device(),
        .mtl_renderpass_descriptor_cb = sapp_metal_get_renderpass_descriptor,
        .mtl_drawable_cb = sapp_metal_get_drawable,
        .d3d11_device = sapp_d3d11_get_device(),
        .d3d11_device_context = sapp_d3d11_get_device_context(),
        .d3d11_render_target_view_cb = sapp_d3d11_get_render_target_view,
        .d3d11_depth_stencil_view_cb = sapp_d3d11_get_depth_stencil_view
    });

    /* quad vertex buffers with and without flipped UVs */
    float verts[] = {
        0.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 1.0f
    };
    float verts_flipped[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f,  1.0f, 0.0f
    };
    sokol_gfx.upscale_draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(verts),
        .content = verts,
    });
    sokol_gfx.draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(verts),
        .content = sg_query_feature(SG_FEATURE_ORIGIN_TOP_LEFT) ? verts : verts_flipped
    });

    /* a shader to render a textured quad */
    sg_shader fsq_shd = sg_make_shader(&(sg_shader_desc){
        .fs.images = {
            [0] = { .name="tex", .type=SG_IMAGETYPE_2D },
        },
        .vs.source = gfx_vs_src,
        .fs.source = gfx_fs_src,
    });

    /* 2 pipeline-state-objects, one for upscaling, one for rendering */
    sg_pipeline_desc pip_desc = {
        .layout = {
            .attrs[0] = { .name="in_pos", .sem_name="POS", .format=SG_VERTEXFORMAT_FLOAT2 },
            .attrs[1] = { .name="in_uv", .sem_name="UV", .format=SG_VERTEXFORMAT_FLOAT2 }
        },
        .shader = fsq_shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    };
    sokol_gfx.draw_state.pipeline = sg_make_pipeline(&pip_desc);
    pip_desc.blend.depth_format = SG_PIXELFORMAT_NONE;
    sokol_gfx.upscale_draw_state.pipeline = sg_make_pipeline(&pip_desc);

    /* a texture with the emulator's raw pixel data */
    sokol_gfx.upscale_draw_state.fs_images[0] = sg_make_image(&(sg_image_desc){
        .width = sokol_gfx.fb_width,
        .height = sokol_gfx.fb_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });
    /* a 2x upscaled render-target-texture */
    sokol_gfx.draw_state.fs_images[0] = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 2*sokol_gfx.fb_width,
        .height = 2*sokol_gfx.fb_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });

    /* a render pass for the 2x upscaling */
    sokol_gfx.upscale_pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = sokol_gfx.draw_state.fs_images[0]
    });
}

static const sg_pass_action gfx_upscale_pass_action = {
    .colors[0] = { .action = SG_ACTION_DONTCARE }
};

static const sg_pass_action gfx_draw_pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.05f, 0.05f, 0.05f, 1.0f } }
};

static void apply_viewport(void) {
    const int canvas_width = sapp_width();
    const int canvas_height = sapp_height();
    const float canvas_aspect = (float)canvas_width / (float)canvas_height;
    const float fb_aspect = (float)(sokol_gfx.fb_width*sokol_gfx.fb_aspect_scale_x) / (float)(sokol_gfx.fb_height*sokol_gfx.fb_aspect_scale_y);
    const int frame_x = 5;
    const int frame_y = 5;
    int vp_x, vp_y, vp_w, vp_h;
    if (fb_aspect < canvas_aspect) {
        vp_y = frame_y;
        vp_h = canvas_height - 2 * frame_y;
        vp_w = (int) (canvas_height * fb_aspect) - 2 * frame_x;
        vp_x = (canvas_width - vp_w) / 2;
    }
    else {
        vp_x = frame_x;
        vp_w = canvas_width - 2 * frame_x;
        vp_h = (int) (canvas_width / fb_aspect) - 2 * frame_y;

        // align top
        // vp_y = frame_y;

        // align vcenter
        vp_y = (canvas_height - vp_h) / 2;
    }
    sg_apply_viewport(vp_x, vp_y, vp_w, vp_h, true);
}

void sokol_gfx_draw(const uint32_t* ptr) {

    /* copy emulator pixel data into upscaling source texture */
    sg_update_image(sokol_gfx.upscale_draw_state.fs_images[0], &(sg_image_content){
        .subimage[0][0] = { 
            .ptr = ptr,
            .size = sokol_gfx.fb_width*sokol_gfx.fb_height*sizeof ptr[0]
        }
    });

    /* upscale the original framebuffer 2x with nearest filtering */
    sg_begin_pass(sokol_gfx.upscale_pass, &gfx_upscale_pass_action);
    sg_apply_draw_state(&sokol_gfx.upscale_draw_state);
    sg_draw(0, 4, 1);
    sg_end_pass();

    /* draw the final pass with linear filtering */
    sg_begin_default_pass(&gfx_draw_pass_action, sapp_width(), sapp_height());
    apply_viewport();
    sg_apply_draw_state(&sokol_gfx.draw_state);
    sg_draw(0, 4, 1);
    sg_end_pass();
    sg_commit();
}
