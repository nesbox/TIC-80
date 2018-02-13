#include "main.h"
#include "studio.h"
#include "net.h"
#include <SDL.h>

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
		SDL_AudioSpec 		spec;
		SDL_AudioDeviceID 	device;
		SDL_AudioCVT 		cvt;
	} audio;

	SDL_Joystick* joysticks[TIC_GAMEPADS];

	struct
	{
		// tic80_gamepads keyboard;
		tic80_gamepads touch;
		tic80_gamepads joystick;

		SDL_Texture* texture;

		bool show;
		s32 counter;
		s32 alpha;
		bool backProcessed;

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

	bool missedFrame;
	bool fullscreen;
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

static u8* _getSpritePtr(tic_tile* tiles, s32 x, s32 y)
{
	enum { SheetCols = (TIC_SPRITESHEET_SIZE / TIC_SPRITESIZE) };
	return tiles[x / TIC_SPRITESIZE + y / TIC_SPRITESIZE * SheetCols].data;
}

static u8 _getSpritePixel(tic_tile* tiles, s32 x, s32 y)
{
	// TODO: check spritesheet rect
	return tic_tool_peek4(_getSpritePtr(tiles, x, y), (x % TIC_SPRITESIZE) + (y % TIC_SPRITESIZE) * TIC_SPRITESIZE);
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
			u8 color = _getSpritePixel(platform.studio->tic->config.bank0.tiles.data, i/Scale, j/Scale);
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
	SDL_GetWindowSize(platform.window, &rect->w, &rect->h);

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
		input->mouse.left = mb & SDL_BUTTON_LMASK;
		input->mouse.middle = mb & SDL_BUTTON_MMASK;
		input->mouse.right = mb & SDL_BUTTON_RMASK;
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
					platform.gamepad.alpha = getConfig()->theme.gamepad.touch.alpha;
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

	for(s32 i = 0; i < COUNT_OF(platform.joysticks); i++)
	{
		SDL_Joystick* joystick = platform.joysticks[i];

		if(joystick && SDL_JoystickGetAttached(joystick))
		{
			tic80_gamepad* gamepad = NULL;

			switch(index)
			{
			case 0: gamepad = &platform.gamepad.joystick.first; break;
			case 1: gamepad = &platform.gamepad.joystick.second; break;
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

						// for(s32 i = 5; i < numButtons; i++)
						// {
						// 	s32 back = SDL_JoystickGetButton(joystick, i);

						// 	if(back)
						// 	{
						// 		if(!platform.gamepad.backProcessed)
						// 		{
						// 			if(isGameMenu())
						// 			{
						// 				platform.mode == TIC_MENU_MODE ? hideGameMenu() : showGameMenu();
						// 				platform.gamepad.backProcessed = true;
						// 			}
						// 		}

						// 		return;
						// 	}
						// }

						// platform.gamepad.backProcessed = false;
					}
				}

				index++;
			}
		}
	}
}

static void processGamepad()
{
	// processKeyboardGamepad();

#if !defined(__EMSCRIPTEN__) && !defined(__MACOSX__)
	processTouchGamepad();
#endif
	processJoysticks();
	
	{
		platform.studio->tic->ram.input.gamepads.data = 0;

		// platform.studio->tic->ram.input.gamepads.data |= platform.gamepad.keyboard.data;
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
// 		case SDL_KEYDOWN:
// 			if(processShortcuts(&event.key)) return NULL;
// 			break;
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
					if(platform.joysticks[id])
						SDL_JoystickClose(platform.joysticks[id]);

					platform.joysticks[id] = SDL_JoystickOpen(id);
				}
			}
			break;

		case SDL_JOYDEVICEREMOVED:
			{
				s32 id = event.jdevice.which;

				if (id < TIC_GAMEPADS && platform.joysticks[id])
				{
					SDL_JoystickClose(platform.joysticks[id]);
					platform.joysticks[id] = NULL;
				}
			}
			break;
		case SDL_WINDOWEVENT:
			switch(event.window.event)
			{
			case SDL_WINDOWEVENT_RESIZED: updateGamepadParts(); break;
// 			case SDL_WINDOWEVENT_FOCUS_GAINED:

// #if defined(TIC80_PRO)

// 				if(platform.mode != TIC_START_MODE)
// 				{
// 					Console* console = platform.console;

// 					u64 mdate = fsMDate(console->fs, console->romName);

// 					if(platform.cart.mdate && mdate > platform.cart.mdate)
// 					{
// 						if(studioCartChanged())
// 						{
// 							static const char* Rows[] =
// 							{
// 								"",
// 								"CART HAS CHANGED!",
// 								"",
// 								"DO YOU WANT",
// 								"TO RELOAD IT?"
// 							};

// 							showDialog(Rows, COUNT_OF(Rows), reloadConfirm, NULL);
// 						}
// 						else console->updateProject(console);						
// 					}
// 				}

// #endif
// 				{
// 					Code* code = platform.editor[platform.bank.index.code].code;
// 					platform.console->codeLiveReload.reload(platform.console, code->src);
// 					if(platform.console->codeLiveReload.active && code->update)
// 						code->update(code);
// 				}
				// break;
			}
			break;
// 		case SDL_FINGERUP:
// 			showSoftKeyboard();
// 			break;
		case SDL_QUIT:
			platform.studio->quit = true;
			// exitStudio();
			break;
		default:
			break;
		}

		// return &event;
	}

	// if(platform.mode != TIC_RUN_MODE)
		// processGesture();

	// if(!platform.gesture.active)

	processMouse();
	processGamepad();
	processKeyboard();

	// if(platform.mode == TIC_RUN_MODE)
	// {
	// 	if(platform.studio->tic->input.gamepad) 	processGamepadInput();
	// 	if(platform.studio->tic->input.mouse) 	processMouseInput();
	// 	if(platform.studio->tic->input.keyboard) 	processKeyboardInput();
	// }
	// else
	// {
	// 	processGamepadInput();
	// }
}

static void blitTexture()
{
	// tic_mem* tic = platform.studio->tic;
	SDL_Rect rect = {0, 0, 0, 0};
	calcTextureRect(&rect);

	void* pixels = NULL;
	s32 pitch = 0;
	SDL_LockTexture(platform.texture, NULL, &pixels, &pitch);

	studioTick(pixels);

	// tic_scanline scanline = NULL;
	// tic_overlap overlap = NULL;
	// void* data = NULL;

	// switch(platform.mode)
	// {
	// case TIC_RUN_MODE:
	// 	scanline = tic->api.scanline;
	// 	overlap = tic->api.overlap;
	// 	break;
	// case TIC_SPRITE_MODE:
	// 	{
	// 		Sprite* sprite = platform.editor[platform.bank.index.sprites].sprite;
	// 		overlap = sprite->overlap;
	// 		data = sprite;
	// 	}
	// 	break;
	// case TIC_MAP_MODE:
	// 	{
	// 		Map* map = platform.editor[platform.bank.index.map].map;
	// 		overlap = map->overlap;
	// 		data = map;
	// 	}
	// 	break;
	// default:
	// 	break;
	// }

	// tic->api.blit(tic, scanline, overlap, data);
	// SDL_memcpy(pixels, tic->screen, sizeof tic->screen);

	// recordFrame(pixels);
	// drawDesyncLabel(pixels);

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

static void renderCursor()
{
	if(platform.studio->tic->ram.vram.vars.cursor.system)
	{
		SDL_SystemCursor sdlCursor = SDL_SYSTEM_CURSOR_ARROW;

		switch(platform.studio->tic->ram.vram.vars.cursor.sprite)
		{
		case tic_cursor_hand: sdlCursor = SDL_SYSTEM_CURSOR_HAND; break;
		case tic_cursor_ibeam: sdlCursor = SDL_SYSTEM_CURSOR_IBEAM; break;
		default: sdlCursor = SDL_SYSTEM_CURSOR_ARROW;
		}

		SDL_SetCursor(SDL_CreateSystemCursor(sdlCursor));
	}
	else
	{
		// render cursor here
	}

// 	if(studioImpl.mode == TIC_RUN_MODE && !studioImpl.studio.tic->input.mouse)
// 	{
// 		SDL_ShowCursor(SDL_DISABLE);
// 		return;
// 	}
// 	if(studioImpl.mode == TIC_RUN_MODE && studioImpl.studio.tic->ram.vram.vars.cursor)
// 	{
// 		SDL_ShowCursor(SDL_DISABLE);
// 		blitCursor(studioImpl.studio.tic->ram.sprites.data[studioImpl.studio.tic->ram.vram.vars.cursor].data);
// 		return;
// 	}

// 	SDL_ShowCursor(getConfig()->theme.cursor.sprite >= 0 ? SDL_DISABLE : SDL_ENABLE);

// 	if(getConfig()->theme.cursor.sprite >= 0)
// 		blitCursor(studioImpl.studio.tic->config.bank0.tiles.data[getConfig()->theme.cursor.sprite].data);
}

static void tick()
{
	pollEvent();

	// if(!platform.fs) return;

// 	if(platform.quit)
// 	{
// #if defined __EMSCRIPTEN__
// 		platform.studio->tic->api.clear(platform.studio->tic, TIC_COLOR_BG);
// 		blitTexture();
// 		emscripten_cancel_main_loop();
// #endif
// 		return;
// 	}

	// SDL_SystemCursor cursor = platform.mouse.system;
	// platform.mouse.system = SDL_SYSTEM_CURSOR_ARROW;

	SDL_RenderClear(platform.renderer);

	
	blitTexture();

	renderCursor();

	// if(platform.mode == TIC_RUN_MODE && platform.studio->tic->input.gamepad)
	renderGamepad();

	// if(platform.mode == TIC_MENU_MODE || platform.mode == TIC_SURF_MODE)
		// renderGamepad();

	// if(platform.mouse.system != cursor)
		// SDL_SetCursor(SDL_CreateSystemCursor(platform.mouse.system));


	SDL_RenderPresent(platform.renderer);

	blitSound();
}

// should work async with callback
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
		free(path);

#endif
		
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
				FS.syncfs(true, function(error)
				{
					if(error) console.log(error);
					else Runtime.dynCall('vi', $1, [$2]);
				});			
			}, appFolder, callback, fs
		);
#endif

	return appFolder;
}

static void _setClipboardText(const char* text)
{
	SDL_SetClipboardText(text);
}

static bool _hasClipboardText()
{
	return SDL_HasClipboardText();
}

static char* _getClipboardText()
{
	return SDL_GetClipboardText();
}

static u64 _getPerformanceCounter()
{
	return SDL_GetPerformanceCounter();
}

static u64 _getPerformanceFrequency()
{
	return SDL_GetPerformanceFrequency();
}

static void _goFullscreen()
{
	platform.fullscreen = !platform.fullscreen;
	SDL_SetWindowFullscreen(platform.window, platform.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

static System sysHandlers = 
{
	.setClipboardText = _setClipboardText,
	.hasClipboardText = _hasClipboardText,
	.getClipboardText = _getClipboardText,
	.getPerformanceCounter = _getPerformanceCounter,
	.getPerformanceFrequency = _getPerformanceFrequency,

	.netGetRequest = netGetRequest,
	.createNet = createNet,
	.closeNet = closeNet,

	.file_dialog_load = file_dialog_load,
	.file_dialog_save = file_dialog_save,

	.goFullscreen = _goFullscreen,
};

s32 main(s32 argc, char **argv)
{
	SDL_SetHint(SDL_HINT_WINRT_HANDLE_BACK_BUTTON, "1");
	SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

	initSound();

	platform.window = SDL_CreateWindow( TIC_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		(TIC80_FULLWIDTH) * STUDIO_UI_SCALE,
		(TIC80_FULLHEIGHT) * STUDIO_UI_SCALE,
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
#if defined(__CHIP__)
			| SDL_WINDOW_FULLSCREEN_DESKTOP
#endif
		);

	platform.studio = studioInit(argc, argv, platform.audio.spec.freq, getAppFolder(), &sysHandlers);

	// set the window icon before renderer is created (issues on Linux)
	setWindowIcon();

	platform.renderer = SDL_CreateRenderer(platform.window, -1, 
#if defined(__CHIP__)
		SDL_RENDERER_SOFTWARE
#elif defined(__EMSCRIPTEN__)
		SDL_RENDERER_ACCELERATED
#else
		// TODO: uncomment this later, also init FS before read config
		SDL_RENDERER_ACCELERATED// | (getConfig()->useVsync ? SDL_RENDERER_PRESENTVSYNC : 0)
#endif
	);

	platform.texture = SDL_CreateTexture(platform.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);

	initTouchGamepad();

#if defined(__EMSCRIPTEN__)

	// call this after FS is initialized
	emscripten_set_main_loop(getConfig()->useVsync ? tick : emstick, TIC_FRAMERATE, 1);
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

	studioClose();

	if(platform.audio.cvt.buf)
		SDL_free(platform.audio.cvt.buf);

	// if(platform.mouse.texture)
		// SDL_DestroyTexture(platform.mouse.texture);

	SDL_DestroyTexture(platform.gamepad.texture);

	SDL_DestroyTexture(platform.texture);
	SDL_DestroyRenderer(platform.renderer);
	SDL_DestroyWindow(platform.window);

	SDL_CloseAudioDevice(platform.audio.device);

	return 0;
}
