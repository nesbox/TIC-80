#include "system.h"
#include "net.h"
#include "tools.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <SDL_gpu.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#define STUDIO_PIXEL_FORMAT GPU_FORMAT_RGBA
#define TEXTURE_SIZE (TIC80_FULLWIDTH)
#define OFFSET_LEFT ((TIC80_FULLWIDTH-TIC80_WIDTH)/2)
#define OFFSET_TOP ((TIC80_FULLHEIGHT-TIC80_HEIGHT)/2)

#define KBD_COLS 22
#define KBD_ROWS 17

#if defined(__TIC_WINRT__) || defined(__TIC_WINDOWS__)
#include <windows.h>
#endif

static struct
{
	Studio* studio;

	SDL_Window* window;

	struct
	{
		GPU_Target* screen;
		GPU_Image* texture;
		u32 shader;
		GPU_ShaderBlock block;
	} gpu;

	struct
	{
		SDL_Joystick* ports[TIC_GAMEPADS];
		GPU_Image* texture;

		tic80_gamepads touch;
		tic80_gamepads joystick;

		struct
		{
			s32 size;
			SDL_Point axis;
			SDL_Point a;
			SDL_Point b;
			SDL_Point x;
			SDL_Point y;
		} part;
	} gamepad;

	struct
	{
		struct
		{
			GPU_Image* up;
			GPU_Image* down;
		} texture;

		bool state[tic_keys_count];

		struct 
		{
			bool state[tic_keys_count];
		} touch;

	} keyboard;

	u32 touchCounter;

	struct
	{
		GPU_Image* texture;
		const u8* src;
	} mouse;

	Net* net;

	bool missedFrame;

	struct
	{
		SDL_AudioSpec 		spec;
		SDL_AudioDeviceID 	device;
		SDL_AudioCVT 		cvt;
	} audio;
} platform;

static inline bool crtMonitorEnabled()
{
	return platform.studio->config()->crtMonitor && platform.gpu.shader;
}

static void initSound()
{
	SDL_AudioSpec want =
	{
		.freq = 44100,
		.format = AUDIO_S16,
		.channels = TIC_STEREO_CHANNLES,
		.userdata = NULL,
	};

	platform.audio.device = SDL_OpenAudioDevice(NULL, 0, &want, &platform.audio.spec, SDL_AUDIO_ALLOW_ANY_CHANGE);

	SDL_BuildAudioCVT(&platform.audio.cvt, want.format, want.channels, platform.audio.spec.freq, platform.audio.spec.format, platform.audio.spec.channels, platform.audio.spec.freq);

	if(platform.audio.cvt.needed)
	{
		platform.audio.cvt.len = platform.audio.spec.freq * platform.audio.spec.channels * sizeof(s16) / TIC_FRAMERATE;
		platform.audio.cvt.buf = SDL_malloc(platform.audio.cvt.len * platform.audio.cvt.len_mult);
	}
}

static const u8* getSpritePtr(const tic_tile* tiles, s32 x, s32 y)
{
	enum { SheetCols = (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE) };
	return tiles[x / TIC_SPRITESIZE + y / TIC_SPRITESIZE * SheetCols].data;
}

static u8 getSpritePixel(const tic_tile* tiles, s32 x, s32 y)
{
	return tic_tool_peek4(getSpritePtr(tiles, x, y), (x % TIC_SPRITESIZE) + (y % TIC_SPRITESIZE) * TIC_SPRITESIZE);
}

static void setWindowIcon()
{
	enum{ Size = 64, TileSize = 16, ColorKey = 14, Cols = TileSize / TIC_SPRITESIZE, Scale = Size/TileSize};
	platform.studio->tic->api.clear(platform.studio->tic, 0);

	u32* pixels = SDL_malloc(Size * Size * sizeof(u32));

	const u32* pal = tic_palette_blit(&platform.studio->config()->cart->bank0.palette);

	for(s32 j = 0, index = 0; j < Size; j++)
		for(s32 i = 0; i < Size; i++, index++)
		{
			u8 color = getSpritePixel(platform.studio->config()->cart->bank0.tiles.data, i/Scale, j/Scale);
			pixels[index] = color == ColorKey ? 0 : pal[color];
		}

	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, Size, Size,
		sizeof(s32) * BITS_IN_BYTE, Size * sizeof(s32),
		0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	SDL_SetWindowIcon(platform.window, surface);
	SDL_FreeSurface(surface);
	SDL_free(pixels);
}

static void updateGamepadParts()
{
	s32 tileSize = TIC_SPRITESIZE;
	s32 offset = 0;
	SDL_Rect rect;

	const s32 JoySize = 3;
	SDL_GetWindowSize(platform.window, &rect.w, &rect.h);

	if(rect.w < rect.h)
	{
		tileSize = rect.w / 2 / JoySize;
		offset = (rect.h * 2 - JoySize * tileSize) / 3;
	}
	else
	{
		tileSize = rect.w / 5 / JoySize;
		offset = (rect.h - JoySize * tileSize) / 2;
	}

	platform.gamepad.part.size = tileSize;
	platform.gamepad.part.axis = (SDL_Point){0, offset};
	platform.gamepad.part.a = (SDL_Point){rect.w - 2*tileSize, 2*tileSize + offset};
	platform.gamepad.part.b = (SDL_Point){rect.w - 1*tileSize, 1*tileSize + offset};
	platform.gamepad.part.x = (SDL_Point){rect.w - 3*tileSize, 1*tileSize + offset};
	platform.gamepad.part.y = (SDL_Point){rect.w - 2*tileSize, 0*tileSize + offset};
}

static void drawKeyboardLabels(s32 shift)
{
	tic_mem* tic = platform.studio->tic;

	typedef struct {const char* text; s32 x; s32 y; bool alt; const char* shift;} Label;
	static const Label Labels[] =
	{
		#include "kbdlabels.inl"
	};

	for(s32 i = 0; i < COUNT_OF(Labels); i++)
	{
		const Label* label = Labels + i;
		if(label->text)
			tic->api.text(tic, label->text, label->x, label->y + shift, tic_color_dark_gray, label->alt);

		if(label->shift)
			tic->api.fixed_text(tic, label->shift, label->x + 6, label->y + shift + 2, tic_color_light_blue, label->alt);
	}
}

static void initTouchKeyboard()
{
	tic_mem* tic = platform.studio->tic;

	enum{Cols=KBD_COLS, Rows=KBD_ROWS};

	// TODO: add touch keyboard to one texture with gamepad (and mouse cursor???)
	if(!platform.keyboard.texture.up)
	{		
		platform.keyboard.texture.up = GPU_CreateImage(TIC80_FULLWIDTH, TIC80_FULLHEIGHT, STUDIO_PIXEL_FORMAT);
		GPU_SetAnchor(platform.keyboard.texture.up, 0, 0);
		GPU_SetImageFilter(platform.keyboard.texture.up, GPU_FILTER_NEAREST);
	}

	{

		memcpy(tic->ram.vram.palette.data, platform.studio->config()->cart->bank0.palette.data, sizeof(tic_palette));

		tic->api.clear(tic, 0);
		tic->api.map(tic, &platform.studio->config()->cart->bank0.map, 
			&platform.studio->config()->cart->bank0.tiles, 8, 0, Cols, Rows, 0, 0, -1, 1);

		drawKeyboardLabels(0);

		tic->api.blit(tic, NULL, NULL, NULL);

		GPU_UpdateImageBytes(platform.keyboard.texture.up, NULL, (const u8*)tic->screen, TIC80_FULLWIDTH * sizeof(u32));
	}

	if(!platform.keyboard.texture.down)
	{		
		platform.keyboard.texture.down = GPU_CreateImage(TIC80_FULLWIDTH, TIC80_FULLHEIGHT, STUDIO_PIXEL_FORMAT);
		GPU_SetAnchor(platform.keyboard.texture.down, 0, 0);
		GPU_SetImageFilter(platform.keyboard.texture.down, GPU_FILTER_NEAREST);
	}

	{
		memcpy(tic->ram.vram.palette.data, platform.studio->config()->cart->bank0.palette.data, sizeof(tic_palette));

		tic->api.map(tic, &platform.studio->config()->cart->bank0.map, 
			&platform.studio->config()->cart->bank0.tiles, TIC_MAP_SCREEN_WIDTH+8, 0, Cols, Rows, 0, 0, -1, 1);

		drawKeyboardLabels(2);

		tic->api.blit(tic, NULL, NULL, NULL);

		GPU_UpdateImageBytes(platform.keyboard.texture.down, NULL, (const u8*)tic->screen, TIC80_FULLWIDTH * sizeof(u32));
	}

}

static void initTouchGamepad()
{
	platform.studio->tic->api.map(platform.studio->tic, &platform.studio->config()->cart->bank0.map, 
		&platform.studio->config()->cart->bank0.tiles, 0, 0, TIC_MAP_SCREEN_WIDTH, TIC_MAP_SCREEN_HEIGHT, 0, 0, -1, 1);

	if(!platform.gamepad.texture)
	{		
		platform.gamepad.texture = GPU_CreateImage(TEXTURE_SIZE, TEXTURE_SIZE, STUDIO_PIXEL_FORMAT);
		GPU_SetAnchor(platform.gamepad.texture, 0, 0);
		GPU_SetImageFilter(platform.gamepad.texture, GPU_FILTER_NEAREST);
		GPU_SetRGBA(platform.gamepad.texture, 0xff, 0xff, 0xff, platform.studio->config()->theme.gamepad.touch.alpha);
	}

	u32* data = SDL_malloc(TEXTURE_SIZE * TEXTURE_SIZE * sizeof(u32));

	if(data)
	{
		u32* out = data;

		const u8* in = platform.studio->tic->ram.vram.screen.data;
		const u8* end = in + sizeof(platform.studio->tic->ram.vram.screen);
		const u32* pal = tic_palette_blit(&platform.studio->config()->cart->bank0.palette);
		const u32 Delta = ((TIC80_FULLWIDTH*sizeof(u32))/sizeof *out - TIC80_WIDTH);

		s32 col = 0;

		while(in != end)
		{
			u8 low = *in & 0x0f;
			u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
			*out++ = low ? *(pal + low) : 0;
			*out++ = hi ? *(pal + hi) : 0;
			in++;

			col += BITS_IN_BYTE / TIC_PALETTE_BPP;

			if (col == TIC80_WIDTH)
			{
				col = 0;
				out += Delta;
			}
		}

		GPU_UpdateImageBytes(platform.gamepad.texture, NULL, (const u8*)data, TEXTURE_SIZE * sizeof(u32));

		SDL_free(data);

		updateGamepadParts();
	}
}

static void calcTextureRect(SDL_Rect* rect)
{
	SDL_GetWindowSize(platform.window, &rect->w, &rect->h);

	if(crtMonitorEnabled())
	{
		enum{Width = TIC80_FULLWIDTH, Height = TIC80_FULLHEIGHT};

		if (rect->w * Height < rect->h * Width)
		{
			rect->x = 0;
			rect->y = 0;

			rect->h = Height * rect->w / Width;
		}
		else
		{
			s32 width = Width * rect->h / Height;

			rect->x = (rect->w - width) / 2;
			rect->y = 0;

			rect->w = width;
		}
	}
	else
	{
		enum{Width = TIC80_WIDTH, Height = TIC80_HEIGHT};

		if (rect->w * Height < rect->h * Width)
		{
			s32 discreteWidth = rect->w - rect->w % Width;
			s32 discreteHeight = Height * discreteWidth / Width;

			rect->x = (rect->w - discreteWidth) / 2;

			rect->y = rect->w > rect->h 
				? (rect->h - discreteHeight) / 2 
				: OFFSET_TOP*discreteWidth/Width;

			rect->w = discreteWidth;
			rect->h = discreteHeight;

		}
		else
		{
			s32 discreteHeight = rect->h - rect->h % Height;
			s32 discreteWidth = Width * discreteHeight / Height;

			rect->x = (rect->w - discreteWidth) / 2;
			rect->y = (rect->h - discreteHeight) / 2;

			rect->w = discreteWidth;
			rect->h = discreteHeight;
		}
	}
}

static void processMouse()
{
	s32 mx = 0, my = 0;
	s32 mb = SDL_GetMouseState(&mx, &my);

	tic80_input* input = &platform.studio->tic->ram.input;

	{
		input->mouse.x = input->mouse.y = 0;

		SDL_Rect rect = {0, 0, 0, 0};
		calcTextureRect(&rect);

		s32 x = -1, y = -1;
		if(crtMonitorEnabled())
		{
			if(rect.w) x = (mx - rect.x) * TIC80_FULLWIDTH / rect.w - OFFSET_LEFT;
			if(rect.h) y = (my - rect.y) * TIC80_FULLHEIGHT / rect.h - OFFSET_TOP;
		}
		else
		{
			if(rect.w) x = (mx - rect.x) * TIC80_WIDTH / rect.w;
			if(rect.h) y = (my - rect.y) * TIC80_HEIGHT / rect.h;
		}

		input->mouse.x = x >= 0 && x < 0xff ? x : 0xff;
		input->mouse.y = y >= 0 && y < 0xff ? y : 0xff;
	}

	{
		input->mouse.left = mb & SDL_BUTTON_LMASK ? 1 : 0;
		input->mouse.middle = mb & SDL_BUTTON_MMASK ? 1 : 0;
		input->mouse.right = mb & SDL_BUTTON_RMASK ? 1 : 0;
	}
}

static void processKeyboard()
{
	tic_mem* tic = platform.studio->tic;

	{
		SDL_Keymod mod = SDL_GetModState();

		platform.keyboard.state[tic_key_shift] = mod & KMOD_SHIFT;
		platform.keyboard.state[tic_key_ctrl] = mod & (KMOD_CTRL | KMOD_GUI);
		platform.keyboard.state[tic_key_alt] = mod & KMOD_LALT;
		platform.keyboard.state[tic_key_capslock] = mod & KMOD_CAPS;

		// it's weird, but system sends CTRL when you press RALT
		if(mod & KMOD_RALT)
			platform.keyboard.state[tic_key_ctrl] = false;
	}	

	tic80_input* input = &tic->ram.input;
	input->keyboard.data = 0;

	enum{BufSize = COUNT_OF(input->keyboard.keys)};

	for(s32 i = 0, c = 0; i < COUNT_OF(platform.keyboard.state) && c < BufSize; i++)
		if(platform.keyboard.state[i] || platform.keyboard.touch.state[i])
			input->keyboard.keys[c++] = i;
}

#if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)

static bool checkTouch(const SDL_Rect* rect, s32* x, s32* y)
{
	s32 devices = SDL_GetNumTouchDevices();
	s32 width = 0, height = 0;
	SDL_GetWindowSize(platform.window, &width, &height);

	for (s32 i = 0; i < devices; i++)
	{
		SDL_TouchID id = SDL_GetTouchDevice(i);

		{
			s32 fingers = SDL_GetNumTouchFingers(id);

			for (s32 f = 0; f < fingers; f++)
			{
				SDL_Finger* finger = SDL_GetTouchFinger(id, f);

				if (finger && finger->pressure > 0.0f)
				{
					SDL_Point point = { (s32)(finger->x * width), (s32)(finger->y * height) };
					if (SDL_PointInRect(&point, rect))
					{
						*x = point.x;
						*y = point.y;
						return true;
					}
				}
			}
		}
	}

	return false;
}

static bool isKbdVisible()
{
	s32 w, h;
	SDL_GetWindowSize(platform.window, &w, &h);

	SDL_Rect rect;
	calcTextureRect(&rect);

	float scale = (float)w / (KBD_COLS*TIC_SPRITESIZE);

	return h - KBD_ROWS*TIC_SPRITESIZE*scale - (rect.h + rect.y*2) >= 0;
}

static void processTouchKeyboard()
{
	if(platform.touchCounter == 0 || !isKbdVisible()) return;

	enum{Cols=KBD_COLS, Rows=KBD_ROWS};

	s32 w, h;
	SDL_GetWindowSize(platform.window, &w, &h);

	float scale = (float)w / (KBD_COLS*TIC_SPRITESIZE);

	SDL_Rect kbd = {0, h - KBD_ROWS*TIC_SPRITESIZE*scale, 
		KBD_COLS*TIC_SPRITESIZE*scale, KBD_ROWS*TIC_SPRITESIZE*scale};

	static const tic_key KbdLayout[] = 
	{
		#include "kbdlayout.inl"
	};

	tic_mem* tic = platform.studio->tic;

	tic80_input* input = &tic->ram.input;

	s32 devices = SDL_GetNumTouchDevices();

	enum {BufSize = COUNT_OF(input->keyboard.keys)};

	SDL_memset(&platform.keyboard.touch.state, 0, sizeof platform.keyboard.touch.state);

	for (s32 i = 0; i < devices; i++)
	{
		SDL_TouchID id = SDL_GetTouchDevice(i);
		s32 fingers = SDL_GetNumTouchFingers(id);

		for (s32 f = 0; f < fingers; f++)
		{
			SDL_Finger* finger = SDL_GetTouchFinger(id, f);

			if (finger && finger->pressure > 0.0f)
			{
				SDL_Point pt = {finger->x * w, finger->y * h};

				if(SDL_PointInRect(&pt, &kbd))
				{
					pt.x -= kbd.x;
					pt.y -= kbd.y;

					pt.x /= scale;
					pt.y /= scale;

					platform.keyboard.touch.state[KbdLayout[pt.x / TIC_SPRITESIZE + pt.y / TIC_SPRITESIZE * Cols]] = true;
				}
			}
		}
	}
}

static void processTouchGamepad()
{	
	platform.gamepad.touch.data = 0;

	if(platform.touchCounter == 0) return;

	const s32 size = platform.gamepad.part.size;
	s32 x = 0, y = 0;

	{
		SDL_Rect axis = {platform.gamepad.part.axis.x, platform.gamepad.part.axis.y, size*3, size*3};

		if(checkTouch(&axis, &x, &y))
		{
			x -= axis.x;
			y -= axis.y;

			s32 xt = x / size;
			s32 yt = y / size;

			if(yt == 0) platform.gamepad.touch.first.up = true;
			else if(yt == 2) platform.gamepad.touch.first.down = true;

			if(xt == 0) platform.gamepad.touch.first.left = true;
			else if(xt == 2) platform.gamepad.touch.first.right = true;

			if(xt == 1 && yt == 1)
			{
				xt = (x - size)/(size/3);
				yt = (y - size)/(size/3);

				if(yt == 0) platform.gamepad.touch.first.up = true;
				else if(yt == 2) platform.gamepad.touch.first.down = true;

				if(xt == 0) platform.gamepad.touch.first.left = true;
				else if(xt == 2) platform.gamepad.touch.first.right = true;
			}
		}
	}

	{
		SDL_Rect a = {platform.gamepad.part.a.x, platform.gamepad.part.a.y, size, size};
		if(checkTouch(&a, &x, &y)) platform.gamepad.touch.first.a = true;
	}

	{
		SDL_Rect b = {platform.gamepad.part.b.x, platform.gamepad.part.b.y, size, size};
		if(checkTouch(&b, &x, &y)) platform.gamepad.touch.first.b = true;
	}

	{
		SDL_Rect xb = {platform.gamepad.part.x.x, platform.gamepad.part.x.y, size, size};
		if(checkTouch(&xb, &x, &y)) platform.gamepad.touch.first.x = true;
	}

	{
		SDL_Rect yb = {platform.gamepad.part.y.x, platform.gamepad.part.y.y, size, size};
		if(checkTouch(&yb, &x, &y)) platform.gamepad.touch.first.y = true;
	}
}

#endif

static s32 getAxisMask(SDL_Joystick* joystick)
{
	s32 mask = 0;

	s32 axesCount = SDL_JoystickNumAxes(joystick);

	for (s32 a = 0; a < axesCount; a++)
	{
		s32 axe = SDL_JoystickGetAxis(joystick, a);

		if (axe)
		{
			if (a == 0)
			{
				if (axe > 16384) mask |= SDL_HAT_RIGHT;
				else if(axe < -16384) mask |= SDL_HAT_LEFT;
			}
			else if (a == 1)
			{
				if (axe > 16384) mask |= SDL_HAT_DOWN;
				else if (axe < -16384) mask |= SDL_HAT_UP;
			}
		}
	}

	return mask;
}

static s32 getJoystickHatMask(s32 hat)
{
	tic80_gamepads gamepad;
	gamepad.data = 0;

	gamepad.first.up = hat & SDL_HAT_UP;
	gamepad.first.down = hat & SDL_HAT_DOWN;
	gamepad.first.left = hat & SDL_HAT_LEFT;
	gamepad.first.right = hat & SDL_HAT_RIGHT;

	return gamepad.data;
}

static void processJoysticks()
{
	platform.gamepad.joystick.data = 0;
	s32 index = 0;

	for(s32 i = 0; i < COUNT_OF(platform.gamepad.ports); i++)
	{
		SDL_Joystick* joystick = platform.gamepad.ports[i];

		if(joystick && SDL_JoystickGetAttached(joystick))
		{
			tic80_gamepad* gamepad = NULL;

			switch(index)
			{
			case 0: gamepad = &platform.gamepad.joystick.first; break;
			case 1: gamepad = &platform.gamepad.joystick.second; break;
			case 2: gamepad = &platform.gamepad.joystick.third; break;
			case 3: gamepad = &platform.gamepad.joystick.fourth; break;
			}

			if(gamepad)
			{
				gamepad->data |= getJoystickHatMask(getAxisMask(joystick));

				for (s32 h = 0; h < SDL_JoystickNumHats(joystick); h++)
					gamepad->data |= getJoystickHatMask(SDL_JoystickGetHat(joystick, h));

				s32 numButtons = SDL_JoystickNumButtons(joystick);
				if(numButtons >= 2)
				{
					gamepad->a = SDL_JoystickGetButton(joystick, 0);
					gamepad->b = SDL_JoystickGetButton(joystick, 1);

					if(numButtons >= 4)
					{
						gamepad->x = SDL_JoystickGetButton(joystick, 2);
						gamepad->y = SDL_JoystickGetButton(joystick, 3);

						for(s32 i = 5; i < numButtons; i++)
						{
							s32 back = SDL_JoystickGetButton(joystick, i);

							if(back)
							{
								tic_mem* tic = platform.studio->tic;
								tic->ram.input.keyboard.keys[0] = tic_key_escape;
							}
						}
					}
				}

				index++;
			}
		}
	}
}

static void processGamepad()
{
	processJoysticks();
	
	{
		platform.studio->tic->ram.input.gamepads.data = 0;

		platform.studio->tic->ram.input.gamepads.data |= platform.gamepad.touch.data;
		platform.studio->tic->ram.input.gamepads.data |= platform.gamepad.joystick.data;
	}
}

static void processTouchInput()
{
	#if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)
	{
		s32 devices = SDL_GetNumTouchDevices();
		for (s32 i = 0; i < devices; i++)
			if(SDL_GetNumTouchFingers(SDL_GetTouchDevice(i)) > 0)
				platform.touchCounter = 10 * TIC_FRAMERATE;

		if(platform.touchCounter)
			platform.touchCounter--;
	}

	platform.studio->isGamepadMode()
		? processTouchGamepad()
		: processTouchKeyboard();
#endif
}

static void handleKeydown(SDL_Keycode keycode, bool down)
{
	static const u32 KeyboardCodes[tic_keys_count] = 
	{
		#include "keycodes.inl"
	};

	for(tic_key i = 0; i < COUNT_OF(KeyboardCodes); i++)
	{
		if(KeyboardCodes[i] == keycode)
		{
			platform.keyboard.state[i] = down;
			break;
		}
	}

	if(keycode == SDLK_AC_BACK)
		platform.keyboard.state[tic_key_escape] = down;
}

static void pollEvent()
{
	tic_mem* tic = platform.studio->tic;
	tic80_input* input = &tic->ram.input;

	input->mouse.btns = 0;

	SDL_Event event;

	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_MOUSEWHEEL:
			{
				input->mouse.scrollx = event.wheel.x;
				input->mouse.scrolly = event.wheel.y;				
			}
			break;
		case SDL_JOYDEVICEADDED:
			{
				s32 id = event.jdevice.which;

				if (id < TIC_GAMEPADS)
				{
					if(platform.gamepad.ports[id])
						SDL_JoystickClose(platform.gamepad.ports[id]);

					platform.gamepad.ports[id] = SDL_JoystickOpen(id);
				}
			}
			break;

		case SDL_JOYDEVICEREMOVED:
			{
				s32 id = event.jdevice.which;

				if (id < TIC_GAMEPADS && platform.gamepad.ports[id])
				{
					SDL_JoystickClose(platform.gamepad.ports[id]);
					platform.gamepad.ports[id] = NULL;
				}
			}
			break;
		case SDL_WINDOWEVENT:
			switch(event.window.event)
			{
			case SDL_WINDOWEVENT_RESIZED: 
				{
					s32 w = 0, h = 0;
					SDL_GetWindowSize(platform.window, &w, &h);
					GPU_SetWindowResolution(w, h);

					updateGamepadParts();
				}
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED: 
				platform.studio->updateProject();
				break;
			}
			break;
		case SDL_KEYDOWN:
			handleKeydown(event.key.keysym.sym, true);
			break;
		case SDL_KEYUP:
			handleKeydown(event.key.keysym.sym, false);
			break;			
		case SDL_QUIT:
			platform.studio->exit();
			break;
		default:
			break;
		}
	}

	processMouse();
	processTouchInput();
	processKeyboard();
	processGamepad();
}

static void blitGpuTexture(GPU_Target* screen, GPU_Image* texture)
{
	SDL_Rect rect = {0, 0, 0, 0};
	calcTextureRect(&rect);

	enum {Header = OFFSET_TOP, Top = OFFSET_TOP, Left = OFFSET_LEFT};

	s32 width = 0;
	SDL_GetWindowSize(platform.window, &width, NULL);

	{
		GPU_Rect srcRect = {0, 0, TIC80_FULLWIDTH, Header};
		GPU_Rect dstRect = {0, 0, width, rect.y};
		GPU_BlitScale(texture, &srcRect, screen, dstRect.x, dstRect.y, dstRect.w / srcRect.w, dstRect.h / srcRect.h);
	}

	{
		GPU_Rect srcRect = {0, TIC80_FULLHEIGHT - Header, TIC80_FULLWIDTH, Header};
		GPU_Rect dstRect = {0, rect.y + rect.h, width, rect.y};
		GPU_BlitScale(texture, &srcRect, screen, dstRect.x, dstRect.y, dstRect.w / srcRect.w, dstRect.h / srcRect.h);
	}

	{
		GPU_Rect srcRect = {0, Header, Left, TIC80_HEIGHT};
		GPU_Rect dstRect = {0, rect.y, width, rect.h};
		GPU_BlitScale(texture, &srcRect, screen, dstRect.x, dstRect.y, dstRect.w / srcRect.w, dstRect.h / srcRect.h);
	}

	{
		GPU_Rect srcRect = {Left, Top, TIC80_WIDTH, TIC80_HEIGHT};
		GPU_Rect dstRect = {rect.x, rect.y, rect.w, rect.h};
		GPU_BlitScale(texture, &srcRect, screen, dstRect.x, dstRect.y, dstRect.w / srcRect.w, dstRect.h / srcRect.h);
	}
}

static void blitSound()
{
	tic_mem* tic = platform.studio->tic;

	SDL_PauseAudioDevice(platform.audio.device, 0);
	
	if(platform.audio.cvt.needed)
	{
		SDL_memcpy(platform.audio.cvt.buf, tic->samples.buffer, tic->samples.size);
		SDL_ConvertAudio(&platform.audio.cvt);
		SDL_QueueAudio(platform.audio.device, platform.audio.cvt.buf, platform.audio.cvt.len_cvt);
	}
	else SDL_QueueAudio(platform.audio.device, tic->samples.buffer, tic->samples.size);
}

#if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)

static void renderKeyboard()
{
	if(platform.touchCounter == 0 || !isKbdVisible()) return;

	SDL_Rect rect;
	SDL_GetWindowSize(platform.window, &rect.w, &rect.h);

	GPU_Rect src = {OFFSET_LEFT, OFFSET_TOP, KBD_COLS*TIC_SPRITESIZE, KBD_ROWS*TIC_SPRITESIZE};
	float scale = rect.w/src.w;
	GPU_Rect dst = (GPU_Rect){0, rect.h - KBD_ROWS*TIC_SPRITESIZE*scale, scale, scale};

	GPU_BlitScale(platform.keyboard.texture.up, &src, platform.gpu.screen, dst.x, dst.y, dst.w, dst.h);

	static const tic_key KbdLayout[] = 
	{
		#include "kbdlayout.inl"
	};

	tic80_input* input = &platform.studio->tic->ram.input;

	enum{Cols=KBD_COLS, Rows=KBD_ROWS};

	for(s32 i = 0; i < COUNT_OF(input->keyboard.keys); i++)
	{
		tic_key key = input->keyboard.keys[i];
		if(key > tic_key_unknown)
		{
			for(s32 k = 0; k < COUNT_OF(KbdLayout); k++)
			{
				if(key == KbdLayout[k])
				{
					GPU_Rect src = {(k % Cols)*TIC_SPRITESIZE + OFFSET_LEFT, (k / Cols)*TIC_SPRITESIZE + OFFSET_TOP, 
						TIC_SPRITESIZE, TIC_SPRITESIZE};

					GPU_BlitScale(platform.keyboard.texture.down, &src, platform.gpu.screen, 
						(src.x - OFFSET_LEFT) * dst.w, (src.y - OFFSET_TOP) * dst.h + dst.y, dst.w, dst.h);
				}
			}
		}		
	}
}

static void renderGamepad()
{
	if(platform.touchCounter == 0) return;

	const s32 tileSize = platform.gamepad.part.size;
	const SDL_Point axis = platform.gamepad.part.axis;
	typedef struct { bool press; s32 x; s32 y;} Tile;
	const Tile Tiles[] =
	{
		{platform.studio->tic->ram.input.gamepads.first.up, 	axis.x + 1*tileSize, axis.y + 0*tileSize},
		{platform.studio->tic->ram.input.gamepads.first.down, 	axis.x + 1*tileSize, axis.y + 2*tileSize},
		{platform.studio->tic->ram.input.gamepads.first.left, 	axis.x + 0*tileSize, axis.y + 1*tileSize},
		{platform.studio->tic->ram.input.gamepads.first.right, 	axis.x + 2*tileSize, axis.y + 1*tileSize},

		{platform.studio->tic->ram.input.gamepads.first.a, 		platform.gamepad.part.a.x, platform.gamepad.part.a.y},
		{platform.studio->tic->ram.input.gamepads.first.b, 		platform.gamepad.part.b.x, platform.gamepad.part.b.y},
		{platform.studio->tic->ram.input.gamepads.first.x, 		platform.gamepad.part.x.x, platform.gamepad.part.x.y},
		{platform.studio->tic->ram.input.gamepads.first.y, 		platform.gamepad.part.y.x, platform.gamepad.part.y.y},
	};

	for(s32 i = 0; i < COUNT_OF(Tiles); i++)
	{
		const Tile* tile = Tiles + i;
		GPU_Rect src = {i * TIC_SPRITESIZE, tile->press ? TIC_SPRITESIZE : 0, TIC_SPRITESIZE, TIC_SPRITESIZE};
		GPU_Rect dest = {tile->x, tile->y, tileSize, tileSize};

		GPU_BlitScale(platform.gamepad.texture, &src, platform.gpu.screen, dest.x, dest.y,
			(float)dest.w / TIC_SPRITESIZE, (float)dest.h / TIC_SPRITESIZE);
	}
}

#endif

static void blitCursor(const u8* in)
{
	if(!platform.mouse.texture)
	{
		platform.mouse.texture = GPU_CreateImage(TIC_SPRITESIZE, TIC_SPRITESIZE, STUDIO_PIXEL_FORMAT);
		GPU_SetAnchor(platform.mouse.texture, 0, 0);
		GPU_SetImageFilter(platform.mouse.texture, GPU_FILTER_NEAREST);
	}

	if(platform.mouse.src != in)
	{
		platform.mouse.src = in;

		const u8* end = in + sizeof(tic_tile);
		const u32* pal = tic_palette_blit(&platform.studio->tic->ram.vram.palette);
		static u32 data[TIC_SPRITESIZE*TIC_SPRITESIZE];
		u32* out = data;

		while(in != end)
		{
			u8 low = *in & 0x0f;
			u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
			*out++ = low ? *(pal + low) : 0;
			*out++ = hi ? *(pal + hi) : 0;

			in++;
		}

		GPU_UpdateImageBytes(platform.mouse.texture, NULL, (const u8*)data, TIC_SPRITESIZE * sizeof(u32));
	}

	SDL_Rect rect = {0, 0, 0, 0};
	calcTextureRect(&rect);
	s32 scale = rect.w / TIC80_WIDTH;

	s32 mx, my;
	SDL_GetMouseState(&mx, &my);

	if(platform.studio->config()->theme.cursor.pixelPerfect)
	{
		mx -= (mx - rect.x) % scale;
		my -= (my - rect.y) % scale;
	}

	if(SDL_GetWindowFlags(platform.window) & SDL_WINDOW_MOUSE_FOCUS)
		GPU_BlitScale(platform.mouse.texture, NULL, platform.gpu.screen, mx, my, (float)scale, (float)scale);
}

static void renderCursor()
{
	if(platform.studio->tic->ram.vram.vars.cursor.system)
	{
		switch(platform.studio->tic->ram.vram.vars.cursor.sprite)
		{
		case tic_cursor_hand: 
			{
				if(platform.studio->config()->theme.cursor.hand >= 0)
				{
					SDL_ShowCursor(SDL_DISABLE);
					blitCursor(platform.studio->config()->cart->bank0.tiles.data[platform.studio->config()->theme.cursor.hand].data);
				}
				else
				{
					SDL_ShowCursor(SDL_ENABLE);
					SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
				}
			}
			break;
		case tic_cursor_ibeam:
			{
				if(platform.studio->config()->theme.cursor.ibeam >= 0)
				{
					SDL_ShowCursor(SDL_DISABLE);
					blitCursor(platform.studio->config()->cart->bank0.tiles.data[platform.studio->config()->theme.cursor.ibeam].data);
				}
				else
				{
					SDL_ShowCursor(SDL_ENABLE);
					SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM));
				}
			}
			break;
		default:
			{
				if(platform.studio->config()->theme.cursor.arrow >= 0)
				{
					SDL_ShowCursor(SDL_DISABLE);
					blitCursor(platform.studio->config()->cart->bank0.tiles.data[platform.studio->config()->theme.cursor.arrow].data);
				}
				else
				{
					SDL_ShowCursor(SDL_ENABLE);
					SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
				}
			}
		}
	}
	else
	{
		SDL_ShowCursor(SDL_DISABLE);
		blitCursor(platform.studio->tic->ram.sprites.data[platform.studio->tic->ram.vram.vars.cursor.sprite].data);
	}

// 	if(platform.mode == TIC_RUN_MODE && !platform.studio.tic->input.mouse)
// 	{
// 		SDL_ShowCursor(SDL_DISABLE);
// 		return;
// 	}
}

static const char* getAppFolder()
{
	static char appFolder[FILENAME_MAX];

#if defined(__EMSCRIPTEN__)

		strcpy(appFolder, "/" TIC_PACKAGE "/" TIC_NAME "/");

#elif defined(__ANDROID__)

		strcpy(appFolder, SDL_AndroidGetExternalStoragePath());
		const char AppFolder[] = "/" TIC_NAME "/";
		strcat(appFolder, AppFolder);
		mkdir(appFolder, 0700);

#else

		char* path = SDL_GetPrefPath(TIC_PACKAGE, TIC_NAME);
		strcpy(appFolder, path);
		SDL_free(path);

#endif

	return appFolder;
}

static void setClipboardText(const char* text)
{
	SDL_SetClipboardText(text);
}

static bool hasClipboardText()
{
	return SDL_HasClipboardText();
}

static char* getClipboardText()
{
	return SDL_GetClipboardText();
}

static void freeClipboardText(const char* text)
{
	SDL_free((void*)text);
}

static u64 getPerformanceCounter()
{
	return SDL_GetPerformanceCounter();
}

static u64 getPerformanceFrequency()
{
	return SDL_GetPerformanceFrequency();
}

static void goFullscreen()
{
	GPU_SetFullscreen(GPU_GetFullscreen() ? false : true, true);
}

static void showMessageBox(const char* title, const char* message)
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, title, message, NULL);
}

static void setWindowTitle(const char* title)
{
	SDL_SetWindowTitle(platform.window, title);
}

#if defined(__WINDOWS__) || defined(__LINUX__) || defined(__MACOSX__)

static void openSystemPath(const char* path)
{
	char command[FILENAME_MAX];

#if defined(__WINDOWS__)

	sprintf(command, "explorer \"%s\"", path);

	wchar_t wcommand[FILENAME_MAX];
	MultiByteToWideChar(CP_UTF8, 0, command, FILENAME_MAX, wcommand, FILENAME_MAX);

	_wsystem(wcommand);

#elif defined(__LINUX__)

	sprintf(command, "xdg-open \"%s\"", path);
	if(system(command)){}

#elif defined(__MACOSX__)

	sprintf(command, "open \"%s\"", path);
	system(command);

#endif
}

#else

static void openSystemPath(const char* path) {}

#endif

static void* getUrlRequest(const char* url, s32* size)
{
	return netGetRequest(platform.net, url, size);
}

static void preseed()
{
#if defined(__MACOSX__)
	srandom(time(NULL));
	random();
#else
	srand(time(NULL));
	rand();
#endif
}

static char* prepareShader(const char* code)
{
	GPU_Renderer* renderer = GPU_GetCurrentRenderer();
	const char* header = "";

	if(renderer->shader_language == GPU_LANGUAGE_GLSL)
	{
		if(renderer->max_shader_version >= 120)
			header = "#version 120\n";
		else
			header = "#version 110\n";
	}
	else if(renderer->shader_language == GPU_LANGUAGE_GLSLES)
		header = "#version 100\nprecision mediump int;\nprecision mediump float;\n";

	char* shader = SDL_malloc(strlen(header) + strlen(code) + 2);

	if(shader)
	{
		strcpy(shader, header);
		strcat(shader, code);		
	}

	return shader;
}

static void loadCrtShader()
{
	char* vertextShader = prepareShader("\
		attribute vec3 gpu_Vertex;\n\
		attribute vec2 gpu_TexCoord;\n\
		attribute vec4 gpu_Color;\n\
		uniform mat4 gpu_ModelViewProjectionMatrix;\n\
		varying vec4 color;\n\
		varying vec2 texCoord;\n\
		void main(void)\n\
		{\n\
			color = gpu_Color;\n\
			texCoord = vec2(gpu_TexCoord);\n\
			gl_Position = gpu_ModelViewProjectionMatrix * vec4(gpu_Vertex, 1.0);\n\
		}");

	u32 vertex = 0;
	if(vertextShader)
	{
		vertex = GPU_CompileShader(GPU_VERTEX_SHADER, vertextShader);
		SDL_free(vertextShader);		
	}
	
	if(!vertex)
	{
		char msg[1024];
		sprintf(msg, "Failed to load vertex shader: %s\n", GPU_GetShaderMessage());
		showMessageBox("Error", msg);
		return;
	}

	char* fragmentShader = prepareShader(platform.studio->config()->crtShader);

	u32 fragment = 0;
	if(fragmentShader)
	{
		fragment = GPU_CompileShader(GPU_PIXEL_SHADER, fragmentShader);
		SDL_free(fragmentShader);		
	}
	
	if(!fragment)
	{
		char msg[1024];
		sprintf(msg, "Failed to load fragment shader: %s\n", GPU_GetShaderMessage());
		showMessageBox("Error", msg);
		return;
	}
	
	if(platform.gpu.shader)
		GPU_FreeShaderProgram(platform.gpu.shader);

	platform.gpu.shader = GPU_LinkShaders(vertex, fragment);
	
	if(platform.gpu.shader)
	{
		platform.gpu.block = GPU_LoadShaderBlock(platform.gpu.shader, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
		GPU_ActivateShaderProgram(platform.gpu.shader, &platform.gpu.block);
	}
	else
	{
		char msg[1024];
		sprintf(msg, "Failed to link shader program: %s\n", GPU_GetShaderMessage());
		showMessageBox("Error", msg);
	}
}

static void updateConfig()
{
	if(platform.gpu.screen)
		initTouchGamepad();
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

static void gpuTick()
{
	tic_mem* tic = platform.studio->tic;

	pollEvent();

	if(platform.studio->quit)
	{
#if defined __EMSCRIPTEN__
		emscripten_cancel_main_loop();
#endif
		return;
	}

	GPU_Clear(platform.gpu.screen);

	{
		platform.studio->tick();

		GPU_UpdateImageBytes(platform.gpu.texture, NULL, (const u8*)tic->screen, TIC80_FULLWIDTH * sizeof(u32));

		{
			if(platform.studio->config()->crtMonitor)
			{
				if(platform.gpu.shader == 0)
					loadCrtShader();

				SDL_Rect rect = {0, 0, 0, 0};
				calcTextureRect(&rect);

				GPU_ActivateShaderProgram(platform.gpu.shader, &platform.gpu.block);

				GPU_SetUniformf(GPU_GetUniformLocation(platform.gpu.shader, "trg_x"), rect.x);
				GPU_SetUniformf(GPU_GetUniformLocation(platform.gpu.shader, "trg_y"), rect.y);
				GPU_SetUniformf(GPU_GetUniformLocation(platform.gpu.shader, "trg_w"), rect.w);
				GPU_SetUniformf(GPU_GetUniformLocation(platform.gpu.shader, "trg_h"), rect.h);

				{
					s32 w, h;
					SDL_GetWindowSize(platform.window, &w, &h);
					GPU_SetUniformf(GPU_GetUniformLocation(platform.gpu.shader, "scr_w"), w);
					GPU_SetUniformf(GPU_GetUniformLocation(platform.gpu.shader, "scr_h"), h);
				}

				GPU_BlitScale(platform.gpu.texture, NULL, platform.gpu.screen, rect.x, rect.y, 
					(float)rect.w / TIC80_FULLWIDTH, (float)rect.h / TIC80_FULLHEIGHT);

				GPU_DeactivateShaderProgram();
			}
			else
			{
				GPU_DeactivateShaderProgram();
				blitGpuTexture(platform.gpu.screen, platform.gpu.texture);
			}
		}

		renderCursor();

#if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)
		platform.studio->isGamepadMode()
			? renderGamepad()
			: renderKeyboard();
#endif
	}

	GPU_Flip(platform.gpu.screen);

	blitSound();
}

#if defined(__EMSCRIPTEN__)

static void emsGpuTick()
{
	static double nextTick = -1.0;

	platform.missedFrame = false;

	if(nextTick < 0.0)
		nextTick = emscripten_get_now();

	nextTick += 1000.0/TIC_FRAMERATE;
	gpuTick();
	double delay = nextTick - emscripten_get_now();

	if(delay < 0.0)
	{
		nextTick -= delay;
		platform.missedFrame = true;
	}
	else
		emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, delay);
}

#endif

static s32 start(s32 argc, char **argv, const char* folder)
{
	SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

	initSound();

	platform.net = createNet();

	platform.studio = studioInit(argc, argv, platform.audio.spec.freq, folder, &systemInterface);

	const s32 Width = TIC80_FULLWIDTH * platform.studio->config()->uiScale;
	const s32 Height = TIC80_FULLHEIGHT * platform.studio->config()->uiScale;

	platform.window = SDL_CreateWindow( TIC_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		Width, Height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE| SDL_WINDOW_OPENGL);

	setWindowIcon();

	GPU_SetInitWindow(SDL_GetWindowID(platform.window));

	platform.gpu.screen = GPU_Init(Width, Height, GPU_INIT_DISABLE_VSYNC);

	{
		s32 w = 0, h = 0;
		SDL_GetWindowSize(platform.window, &w, &h);
		GPU_SetWindowResolution(w, h);
	}

	initTouchGamepad();
	initTouchKeyboard();

	platform.gpu.texture = GPU_CreateImage(TIC80_FULLWIDTH, TIC80_FULLHEIGHT, STUDIO_PIXEL_FORMAT);
	GPU_SetAnchor(platform.gpu.texture, 0, 0);
	GPU_SetImageFilter(platform.gpu.texture, GPU_FILTER_NEAREST);

#if defined(__EMSCRIPTEN__)

	emscripten_set_main_loop(emsGpuTick, 0, 1);

#else
	{
		u64 nextTick = SDL_GetPerformanceCounter();
		const u64 Delta = SDL_GetPerformanceFrequency() / TIC_FRAMERATE;

		while (!platform.studio->quit)
		{
			platform.missedFrame = false;

			nextTick += Delta;
			
			gpuTick();

			{
				s64 delay = nextTick - SDL_GetPerformanceCounter();

				if(delay < 0)
				{
					nextTick -= delay;
					platform.missedFrame = true;
				}
				else SDL_Delay((u32)(delay * 1000 / SDL_GetPerformanceFrequency()));
			}
		}
	}

#endif

	platform.studio->close();

	closeNet(platform.net);

	if(platform.audio.cvt.buf)
		SDL_free(platform.audio.cvt.buf);

	if(platform.gpu.shader)
		GPU_FreeShaderProgram(platform.gpu.shader);

	GPU_FreeImage(platform.gpu.texture);

	if(platform.gamepad.texture)
		GPU_FreeImage(platform.gamepad.texture);

	if(platform.keyboard.texture.up)
		GPU_FreeImage(platform.keyboard.texture.up);

	if(platform.keyboard.texture.down)
		GPU_FreeImage(platform.keyboard.texture.down);

	if(platform.mouse.texture)
		GPU_FreeImage(platform.mouse.texture);

	SDL_DestroyWindow(platform.window);
	SDL_CloseAudioDevice(platform.audio.device);

	GPU_Quit();

	return 0;
}

#if defined(__EMSCRIPTEN__)

#define DEFAULT_CART "cart.tic"

static struct
{
	s32 argc;
	char **argv;
	const char* folder;
} startVars;

static void onEmscriptenWget(const char* file)
{
	startVars.argv[1] = DEFAULT_CART;
	start(startVars.argc, startVars.argv, startVars.folder);
}

static void onEmscriptenWgetError(const char* error) {}

static void emsStart(s32 argc, char **argv, const char* folder)
{
	if(argc == 2)
	{
		startVars.argc = argc;
		startVars.argv = argv;
		startVars.folder = folder;

		emscripten_async_wget(argv[1], DEFAULT_CART, onEmscriptenWget, onEmscriptenWgetError);
	}
	else start(argc, argv, folder);
}

#endif

s32 main(s32 argc, char **argv)
{
	const char* folder = getAppFolder();

#if defined(__EMSCRIPTEN__)

	EM_ASM_
	(
		{
			var dir = "";
			Module.Pointer_stringify($0).split("/").forEach(function(val)
			{
				if(val.length)
				{
					dir += "/" + val;
					FS.mkdir(dir);
				}
			});
			
			FS.mount(IDBFS, {}, dir);
			FS.syncfs(true, function()
			{
				Runtime.dynCall('viii', $1, [$2, $3, $0]);
			});

		}, folder, emsStart, argc, argv
	);

#else

	return start(argc, argv, folder);
	
#endif
}
