#include "system.h"
#include "net.h"
#include "tools.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <SDL.h>

#include <SDL_gpu.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#define STUDIO_UI_SCALE 3
#define STUDIO_PIXEL_FORMAT SDL_PIXELFORMAT_ARGB8888
#define TEXTURE_SIZE (TIC80_FULLWIDTH)
#define OFFSET_LEFT ((TIC80_FULLWIDTH-TIC80_WIDTH)/2)
#define OFFSET_TOP ((TIC80_FULLHEIGHT-TIC80_HEIGHT)/2)

static struct
{
	Studio* studio;

	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;

	struct
	{
		SDL_Joystick* ports[TIC_GAMEPADS];
		SDL_Texture* texture;

		tic80_gamepads touch;
		tic80_gamepads joystick;

		bool show;
		s32 counter;
		s32 alpha;

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
		SDL_Texture* texture;
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

static void initSound()
{
	SDL_AudioSpec want =
	{
		.freq = 44100,
		.format = AUDIO_S16,
		.channels = 1,
		.userdata = NULL,
	};

	platform.audio.device = SDL_OpenAudioDevice(NULL, 0, &want, &platform.audio.spec, SDL_AUDIO_ALLOW_ANY_CHANGE);

	SDL_BuildAudioCVT(&platform.audio.cvt, want.format, want.channels, platform.audio.spec.freq, platform.audio.spec.format, platform.audio.spec.channels, platform.audio.spec.freq);

	if(platform.audio.cvt.needed)
	{
		platform.audio.cvt.len = platform.audio.spec.freq * sizeof(s16) / TIC_FRAMERATE;
		platform.audio.cvt.buf = SDL_malloc(platform.audio.cvt.len * platform.audio.cvt.len_mult);
	}
}

static u8* getSpritePtr(tic_tile* tiles, s32 x, s32 y)
{
	enum { SheetCols = (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE) };
	return tiles[x / TIC_SPRITESIZE + y / TIC_SPRITESIZE * SheetCols].data;
}

static u8 getSpritePixel(tic_tile* tiles, s32 x, s32 y)
{
	return tic_tool_peek4(getSpritePtr(tiles, x, y), (x % TIC_SPRITESIZE) + (y % TIC_SPRITESIZE) * TIC_SPRITESIZE);
}

static void setWindowIcon()
{
	enum{ Size = 64, TileSize = 16, ColorKey = 14, Cols = TileSize / TIC_SPRITESIZE, Scale = Size/TileSize};
	platform.studio->tic->api.clear(platform.studio->tic, 0);

	u32* pixels = SDL_malloc(Size * Size * sizeof(u32));

	const u32* pal = tic_palette_blit(&platform.studio->tic->config.palette);

	for(s32 j = 0, index = 0; j < Size; j++)
		for(s32 i = 0; i < Size; i++, index++)
		{
			u8 color = getSpritePixel(platform.studio->tic->config.bank0.tiles.data, i/Scale, j/Scale);
			pixels[index] = color == ColorKey ? 0 : pal[color];
		}

	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, Size, Size,
		sizeof(s32) * BITS_IN_BYTE, Size * sizeof(s32),
		0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

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

static void transparentBlit(u32* out, s32 pitch)
{
	const u8* in = platform.studio->tic->ram.vram.screen.data;
	const u8* end = in + sizeof(platform.studio->tic->ram.vram.screen);
	const u32* pal = tic_palette_blit(&platform.studio->tic->config.palette);
	const u32 Delta = (pitch/sizeof *out - TIC80_WIDTH);

	s32 col = 0;

	while(in != end)
	{
		u8 low = *in & 0x0f;
		u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
		*out++ = low ? (*(pal + low) | 0xff000000) : 0;
		*out++ = hi ? (*(pal + hi) | 0xff000000) : 0;
		in++;

		col += BITS_IN_BYTE / TIC_PALETTE_BPP;

		if (col == TIC80_WIDTH)
		{
			col = 0;
			out += Delta;
		}
	}
}

static void initTouchGamepad()
{
	if (!platform.renderer)
		return;

	platform.studio->tic->api.map(platform.studio->tic, &platform.studio->tic->config.bank0.map, &platform.studio->tic->config.bank0.tiles, 0, 0, TIC_MAP_SCREEN_WIDTH, TIC_MAP_SCREEN_HEIGHT, 0, 0, -1, 1);

	if(!platform.gamepad.texture)
	{
		platform.gamepad.texture = SDL_CreateTexture(platform.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);
		SDL_SetTextureBlendMode(platform.gamepad.texture, SDL_BLENDMODE_BLEND);
	}

	{
		void* pixels = NULL;
		s32 pitch = 0;

		SDL_LockTexture(platform.gamepad.texture, NULL, &pixels, &pitch);
		transparentBlit(pixels, pitch);
		SDL_UnlockTexture(platform.gamepad.texture);
	}

	updateGamepadParts();
}

static void calcTextureRect(SDL_Rect* rect)
{
	// SDL_GetWindowSize(platform.window, &rect->w, &rect->h);

	rect->w = TIC80_FULLWIDTH * STUDIO_UI_SCALE;
	rect->h = TIC80_FULLHEIGHT * STUDIO_UI_SCALE;

	if (rect->w * TIC80_HEIGHT < rect->h * TIC80_WIDTH)
	{
		s32 discreteWidth = rect->w - rect->w % TIC80_WIDTH;
		s32 discreteHeight = TIC80_HEIGHT * discreteWidth / TIC80_WIDTH;

		rect->x = (rect->w - discreteWidth) / 2;

		rect->y = rect->w > rect->h 
			? (rect->h - discreteHeight) / 2 
			: OFFSET_LEFT*discreteWidth/TIC80_WIDTH;

		rect->w = discreteWidth;
		rect->h = discreteHeight;

	}
	else
	{
		s32 discreteHeight = rect->h - rect->h % TIC80_HEIGHT;
		s32 discreteWidth = TIC80_WIDTH * discreteHeight / TIC80_HEIGHT;

		rect->x = (rect->w - discreteWidth) / 2;
		rect->y = (rect->h - discreteHeight) / 2;

		rect->w = discreteWidth;
		rect->h = discreteHeight;
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

		if(rect.w) input->mouse.x = (mx - rect.x) * TIC80_WIDTH / rect.w;
		if(rect.h) input->mouse.y = (my - rect.y) * TIC80_HEIGHT / rect.h;
	}

	{
		input->mouse.left = mb & SDL_BUTTON_LMASK ? 1 : 0;
		input->mouse.middle = mb & SDL_BUTTON_MMASK ? 1 : 0;
		input->mouse.right = mb & SDL_BUTTON_RMASK ? 1 : 0;
	}
}

static void processKeyboard()
{
	static const u8 KeyboardCodes[] = 
	{
		#include "keycodes.c"
	};

	tic80_input* input = &platform.studio->tic->ram.input;
	input->keyboard.data = 0;

	const u8* keyboard = SDL_GetKeyboardState(NULL);

	for(s32 i = 0, c = 0; i < COUNT_OF(KeyboardCodes) && c < COUNT_OF(input->keyboard.keys); i++)
		if(keyboard[i] && KeyboardCodes[i] > tic_key_unknown)
			input->keyboard.keys[c++] = KeyboardCodes[i];
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

		// very strange, but on Android id always == 0
		//if (id)
		{
			s32 fingers = SDL_GetNumTouchFingers(id);

			if(fingers)
			{
				platform.gamepad.counter = 0;

				if (!platform.gamepad.show)
				{
					platform.gamepad.alpha = platform.studio->config()->theme.gamepad.touch.alpha;
					SDL_SetTextureAlphaMod(platform.gamepad.texture, platform.gamepad.alpha);
					platform.gamepad.show = true;
					return false;
				}
			}

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

static void processTouchGamepad()
{
	platform.gamepad.touch.data = 0;

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

								for(s32 i = 0; i < TIC80_KEY_BUFFER; i++)
								{
									if(!tic->ram.input.keyboard.keys[i])
									{
										tic->ram.input.keyboard.keys[i] = tic_key_escape;
										break;
									}
								}
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
#if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)
	processTouchGamepad();
#endif
	processJoysticks();
	
	{
		platform.studio->tic->ram.input.gamepads.data = 0;

		platform.studio->tic->ram.input.gamepads.data |= platform.gamepad.touch.data;
		platform.studio->tic->ram.input.gamepads.data |= platform.gamepad.joystick.data;
	}
}

static void pollEvent()
{
	tic80_input* input = &platform.studio->tic->ram.input;

	{
		input->mouse.btns = 0;
	}

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
			case SDL_WINDOWEVENT_RESIZED: updateGamepadParts(); break;
			case SDL_WINDOWEVENT_FOCUS_GAINED: platform.studio->updateProject(); break;
			}
			break;
		case SDL_QUIT:
			platform.studio->exit();
			break;
		default:
			break;
		}
	}

	processMouse();
	processKeyboard();
	processGamepad();
}

static void blitTexture()
{
	tic_mem* tic = platform.studio->tic;

	SDL_Rect rect = {0, 0, 0, 0};
	calcTextureRect(&rect);

	void* pixels = NULL;
	s32 pitch = 0;
	SDL_LockTexture(platform.texture, NULL, &pixels, &pitch);

	platform.studio->tick();

	memcpy(pixels, tic->screen, sizeof tic->screen);

	SDL_UnlockTexture(platform.texture);

	{
		enum {Header = OFFSET_TOP};
		SDL_Rect srcRect = {0, 0, TIC80_FULLWIDTH, Header};
		SDL_Rect dstRect = {0};
		SDL_GetWindowSize(platform.window, &dstRect.w, &dstRect.h);
		dstRect.h = rect.y;
		SDL_RenderCopy(platform.renderer, platform.texture, &srcRect, &dstRect);
	}

	{
		enum {Header = OFFSET_TOP};
		SDL_Rect srcRect = {0, TIC80_FULLHEIGHT - Header, TIC80_FULLWIDTH, Header};
		SDL_Rect dstRect = {0};
		SDL_GetWindowSize(platform.window, &dstRect.w, &dstRect.h);
		dstRect.y = rect.y + rect.h;
		dstRect.h = rect.y;
		SDL_RenderCopy(platform.renderer, platform.texture, &srcRect, &dstRect);
	}

	{
		enum {Header = OFFSET_TOP};
		enum {Left = OFFSET_LEFT};
		SDL_Rect srcRect = {0, Header, Left, TIC80_HEIGHT};
		SDL_Rect dstRect = {0};
		SDL_GetWindowSize(platform.window, &dstRect.w, &dstRect.h);
		dstRect.y = rect.y;
		dstRect.h = rect.h;
		SDL_RenderCopy(platform.renderer, platform.texture, &srcRect, &dstRect);
	}

	{
		enum {Top = OFFSET_TOP};
		enum {Left = OFFSET_LEFT};

		SDL_Rect srcRect = {Left, Top, TIC80_WIDTH, TIC80_HEIGHT};

		SDL_RenderCopy(platform.renderer, platform.texture, &srcRect, &rect);
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

static void renderGamepad()
{
	if(platform.gamepad.show || platform.gamepad.alpha); else return;

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

	enum {ButtonsCount = 8};

	for(s32 i = 0; i < COUNT_OF(Tiles); i++)
	{
		const Tile* tile = Tiles + i;
		SDL_Rect src = {(tile->press ? ButtonsCount + i : i) * TIC_SPRITESIZE, 0, TIC_SPRITESIZE, TIC_SPRITESIZE};
		SDL_Rect dest = {tile->x, tile->y, tileSize, tileSize};

		SDL_RenderCopy(platform.renderer, platform.gamepad.texture, &src, &dest);
	}

	if(!platform.gamepad.show && platform.gamepad.alpha)
	{
		enum {Step = 3};

		if(platform.gamepad.alpha - Step >= 0) platform.gamepad.alpha -= Step;
		else platform.gamepad.alpha = 0;

		SDL_SetTextureAlphaMod(platform.gamepad.texture, platform.gamepad.alpha);
	}

	platform.gamepad.counter = platform.gamepad.touch.data ? 0 : platform.gamepad.counter+1;

	// wait 5 seconds and hide touch gamepad
	if(platform.gamepad.counter >= 5 * TIC_FRAMERATE)
		platform.gamepad.show = false;
}

static void blitCursor(const u8* in)
{
	if(!platform.mouse.texture)
	{
		platform.mouse.texture = SDL_CreateTexture(platform.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TIC_SPRITESIZE, TIC_SPRITESIZE);
		SDL_SetTextureBlendMode(platform.mouse.texture, SDL_BLENDMODE_BLEND);
	}

	if(platform.mouse.src != in)
	{
		platform.mouse.src = in;

		void* pixels = NULL;
		s32 pitch = 0;
		SDL_LockTexture(platform.mouse.texture, NULL, &pixels, &pitch);

		{
			const u8* end = in + sizeof(tic_tile);
			const u32* pal = tic_palette_blit(&platform.studio->tic->ram.vram.palette);
			u32* out = pixels;

			while(in != end)
			{
				u8 low = *in & 0x0f;
				u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
				*out++ = low ? (*(pal + low) | 0xff000000) : 0;
				*out++ = hi ? (*(pal + hi) | 0xff000000) : 0;

				in++;
			}
		}

		SDL_UnlockTexture(platform.mouse.texture);
	}

	SDL_Rect rect = {0, 0, 0, 0};
	calcTextureRect(&rect);
	s32 scale = rect.w / TIC80_WIDTH;

	SDL_Rect src = {0, 0, TIC_SPRITESIZE, TIC_SPRITESIZE};
	SDL_Rect dst = {0, 0, TIC_SPRITESIZE * scale, TIC_SPRITESIZE * scale};

	SDL_GetMouseState(&dst.x, &dst.y);

	if(platform.studio->config()->theme.cursor.pixelPerfect)
	{
		dst.x -= (dst.x - rect.x) % scale;
		dst.y -= (dst.y - rect.y) % scale;
	}

	if(SDL_GetWindowFlags(platform.window) & SDL_WINDOW_MOUSE_FOCUS)
		SDL_RenderCopy(platform.renderer, platform.mouse.texture, &src, &dst);
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
					blitCursor(platform.studio->tic->config.bank0.tiles.data[platform.studio->config()->theme.cursor.hand].data);
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
					blitCursor(platform.studio->tic->config.bank0.tiles.data[platform.studio->config()->theme.cursor.ibeam].data);
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
					blitCursor(platform.studio->tic->config.bank0.tiles.data[platform.studio->config()->theme.cursor.arrow].data);
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

static void tick()
{
	pollEvent();

	if(platform.studio->quit)
	{
#if defined __EMSCRIPTEN__
		emscripten_cancel_main_loop();
#endif
		return;
	}

	SDL_RenderClear(platform.renderer);
	
	blitTexture();
	renderCursor();
	renderGamepad();

	SDL_RenderPresent(platform.renderer);

	blitSound();
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
	// SDL_SetWindowFullscreen(platform.window, SDL_GetWindowFlags(platform.window) & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
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
	mbstowcs(wcommand, command, FILENAME_MAX);

	_wsystem(wcommand);

#elif defined(__LINUX__)

	sprintf(command, "xdg-open \"%s\"", path);
	system(command);

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

static System systemInterface = 
{
	.setClipboardText = setClipboardText,
	.hasClipboardText = hasClipboardText,
	.getClipboardText = getClipboardText,
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
};

#if defined(__EMSCRIPTEN__)

static void emstick()
{
	static double nextTick = -1.0;

	platform.missedFrame = false;

	if(nextTick < 0.0)
		nextTick = emscripten_get_now();

	nextTick += 1000.0/TIC_FRAMERATE;
	tick();
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

Uint32 load_shader(GPU_ShaderEnum shader_type, const char* filename)
{
	SDL_RWops* rwops;
	Uint32 shader;
	char* source;
	int header_size, file_size;
	const char* header = "";
	GPU_Renderer* renderer = GPU_GetCurrentRenderer();
	
	// Open file
	rwops = SDL_RWFromFile(filename, "rb");
	if(rwops == NULL)
	{
		GPU_PushErrorCode("load_shader", GPU_ERROR_FILE_NOT_FOUND, "Shader file \"%s\" not found", filename);
		return 0;
	}
	
	// Get file size
	file_size = SDL_RWseek(rwops, 0, SEEK_END);
	SDL_RWseek(rwops, 0, SEEK_SET);
	
	// Get size from header
	if(renderer->shader_language == GPU_LANGUAGE_GLSL)
	{
		if(renderer->max_shader_version >= 120)
			header = "#version 120\n";
		else
			header = "#version 110\n";  // Maybe this is good enough?
	}
	else if(renderer->shader_language == GPU_LANGUAGE_GLSLES)
		header = "#version 100\nprecision mediump int;\nprecision mediump float;\n";
	
	header_size = strlen(header);
	
	// Allocate source buffer
	source = (char*)malloc(sizeof(char)*(header_size + file_size + 1));
	
	// Prepend header
	strcpy(source, header);
	
	// Read in source code
	SDL_RWread(rwops, source + strlen(source), 1, file_size);
	source[header_size + file_size] = '\0';
	
	// Compile the shader
	shader = GPU_CompileShader(shader_type, source);
	
	// Clean up
	free(source);
	SDL_RWclose(rwops);
	
	return shader;
}

GPU_ShaderBlock load_shader_program(Uint32* p, const char* vertex_shader_file, const char* fragment_shader_file)
{
	Uint32 v, f;
	v = load_shader(GPU_VERTEX_SHADER, vertex_shader_file);
	
	if(!v)
		GPU_LogError("Failed to load vertex shader (%s): %s\n", vertex_shader_file, GPU_GetShaderMessage());
	
	f = load_shader(GPU_PIXEL_SHADER, fragment_shader_file);
	
	if(!f)
		GPU_LogError("Failed to load fragment shader (%s): %s\n", fragment_shader_file, GPU_GetShaderMessage());
	
	*p = GPU_LinkShaders(v, f);
	
	if(!*p)
	{
		GPU_ShaderBlock b = {-1, -1, -1, -1};
		GPU_LogError("Failed to link shader program (%s + %s): %s\n", vertex_shader_file, fragment_shader_file, GPU_GetShaderMessage());
		return b;
	}
	
	{
		GPU_ShaderBlock block = GPU_LoadShaderBlock(*p, "gpu_Vertex", "gpu_TexCoord", "gpu_Color", "gpu_ModelViewProjectionMatrix");
		GPU_ActivateShaderProgram(*p, &block);

		return block;
	}
}

void free_shader(Uint32 p)
{
	GPU_FreeShaderProgram(p);
}

void update_color_shader(float r, float g, float b, float a, int color_loc)
{
	float fcolor[4] = {r, g, b, a};
	GPU_SetUniformfv(color_loc, 4, 1, fcolor);
}

#include <math.h>

static s32 start(s32 argc, char **argv, const char* folder)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

	initSound();

	platform.net = createNet();

	platform.studio = studioInit(argc, argv, platform.audio.spec.freq, folder, &systemInterface);
	tic_mem* tic = platform.studio->tic;

	setWindowIcon();

	initTouchGamepad();

	GPU_Target* screen = GPU_Init(TIC80_FULLWIDTH * STUDIO_UI_SCALE, TIC80_FULLHEIGHT * STUDIO_UI_SCALE, GPU_INIT_DISABLE_VSYNC);
	GPU_Image* texture = GPU_CreateImage(TIC80_FULLWIDTH, TIC80_FULLHEIGHT, GPU_FORMAT_BGRA);

	s32 color_shader = 0;
	GPU_ShaderBlock color_block = load_shader_program(&color_shader, "data/shaders/common.vert", "data/shaders/color.frag");
	int color_loc = GPU_GetUniformLocation(color_shader, "myColor");

	{
		u64 nextTick = SDL_GetPerformanceCounter();
		const u64 Delta = SDL_GetPerformanceFrequency() / TIC_FRAMERATE;

		while (!platform.studio->quit)
		{
			platform.missedFrame = false;

			nextTick += Delta;
			
			{
				pollEvent();

				float t = SDL_GetTicks()/1000.0f;

				GPU_Clear(screen);
			
				{
					platform.studio->tick();
					GPU_UpdateImageBytes(texture, NULL, (const u8*)tic->screen, TIC80_FULLWIDTH * sizeof(u32));
				}

				GPU_ActivateShaderProgram(color_shader, &color_block);
				update_color_shader((1+sin(t))/2, (1+sin(t+1))/2, (1+sin(t+2))/2, 1.0f, color_loc);

				// GPU_BlitScale(texture, NULL, screen, TIC80_FULLWIDTH/2*STUDIO_UI_SCALE, TIC80_FULLHEIGHT/2*STUDIO_UI_SCALE, STUDIO_UI_SCALE, STUDIO_UI_SCALE);
				GPU_Blit(texture, NULL, screen, TIC80_FULLWIDTH/2*STUDIO_UI_SCALE, TIC80_FULLHEIGHT/2*STUDIO_UI_SCALE);

				GPU_Flip(screen);

				blitSound();
			}

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

	platform.studio->close();

	closeNet(platform.net);

	SDL_CloseAudioDevice(platform.audio.device);

	GPU_FreeImage(texture);

	GPU_Quit();

	return 0;
}

static s32 start2(s32 argc, char **argv, const char* folder)
{
	SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

	initSound();

	platform.net = createNet();

	platform.window = SDL_CreateWindow( TIC_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		(TIC80_FULLWIDTH) * STUDIO_UI_SCALE,
		(TIC80_FULLHEIGHT) * STUDIO_UI_SCALE,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
#if defined(__CHIP__)
			| SDL_WINDOW_FULLSCREEN_DESKTOP
#endif
		);

	platform.studio = studioInit(argc, argv, platform.audio.spec.freq, folder, &systemInterface);

	// set the window icon before renderer is created (issues on Linux)
	setWindowIcon();

	platform.renderer = SDL_CreateRenderer(platform.window, -1, 
#if defined(__CHIP__)
		SDL_RENDERER_SOFTWARE
#else
		SDL_RENDERER_ACCELERATED | (platform.studio->config()->useVsync ? SDL_RENDERER_PRESENTVSYNC : 0)
#endif
	);

	platform.texture = SDL_CreateTexture(platform.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);

	initTouchGamepad();

#if defined(__EMSCRIPTEN__)

	emscripten_set_main_loop(platform.studio->config()->useVsync ? tick : emstick, 0, 1);

#else
	{
		u64 nextTick = SDL_GetPerformanceCounter();
		const u64 Delta = SDL_GetPerformanceFrequency() / TIC_FRAMERATE;

		while (!platform.studio->quit)
		{
			platform.missedFrame = false;

			nextTick += Delta;

			tick();

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

	if(platform.mouse.texture)
		SDL_DestroyTexture(platform.mouse.texture);

	SDL_DestroyTexture(platform.gamepad.texture);

	SDL_DestroyTexture(platform.texture);
	SDL_DestroyRenderer(platform.renderer);
	SDL_DestroyWindow(platform.window);

	SDL_CloseAudioDevice(platform.audio.device);

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
