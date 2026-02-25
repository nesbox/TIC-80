#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "tic.h"
#include "libretro-common/include/libretro.h"
#include "retro_inline.h"
#include "retro_endianness.h"
#include "libretro_core_options.h"
#include "api.h"
#include "core/core.h"
#include "script.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>

/**
 * system.h is used for:
 * - TIC80_OFFSET_LEFT
 * - TIC80_OFFSET_TOP,
 * - TIC_NAME
 * - TIC_VERSION
 */
#include "studio/system.h"

static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

#define RETRO_ANALOG_RANGE 0x8000
#define RETRO_BASE_POINTER_SPEED_PHYSICAL 0.4f
#define RETRO_BASE_POINTER_SPEED_ANALOG 4.3f
#define RETRO_BASE_POINTER_SPEED_DPAD 1.6f
#define RETRO_SLOW_MOUSE_FACTOR_ANALOG 0.3f
#define RETRO_SLOW_MOUSE_FACTOR_DPAD 0.4f
#ifndef TIC80_FREQUENCY
#define TIC80_FREQUENCY 1000000
#endif

enum pointer_device_type
{
	POINTER_DEVICE_MOUSE = 0,
	POINTER_DEVICE_TOUCHSCREEN,
	POINTER_DEVICE_LEFT_ANALOG,
	POINTER_DEVICE_RIGHT_ANALOG,
	POINTER_DEVICE_DPAD
};

enum mouse_cursor_type
{
	MOUSE_CURSOR_NONE = 0,
	MOUSE_CURSOR_DOT,
	MOUSE_CURSOR_CROSS,
	MOUSE_CURSOR_ARROW
};

struct tic80_state
{
	bool quit;
	tic80_input input;
	int keymap[RETROK_LAST];
	bool cropBorder;
	enum pointer_device_type pointerDevice;
	float pointerSpeed;
	bool slowGamepadMouse;
	enum mouse_cursor_type mouseCursor;
	u8 mouseCursorColor;
	int analogDeadzone;
	u16 mouseX;
	u16 mouseY;
	u16 mousePreviousX;
	u16 mousePreviousY;
	float mouseXAccumulator;
	float mouseYAccumulator;
	int mouseHideTimer;
	int mouseHideTimerStart;
	tic80* tic;
	retro_usec_t frameTime;
	retro_usec_t lastSerializedTick;
};
static struct tic80_state* state = NULL;

/**
 * TIC-80 callback; Request counter.
 */
static u64 tic80_libretro_counter()
{
	if (state == NULL) {
		return 0;
	}

	return (u64)state->frameTime;
}

/**
 * TIC-80 callback; Request frequency.
 */
static u64 tic80_libretro_frequency()
{
	return TIC80_FREQUENCY;
}

/**
 * TIC-80 callback; Requests the content to exit.
 */
void tic80_libretro_exit()
{
	if (state == NULL) {
		return;
	}

	state->quit = true;
}

/**
 * TIC-80 callback; Report an error from the TIC-80 cart.
 */
void tic80_libretro_error(const char* info)
{
	// Report the error to the log.
	log_cb(RETRO_LOG_ERROR, "[TIC-80]: %s\n", info);

	// Display the error on the screen, if possible.
	if (environ_cb) {
		struct retro_message msg = {
			info,
			6 * TIC80_FRAMERATE
		};
		environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
	}

	// Finally, on an error, close the core.
	tic80_libretro_exit();
}

/**
 * TIC-80 callback; Report a trace log from the TIC-80 cart.
 */
void tic80_libretro_trace(const char* text, u8 color)
{
	TIC_UNUSED(color);
	log_cb(RETRO_LOG_DEBUG, "[TIC-80] %s\n", text);
}

/**
 * libretro callback; Handles the logging internally when the logging isn't set.
 */
void tic80_libretro_fallback_log(enum retro_log_level level, const char *fmt, ...)
{
	TIC_UNUSED(level);
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

/**
 * libretro callback; Called to indicate how much time has passed since last retro_run().
 */
void tic80_libretro_frame_time(retro_usec_t usec) {
	if (state == NULL) {
		return;
	}

	state->frameTime += usec;
}

/**
 * libretro callback; Global initialization.
 */
RETRO_API void retro_init(void)
{
	// Do not re-initialize.
	if (state != NULL) {
		return;
	}

	// Initialize the base state with some default values.
	state = (struct tic80_state*) malloc(sizeof(struct tic80_state));
	memset(state, 0, sizeof(struct tic80_state));
	state->pointerSpeed = 1.0f;
	state->mouseCursorColor = 15;
	state->analogDeadzone = (int)(0.15f * (float)RETRO_ANALOG_RANGE);

	// Initialize the keyboard mappings.
	state->keymap[RETROK_UNKNOWN] = tic_key_unknown;
	state->keymap[RETROK_a] = tic_key_a;
	state->keymap[RETROK_b] = tic_key_b;
	state->keymap[RETROK_c] = tic_key_c;
	state->keymap[RETROK_d] = tic_key_d;
	state->keymap[RETROK_e] = tic_key_e;
	state->keymap[RETROK_f] = tic_key_f;
	state->keymap[RETROK_g] = tic_key_g;
	state->keymap[RETROK_h] = tic_key_h;
	state->keymap[RETROK_i] = tic_key_i;
	state->keymap[RETROK_j] = tic_key_j;
	state->keymap[RETROK_k] = tic_key_k;
	state->keymap[RETROK_l] = tic_key_l;
	state->keymap[RETROK_m] = tic_key_m;
	state->keymap[RETROK_n] = tic_key_n;
	state->keymap[RETROK_o] = tic_key_o;
	state->keymap[RETROK_p] = tic_key_p;
	state->keymap[RETROK_q] = tic_key_q;
	state->keymap[RETROK_r] = tic_key_r;
	state->keymap[RETROK_s] = tic_key_s;
	state->keymap[RETROK_t] = tic_key_t;
	state->keymap[RETROK_u] = tic_key_u;
	state->keymap[RETROK_v] = tic_key_v;
	state->keymap[RETROK_w] = tic_key_w;
	state->keymap[RETROK_x] = tic_key_x;
	state->keymap[RETROK_y] = tic_key_y;
	state->keymap[RETROK_z] = tic_key_z;
	state->keymap[RETROK_0] = tic_key_0;
	state->keymap[RETROK_1] = tic_key_1;
	state->keymap[RETROK_2] = tic_key_2;
	state->keymap[RETROK_3] = tic_key_3;
	state->keymap[RETROK_4] = tic_key_4;
	state->keymap[RETROK_5] = tic_key_5;
	state->keymap[RETROK_6] = tic_key_6;
	state->keymap[RETROK_7] = tic_key_7;
	state->keymap[RETROK_8] = tic_key_8;
	state->keymap[RETROK_9] = tic_key_9;
	state->keymap[RETROK_KP0] = tic_key_0;
	state->keymap[RETROK_KP1] = tic_key_1;
	state->keymap[RETROK_KP2] = tic_key_2;
	state->keymap[RETROK_KP3] = tic_key_3;
	state->keymap[RETROK_KP4] = tic_key_4;
	state->keymap[RETROK_KP5] = tic_key_5;
	state->keymap[RETROK_KP6] = tic_key_6;
	state->keymap[RETROK_KP7] = tic_key_7;
	state->keymap[RETROK_KP8] = tic_key_8;
	state->keymap[RETROK_KP9] = tic_key_9;
	state->keymap[RETROK_MINUS] = tic_key_minus;
	state->keymap[RETROK_EQUALS] = tic_key_equals;
	state->keymap[RETROK_LEFTBRACKET] = tic_key_leftbracket;
	state->keymap[RETROK_RIGHTBRACKET] = tic_key_rightbracket;
	state->keymap[RETROK_BACKSLASH] = tic_key_backslash;
	state->keymap[RETROK_SEMICOLON] = tic_key_semicolon;
	state->keymap[RETROK_QUOTE] = tic_key_apostrophe;
	state->keymap[RETROK_TILDE] = tic_key_grave;
	state->keymap[RETROK_COMMA] = tic_key_comma;
	state->keymap[RETROK_PERIOD] = tic_key_period;
	state->keymap[RETROK_SLASH] = tic_key_slash;
	state->keymap[RETROK_SPACE] = tic_key_space;
	state->keymap[RETROK_TAB] = tic_key_tab;
	state->keymap[RETROK_RETURN] = tic_key_return;
	state->keymap[RETROK_BACKSPACE] = tic_key_backspace;
	state->keymap[RETROK_DELETE] = tic_key_delete;
	state->keymap[RETROK_INSERT] = tic_key_insert;
	state->keymap[RETROK_PAGEUP] = tic_key_pageup;
	state->keymap[RETROK_PAGEDOWN] = tic_key_pagedown;
	state->keymap[RETROK_HOME] = tic_key_home;
	state->keymap[RETROK_END] = tic_key_end;
	state->keymap[RETROK_UP] = tic_key_up;
	state->keymap[RETROK_DOWN] = tic_key_down;
	state->keymap[RETROK_LEFT] = tic_key_left;
	state->keymap[RETROK_RIGHT] = tic_key_right;
	state->keymap[RETROK_CAPSLOCK] = tic_key_capslock;
	state->keymap[RETROK_LCTRL] = tic_key_ctrl;
	state->keymap[RETROK_RCTRL] = tic_key_ctrl;
	state->keymap[RETROK_LSHIFT] = tic_key_shift;
	state->keymap[RETROK_RSHIFT] = tic_key_shift;
	state->keymap[RETROK_LALT] = tic_key_alt;
	state->keymap[RETROK_RALT] = tic_key_alt;
	state->keymap[RETROK_ESCAPE] = tic_key_escape;
	state->keymap[RETROK_F1] = tic_key_f1;
	state->keymap[RETROK_F2] = tic_key_f2;
	state->keymap[RETROK_F3] = tic_key_f3;
	state->keymap[RETROK_F4] = tic_key_f4;
	state->keymap[RETROK_F5] = tic_key_f5;
	state->keymap[RETROK_F6] = tic_key_f6;
	state->keymap[RETROK_F7] = tic_key_f7;
	state->keymap[RETROK_F8] = tic_key_f8;
	state->keymap[RETROK_F9] = tic_key_f9;
	state->keymap[RETROK_F10] = tic_key_f10;
	state->keymap[RETROK_F11] = tic_key_f11;
	state->keymap[RETROK_F12] = tic_key_f12;
	state->keymap[RETROK_F12] = tic_key_f12;
	state->keymap[RETROK_KP0] = tic_key_numpad0;
	state->keymap[RETROK_KP1] = tic_key_numpad1;
	state->keymap[RETROK_KP2] = tic_key_numpad2;
	state->keymap[RETROK_KP3] = tic_key_numpad3;
	state->keymap[RETROK_KP4] = tic_key_numpad4;
	state->keymap[RETROK_KP5] = tic_key_numpad5;
	state->keymap[RETROK_KP6] = tic_key_numpad6;
	state->keymap[RETROK_KP7] = tic_key_numpad7;
	state->keymap[RETROK_KP8] = tic_key_numpad8;
	state->keymap[RETROK_KP9] = tic_key_numpad9;
	state->keymap[RETROK_KP_PERIOD] = tic_key_numpadperiod;
	state->keymap[RETROK_KP_DIVIDE] = tic_key_numpaddivide;
	state->keymap[RETROK_KP_MULTIPLY] = tic_key_numpadmultiply;
	state->keymap[RETROK_KP_MINUS] = tic_key_numpadminus;
	state->keymap[RETROK_KP_PLUS] = tic_key_numpadplus;
	state->keymap[RETROK_KP_ENTER] = tic_key_numpadenter;
}

/**
 * libretro callback; Global deinitialization.
 */
RETRO_API void retro_deinit(void)
{
	// Make sure the game is unloaded.
	retro_unload_game();

	// Free up the state.
	if (state == NULL) {
		return;
	}

	free(state);
	state = NULL;
}

/**
 * libretro callback; Retrieves the internal libretro API version.
 */
RETRO_API unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

/**
 * libretro callback; Reports device changes.
 */
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
	log_cb(RETRO_LOG_INFO, "[TIC-80] Plugging device %u into port %u.\n", device, port);
}

/**
 * libretro callback; Retrieves information about the core.
 */
RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name     = TIC_NAME;
	info->library_version  = TIC_VERSION;
	info->valid_extensions = "tic|png";
	info->need_fullpath    = false;
	info->block_extract    = false;
}

/**
 * libretro callback; Get information about the desired audio and video.
 */
RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
	info->timing = (struct retro_system_timing) {
		.fps = TIC80_FRAMERATE,
		.sample_rate = TIC80_SAMPLERATE,
	};

	info->geometry = (struct retro_game_geometry) {
		.base_width   = TIC80_FULLWIDTH,
		.base_height  = TIC80_FULLHEIGHT,
		.max_width    = TIC80_FULLWIDTH,
		.max_height   = TIC80_FULLHEIGHT,
		.aspect_ratio = (float)TIC80_FULLWIDTH / (float)TIC80_FULLHEIGHT,
	};

	if (state->cropBorder) {
		info->geometry.base_width   = TIC80_WIDTH;
		info->geometry.base_height  = TIC80_HEIGHT;
		info->geometry.aspect_ratio = (float)TIC80_WIDTH / (float)TIC80_HEIGHT;
	}
}

/**
 * libretro callback; Sets up the environment callback.
 */
RETRO_API void retro_set_environment(retro_environment_t cb)
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

	// Configure the core settings.
	libretro_set_core_options(environ_cb);
}

/**
 * libretro callback; Set up the audio sample callback.
 */
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)
{
	audio_cb = cb;
}

/**
 * libretro callback; Set up the audio sample batch callback.
 *
 * @see tic80_libretro_audio()
 */
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}

/**
 * libretro callback; Set up the input poll callback.
 */
RETRO_API void retro_set_input_poll(retro_input_poll_t cb)
{
	input_poll_cb = cb;
}

/**
 * libretro callback; Set up the input state callback.
 */
RETRO_API void retro_set_input_state(retro_input_state_t cb)
{
	input_state_cb = cb;
}

/**
 * libretro callback; Set up the video refresh callback.
 */
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

/**
 * libretro callback; Reset the game.
 */
RETRO_API void retro_reset(void)
{
	if (state != NULL && state->tic != NULL) {
		tic_mem* tic = (tic_mem*)state->tic;
		tic_api_reset(tic);
	}
}

/**
 * libretro callback; Load the labels for the input buttons.
 *
 * @see tic80_libretro_update()
 */
void tic80_libretro_input_descriptors()
{
	// TIC-80's controller has flipped A/B and X/Y buttons than RetroPad.
	struct retro_input_descriptor desc[] = {

#if TIC_MAXPLAYERS >= 1
		// Player 1
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Y" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "X" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Slow Mouse" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Mouse Right Click" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Mouse Left Click" },
#endif

#if TIC_MAXPLAYERS >= 2
		// Player 2
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Y" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "X" },
#endif

#if TIC_MAXPLAYERS >= 3
		// Player 3
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Y" },
		{ 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "X" },
#endif

#if TIC_MAXPLAYERS >= 4
		// Player 4
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "A" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "B" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Y" },
		{ 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "X" },
#endif

		{ 0 },
	};

	environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
}

/**
 * Retrieve gamepad information from libretro.
 *
 * @see tic80_libretro_update()
 */
void tic80_libretro_update_gamepad(tic80_gamepad* gamepad, tic80_mouse* mouse, int player, bool dpad)
{
	// D-Pad
	if (dpad) {
		gamepad->up = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
		gamepad->down = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
		gamepad->left = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
		gamepad->right = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
	}

	// A/B and X/Y are switched in TIC-80
	gamepad->a = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
	gamepad->b = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
	gamepad->x = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y);
	gamepad->y = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X);

	// Port 1 shoulder buttons mapped to mouse left/right click/slow mouse
	if (mouse && (player == 0)) {
		mouse->left = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2);
		mouse->right = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2);
		state->slowGamepadMouse = input_state_cb(player, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L);
	}
}

/**
 * Converts a Pointer API coordinates to screen pixel position.
 *
 * @see tic80_libretro_update_mouse()
 * @see RETRO_DEVICE_POINTER
 */
int tic80_libretro_mouse_pointer_convert(float coord, float full, float margin)
{
	float max         = (float)0x7fff;
	float screenCoord = (((coord + max) / (max * 2.0f) ) * full) - margin;

	// Keep the mouse on the screen.
	if (margin > 0.0f) {
		float limit = full - (margin * 2.0f) - 1.0f;
		screenCoord = (screenCoord < 0.0f)  ? 0.0f  : screenCoord;
		screenCoord = (screenCoord > limit) ? limit : screenCoord;
	}

	return (int)(screenCoord + 0.5f);
}

static void tic80_libretro_update_mouse_wheels(tic80_mouse* mouse)
{
	if (input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP) > 0) {
		mouse->scrollx = 1;
	} else if (input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN) > 0) {
		mouse->scrollx = -1;
	}
	if (input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP) > 0) {
		mouse->scrolly = 1;
	} else if (input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN) > 0) {
		mouse->scrolly = -1;
	}
}

static INLINE float tic80_libretro_get_mouse_delta_physical(unsigned axis)
{
	int delta = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, axis);
	return (float)delta * RETRO_BASE_POINTER_SPEED_PHYSICAL * state->pointerSpeed;
}

static INLINE float tic80_libretro_get_mouse_delta_analog(unsigned index, unsigned axis)
{
	int delta = input_state_cb(0, RETRO_DEVICE_ANALOG, index, axis);
	float delta_amp = 0.0f;

	if ((delta < -state->analogDeadzone) || (delta > state->analogDeadzone)) {
		delta_amp = (float)((delta > state->analogDeadzone) ?
				(delta - state->analogDeadzone) :
						(delta + state->analogDeadzone)) /
								(float)(RETRO_ANALOG_RANGE - state->analogDeadzone);

		delta_amp *= RETRO_BASE_POINTER_SPEED_ANALOG * state->pointerSpeed *
				(state->slowGamepadMouse ? RETRO_SLOW_MOUSE_FACTOR_ANALOG : 1.0f);
	}

	return delta_amp;
}

static INLINE float tic80_libretro_get_mouse_delta_dpad(unsigned axisPlus, unsigned axisMinus)
{
	float delta_amp = 0.0f;

	if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, axisPlus) > 0) {
		delta_amp = RETRO_BASE_POINTER_SPEED_DPAD * state->pointerSpeed *
				(state->slowGamepadMouse ? RETRO_SLOW_MOUSE_FACTOR_DPAD : 1.0f);
	} else if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, axisMinus) > 0) {
		delta_amp = -1.0 * RETRO_BASE_POINTER_SPEED_DPAD * state->pointerSpeed *
				(state->slowGamepadMouse ? RETRO_SLOW_MOUSE_FACTOR_DPAD : 1.0f);
	}

	return delta_amp;
}

/**
 * Retrieve mouse information from libretro.
 */
void tic80_libretro_update_mouse(tic80_mouse* mouse)
{
	mouse->scrollx = 0;
	mouse->scrolly = 0;
	mouse->middle  = 0;

	// Check which device type to poll
	if (state->pointerDevice == POINTER_DEVICE_TOUCHSCREEN) {
		float screenWidth    = (float)TIC80_FULLWIDTH;
		float screenHeight   = (float)TIC80_FULLHEIGHT;
		float screenMarginX  = (float)TIC80_OFFSET_LEFT;
		float screenMarginY  = (float)TIC80_OFFSET_TOP;
		if (state->cropBorder) {
			screenWidth       = (float)TIC80_WIDTH;
			screenHeight      = (float)TIC80_HEIGHT;
			screenMarginX     = 0.0f;
			screenMarginY     = 0.0f;
		}

		// Get the Pointer X and Y, and convert it to screen position
		state->mouseX = tic80_libretro_mouse_pointer_convert(
				input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X),
				screenWidth, screenMarginX);
		state->mouseY = tic80_libretro_mouse_pointer_convert(
				input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y),
				screenHeight, screenMarginY);

		// Pointer pressed is considered mouse left button
		mouse->left = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
		// Touchscreens do not have right or middle buttons,
		// but on Unix at least, the mouse registers as a
		// touchscreen (pointer API) device - so might as
		// well poll the additional mouse buttons
		mouse->right = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
		mouse->middle = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
		tic80_libretro_update_mouse_wheels(mouse);
	} else {
		// All other input devices use relative positioning
		float mouseDeltaX = 0;
		float mouseDeltaY = 0;
		int mouseDeltaXInt = 0;
		int mouseDeltaYInt = 0;

		switch (state->pointerDevice) {
			case POINTER_DEVICE_MOUSE:
				// Get Mouse X and Y offsets
				mouseDeltaX = tic80_libretro_get_mouse_delta_physical(RETRO_DEVICE_ID_MOUSE_X);
				mouseDeltaY = tic80_libretro_get_mouse_delta_physical(RETRO_DEVICE_ID_MOUSE_Y);

				// Mouse buttons
				mouse->left = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
				mouse->right = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
				mouse->middle = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
				tic80_libretro_update_mouse_wheels(mouse);
			break;
#if TIC_MAXPLAYERS >= 1
			case POINTER_DEVICE_TOUCHSCREEN:
				// Already handled above.
			break;
			case POINTER_DEVICE_LEFT_ANALOG:
				// Get Mouse X and Y offsets
				mouseDeltaX = tic80_libretro_get_mouse_delta_analog(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
				mouseDeltaY = tic80_libretro_get_mouse_delta_analog(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
			break;
			case POINTER_DEVICE_RIGHT_ANALOG:
				// Get Mouse X and Y offsets
				mouseDeltaX = tic80_libretro_get_mouse_delta_analog(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
				mouseDeltaY = tic80_libretro_get_mouse_delta_analog(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
			break;
			case POINTER_DEVICE_DPAD:
				// Get Mouse X and Y offsets
				mouseDeltaX = tic80_libretro_get_mouse_delta_dpad(RETRO_DEVICE_ID_JOYPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_LEFT);
				mouseDeltaY = tic80_libretro_get_mouse_delta_dpad(RETRO_DEVICE_ID_JOYPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_UP);
			break;
#endif
		}

		// Determine mouse x/y positions
		if (mouseDeltaX < 0) {
			// Reset accumulator when changing direction,
			// otherwise apply delta
			state->mouseXAccumulator = (state->mouseXAccumulator > 0.0f) ?
					mouseDeltaX : state->mouseXAccumulator + mouseDeltaX;
			// Get integer component of accumulator
			mouseDeltaXInt = (int)state->mouseXAccumulator;
			// Update x position
			mouseDeltaXInt *= -1;
			state->mouseX = (state->mouseX > mouseDeltaXInt) ?
					(state->mouseX - mouseDeltaXInt) : 0;
			// Update accumulator
			state->mouseXAccumulator += (float)mouseDeltaXInt;
		} else {
			// Reset accumulator when changing direction,
			// otherwise apply delta
			state->mouseXAccumulator = (state->mouseXAccumulator < 0.0f) ?
					mouseDeltaX : state->mouseXAccumulator + mouseDeltaX;
			// Get integer component of accumulator
			mouseDeltaXInt = (int)state->mouseXAccumulator;
			// Update x position
			state->mouseX = (state->mouseX + mouseDeltaXInt < TIC80_WIDTH) ?
					(state->mouseX + mouseDeltaXInt) : (TIC80_WIDTH - 1);
			// Update accumulator
			state->mouseXAccumulator -= (float)mouseDeltaXInt;
		}

		if (mouseDeltaY < 0) {
			// Reset accumulator when changing direction,
			// otherwise apply delta
			state->mouseYAccumulator = (state->mouseYAccumulator > 0.0f) ?
					mouseDeltaY : state->mouseYAccumulator + mouseDeltaY;
			// Get integer component of accumulator
			mouseDeltaYInt = (int)state->mouseYAccumulator;
			// Update y position
			mouseDeltaYInt *= -1;
			state->mouseY = (state->mouseY > mouseDeltaYInt) ?
					(state->mouseY - mouseDeltaYInt) : 0;
			// Update accumulator
			state->mouseYAccumulator += (float)mouseDeltaYInt;
		} else {
			// Reset accumulator when changing direction,
			// otherwise apply delta
			state->mouseYAccumulator = (state->mouseYAccumulator < 0.0f) ?
					mouseDeltaY : state->mouseYAccumulator + mouseDeltaY;
			// Get integer component of accumulator
			mouseDeltaYInt = (int)state->mouseYAccumulator;
			// Update y position
			state->mouseY = (state->mouseY + mouseDeltaYInt < TIC80_HEIGHT) ?
					(state->mouseY + mouseDeltaYInt) : (TIC80_HEIGHT - 1);
			// Update accumulator
			state->mouseYAccumulator -= (float)mouseDeltaYInt;
		}
	}

	// Have the mouse disappear after a certain time of inactivity.
	if (state->mouseX != state->mousePreviousX || state->mouseY != state->mousePreviousY) {
		state->mouseHideTimer = state->mouseHideTimerStart;
		state->mousePreviousX = state->mouseX;
		state->mousePreviousY = state->mouseY;
	}
	else if (state->mouseHideTimer > 0) {
		state->mouseHideTimer--;
	}

	// TIC-80 internally offsets the mouse x/y coordinates,
	// so have to adjust libretro values...
	mouse->x = state->mouseX + TIC80_OFFSET_LEFT;
	mouse->y = state->mouseY + TIC80_OFFSET_TOP;
}

/**
 * Gets the 32-bit color value from the TIC-80 palette.
 */
static u32 get_screen_color(tic_mem* tic, u8 index)
{
	tic_rgb color = tic->ram->vram.palette.colors[index];
	// The core requests RETRO_PIXEL_FORMAT_XRGB8888, so we format the color as 0x00RRGGBB.
	return (color.r << 16) | (color.g << 8) | (color.b);
}

/**
 * Draws a single pixel directly to the final screen buffer.
 */
static void draw_pixel_on_screen(u32* screen, s32 x, s32 y, u32 color)
{
	// Bounds check against the visible screen area
	if (x < 0 || x >= TIC80_WIDTH || y < 0 || y >= TIC80_HEIGHT)
		return;

	s32 full_x = x + TIC80_OFFSET_LEFT;
	s32 full_y = y + TIC80_OFFSET_TOP;

	screen[full_y * TIC80_FULLWIDTH + full_x] = color;
}

/**
 * Draws a horizontal line directly to the final screen buffer.
 */
static void draw_hline_on_screen(u32* screen, s32 x1, s32 x2, s32 y, u32 color)
{
	for (s32 x = x1; x <= x2; x++)
		draw_pixel_on_screen(screen, x, y, color);
}

/**
 * Draws a vertical line directly to the final screen buffer.
 */
static void draw_vline_on_screen(u32* screen, s32 x, s32 y1, s32 y2, u32 color)
{
	for (s32 y = y1; y <= y2; y++)
		draw_pixel_on_screen(screen, x, y, color);
}

/**
 * Draws a software cursor on the screen where the mouse is.
 */
void tic80_libretro_mousecursor(tic80* game, tic80_mouse* mouse, enum mouse_cursor_type cursortype)
{
	TIC_UNUSED(mouse);

	// Only draw the mouse cursor if it's active.
	if (state->mouseHideTimerStart > 0 && state->mouseHideTimer == 0) {
		return;
	}

	tic_mem* tic = (tic_mem*)game;
	u32* screen = game->screen;
	s32 mx = state->mouseX;
	s32 my = state->mouseY;

	// Calculate the final 32-bit color value
	u32 cursor_color = get_screen_color(tic, state->mouseCursorColor);

	// Draw the cursor directly to the screen buffer.
	switch (cursortype) {
		case MOUSE_CURSOR_NONE:
			// Nothing.
		break;
		case MOUSE_CURSOR_DOT:
			draw_pixel_on_screen(screen, mx, my, cursor_color);
		break;
		case MOUSE_CURSOR_CROSS:
			draw_hline_on_screen(screen, mx - 4, mx - 2, my, cursor_color);
			draw_hline_on_screen(screen, mx + 2, mx + 4, my, cursor_color);
			draw_vline_on_screen(screen, mx, my - 4, my - 2, cursor_color);
			draw_vline_on_screen(screen, mx, my + 2, my + 4, cursor_color);
		break;
		case MOUSE_CURSOR_ARROW:
		{
			// Calculate black for the outline.
			u32 black_color = get_screen_color(tic, tic_color_black);

			// Draw the filled triangle part of the arrow
			for (int y = 0; y <= 2; y++) {
				for (int x = 0; x <= 2 - y; x++) {
					draw_pixel_on_screen(screen, mx + x, my + y, cursor_color);
				}
			}
			// Draw the black outline (hypotenuse of the triangle)
			draw_pixel_on_screen(screen, mx + 3, my, black_color);
			draw_pixel_on_screen(screen, mx + 2, my + 1, black_color);
			draw_pixel_on_screen(screen, mx + 1, my + 2, black_color);
			draw_pixel_on_screen(screen, mx, my + 3, black_color);
		}
		break;
	}
}

/**
 * Retrieve keyboard information from libretro.
 */
void tic80_libretro_update_keyboard(tic80_keyboard* keyboard)
{
	// Clear the key buffer.
	for (int i = 0; i < TIC80_KEY_BUFFER; i++) {
		keyboard->keys[i] = tic_key_unknown;
	}

	// Load up the active keys into the buffer.
	for (int key = RETROK_FIRST, keyBuffer = 0; key < RETROK_LAST && keyBuffer < TIC80_KEY_BUFFER; key++) {
		if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, key)) {
			keyboard->keys[keyBuffer++] = state->keymap[key];
		}
	}
}

/**
 * Update the input state, and tick the game.
 */
void tic80_libretro_update(tic80* game)
{
	// Let libretro know that we need updated input states.
	input_poll_cb();

	// Gamepads
#if TIC_MAXPLAYERS >= 1
	tic80_libretro_update_gamepad(&state->input.gamepads.first,
			&state->input.mouse, 0, state->pointerDevice != POINTER_DEVICE_DPAD);
#endif

#if TIC_MAXPLAYERS >= 2
	tic80_libretro_update_gamepad(&state->input.gamepads.second, NULL, 1, true);
#endif

#if TIC_MAXPLAYERS >= 3
	tic80_libretro_update_gamepad(&state->input.gamepads.third, NULL, 2, true);
#endif

#if TIC_MAXPLAYERS >= 4
	tic80_libretro_update_gamepad(&state->input.gamepads.fourth, NULL, 3, true);
#endif

	// Mouse
	tic80_libretro_update_mouse(&state->input.mouse);

	// Keyboard
	tic80_libretro_update_keyboard(&state->input.keyboard);

	// Update the game state.
	tic80_tick(game, state->input, tic80_libretro_counter, tic80_libretro_frequency);
	tic80_sound(game);
}

/**
 * Draw the screen.
 */
void tic80_libretro_draw(tic80* game)
{
	// Render the mouse cursor if needed.
	tic80_libretro_mousecursor((tic80*)game, &state->input.mouse, state->mouseCursor);

	// Render to the screen.
	if (state->cropBorder) {
		u32 *screen = (u32*)game->screen + (TIC80_FULLWIDTH * TIC80_OFFSET_TOP) + TIC80_OFFSET_LEFT;
		video_cb(screen, TIC80_WIDTH, TIC80_HEIGHT, TIC80_FULLWIDTH << 2);
	} else {
		video_cb(game->screen, TIC80_FULLWIDTH, TIC80_FULLHEIGHT, TIC80_FULLWIDTH << 2);
	}
}

/**
 * Play the TIC-80 audio.
 *
 * @see retro_run()
 */
void tic80_libretro_audio(tic80* game)
{
	// Tell libretro about the samples.
	audio_batch_cb(game->samples.buffer, game->samples.count / TIC80_SAMPLE_CHANNELS);
}

/**
 * Update the state of the core variables.
 *
 * @see retro_run()
 */
void tic80_libretro_variables(bool startup)
{
	// Check all the individual variables for the core.
	struct retro_variable var;
	bool lastCropBorder = state->cropBorder;

	// Crop Border
	state->cropBorder = false;
	var.key = "tic80_crop_border";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "enabled") == 0) {
			state->cropBorder = true;
		}
	}

	if (!startup && (state->cropBorder != lastCropBorder)) {
		struct retro_system_av_info av_info;
		retro_get_system_av_info(&av_info);
		environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &av_info);
	}

	// Pointer device
	state->pointerDevice = POINTER_DEVICE_MOUSE;
	var.key = "tic80_pointer_device";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "touchscreen") == 0) {
			state->pointerDevice = POINTER_DEVICE_TOUCHSCREEN;
		}
#if TIC_MAXPLAYERS >= 1
		else if (strcmp(var.value, "left_analog") == 0) {
			state->pointerDevice = POINTER_DEVICE_LEFT_ANALOG;
		}
		else if (strcmp(var.value, "right_analog") == 0) {
			state->pointerDevice = POINTER_DEVICE_RIGHT_ANALOG;
		}
		else if (strcmp(var.value, "dpad") == 0) {
			state->pointerDevice = POINTER_DEVICE_DPAD;
		}
#endif
	}

	// Pointer Speed
	state->pointerSpeed = 1.0f;
	var.key = "tic80_pointer_speed";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		state->pointerSpeed = (float)atoi(var.value) * 0.01f;
	}

	// Mouse Cursor
	state->mouseCursor = MOUSE_CURSOR_NONE;
	var.key = "tic80_mouse_cursor";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "dot") == 0) {
			state->mouseCursor = MOUSE_CURSOR_DOT;
		}
		else if (strcmp(var.value, "cross") == 0) {
			state->mouseCursor = MOUSE_CURSOR_CROSS;
		}
		else if (strcmp(var.value, "arrow") == 0) {
			state->mouseCursor = MOUSE_CURSOR_ARROW;
		}
	}

	// Mouse Cursor Color
	state->mouseCursorColor = 15;
	var.key = "tic80_mouse_cursor_color";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		state->mouseCursorColor = (u8)atoi(var.value);
	}

	// Mouse Hide Delay
	state->mouseHideTimerStart = 0;
	var.key = "tic80_mouse_hide_delay";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		state->mouseHideTimerStart = atoi(var.value);
		if (state->mouseHideTimerStart > 0) {
			state->mouseHideTimerStart = state->mouseHideTimerStart * TIC80_FRAMERATE;
			state->mouseHideTimer = 0; // Cursor starts hidden
		}
		else {
			state->mouseHideTimerStart = 0;
			state->mouseHideTimer = 0;
		}
	}

	// Gamepad Analog Deadzone
	state->analogDeadzone = (int)(0.15f * (float)RETRO_ANALOG_RANGE);
	var.key = "tic80_analog_deadzone";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		state->analogDeadzone = (int)((float)atoi(var.value) * 0.01f * (float)RETRO_ANALOG_RANGE);
	}
}

/**
 * libretro callback; Render the screen and play the audio.
 */
RETRO_API void retro_run(void)
{
	// Ensure the state is set up.
	if (state == NULL || state->tic == NULL) {
		return;
	}

	// Update the TIC-80 environment.
	tic80_libretro_update(state->tic);

	// Check if the game requested to quit.
	if (state->quit) {
		retro_deinit();
		environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
		return;
	}

	// Render the screen.
	tic80_libretro_draw(state->tic);

	// Play the audio.
	tic80_libretro_audio(state->tic);

	// Update core options, if needed.
	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
		tic80_libretro_variables(false);
	}
}

/**
 * libretro callback; Load a game.
 */
RETRO_API bool retro_load_game(const struct retro_game_info *info)
{
	// TODO: Warn that Audio Synchronization required to run at a proper speed.
	// TODO: Warn that the core doesn't support Runahead.

	// Initialize the core if it hasn't been yet.
	if (state == NULL) {
		retro_init();
	}

	// Pixel format.
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] RETRO_PIXEL_FORMAT_XRGB8888 is not supported.\n");
		return false;
	}

	// Check for the content.
	if (info == NULL) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] No content information provided.\n");
		return false;
	}

	// Ensure content data is available.
	if (info->data == NULL) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] No content data provided.\n");
		return false;
	}

	// Set up the frame time callback.
	struct retro_frame_time_callback frame_time = {
		.callback = tic80_libretro_frame_time,
		.reference = TIC80_FREQUENCY / TIC80_FRAMERATE,
	};
	if (!environ_cb(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_time)) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] Failed to set frame time callback.\n");
		return false;
	}

	// Set up the TIC-80 environment.
#if RETRO_IS_BIG_ENDIAN
	state->tic = tic80_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_ARGB8888);
#else
	state->tic = tic80_create(TIC80_SAMPLERATE, TIC80_PIXEL_COLOR_BGRA8888);
#endif
	if (state->tic == NULL) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] Failed to initialize TIC-80 environment.\n");
		return false;
	}

	// Set up the environment variables.
	state->tic->callback.exit = tic80_libretro_exit;
	state->tic->callback.error = tic80_libretro_error;
	state->tic->callback.trace = tic80_libretro_trace;

	// Initialize some of the game state.
	state->quit = false;
	state->input.mouse.x = 0;
	state->input.mouse.y = 0;

	// Load the content.
	// TODO: Allow loading code files directly.
	tic80_load(state->tic, (void*)(info->data), (int)info->size);
	if (state->tic == NULL) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] Content loaded, but failed to load game.\n");
		retro_unload_game();
		return false;
	}

	// Set up the input descriptors.
	tic80_libretro_input_descriptors();

	// Load up any core variables.
	tic80_libretro_variables(true);

	return true;
}

/**
 * libretro callback; Tells the core to unload the game.
 */
RETRO_API void retro_unload_game(void)
{
	if (state == NULL || state->tic == NULL) {
		return;
	}

	tic80_delete(state->tic);
	state->tic = NULL;
}

/**
 * libretro callback; Retrieves the region for the content.
 */
RETRO_API unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

/**
 * libretro callback; Load a game using a subsystem.
 */
RETRO_API bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
	TIC_UNUSED(num);
	TIC_UNUSED(type);
	// Forward subsystem requests over to retro_load_game().
	return retro_load_game(info);
}

static char* SerializedLuaData = NULL;
static size_t SerializedLuaSize = 0;

// Helper to free the cached serialized Lua data
static void free_serialized_lua() {
	if (SerializedLuaData) {
		free(SerializedLuaData);
		SerializedLuaData = NULL;
	}
	SerializedLuaSize = 0;
}

static int internal_lua_type(lua_State* L) {
   lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
   return 1;
}

// Helper to serialize Lua _G table into a string
static void serialize_lua(tic_core* core) {
	// Optimization: If we already serialized for this tick, reuse the cached data.
	// This prevents triple-execution of the serialization script when the frontend calls
	// retro_serialize_size multiple times before retro_serialize.
	if (SerializedLuaData && state && state->lastSerializedTick == state->frameTime) {
		return;
	}

	free_serialized_lua();

	lua_State* lua = core->currentVM;
	if (!lua) {
		log_cb(RETRO_LOG_INFO, "Error, currentVM is NULL\n");
		return;
	}

	// Important for Ghost game (1777). (overwritten type function)
	lua_pushcfunction(lua, internal_lua_type);
	lua_setglobal(lua, "__builtin_type");

	// Lua script to serialize the _G table
	// Properly handles standard library references by serializing them as special markers
	// Skips unserializable functions so they can be preserved during merge
	const char* script =
		"local type = __builtin_type "
		"local next = next "
		"local pairs = pairs "
		"if type(pairs)~='function' then pairs = function(x) return next,x,nil end end " // Important for OddSocks (pairs is overwritten)
		"local dbg = package.loaded.debug " // Important: use real debug lib
		"local names = {} "
		"local libs = {'math','table','string','coroutine','package','io','os','utf8','debug'} "
		"for _,n in ipairs(libs) do if _G[n] then names[_G[n]] = n end end "
		"names[_G] = '_G' "
		"local visited = {} "
		"local function scan(t, path, d) "
		"  if d>8 or visited[t] then return end "
		"  visited[t] = true "
		"  for k,v in pairs(t) do "
		"    if type(k)=='string' and k:match('^[%a_][%w_]*$') then "
		"      local sub = (path=='_G') and k or (path..'.'..k) "
		"      if type(v)=='function' then "
		"        names[v]=sub "
		"      elseif type(v)=='table' then "
		"        if k~='_G' and k~='package' then "
		"          local is_class = false " // Important for Beyond The Underground (detects Vec, Mat, etc.)
		"          if type(v.new)=='function' then is_class=true end "
		"          if v.__index then is_class=true end "
		"          if is_class and not names[v] then names[v] = sub end " // Only reference classes, serialize instances by value (Color Critters)
		"          scan(v, sub, d+1) "
		"        end "
		"      end "
		"    end "
		"  end "
		"end "
		"scan(_G, '_G', 0) "
		"local function getExpr(o) "
		"  local n = names[o] if not n then return nil end "
		"  local res = '_G' "
		"  for part in n:gmatch('[^.]+') do res = res .. '[' .. string.format('%q', part) .. ']' end "
		"  return res "
		"end "
		"local function ser(o,v,d,ref_tables) " // Added ref_tables param
		"  d=d or 0 if d>30 then return nil end "
		"  local t=type(o) "
		"  if t=='number' then " // Important for Bouncelot (preserves float vs int)
		"    if o~=o then return '(0/0)' end " // Important for Katzu: NaN check
		"    if o==math.huge then return '(1/0)' end " // Positive infinity
		"    if o==-math.huge then return '(-1/0)' end " // Negative infinity
		"    local s=string.format('%.17g',o) " // Important for Buried Deep (preserves precision)
		"    if math.type and math.type(o)=='float' and not s:find('[^%-0-9]') then s=s..'.0' end " // Safety check for math.type
		"    return s "
		"  end "
		"  if t=='boolean' then return tostring(o) end "
		"  if t=='string' then return string.format('%q',o) end "
		"  if t=='function' then return getExpr(o) end "
		"  if t=='table' then "
		"    if ref_tables and names[o] then return getExpr(o) end " // Use references for Upvalues
		"    if o==math then return '\"__STDLIB_MATH__\"' end "
		"    if o==table then return '\"__STDLIB_TABLE__\"' end "
		"    if o==string then return '\"__STDLIB_STRING__\"' end "
		"    if o==coroutine then return '\"__STDLIB_COROUTINE__\"' end "
		"    if o==package then return '\"__STDLIB_PACKAGE__\"' end "
		"    if o==io then return '\"__STDLIB_IO__\"' end "
		"    if o==os then return '\"__STDLIB_OS__\"' end "
		"    if o==utf8 then return '\"__STDLIB_UTF8__\"' end "
		"    if o==debug then return '\"__STDLIB_DEBUG__\"' end "
		"    if v[o] then return nil end "
		"    v[o]=true "
		"    local mt = getmetatable(o) "
		"    local mte = mt and getExpr(mt) "
		"    local s = mte and 'setmetatable(' or '' "
		"    s = s .. '{' "
		"    for k,val in pairs(o) do "
		"      if (type(k)=='string' or type(k)=='number') and k~='_G' and k~='package' and "
		"         k~='coroutine' and k~='table' and k~='io' and k~='os' and k~='string' and "
		"         k~='math' and k~='utf8' and k~='debug' then "
		"        local ks=ser(k,v,d+1,ref_tables) local vs=ser(val,v,d+1,ref_tables) "
		"        if ks and vs then s=s..'['..ks..']='..vs..',' end "
		"      end "
		"    end "
		"    s = s .. '}' "
		"    if mte then s = s .. ',' .. mte .. ')' end "
		"    v[o]=nil "
		"    return s "
		"  end "
		"  return nil "
		"end "

		// Important for Bone Knight: Scan upvalues to capture file-local state variables
		// We use debug.upvalueid to detect shared locals (like 'player' used in multiple functions)
		"local uvs={} "
		"local joins={} "
		"local u_ids={} "
		"local q={} "
		"local u_seen={} "
		"if dbg then "
		"  for k,v in pairs(_G) do if type(v)=='function' then table.insert(q,{v,{k}}); u_seen[v]=true end end "
		"  local ptr=1 "
		"  while ptr<=#q do "
		"    local item=q[ptr]; ptr=ptr+1 "
		"    local f=item[1]; local p=item[2] "
		"    local i=1 "
		"    while true do "
		"      local n,v=dbg.getupvalue(f,i) "
		"      if not n then break end "
		"      if n~='_ENV' then "
		"        local np={table.unpack(p)}; table.insert(np,i) "
		"        local id=dbg.upvalueid(f,i) "
		"        if u_ids[id] then "
		"          table.insert(joins,{src=u_ids[id],dest=np}) "
		"        else "
		"          u_ids[id]=np "
		"          if type(v)=='function' then "
		"            if not u_seen[v] then u_seen[v]=true; table.insert(q,{v,np}) end "
		"          elseif type(v)=='table' or type(v)=='number' or type(v)=='string' or type(v)=='boolean' then "
		"            table.insert(uvs,{p=np,v=v}) "
		"          end "
		"        end "
		"      end "
		"      i=i+1 "
		"    end "
		"  end "
		"end "

		"local g_str=ser(_G,{},0,false) " // No refs for G (serialize by value)
		"local u_str=ser(uvs,{},0,true) " // Refs for U (point to G tables if possible)
		"local j_str=ser(joins,{},0,true) "
		"return '{g='..(g_str or '{}')..',u='..(u_str or 'nil')..',j='..(j_str or 'nil')..'}' ";

	if (luaL_dostring(lua, script) == LUA_OK) {
		if (lua_isstring(lua, -1)) {
			size_t len = 0;
			const char* str = lua_tolstring(lua, -1, &len);
			if (str) {
				SerializedLuaData = malloc(len + 1);
				if (SerializedLuaData) {
					memcpy(SerializedLuaData, str, len);
					SerializedLuaData[len] = '\0';
					SerializedLuaSize = len;
					if (state) state->lastSerializedTick = state->frameTime;
				}
			} else {
				log_cb(RETRO_LOG_ERROR, "[TIC-80] Lua serialization script returned nil\n");
			}
		} else {
			const char* error_msg = lua_tostring(lua, -1);
			if(!error_msg) {
				log_cb(RETRO_LOG_ERROR, "[TIC-80] Lua serialization script probably returned nil\n");
			} else {
				log_cb(RETRO_LOG_ERROR, "[TIC-80] Lua serialization error: %s\n", error_msg);
			}
		}
		lua_pop(lua, 1);
	} else {
		const char* error_msg = lua_tostring(lua, -1);
		log_cb(RETRO_LOG_ERROR, "[TIC-80] Lua serialization script compile error: %s\n", error_msg ? error_msg : "unknown error");
		lua_pop(lua, 1);  // Remove error message from stack
	}

	lua_pushnil(lua);
	lua_setglobal(lua, "__builtin_type");
}

/**
 * libretro callback; Retrieve the size of the serialized memory.
 */
size_t retro_serialize_size(void)
{
	size_t size = sizeof(tic_ram) + sizeof(tic_core_state_data);

	if (state && state->tic) {
		tic_core* core = (tic_core*)state->tic;
		const tic_script* config = core->currentScript;
		if (config == NULL) config = tic_get_script(&core->memory);

		// Only support Lua for now
		if (config && strcmp(config->name, "lua") == 0 && core->currentVM) {
			serialize_lua(core); // Populate SerializedLuaData and SerializedLuaSize
			size += sizeof(u32) + SerializedLuaSize; // Size header + data
		}
	}

	return size + sizeof(retro_usec_t);
}

/**
 * libretro callback; Get the current persistent memory.
 */
RETRO_API bool retro_serialize(void *data, size_t size)
{
	if (state == NULL || state->tic == NULL || data == NULL) {
		return false;
	}

	tic_core* core = (tic_core*)state->tic;

	const tic_script* config = core->currentScript;
	if (config == NULL) config = tic_get_script(&core->memory);

	if (config == NULL || strcmp(config->name, "lua") != 0) {
		if (environ_cb) {
			const char *txt = "Savestate currently not supported in non-Lua TIC-80 cartridges";

			/* Attempt to use the modern 'Widget' notification style */
			struct retro_message_ext msg_ext = {
				.msg      = txt,
				.duration = 3000,                      /* 3 seconds (milliseconds) */
				.priority = 3,                         /* High priority triggers the widget */
				.level    = RETRO_LOG_INFO,            /* Also sends to logs */
				.target   = RETRO_MESSAGE_TARGET_ALL,  /* Show on OSD and logs */
				.type     = RETRO_MESSAGE_TYPE_NOTIFICATION
			};

			/* If the frontend doesn't support _EXT (returns false), use the old fallback */
			if (!environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE_EXT, &msg_ext)) {
				struct retro_message msg = { txt, 180 }; /* 180 frames (~3s at 60fps) */
				environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
			}
		}
		return false;
	}

	u8* dst = (u8*)data;

	// Check if the provided buffer is large enough for the base state
	if (size < sizeof(tic_ram) + sizeof(tic_core_state_data))
		return false;

	memcpy(dst, core->memory.ram, sizeof(tic_ram));
	dst += sizeof(tic_ram);

	memcpy(dst, &core->state, sizeof(tic_core_state_data));
	dst += sizeof(tic_core_state_data);

	// Append Lua state if available and fits
	if (SerializedLuaSize > 0 && SerializedLuaData) {
		if ((dst - (u8*)data) + sizeof(u32) + SerializedLuaSize <= size) {
			*(u32*)dst = (u32)SerializedLuaSize;
			dst += sizeof(u32);
			memcpy(dst, SerializedLuaData, SerializedLuaSize);
			dst += SerializedLuaSize;
		} else {
			// Not enough space for Lua data, but header might fit
			if ((dst - (u8*)data) + sizeof(u32) <= size) {
				*(u32*)dst = 0; // Indicate no Lua data saved
				dst += sizeof(u32);
			}
		}
	} else {
		// If we allocated space for Lua in retro_serialize_size but no data was generated (e.g., not Lua core)
		// or if serialization failed, write 0 size for Lua data.
		// This ensures retro_unserialize can correctly skip.
		if ((dst - (u8*)data) + sizeof(u32) <= size) {
			*(u32*)dst = 0;
			dst += sizeof(u32);
		}
	}

	// Save frame time
	if ((dst - (u8*)data) + sizeof(retro_usec_t) <= size) {
		memcpy(dst, &state->frameTime, sizeof(retro_usec_t));
	}

	// Free the cached Lua data after serialization
	free_serialized_lua();

	return true;
}

/**
 * libretro callback; Given the serialized data, load it into the persistent memory.
 */
RETRO_API bool retro_unserialize(const void *data, size_t size)
{
	// Invalidate the cache when loading a state
	free_serialized_lua();

	if (state == NULL || state->tic == NULL || size < sizeof(tic_ram) + sizeof(tic_core_state_data) || data == NULL) {
		return false;
	}

	tic_core* core = (tic_core*)state->tic;
	const u8* src = (const u8*)data;

	memcpy(core->memory.ram, src, sizeof(tic_ram));
	src += sizeof(tic_ram);

	memcpy(&core->state, src, sizeof(tic_core_state_data));
	src += sizeof(tic_core_state_data);

	// Fix pointers that were invalidated by the restore
	for (s32 i = 0; i < TIC_SOUND_CHANNELS; i++)
	{
		core->state.sfx.channels[i].pos = &core->memory.ram->sfxpos[i];
		core->state.music.channels[i].pos = &core->state.music.sfxpos[i];
		core->state.music.commands[i].delay.row = NULL;
	}

	const tic_script* config = core->currentScript;
	if (config == NULL) config = tic_get_script(&core->memory);

	if (config)
	{
		core->state.tick = config->tick;
		core->state.callback = config->callback;

		// Restore Lua state if present and correct VM
		if (strcmp(config->name, "lua") == 0 && core->currentVM) {
			u32 luaSize = 0;
			// Check if there's enough space to read the Lua size header
			if ((src - (const u8*)data) + sizeof(u32) <= size) {
				luaSize = *(const u32*)src;
				src += sizeof(u32);

				// If Lua data exists and fits within the remaining buffer
				if (luaSize > 0 && (src - (const u8*)data) + luaSize <= size) {
					lua_State* lua = core->currentVM;

					// Important for Ghost game (1777). (overwritten type function)
					// Inject __builtin_type helper for the merge script
					lua_pushcfunction(lua, internal_lua_type);
					lua_setglobal(lua, "__builtin_type");

					// Push "return " string
					lua_pushstring(lua, "return ");
					// Push the serialized Lua table string
					lua_pushlstring(lua, (const char*)src, luaSize);
					// Concatenate them: "return { ... }"
					lua_concat(lua, 2);

					// Load the concatenated string as a Lua chunk
					if (luaL_loadstring(lua, lua_tostring(lua, -1)) == LUA_OK) {
						// Execute the chunk, which should return the serialized table
						if (lua_pcall(lua, 0, 1, 0) == LUA_OK) {
							// S is at -1

							const char* merge_script = 
								"local type = __builtin_type\n" // Important for Ghost (overwritten type function)
								"_G.__builtin_type = nil\n"
								"local next = next\n"
								"local pairs = pairs\n"
								"if type(pairs) ~= 'function' then pairs = function(t) return next, t, nil end end\n" // Important for OddSocks (pairs is overwritten)
								"local dbg = package.loaded.debug\n" // Important for Bone Knight (access hidden debug lib)
								"local S = ...\n"
								"local g = S\n"
								"local u = nil\n"
								"local j = nil\n"
								"if type(S) == 'table' and S.g then\n"
								"    g = S.g\n"
								"    u = S.u\n"
								"    j = S.j\n"
								"end\n"
								// Helper to resolve upvalue path
								"local function resolve(path)\n"
								"    local f = _G[path[1]]\n"
								"    if not f then return nil end\n"
								"    for i = 2, #path - 1 do\n"
								"        local n, v = dbg.getupvalue(f, path[i])\n"
								"        if type(v) == 'function' then f = v else return nil end\n"
								"    end\n"
								"    return f, path[#path]\n"
								"end\n"
								"local seen = {}\n"
								"local STDLIB = {\n"
								"  __STDLIB_MATH__ = math, __STDLIB_TABLE__ = table, __STDLIB_STRING__ = string,\n"
								"  __STDLIB_COROUTINE__ = coroutine, __STDLIB_PACKAGE__ = package, __STDLIB_IO__ = io,\n"
								"  __STDLIB_OS__ = os, __STDLIB_UTF8__ = utf8, __STDLIB_DEBUG__ = debug\n"
								"}\n"
								"local function has_methods(t)\n"
									 // Important for MadPhysicist: Check if table has function fields (methods)
								"    if type(t) ~= 'table' then return false end\n"
								"    for k,v in pairs(t) do\n"
								"        if type(v) == 'function' then return true end\n"
								"    end\n"
								"    return false\n"
								"end\n"
								"local function is_array(t)\n"
								"    if type(t) ~= 'table' then return false end\n"
								"    local i = 0 for _ in pairs(t) do i = i + 1 end\n"
								"    return #t == i\n"
								"end\n"
								"local function update(d, s)\n"
								"    if seen[d] then return end\n"
								"    seen[d] = true\n"
								     // Pass 1: Copy/Merge from Save(s) to Dest(d)
								"    for k,v in pairs(s) do\n"
								"        if type(v) == 'string' and STDLIB[v] then\n"
								"            d[k] = STDLIB[v]\n"
								"        elseif is_array(v) then\n"
								"            if type(d[k]) ~= 'table' then d[k] = {} end\n"
								"            for i = #v + 1, #d[k] do d[k][i] = nil end\n"
								"            for i = 1, #v do\n"
								"                if type(v[i]) == 'table' then\n"
								                     // Important for 8bitpanda: Replace table if entity ID (eid) differs or target is not a table
								                     // This prevents merging incompatible entity data and fixes the "nil comparison" crash
								"                    if type(d[k][i]) == 'table' and d[k][i].eid == v[i].eid then\n"
								"                        update(d[k][i], v[i])\n"
								"                    else\n"
								"                        d[k][i] = v[i]\n"
								"                    end\n"
								"                else\n"
								                     // Important for MadPhysicist: Don't overwrite objects that have methods with nil/non-table values
								"                    if not has_methods(d[k][i]) then\n"
								"                        d[k][i] = v[i]\n"
								"                    end\n"
								"                end\n"
								"            end\n"
								"        elseif type(v) == 'table' then\n"
								             // Important for 8bitpanda: Use identity check for global tables too
								"            if type(d[k]) == 'table' and d[k].eid == v.eid then\n"
								"                update(d[k], v)\n"
								"            else\n"
								"                d[k] = v\n"
								"            end\n"
								"        else\n"
								             // Primitive value: overwrite unless d[k] is an object with methods
								"            if not has_methods(d[k]) then\n"
								"                d[k] = v\n"
								"            end\n"
								"        end\n"
								"    end\n"
								     // Pass 2: Cleanup Dest(d) - Remove keys not in Save(s)
								     // Important for Robot Pilfer: Removes dead entities/flags
								"    for k,v in pairs(d) do\n"
								"        if s[k] == nil then\n"
								             // Important for Searching for Pixel: Keep methods and system globals
								             // Protect standard libraries (table, math, etc.) to prevent 'attempt to index nil' on next save
								"            if type(v) ~= 'function' and k ~= '_G' and k ~= 'package' and\n"
								"               k ~= 'table' and k ~= 'math' and k ~= 'string' and k ~= 'coroutine' and\n"
								"               k ~= 'io' and k ~= 'os' and k ~= 'utf8' and k ~= 'debug' then\n"
								"                d[k] = nil\n"
								"            end\n"
								"        end\n"
								"    end\n"
								"end\n"
								"update(_G, g)\n"
								// Important for Bone Knight: Restore upvalues (local variables)
								"if u and dbg then\n"
								"    for _,x in ipairs(u) do\n"
								"        local f, i = resolve(x.p)\n"
								"        if f then\n"
								"            local n, curr = dbg.getupvalue(f, i)\n"
								             // Use update() for tables to preserve methods on locals (Searching for Pixel)
								"            if type(curr) == 'table' and type(x.v) == 'table' then\n"
								"                update(curr, x.v)\n"
								"            else\n"
								"                dbg.setupvalue(f, i, x.v)\n"
								"            end\n"
								"        end\n"
								"    end\n"
								"end\n"
								// Restore shared upvalues links
								"if j and dbg then\n"
								"    for _,x in ipairs(j) do\n"
								"        local f1, i1 = resolve(x.dest)\n"
								"        local f2, i2 = resolve(x.src)\n"
								"        if f1 and f2 then dbg.upvaluejoin(f1, i1, f2, i2) end\n"
								"    end\n"
								"end\n";

							if (luaL_loadstring(lua, merge_script) == LUA_OK) {
								lua_pushvalue(lua, -2); // Push the table S (currently at -2)
								if (lua_pcall(lua, 1, 0, 0) != LUA_OK) {
									log_cb(RETRO_LOG_ERROR, "[TIC-80] Lua merge script error: %s\n", lua_tostring(lua, -1));
									lua_pop(lua, 1); // pop error
								}
							} else {
								log_cb(RETRO_LOG_ERROR, "[TIC-80] Lua merge compile error: %s\n", lua_tostring(lua, -1));
								lua_pop(lua, 1); // pop error
							}
						} else {
							log_cb(RETRO_LOG_ERROR, "[TIC-80] Lua state unserialize error: %s\n", lua_tostring(lua, -1));
						}
					} else {
						log_cb(RETRO_LOG_ERROR, "[TIC-80] Lua state loadstring error: %s\n", lua_tostring(lua, -1));
					}
					lua_pop(lua, 1); // Pop the result of the chunk (table S or error message)
					lua_pop(lua, 1); // Pop the concatenated "return { ... }" string

					src += luaSize; // Advance source pointer past Lua data
				}
			}
		}
	}

	// Restore frame time
	if ((src - (const u8*)data) + sizeof(retro_usec_t) <= size) {
		memcpy(&state->frameTime, src, sizeof(retro_usec_t));
	}

	return true;
}

/**
 * libretro callback; Gets region of memory.
 *
 * https://github.com/nesbox/TIC-80/wiki/ram
 */
RETRO_API void *retro_get_memory_data(unsigned id)
{
	if (state == NULL || state->tic == NULL) {
		return NULL;
	}

	tic_mem* tic = (tic_mem*)state->tic;
	switch (id) {
		case RETRO_MEMORY_SAVE_RAM:
			return tic->ram->persistent.data;
		case RETRO_MEMORY_SYSTEM_RAM:
			return tic->ram->data;
		case RETRO_MEMORY_VIDEO_RAM:
			return tic->ram->vram.data;
		default:
			return NULL;
	}
}

/**
 * libretro callback; Gets the size of the given memory slot.
 */
RETRO_API size_t retro_get_memory_size(unsigned id)
{
    if (state == NULL || state->tic == NULL) {
        return 0;
    }

    tic_mem* tic = (tic_mem*)state->tic;
    switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            return sizeof(tic->ram->persistent.data);
        case RETRO_MEMORY_SYSTEM_RAM:
            return sizeof(tic->ram->data);
        case RETRO_MEMORY_VIDEO_RAM:
            return sizeof(tic->ram->vram.data);
        default:
            return 0;
    }
}

/**
 * libretro callback; Reset all cheats to disabled.
 */
RETRO_API void retro_cheat_reset(void)
{
	// Nothing.
}

/**
 * libretro callback; Enable/disable the given cheat code.
 *
 * TIC-80 codes expect a space-seperated list of paired
 * integers. The first number is the index in pmem(), the
 * second value is the value for the pmem() call. Example:
 *
 * ```
 * cheats = 1
 *
 * cheat0_desc = "3 Lives"
 * cheat0_code = "255 3"
 * cheat0_enable = false
 * ```
 *
 * The above cheat would be the same as calling:
 *
 * pmem(255, 3)
 *
 * @see https://github.com/nesbox/TIC-80/wiki/pmem
 */
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
	TIC_UNUSED(index);
	TIC_UNUSED(enabled);
	if (!state || !state->tic) {
		return;
	}

	u32 codes[TIC_PERSISTENT_SIZE];
	u32 codeIndex = 0;
	char *str = (char*)code;
	char *end = str;

	// Split the code by spaces, to get an array of integers.
	while (*end && codeIndex < TIC_PERSISTENT_SIZE) {
		codes[codeIndex++] = strtol(str, &end, 10);
		while (*end == ' ') {
			end++;
		}
		str = end;
	}

	// Finally, set each given code pair.
	tic_mem* tic = (tic_mem*)state->tic;
	for (u32 i = 0; i < codeIndex; i = i + 2) {
		tic->ram->persistent.data[codes[i]] = codes[i+1];
	}
}
