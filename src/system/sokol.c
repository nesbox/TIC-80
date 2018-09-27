#include <sokol_app.h>
#include <sokol_gfx.h>
#include <sokol_time.h>
#include <sokol_audio.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include "system.h"

static struct
{
	Studio* studio;

	struct
	{
		saudio_desc desc;
		float* samples;
	} audio;

	struct {
	    sg_draw_state upscale_draw_state;
	    sg_pass upscale_pass;
	    sg_draw_state draw_state;
	    s32 fb_width;
	    s32 fb_height;
	    s32 fb_aspect_scale_x;
	    s32 fb_aspect_scale_y;
	} gfx;
	
} platform;

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

void gfx_init(s32 w, s32 h, s32 sx, s32 sy) {
    platform.gfx.fb_width = w;
    platform.gfx.fb_height = h;
    platform.gfx.fb_aspect_scale_x = sx;
    platform.gfx.fb_aspect_scale_y = sy;
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
    platform.gfx.upscale_draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(verts),
        .content = verts,
    });
    platform.gfx.draw_state.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
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
    platform.gfx.draw_state.pipeline = sg_make_pipeline(&pip_desc);
    pip_desc.blend.depth_format = SG_PIXELFORMAT_NONE;
    platform.gfx.upscale_draw_state.pipeline = sg_make_pipeline(&pip_desc);

    /* a texture with the emulator's raw pixel data */
    platform.gfx.upscale_draw_state.fs_images[0] = sg_make_image(&(sg_image_desc){
        .width = platform.gfx.fb_width,
        .height = platform.gfx.fb_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });
    /* a 2x upscaled render-target-texture */
    platform.gfx.draw_state.fs_images[0] = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 2*platform.gfx.fb_width,
        .height = 2*platform.gfx.fb_height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });

    /* a render pass for the 2x upscaling */
    platform.gfx.upscale_pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = platform.gfx.draw_state.fs_images[0]
    });
}

static const sg_pass_action gfx_upscale_pass_action = {
    .colors[0] = { .action = SG_ACTION_DONTCARE }
};

static const sg_pass_action gfx_draw_pass_action = {
    .colors[0] = { .action = SG_ACTION_CLEAR, .val = { 0.05f, 0.05f, 0.05f, 1.0f } }
};

static void apply_viewport(void) {
    const s32 canvas_width = sapp_width();
    const s32 canvas_height = sapp_height();
    const float canvas_aspect = (float)canvas_width / (float)canvas_height;
    const float fb_aspect = (float)(platform.gfx.fb_width*platform.gfx.fb_aspect_scale_x) / (float)(platform.gfx.fb_height*platform.gfx.fb_aspect_scale_y);
    const s32 frame_x = 5;
    const s32 frame_y = 5;
    s32 vp_x, vp_y, vp_w, vp_h;
    if (fb_aspect < canvas_aspect) {
        vp_y = frame_y;
        vp_h = canvas_height - 2 * frame_y;
        vp_w = (s32) (canvas_height * fb_aspect) - 2 * frame_x;
        vp_x = (canvas_width - vp_w) / 2;
    }
    else {
        vp_x = frame_x;
        vp_w = canvas_width - 2 * frame_x;
        vp_h = (s32) (canvas_width / fb_aspect) - 2 * frame_y;
        vp_y = frame_y;
    }
    sg_apply_viewport(vp_x, vp_y, vp_w, vp_h, true);
}

static void gfx_draw() {

    /* copy emulator pixel data into upscaling source texture */
    sg_update_image(platform.gfx.upscale_draw_state.fs_images[0], &(sg_image_content){
        .subimage[0][0] = { 
            .ptr = platform.studio->tic->screen,
            .size = platform.gfx.fb_width*platform.gfx.fb_height*sizeof platform.studio->tic->screen[0]
        }
    });

    /* upscale the original framebuffer 2x with nearest filtering */
    sg_begin_pass(platform.gfx.upscale_pass, &gfx_upscale_pass_action);
    sg_apply_draw_state(&platform.gfx.upscale_draw_state);
    sg_draw(0, 4, 1);
    sg_end_pass();

    /* draw the final pass with linear filtering */
    sg_begin_default_pass(&gfx_draw_pass_action, sapp_width(), sapp_height());
    apply_viewport();
    sg_apply_draw_state(&platform.gfx.draw_state);
    sg_draw(0, 4, 1);
    sg_end_pass();
    sg_commit();
}

static void setClipboardText(const char* text)
{
}

static bool hasClipboardText()
{
	return false;
}

static char* getClipboardText()
{
	return NULL;
}

static void freeClipboardText(const char* text)
{
}

static u64 getPerformanceCounter()
{
	return 0;
}

static u64 getPerformanceFrequency()
{
	return 1000;
}

static void* getUrlRequest(const char* url, s32* size)
{
	return NULL;
}

static void goFullscreen()
{
}

static void showMessageBox(const char* title, const char* message)
{
}

static void setWindowTitle(const char* title)
{
}

static void openSystemPath(const char* path)
{

}

static void preseed()
{
#if defined(__TIC_MACOSX__)
	srandom(time(NULL));
	random();
#else
	srand(time(NULL));
	rand();
#endif
}

static void pollEvent()
{

}

static void updateConfig()
{

}

static System systemInterface = 
{
	.setClipboardText = setClipboardText,
	.hasClipboardText = hasClipboardText,
	.getClipboardText = getClipboardText,
	.freeClipboardText = freeClipboardText,

	.getPerformanceCounter = getPerformanceCounter,
	.getPerformanceFrequency = getPerformanceFrequency,

	.getUrlRequest = getUrlRequest,

	.fileDialogLoad = file_dialog_load,
	.fileDialogSave = file_dialog_save,

	.goFullscreen = goFullscreen,
	.showMessageBox = showMessageBox,
	.setWindowTitle = setWindowTitle,

	.openSystemPath = openSystemPath,
	.preseed = preseed,
	.poll = pollEvent,
	.updateConfig = updateConfig,
};

static void app_init(void)
{
	gfx_init(TIC80_FULLWIDTH, TIC80_FULLHEIGHT, 1, 1);

    platform.audio.samples = calloc(sizeof platform.audio.samples[0], saudio_sample_rate() / TIC_FRAMERATE * TIC_STEREO_CHANNLES);
}

static void app_frame(void)
{
	tic_mem* tic = platform.studio->tic;

	if(platform.studio->quit)
	{
		return;
	}

	platform.studio->tick();

	gfx_draw();

	s32 count = tic->samples.size / sizeof tic->samples.buffer[0];
    for(s32 i = 0; i < count; i++)
        platform.audio.samples[i] = (float)tic->samples.buffer[i] / SHRT_MAX;

    saudio_push(platform.audio.samples, count / 2);
}

static void app_input(const sapp_event* event)
{
	
}

static void app_cleanup(void)
{
	platform.studio->close();
	free(platform.audio.samples);
}

sapp_desc sokol_main(s32 argc, char* argv[])
{
	memset(&platform, 0, sizeof platform);

    platform.audio.desc.num_channels = TIC_STEREO_CHANNLES;
    saudio_setup(&platform.audio.desc);

	platform.studio = studioInit(argc, argv, saudio_sample_rate(), ".", &systemInterface);

	const s32 Width = TIC80_FULLWIDTH * platform.studio->config()->uiScale;
	const s32 Height = TIC80_FULLHEIGHT * platform.studio->config()->uiScale;

	return(sapp_desc)
	{
		.init_cb = app_init,
		.frame_cb = app_frame,
		.event_cb = app_input,
		.cleanup_cb = app_cleanup,
		.width = Width,
		.height = Height,
		.window_title = TIC_TITLE,
		.ios_keyboard_resizes_canvas = true
	};
}
