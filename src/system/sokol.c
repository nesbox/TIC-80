#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include "system.h"
#include "system/sokol.h"

static struct
{
	Studio* studio;

	struct
	{
		saudio_desc desc;
		float* samples;
	} audio;

} platform;

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
	sokol_gfx_init(TIC80_FULLWIDTH, TIC80_FULLHEIGHT, 1, 1);

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

	sokol_gfx_draw(platform.studio->tic->screen);

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
