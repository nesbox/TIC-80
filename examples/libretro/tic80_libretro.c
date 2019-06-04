#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <tic.h>
#include "libretro.h"

#define TIC_FRAMERATE 60
#define TIC_FREQUENCY 44100

static uint32_t *frame_buf;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
tic80* tic;
static struct
{
	bool quit;
	tic80_input input;
	int keymap[tic_keys_count];
} state =
{
	.quit = false,
};

/**
 * TIC-80 callback; Requests the content to exit.
 */
static void tic80_libretro_exit()
{
	state.quit = true;
}

/** 
 * TIC-80 callback; Report an error from the TIC-80 cart.
 */
static void tic80_libretro_error(const char* info) {
	log_cb(RETRO_LOG_ERROR, info);
}

/**
 * TIC-80 callback; Report a trace log from the TIC-80 cart.
 */
static void tic80_libretro_trace(const char* text, u8 color) {
	log_cb(RETRO_LOG_INFO, text);
}

static void tic80_libretro_fallback_log(enum retro_log_level level, const char *fmt, ...)
{
	(void)level;
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

/**
 * libretro callback; Global initialization.
 */
void retro_init(void)
{
	// Create the frame buffer.
	frame_buf = calloc(320 * 240, sizeof(uint32_t));

	// Initialize the keyboard mappings.
	state.keymap[tic_key_unknown] = RETROK_UNKNOWN;
	state.keymap[tic_key_a] = RETROK_a;
	state.keymap[tic_key_b] = RETROK_b;
	state.keymap[tic_key_c] = RETROK_c;
	state.keymap[tic_key_d] = RETROK_d;
	state.keymap[tic_key_e] = RETROK_e;
	state.keymap[tic_key_f] = RETROK_f;
	state.keymap[tic_key_g] = RETROK_g;
	state.keymap[tic_key_h] = RETROK_h;
	state.keymap[tic_key_i] = RETROK_i;
	state.keymap[tic_key_j] = RETROK_j;
	state.keymap[tic_key_k] = RETROK_k;
	state.keymap[tic_key_l] = RETROK_l;
	state.keymap[tic_key_m] = RETROK_m;
	state.keymap[tic_key_n] = RETROK_n;
	state.keymap[tic_key_o] = RETROK_o;
	state.keymap[tic_key_p] = RETROK_p;
	state.keymap[tic_key_q] = RETROK_q;
	state.keymap[tic_key_r] = RETROK_r;
	state.keymap[tic_key_s] = RETROK_s;
	state.keymap[tic_key_t] = RETROK_t;
	state.keymap[tic_key_u] = RETROK_u;
	state.keymap[tic_key_v] = RETROK_v;
	state.keymap[tic_key_w] = RETROK_w;
	state.keymap[tic_key_x] = RETROK_x;
	state.keymap[tic_key_y] = RETROK_y;
	state.keymap[tic_key_z] = RETROK_z;
	state.keymap[tic_key_0] = RETROK_0;
	state.keymap[tic_key_1] = RETROK_1;
	state.keymap[tic_key_2] = RETROK_2;
	state.keymap[tic_key_3] = RETROK_3;
	state.keymap[tic_key_4] = RETROK_4;
	state.keymap[tic_key_5] = RETROK_5;
	state.keymap[tic_key_6] = RETROK_6;
	state.keymap[tic_key_7] = RETROK_7;
	state.keymap[tic_key_8] = RETROK_8;
	state.keymap[tic_key_9] = RETROK_9;
	state.keymap[tic_key_minus] = RETROK_MINUS;
	state.keymap[tic_key_equals] = RETROK_EQUALS;
	state.keymap[tic_key_leftbracket] = RETROK_LEFTBRACKET;
	state.keymap[tic_key_rightbracket] = RETROK_RIGHTBRACKET;
	state.keymap[tic_key_backslash] = RETROK_BACKSLASH;
	state.keymap[tic_key_semicolon] = RETROK_SEMICOLON;
	state.keymap[tic_key_apostrophe] = RETROK_QUOTE;
	state.keymap[tic_key_grave] = RETROK_TILDE;
	state.keymap[tic_key_comma] = RETROK_COMMA;
	state.keymap[tic_key_period] = RETROK_PERIOD;
	state.keymap[tic_key_slash] = RETROK_SLASH;
	state.keymap[tic_key_space] = RETROK_SPACE;
	state.keymap[tic_key_tab] = RETROK_TAB;
	state.keymap[tic_key_return] = RETROK_RETURN;
	state.keymap[tic_key_backspace] = RETROK_BACKSPACE;
	state.keymap[tic_key_delete] = RETROK_DELETE;
	state.keymap[tic_key_insert] = RETROK_INSERT;
	state.keymap[tic_key_pageup] = RETROK_PAGEUP;
	state.keymap[tic_key_pagedown] = RETROK_PAGEDOWN;
	state.keymap[tic_key_home] = RETROK_HOME;
	state.keymap[tic_key_end] = RETROK_END;
	state.keymap[tic_key_up] = RETROK_UP;
	state.keymap[tic_key_down] = RETROK_DOWN;
	state.keymap[tic_key_left] = RETROK_LEFT;
	state.keymap[tic_key_right] = RETROK_RIGHT;
	state.keymap[tic_key_capslock] = RETROK_CAPSLOCK;
	state.keymap[tic_key_ctrl] = RETROK_LCTRL;
	state.keymap[tic_key_shift] = RETROK_LSHIFT;
	state.keymap[tic_key_alt] = RETROK_LALT;
	state.keymap[tic_key_escape] = RETROK_ESCAPE;
	state.keymap[tic_key_f1] = RETROK_F1;
	state.keymap[tic_key_f2] = RETROK_F2;
	state.keymap[tic_key_f3] = RETROK_F3;
	state.keymap[tic_key_f4] = RETROK_F4;
	state.keymap[tic_key_f5] = RETROK_F5;
	state.keymap[tic_key_f6] = RETROK_F6;
	state.keymap[tic_key_f7] = RETROK_F7;
	state.keymap[tic_key_f8] = RETROK_F8;
	state.keymap[tic_key_f9] = RETROK_F9;
	state.keymap[tic_key_f10] = RETROK_F10;
	state.keymap[tic_key_f11] = RETROK_F11;
	state.keymap[tic_key_f12] = RETROK_F12;
	state.keymap[tic_key_f12] = RETROK_F12;
	state.keymap[tic_keys_count] = RETROK_LAST;
}

/**
 * libretro callback; Global deinitialization.
 */
void retro_deinit(void)
{
	// Clear out the frame buffer.
	free(frame_buf);
	frame_buf = NULL;
}

/**
 * libretro callback; Retrieves the internal libretro API version.
 */
unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

/**
 * libretro callback; Reports device changes.
 */
void retro_set_controller_port_device(unsigned port, unsigned device)
{
	log_cb(RETRO_LOG_INFO, "[TIC-80] Plugging device %u into port %u.\n", device, port);
}

/**
 * libretro callback; Retrieves information about the core.
 */
void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name     = TIC_NAME;
	info->library_version  = TIC_VERSION_LABEL;
	info->need_fullpath    = false;
	info->valid_extensions = "tic";
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

/**
 * libretro callback; Get information about the desired audio and video.
 */
void retro_get_system_av_info(struct retro_system_av_info *info)
{
	info->timing = (struct retro_system_timing) {
		.fps = TIC_FRAMERATE,
		.sample_rate = TIC_FREQUENCY,
	};

	info->geometry = (struct retro_game_geometry) {
		.base_width   = TIC80_FULLWIDTH,
		.base_height  = TIC80_FULLHEIGHT,
		.max_width    = TIC80_FULLWIDTH,
		.max_height   = TIC80_FULLHEIGHT,
		.aspect_ratio = (float)TIC80_FULLWIDTH / (float)TIC80_FULLHEIGHT,
	};
}

/**
 * libretro callback; Sets up the environment callback.
 */
void retro_set_environment(retro_environment_t cb)
{
	// Update the environment callback to make environment calls.
	environ_cb = cb;

	// TIC-80 runner requires a cartridge.
	bool no_content = false;
	cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

	// Set up the logging interface.
	if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging)) {
		log_cb = logging.log;
	}
	else {
		log_cb = tic80_libretro_fallback_log;
	}
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
	// TODO: Allow reseting the game?
}

/**
 * libretro callback; Load the labels for the input buttons.
 */
void tic80_libretro_input_descriptors() {
	// TIC-80's controller has flipped A/B and X/Y buttons than RetroPad.
	struct retro_input_descriptor desc[] = {
		// Player 1
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Y" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "X" },

		// Player 2
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Y" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "X" },

		// Player 3: Disabled for now.
		/*
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Y" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "X" },
		*/

		// Player 4: Disabled for now.
		/*
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Y" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "X" },
		*/

		{ 0 },
	};

	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

/**
 * Retrieve gamepad information from libretro.
 */
static void tic80_libretro_update_gamepad(tic80_gamepad* gamepad, int player) {
	// D-Pad
	gamepad->up = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
	gamepad->down = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
	gamepad->left = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
	gamepad->right = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);

	// A/B and X/Y are switched in TIC-80
	gamepad->a = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
	gamepad->b = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
	gamepad->x = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
	gamepad->y = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);
}

/**
 * Retrieve gamepad information from libretro.
 */
static void tic80_libretro_update_mouse(tic80_mouse* mouse) {
	// TODO: Is there a cleaner way to handle the mouse?
	mouse->x += input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
	mouse->y += input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
	mouse->left = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
	mouse->right = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
	mouse->middle = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
}

/**
 * Retrieve keyboard information from libretro.
 */
static void tic80_libretro_update_keyboard(tic80_keyboard* keyboard) {
	// Clear the key buffer.
	for (int i = 0; i < TIC80_KEY_BUFFER; i++) {
		keyboard->keys[i] = tic_key_unknown;
	}

	// Load up the active keys into the buffer.
	for (int key = tic_key_unknown, keyBuffer = 0; key < tic_keys_count && keyBuffer < TIC80_KEY_BUFFER; key++) {
		if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, state.keymap[key])) {
			keyboard->keys[keyBuffer++] = key;
		}
	}
}

/**
 * libretro callback; Update the input state, and tick the game.
 */
static void tic80_libretro_update()
{
	// Make sure we only act when a TIC-80 environment is available.
	if (tic) {
		// Let libretro know that we need updated input states.
		input_poll_cb();

		// Port the libretro inputs to TIC-80.
		tic80_libretro_update_gamepad(&state.input.gamepads.first, 0);
		tic80_libretro_update_gamepad(&state.input.gamepads.second, 1);

		// Player 3 and 4 are commented out for now.
		// tic80_libretro_update_gamepad(&state.input.gamepads.third, 2);
		// tic80_libretro_update_gamepad(&state.input.gamepads.fourth, 3);

		tic80_libretro_update_mouse(&state.input.mouse);
		tic80_libretro_update_keyboard(&state.input.keyboard);

		// Update the game state.
		tic80_tick(tic, state.input);
	}
}

/**
 * Convert between argb8888 and abgr8888.
 */
void tic80_libretro_conv_argb8888_abgr8888(uint32_t *output, uint32_t *input,
	int width, int height,
	int out_stride, int in_stride)
{
	int h, w;
	for (h = 0; h < height; h++, output += out_stride >> 2, input += in_stride >> 2) {
		for (w = 0; w < width; w++) {
			uint32_t col = input[w];
			output[w] = ((col << 16) & 0xff0000) | 
				((col >> 16) & 0xff) |
				(col & 0xff00ff00);
		}
	}
}

/**
 * Draw the screen.
 */
static void tic80_libretro_draw(void)
{
	// Check if there is a screen to render.
	if (tic && tic->screen) {
		// TIC-80 uses ABGR8888, so we need to convert it.
		tic80_libretro_conv_argb8888_abgr8888(frame_buf, tic->screen,
			TIC80_FULLWIDTH, TIC80_FULLHEIGHT,
			TIC80_FULLWIDTH << 2, TIC80_FULLWIDTH << 2);

		// Render to the screen.
		video_cb(frame_buf, TIC80_FULLWIDTH, TIC80_FULLHEIGHT, TIC80_FULLWIDTH << 2);
	}
}

/**
 * libretro callback; Play the audio.
 */
void tic80_libretro_audio() {
	// Only render audio if there is a TIC-80 environment.
	if (tic) {
		// Tell libretro about the samples.
		audio_batch_cb(tic->sound.samples, tic->sound.count / 2);
	}
}

/**
 * libretro callback; Render the screen and play the audio.
 */
void retro_run(void)
{
	// Check if we are to quit.
	if (state.quit) {
		// If the game is still running, ask it to shut it down.
		if (tic) {
			environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
			retro_unload_game();
		}
		return;
	}

	// Update the TIC-80 environment.
	tic80_libretro_update();

	// Render the screen.
	tic80_libretro_draw();

	// Play the audio.
	tic80_libretro_audio();
}

/**
 * libretro callback; Set the current state of the audio.
 */
void tic80_libretro_audio_set_state(bool enabled) {
	if (enabled) {
		log_cb(RETRO_LOG_INFO, "[TIC-80] Audio: Enabled\n");
	}
	else {
		log_cb(RETRO_LOG_INFO, "[TIC-80] Audio: Disabled\n");
	}
}

/**
 * libretro callback; Load a game.
 */
bool retro_load_game(const struct retro_game_info *info)
{
	// Pixel format.
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
		log_cb(RETRO_LOG_INFO, "[TIC-80] RETRO_PIXEL_FORMAT_XRGB8888 is not supported.\n");
		return false;
	}

	// Update the input button descriptions.
	tic80_libretro_input_descriptors();

	// Check for the content.
	if (!info) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] No content information provided.\n");
		return false;
	}
	if (info->data == NULL) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] No content data provided.\n");
		return false;
	}

	// Ensure a game is not loaded before loading.
	retro_unload_game();

	// Set up the TIC-80 environment.
	tic = tic80_create(TIC_FREQUENCY);
	tic->callback.exit = tic80_libretro_exit;
	tic->callback.error = tic80_libretro_error;
	tic->callback.trace = tic80_libretro_trace;

	// Initialize some of the game state.
	state.quit = false;
	state.input.mouse.x = 0;
	state.input.mouse.y = 0;

	// Load the content.
	tic80_load(tic, (void*)(info->data), info->size);
	if (!tic) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] Content loaded, but failed to load game.\n");
		return false;
	}

	return true;
}

void retro_unload_game(void)
{
	// Close the game if it's loaded.
	state.quit = true;
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
	return retro_load_game(info);
}

size_t retro_serialize_size(void)
{
	return 0;
}

bool retro_serialize(void *data_, size_t size)
{
	return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
	return false;
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
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
	(void)index;
	(void)enabled;
	(void)code;
}
