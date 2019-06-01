#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <tic.h>

#include "libretro.h"

static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
int64_t timeCounter = 0;

#define TIC_FRAMERATE 60

static struct
{
	bool quit;
} state =
{
	.quit = false,
};

tic80* tic;

static void onExit()
{
	state.quit = true;
}

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
	(void)level;
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

void retro_init(void)
{
}

void retro_deinit(void)
{
}

unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
	log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);
}

void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name     = "TIC-80";
	info->library_version  = "0.0.1";
	info->need_fullpath    = false;
	info->valid_extensions = "tic";
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	info->timing = (struct retro_system_timing) {
		.fps = TIC_FRAMERATE,
		.sample_rate = 0.0,
	};

	info->geometry = (struct retro_game_geometry) {
		.base_width   = TIC80_FULLWIDTH,
		.base_height  = TIC80_FULLHEIGHT,
		.max_width    = TIC80_FULLWIDTH,
		.max_height   = TIC80_FULLHEIGHT,
		.aspect_ratio = (float)TIC80_WIDTH / (float)TIC80_HEIGHT,
	};
}

void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;

	bool no_content = false;
	cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

	if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
		log_cb = logging.log;
	else
		log_cb = fallback_log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
	input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
	input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

void retro_reset(void)
{
}

static void update_input(void)
{
	input_poll_cb();

	tic80_input input;
	input.gamepads.first.up = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
	input.gamepads.first.down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
	input.gamepads.first.left = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
	input.gamepads.first.right = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
	input.gamepads.first.a = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
	input.gamepads.first.b = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
	input.gamepads.first.x = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
	input.gamepads.first.y = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);

	tic80_tick(tic, input);
}

static void render_checkered(void)
{
	video_cb(tic->screen, TIC80_FULLWIDTH, TIC80_FULLHEIGHT, TIC80_FULLWIDTH << 2);
}

static void check_variables(void)
{
	/* Nothing */
}

static void audio_callback(void)
{
	audio_cb(0, 0);
}


void retro_run(void)
{
	if (timeCounter >= 1000000 / TIC_FRAMERATE) {
		update_input();
		render_checkered();
		audio_callback();

		bool updated = false;
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
			check_variables();

		timeCounter = 0;
	}
}

/**
 * libretro callback; Step the core forwards a step.
 */
void frame_time_cb(retro_usec_t usec) {
	timeCounter += usec;
}

void retro_audio_cb() {
	int bufferSize = 44100 / TIC_FRAMERATE;
	float samples[1470] = { 0 };  // 44100 / 60 * 2
	int16_t samples2[1470] = { 0 };  // 44100 / 60 * 2
	audio_mixer_mix(samples, bufferSize, 1.0, false);
	convert_float_to_s16(samples2, samples, 2 * bufferSize);
	audio_batch_cb(samples2, bufferSize);
}

/**
 * libretro callback; Set the current state of the audio.
 */
void audio_set_state(bool enabled) {
	printf("CAlled audio_set_state()\n");
}

retro_audio_sample_t audio_cb;
bool retro_load_game(const struct retro_game_info *info)
{
	// Pixel format.
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
	{
		log_cb(RETRO_LOG_INFO, "RETRO_PIXEL_FORMAT_XRGB8888 is not supported.\n");
		return false;
	}

	// Frame timer.
	struct retro_frame_time_callback frame_cb = {
		frame_time_cb,
		1000000 / TIC_FRAMERATE
	};
	if (!environ_cb(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_cb)) {
		log_cb(RETRO_LOG_INFO, "Failed to set frame time callback.\n");
		return false;
	}

	// Set the audio callback.
	struct retro_audio_callback retro_audio = { retro_audio_cb, audio_set_state };
	ChaiLove::environ_cb(RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK, &retro_audio);

	// Set up the variables.
	check_variables();

	// Load the game.
	if (info == NULL) {
		printf("info IS NULL\n");
		return false;
	}
	if (info->data == NULL) {
		printf("DATA IS NULL\n");
		return false;
	}
	printf("LOADING! %i\n", info->size);

	// Ensure the game is not loaded before loading.
	retro_unload_game();
	tic = tic80_create(44100);
	state.quit = false;
	tic->callback.exit = onExit;
	tic80_load(tic, (void*)(info->data), info->size);
	if (tic) {
		printf("LOADED!\n");
	}

	(void)info;
	return true;
}

void retro_unload_game(void)
{
	onExit();
	if (tic != NULL) {
		tic80_delete(tic);
		tic = NULL;
	}
}

unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
	if (type != 0x200)
		return false;
	if (num != 2)
		return false;
	return retro_load_game(NULL);
}

size_t retro_serialize_size(void)
{
	return 2;
}

bool retro_serialize(void *data_, size_t size)
{
	if (size < 2)
		return false;

	return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
	if (size < 2)
		return false;

	return true;
}

void *retro_get_memory_data(unsigned id)
{
	(void)id;
	return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
	(void)id;
	return 0;
}

void retro_cheat_reset(void)
{
	/* Nothing */
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
	(void)index;
	(void)enabled;
	(void)code;
}
