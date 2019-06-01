#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <tic.h>

#include "libretro.h"

static uint32_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;

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
	frame_buf = calloc(320 * 240, sizeof(uint32_t));
}

void retro_deinit(void)
{
	free(frame_buf);
	frame_buf = NULL;
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
		.base_width   = TIC80_WIDTH,
		.base_height  = TIC80_HEIGHT,
		.max_width    = TIC80_WIDTH,
		.max_height   = TIC80_HEIGHT,
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

static unsigned x_coord;
static unsigned y_coord;
static int mouse_rel_x;
static int mouse_rel_y;

void retro_reset(void)
{
	x_coord = 0;
	y_coord = 0;
}

static void update_input(void)
{
	input_poll_cb();
	if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
	{
		/* stub */
	}
}

static void render_checkered(void)
{
	uint32_t *buf    = frame_buf;
	unsigned stride  = TIC80_WIDTH;
	uint32_t color_r = 0xff << 16;
	uint32_t color_g = 0xff <<  8;
	uint32_t *line   = buf;

	for (unsigned y = 0; y < TIC80_HEIGHT; y++, line += stride)
	{
		unsigned index_y = ((y - y_coord) >> 4) & 1;
		for (unsigned x = 0; x < TIC80_WIDTH; x++)
		{
			unsigned index_x = ((x - x_coord) >> 4) & 1;
			line[x] = (index_y ^ index_x) ? color_r : color_g;
		}
	}

	for (unsigned y = mouse_rel_y - 5; y <= mouse_rel_y + 5; y++)
		for (unsigned x = mouse_rel_x - 5; x <= mouse_rel_x + 5; x++)
			buf[y * stride + x] = 0xff;

	video_cb(buf, TIC80_WIDTH, TIC80_HEIGHT, stride << 2);
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
	update_input();
	render_checkered();
	audio_callback();

	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
		check_variables();
}

bool retro_load_game(const struct retro_game_info *info)
{
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
	{
		log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
		return false;
	}

	check_variables();


	if (info->data == NULL) {
		printf("DATA IS NULL\n");
		return false;
	}

	tic80_load(tic, (void*)info->data, info->size);
	if (tic) {
		printf("LOADED!");
		tic->callback.exit = onExit;
		state.quit = false;
	}

	(void)info;
	return true;
}

void retro_unload_game(void)
{
	onExit();
	tic80_delete(tic);
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

	uint8_t *data = data_;
	data[0] = x_coord;
	data[1] = y_coord;
	return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
	if (size < 2)
		return false;

	const uint8_t *data = data_;
	x_coord = data[0] & 31;
	y_coord = data[1] & 31;
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
