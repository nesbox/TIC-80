#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <tic.h>
#include "libretro-common/include/libretro.h"
#include "libretro_core_options.h"
#include "../../ticapi.h"

/**
 * system.h is used for TIC80_OFFSET_LEFT and TIC80_OFFSET_TOP
 */
#include "../../system.h"

// The maximum amount of inputs (2, 3 or 4)
#define TIC_MAXPLAYERS 4

// How long to wait before hiding the mouse.
#define TIC_LIBRETRO_MOUSE_HIDE_TIMER_START 300

static uint32_t *frame_buf = NULL;
static struct retro_log_callback logging;
static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
tic80* tic;
static struct
{
	bool quit;
	tic80_input input;
	int keymap[RETROK_LAST];
	bool variablePointerApi;
	u8 mouseCursor;
	u16 mousePreviousX;
	u16 mousePreviousY;
	u16 mouseHideTimer;
} state =
{
	.quit = false,
	.variablePointerApi = false,
	.mouseCursor = 0,
	.mousePreviousX = 0,
	.mousePreviousY = 0,
	.mouseHideTimer = TIC_LIBRETRO_MOUSE_HIDE_TIMER_START
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
static void tic80_libretro_error(const char* info)
{
	log_cb(RETRO_LOG_ERROR, info);
}

/**
 * TIC-80 callback; Report a trace log from the TIC-80 cart.
 */
static void tic80_libretro_trace(const char* text, u8 color)
{
	log_cb(RETRO_LOG_INFO, text);
}

/**
 * libretro callback; Handles the logging internally when the logging isn't set.
 */
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
	frame_buf = calloc(TIC80_FULLWIDTH * TIC80_FULLHEIGHT, sizeof(uint32_t));

	// Initialize the keyboard mappings.
	state.keymap[RETROK_UNKNOWN] = tic_key_unknown;
	state.keymap[RETROK_a] = tic_key_a;
	state.keymap[RETROK_b] = tic_key_b;
	state.keymap[RETROK_c] = tic_key_c;
	state.keymap[RETROK_d] = tic_key_d;
	state.keymap[RETROK_e] = tic_key_e;
	state.keymap[RETROK_f] = tic_key_f;
	state.keymap[RETROK_g] = tic_key_g;
	state.keymap[RETROK_h] = tic_key_h;
	state.keymap[RETROK_i] = tic_key_i;
	state.keymap[RETROK_j] = tic_key_j;
	state.keymap[RETROK_k] = tic_key_k;
	state.keymap[RETROK_l] = tic_key_l;
	state.keymap[RETROK_m] = tic_key_m;
	state.keymap[RETROK_n] = tic_key_n;
	state.keymap[RETROK_o] = tic_key_o;
	state.keymap[RETROK_p] = tic_key_p;
	state.keymap[RETROK_q] = tic_key_q;
	state.keymap[RETROK_r] = tic_key_r;
	state.keymap[RETROK_s] = tic_key_s;
	state.keymap[RETROK_t] = tic_key_t;
	state.keymap[RETROK_u] = tic_key_u;
	state.keymap[RETROK_v] = tic_key_v;
	state.keymap[RETROK_w] = tic_key_w;
	state.keymap[RETROK_x] = tic_key_x;
	state.keymap[RETROK_y] = tic_key_y;
	state.keymap[RETROK_z] = tic_key_z;
	state.keymap[RETROK_0] = tic_key_0;
	state.keymap[RETROK_1] = tic_key_1;
	state.keymap[RETROK_2] = tic_key_2;
	state.keymap[RETROK_3] = tic_key_3;
	state.keymap[RETROK_4] = tic_key_4;
	state.keymap[RETROK_5] = tic_key_5;
	state.keymap[RETROK_6] = tic_key_6;
	state.keymap[RETROK_7] = tic_key_7;
	state.keymap[RETROK_8] = tic_key_8;
	state.keymap[RETROK_9] = tic_key_9;
	state.keymap[RETROK_KP0] = tic_key_0;
	state.keymap[RETROK_KP1] = tic_key_1;
	state.keymap[RETROK_KP2] = tic_key_2;
	state.keymap[RETROK_KP3] = tic_key_3;
	state.keymap[RETROK_KP4] = tic_key_4;
	state.keymap[RETROK_KP5] = tic_key_5;
	state.keymap[RETROK_KP6] = tic_key_6;
	state.keymap[RETROK_KP7] = tic_key_7;
	state.keymap[RETROK_KP8] = tic_key_8;
	state.keymap[RETROK_KP9] = tic_key_9;
	state.keymap[RETROK_MINUS] = tic_key_minus;
	state.keymap[RETROK_EQUALS] = tic_key_equals;
	state.keymap[RETROK_LEFTBRACKET] = tic_key_leftbracket;
	state.keymap[RETROK_RIGHTBRACKET] = tic_key_rightbracket;
	state.keymap[RETROK_BACKSLASH] = tic_key_backslash;
	state.keymap[RETROK_SEMICOLON] = tic_key_semicolon;
	state.keymap[RETROK_QUOTE] = tic_key_apostrophe;
	state.keymap[RETROK_TILDE] = tic_key_grave;
	state.keymap[RETROK_COMMA] = tic_key_comma;
	state.keymap[RETROK_PERIOD] = tic_key_period;
	state.keymap[RETROK_SLASH] = tic_key_slash;
	state.keymap[RETROK_SPACE] = tic_key_space;
	state.keymap[RETROK_TAB] = tic_key_tab;
	state.keymap[RETROK_RETURN] = tic_key_return;
	state.keymap[RETROK_BACKSPACE] = tic_key_backspace;
	state.keymap[RETROK_DELETE] = tic_key_delete;
	state.keymap[RETROK_INSERT] = tic_key_insert;
	state.keymap[RETROK_PAGEUP] = tic_key_pageup;
	state.keymap[RETROK_PAGEDOWN] = tic_key_pagedown;
	state.keymap[RETROK_HOME] = tic_key_home;
	state.keymap[RETROK_END] = tic_key_end;
	state.keymap[RETROK_UP] = tic_key_up;
	state.keymap[RETROK_DOWN] = tic_key_down;
	state.keymap[RETROK_LEFT] = tic_key_left;
	state.keymap[RETROK_RIGHT] = tic_key_right;
	state.keymap[RETROK_CAPSLOCK] = tic_key_capslock;
	state.keymap[RETROK_LCTRL] = tic_key_ctrl;
	state.keymap[RETROK_RCTRL] = tic_key_ctrl;
	state.keymap[RETROK_LSHIFT] = tic_key_shift;
	state.keymap[RETROK_RSHIFT] = tic_key_shift;
	state.keymap[RETROK_LALT] = tic_key_alt;
	state.keymap[RETROK_RALT] = tic_key_alt;
	state.keymap[RETROK_ESCAPE] = tic_key_escape;
	state.keymap[RETROK_F1] = tic_key_f1;
	state.keymap[RETROK_F2] = tic_key_f2;
	state.keymap[RETROK_F3] = tic_key_f3;
	state.keymap[RETROK_F4] = tic_key_f4;
	state.keymap[RETROK_F5] = tic_key_f5;
	state.keymap[RETROK_F6] = tic_key_f6;
	state.keymap[RETROK_F7] = tic_key_f7;
	state.keymap[RETROK_F8] = tic_key_f8;
	state.keymap[RETROK_F9] = tic_key_f9;
	state.keymap[RETROK_F10] = tic_key_f10;
	state.keymap[RETROK_F11] = tic_key_f11;
	state.keymap[RETROK_F12] = tic_key_f12;
	state.keymap[RETROK_F12] = tic_key_f12;
}

/**
 * libretro callback; Global deinitialization.
 */
void retro_deinit(void)
{
	// Clear out the frame buffer if it exists.
	if (frame_buf != NULL) {
		free(frame_buf);
		frame_buf = NULL;
	}
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
	info->valid_extensions = "tic";
	info->need_fullpath    = false;
	info->block_extract    = false;
}

/**
 * libretro callback; Get information about the desired audio and video.
 */
void retro_get_system_av_info(struct retro_system_av_info *info)
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

	// Configure the core settings.
	libretro_set_core_options(environ_cb);
}

/**
 * libretro callback; Set up the audio sample callback.
 */
void retro_set_audio_sample(retro_audio_sample_t cb)
{
	audio_cb = cb;
}

/**
 * libretro callback; Set up the audio sample batch callback.
 *
 * @see tic80_libretro_audio()
 */
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}

/**
 * libretro callback; Set up the input poll callback.
 */
void retro_set_input_poll(retro_input_poll_t cb)
{
	input_poll_cb = cb;
}

/**
 * libretro callback; Set up the input state callback.
 */
void retro_set_input_state(retro_input_state_t cb)
{
	input_state_cb = cb;
}

/**
 * libretro callback; Set up the video refresh callback.
 */
void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

/**
 * libretro callback; Reset the game.
 */
void retro_reset(void)
{
	if (tic) {
		tic80_local* tic80 = (tic80_local*)tic;
		tic_api_reset(tic80->memory);
	}
}

/**
 * libretro callback; Load the labels for the input buttons.
 *
 * @see tic80_libretro_update()
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
 * Converts a Pointer API coordinates to screen pixel position.
 *
 * @see tic80_libretro_update_mouse()
 * @see RETRO_DEVICE_POINTER
 */
static int tic80_libretro_mouse_pointer_convert(float coord, float full, int padding)
{
	float max = 0x7fff;
	return ((coord + max) / (max * 2.0f) * full) - padding;
}

/**
 * Retrieve gamepad information from libretro.
 */
static void tic80_libretro_update_mouse(tic80_mouse* mouse)
{
	// Check if we are to use the Mouse API or Pointer API.
	if (!state.variablePointerApi) {
		// Get the Mouse X and Y, which is the relative positioning from last tick.
		mouse->x += input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
		mouse->y += input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

		// Mouse buttons.
		mouse->left = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
		mouse->right = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT);
		mouse->middle = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE);
	}
	else {
		// Get the Pointer X and Y, and convert it to screen position.
		mouse->x = tic80_libretro_mouse_pointer_convert(
			input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X),
			TIC80_FULLWIDTH,
			TIC80_OFFSET_LEFT);
		mouse->y = tic80_libretro_mouse_pointer_convert(
			input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y),
			TIC80_FULLHEIGHT,
			TIC80_OFFSET_TOP);

		// Pointer pressed is considered mouse left button.
		mouse->left = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
	}

	// Keep the mouse on the screen.
	if (mouse->x > TIC80_WIDTH) {
		mouse->x = TIC80_WIDTH;
	}
	if (mouse->y > TIC80_HEIGHT) {
		mouse->y = TIC80_HEIGHT;
	}
	if (mouse->x < 0) {
		mouse->x = 0;
	}
	if (mouse->y < 0) {
		mouse->y = 0;
	}

	// Have the mouse disappear after a certain time of inactivity.
	if (mouse->x != state.mousePreviousX || mouse->y != state.mousePreviousY) {
		state.mouseHideTimer = TIC_LIBRETRO_MOUSE_HIDE_TIMER_START;
		state.mousePreviousX = mouse->x;
		state.mousePreviousY = mouse->y;
	}
	if (state.mouseHideTimer > 0) {
		state.mouseHideTimer--;
	}

	// Mouse Scroll Wheels
	mouse->scrollx = mouse->scrolly = 0;
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

/**
 * Draws a software cursor on the screen where the mouse is.
 */
static void tic80_libretro_mousecursor(tic80_local* game, tic80_mouse* mouse, int cursortype)
{
	// Only draw the mouse cursor if it's active.
	if (state.mouseHideTimer <= 0) {
		return;
	}

	// Determine which cursor to draw.
	switch (state.mouseCursor) {
		case 1: // Dot
			tic_api_pix(game->memory, state.input.mouse.x, state.input.mouse.y, 15, false);
		break;
		case 2: // Cursor
			tic_api_line(game->memory, state.input.mouse.x - 4, state.input.mouse.y, state.input.mouse.x - 2, state.input.mouse.y, 15);
			tic_api_line(game->memory, state.input.mouse.x + 2, state.input.mouse.y, state.input.mouse.x + 4, state.input.mouse.y, 15);
			tic_api_line(game->memory, state.input.mouse.x, state.input.mouse.y - 4, state.input.mouse.x, state.input.mouse.y - 2, 15);
			tic_api_line(game->memory, state.input.mouse.x, state.input.mouse.y + 2, state.input.mouse.x, state.input.mouse.y + 4, 15);
		break;
		case 3: // Arrow
			tic_api_tri(game->memory, state.input.mouse.x, state.input.mouse.y, state.input.mouse.x + 3, state.input.mouse.y, state.input.mouse.x, state.input.mouse.y + 3, 15);
			tic_api_line(game->memory, state.input.mouse.x + 3, state.input.mouse.y, state.input.mouse.x, state.input.mouse.y + 3, 0);
		break;
	}
}

/**
 * Retrieve keyboard information from libretro.
 */
static void tic80_libretro_update_keyboard(tic80_keyboard* keyboard)
{
	// Clear the key buffer.
	for (int i = 0; i < TIC80_KEY_BUFFER; i++) {
		keyboard->keys[i] = tic_key_unknown;
	}

	// Load up the active keys into the buffer.
	for (int key = RETROK_FIRST, keyBuffer = 0; key < RETROK_LAST && keyBuffer < TIC80_KEY_BUFFER; key++) {
		if (input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, key)) {
			keyboard->keys[keyBuffer++] = state.keymap[key];
		}
	}
}

/**
 * Update the input state, and tick the game.
 */
static void tic80_libretro_update(tic80* game)
{
	// Let libretro know that we need updated input states.
	input_poll_cb();

	// Gamepads
	tic80_libretro_update_gamepad(&state.input.gamepads.first, 0);
	tic80_libretro_update_gamepad(&state.input.gamepads.second, 1);

#if TIC_MAXPLAYERS >= 3
	tic80_libretro_update_gamepad(&state.input.gamepads.third, 2);
#endif
#if TIC_MAXPLAYERS >= 4
	tic80_libretro_update_gamepad(&state.input.gamepads.fourth, 3);
#endif

	// Mouse
	tic80_libretro_update_mouse(&state.input.mouse);

	// Keyboard
	tic80_libretro_update_keyboard(&state.input.keyboard);

	// Update the game state.
	tic80_tick(game, state.input);
}

/**
 * Convert between argb8888 and abgr8888.
 *
 * This was copied from libretro-common's scaler.c:
 * https://github.com/libretro/libretro-common/blob/master/gfx/scaler/scaler.c
 *
 * @see tic80_libretro_draw()
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
static void tic80_libretro_draw(tic80* game)
{
	// Mouse Cursor
	tic80_libretro_mousecursor((tic80_local*)game, &state.input.mouse, state.mouseCursor);

	// TIC-80 uses ABGR8888, so we need to convert it.
	tic80_libretro_conv_argb8888_abgr8888(frame_buf, game->screen,
		TIC80_FULLWIDTH, TIC80_FULLHEIGHT,
		TIC80_FULLWIDTH << 2, TIC80_FULLWIDTH << 2);

	// Render to the screen.
	video_cb(frame_buf, TIC80_FULLWIDTH, TIC80_FULLHEIGHT, TIC80_FULLWIDTH << 2);
}

/**
 * Play the TIC-80 audio.
 *
 * @see retro_run()
 */
void tic80_libretro_audio(tic80* game)
{
	// Tell libretro about the samples.
	audio_batch_cb(game->sound.samples, game->sound.count / TIC_STEREO_CHANNELS);
}

/**
 * Update the state of the core variables.
 *
 * @see retro_run()
 */
static void tic80_libretro_variables(void)
{
	// Check all the individual variables for the core.
	struct retro_variable var;

	// Mouse is Pointer device.
	state.variablePointerApi = false;
	var.key = "tic80_mouse_pointer";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		state.variablePointerApi = strcmp(var.value, "enabled") == 0;
	}

	// Mouse Cursor
	state.mouseCursor = 0;
	var.key = "tic80_mouse_cursor";
	var.value = NULL;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
		if (strcmp(var.value, "dot") == 0) {
			state.mouseCursor = 1;
		}
		else if (strcmp(var.value, "cross") == 0) {
			state.mouseCursor = 2;
		}
		else if (strcmp(var.value, "arrow") == 0) {
			state.mouseCursor = 3;
		}
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
			retro_unload_game();
			environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
		}
		return;
	}

	// Ensure we act only when a tic instance is available.
	if (!tic) {
		return;
	}

	// Update the TIC-80 environment.
	tic80_libretro_update(tic);

	// Render the screen.
	tic80_libretro_draw(tic);

	// Play the audio.
	tic80_libretro_audio(tic);

	// Update core options, if needed.
	bool updated = false;
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
		tic80_libretro_variables();
	}
}

/**
 * libretro callback; Load a game.
 */
bool retro_load_game(const struct retro_game_info *info)
{
	// TODO: Warn that Audio Synchronization required to run at a proper speed.
	// TODO: Warn that the core doesn't support Runahead.

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

	// Ensure content data is available.
	if (info->data == NULL) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] No content data provided.\n");
		return false;
	}

	// Ensure a game is not loaded before loading.
	retro_unload_game();

	// Set up the TIC-80 environment.
	tic = tic80_create(TIC80_SAMPLERATE);
	tic->callback.exit = tic80_libretro_exit;
	tic->callback.error = tic80_libretro_error;
	tic->callback.trace = tic80_libretro_trace;

	// Initialize some of the game state.
	state.quit = false;
	state.input.mouse.x = 0;
	state.input.mouse.y = 0;

	// Load up any core variables.
	tic80_libretro_variables();

	// Load the content.
	tic80_load(tic, (void*)(info->data), info->size);
	if (!tic) {
		log_cb(RETRO_LOG_ERROR, "[TIC-80] Content loaded, but failed to load game.\n");
		return false;
	}

	return true;
}

/**
 * libretro callback; Tells the core to unload the game.
 */
void retro_unload_game(void)
{
	// Close the game if it's loaded.
	state.quit = true;
	if (tic != NULL) {
		tic80_delete(tic);
		tic = NULL;
	}
}

/**
 * libretro callback; Retrieves the region for the content.
 */
unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

/**
 * libretro callback; Load a game using a subsystem.
 */
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
	// Forward subsystem requests over to retro_load_game().
	return retro_load_game(info);
}

/**
 * libretro callback; Retrieve the size of the serialized memory.
 */
size_t retro_serialize_size(void)
{
	return TIC_PERSISTENT_SIZE * sizeof(u32);
}

/**
 * libretro callback; Get the current persistent memory.
 */
bool retro_serialize(void *data, size_t size)
{
	if (!tic || !data) {
		return false;
	}

	tic80_local* tic80 = (tic80_local*)tic;
	u32* udata = (u32*)data;
	for (int i = 0; i < TIC_PERSISTENT_SIZE; i++) {
		udata[i] = tic80->memory->ram.persistent.data[i];
	}
	tic80->tickData.syncPMEM = true;

	return true;
}

/**
 * libretro callback; Given the serialized data, load it into the persistent memory.
 */
bool retro_unserialize(const void *data, size_t size)
{
	if (!tic || size != retro_serialize_size() || !data) {
		return false;
	}

	tic80_local* tic80 = (tic80_local*)tic;
	u32* uData = (u32*)data;
	for (int i = 0; i < TIC_PERSISTENT_SIZE; i++) {
		tic80->memory->ram.persistent.data[i] = uData[i];
	}
	tic80->tickData.syncPMEM = true;

	return true;
}

/**
 * libretro callback; Gets region of memory. Used for achievement tracking.
 */
void *retro_get_memory_data(unsigned id)
{
	if (!tic || id >= TIC_PERSISTENT_SIZE) {
		return NULL;
	}

	tic80_local* tic80 = (tic80_local*)tic;
	return &(tic80->memory->ram.persistent.data[id]);
}

/**
 * libretro callback; Gets the size of memory. Used for achievement tracking.
 */
size_t retro_get_memory_size(unsigned id)
{
	return sizeof(u32);
}

/**
 * libretro callback; Reset all cheats to disabled.
 */
void retro_cheat_reset(void)
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
 * cheats = 1
 *
 * cheat0_desc = "3 Lives"
 * cheat0_code = "255 3"
 * cheat0_enable = false
 *
 * The above cheat would be the same as calling:
 *
 * pmem(255, 3)
 *
 * @see https://github.com/nesbox/TIC-80/wiki/pmem
 */
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
	u32 codes[TIC_PERSISTENT_SIZE];
	int codeIndex = 0;
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
	tic80_local* tic80 = (tic80_local*)tic;
	for (int i = 0; i < codeIndex; i = i + 2) {
		tic80->memory->ram.persistent.data[codes[i]] = codes[i+1];
		tic80->tickData.syncPMEM = true;
	}
}
