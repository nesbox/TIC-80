#include "main.h"
#include "studio.h"

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

	bool quitFlag;
	bool missedFrame;
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

static void setWindowIcon()
{
	// enum{ Size = 64, TileSize = 16, ColorKey = 14, Cols = TileSize / TIC_SPRITESIZE, Scale = Size/TileSize};
	// platform.tic->api.clear(platform.tic, 0);

	// u32* pixels = SDL_malloc(Size * Size * sizeof(u32));

	// const u32* pal = tic_palette_blit(&platform.tic->config.palette);

	// for(s32 j = 0, index = 0; j < Size; j++)
	// 	for(s32 i = 0; i < Size; i++, index++)
	// 	{
	// 		u8 color = getSpritePixel(platform.tic->config.bank0.tiles.data, i/Scale, j/Scale);
	// 		pixels[index] = color == ColorKey ? 0 : pal[color];
	// 	}

	// SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, Size, Size,
	// 	sizeof(s32) * BITS_IN_BYTE, Size * sizeof(s32),
	// 	0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);

	// SDL_SetWindowIcon(platform.window, surface);
	// SDL_FreeSurface(surface);
	// SDL_free(pixels);
}

static void updateGamepadParts()
{
	// s32 tileSize = TIC_SPRITESIZE;
	// s32 offset = 0;
	// SDL_Rect rect;

	// const s32 JoySize = 3;
	// SDL_GetWindowSize(platform.window, &rect.w, &rect.h);

	// if(rect.w < rect.h)
	// {
	// 	tileSize = rect.w / 2 / JoySize;
	// 	offset = (rect.h * 2 - JoySize * tileSize) / 3;
	// }
	// else
	// {
	// 	tileSize = rect.w / 5 / JoySize;
	// 	offset = (rect.h - JoySize * tileSize) / 2;
	// }

	// platform.gamepad.part.size = tileSize;
	// platform.gamepad.part.axis = (SDL_Point){0, offset};
	// platform.gamepad.part.a = (SDL_Point){rect.w - 2*tileSize, 2*tileSize + offset};
	// platform.gamepad.part.b = (SDL_Point){rect.w - 1*tileSize, 1*tileSize + offset};
	// platform.gamepad.part.x = (SDL_Point){rect.w - 3*tileSize, 1*tileSize + offset};
	// platform.gamepad.part.y = (SDL_Point){rect.w - 2*tileSize, 0*tileSize + offset};
}

static void transparentBlit(u32* out, s32 pitch)
{
	// const u8* in = platform.tic->ram.vram.screen.data;
	// const u8* end = in + sizeof(platform.tic->ram.vram.screen);
	// const u32* pal = tic_palette_blit(&platform.tic->config.palette);
	// const u32 Delta = (pitch/sizeof *out - TIC80_WIDTH);

	// s32 col = 0;

	// while(in != end)
	// {
	// 	u8 low = *in & 0x0f;
	// 	u8 hi = (*in & 0xf0) >> TIC_PALETTE_BPP;
	// 	*out++ = low ? (*(pal + low) | 0xff000000) : 0;
	// 	*out++ = hi ? (*(pal + hi) | 0xff000000) : 0;
	// 	in++;

	// 	col += BITS_IN_BYTE / TIC_PALETTE_BPP;

	// 	if (col == TIC80_WIDTH)
	// 	{
	// 		col = 0;
	// 		out += Delta;
	// 	}
	// }
}

static void initTouchGamepad()
{
	// if (!platform.renderer)
	// 	return;

	// platform.tic->api.map(platform.tic, &platform.tic->config.bank0.map, &platform.tic->config.bank0.tiles, 0, 0, TIC_MAP_SCREEN_WIDTH, TIC_MAP_SCREEN_HEIGHT, 0, 0, -1, 1);

	// if(!platform.gamepad.texture)
	// {
	// 	platform.gamepad.texture = SDL_CreateTexture(platform.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);
	// 	SDL_SetTextureBlendMode(platform.gamepad.texture, SDL_BLENDMODE_BLEND);
	// }

	// {
	// 	void* pixels = NULL;
	// 	s32 pitch = 0;

	// 	SDL_LockTexture(platform.gamepad.texture, NULL, &pixels, &pitch);
	// 	transparentBlit(pixels, pitch);
	// 	SDL_UnlockTexture(platform.gamepad.texture);
	// }

	// updateGamepadParts();
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

	// for(int i = 0; i < COUNT_OF(studioImpl.mouse.state); i++)
	// {
	// 	MouseState* state = &studioImpl.mouse.state[i];

	// 	if(!state->down && (studioImpl.mouse.button & SDL_BUTTON(i + 1)))
	// 	{
	// 		state->down = true;

	// 		state->start.x = studioImpl.mouse.cursor.x;
	// 		state->start.y = studioImpl.mouse.cursor.y;
	// 	}
	// 	else if(state->down && !(studioImpl.mouse.button & SDL_BUTTON(i + 1)))
	// 	{
	// 		state->end.x = studioImpl.mouse.cursor.x;
	// 		state->end.y = studioImpl.mouse.cursor.y;

	// 		state->click = true;
	// 		state->down = false;
	// 	}
	// }
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

static void pollEvent()
{
	SDL_Event event;

	if(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
// 		case SDL_KEYDOWN:
// 			if(processShortcuts(&event.key)) return NULL;
// 			break;
// 		case SDL_JOYDEVICEADDED:
// 			{
// 				s32 id = event.jdevice.which;

// 				if (id < TIC_GAMEPADS)
// 				{
// 					if(platform.joysticks[id])
// 						SDL_JoystickClose(platform.joysticks[id]);

// 					platform.joysticks[id] = SDL_JoystickOpen(id);
// 				}
// 			}
// 			break;

// 		case SDL_JOYDEVICEREMOVED:
// 			{
// 				s32 id = event.jdevice.which;

// 				if (id < TIC_GAMEPADS && platform.joysticks[id])
// 				{
// 					SDL_JoystickClose(platform.joysticks[id]);
// 					platform.joysticks[id] = NULL;
// 				}
// 			}
// 			break;
// 		case SDL_WINDOWEVENT:
// 			switch(event.window.event)
// 			{
// 			case SDL_WINDOWEVENT_RESIZED: updateGamepadParts(); break;
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
// 				break;
// 			}
// 			break;
// 		case SDL_FINGERUP:
// 			showSoftKeyboard();
// 			break;
		case SDL_QUIT:
			platform.quitFlag = true;
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
	processKeyboard();

	// if(platform.mode == TIC_RUN_MODE)
	// {
	// 	if(platform.tic->input.gamepad) 	processGamepadInput();
	// 	if(platform.tic->input.mouse) 	processMouseInput();
	// 	if(platform.tic->input.keyboard) 	processKeyboardInput();
	// }
	// else
	// {
	// 	processGamepadInput();
	// }
}

static void blitTexture()
{
	// tic_mem* tic = platform.tic;
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

static void tick()
{
	pollEvent();

	// if(!platform.fs) return;

// 	if(platform.quitFlag)
// 	{
// #if defined __EMSCRIPTEN__
// 		platform.tic->api.clear(platform.tic, TIC_COLOR_BG);
// 		blitTexture();
// 		emscripten_cancel_main_loop();
// #endif
// 		return;
// 	}

	// SDL_SystemCursor cursor = platform.mouse.system;
	// platform.mouse.system = SDL_SYSTEM_CURSOR_ARROW;

	SDL_RenderClear(platform.renderer);

	

	// if(platform.mode == TIC_RUN_MODE && platform.tic->input.gamepad)
		// renderGamepad();

	// if(platform.mode == TIC_MENU_MODE || platform.mode == TIC_SURF_MODE)
		// renderGamepad();

	// if(platform.mouse.system != cursor)
		// SDL_SetCursor(SDL_CreateSystemCursor(platform.mouse.system));

	blitTexture();

	SDL_RenderPresent(platform.renderer);

	blitSound();
}

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

	// set the window icon before renderer is created (issues on Linux)
	setWindowIcon();

	platform.renderer = SDL_CreateRenderer(platform.window, -1, 
#if defined(__CHIP__)
		SDL_RENDERER_SOFTWARE
#elif defined(__EMSCRIPTEN__)
		SDL_RENDERER_ACCELERATED
#else
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC//(getConfig()->useVsync ? SDL_RENDERER_PRESENTVSYNC : 0)
#endif
	);

	platform.texture = SDL_CreateTexture(platform.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);

	initTouchGamepad();

	platform.studio = studioInit(argc, argv, platform.audio.spec.freq);

#if defined(__EMSCRIPTEN__)
	emscripten_set_main_loop(emstick, TIC_FRAMERATE, 1);
#else
	{

		bool useDelay = false;
		{
			SDL_RendererInfo info;
			SDL_DisplayMode mode;

			SDL_GetRendererInfo(platform.renderer, &info);
			SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(platform.window), &mode);

			useDelay = !(info.flags & SDL_RENDERER_PRESENTVSYNC) || mode.refresh_rate > TIC_FRAMERATE;
		}

		u64 nextTick = SDL_GetPerformanceCounter();
		const u64 Delta = SDL_GetPerformanceFrequency() / TIC_FRAMERATE;

		while (!platform.quitFlag)
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
				else
				{
					if(useDelay || SDL_GetWindowFlags(platform.window) & SDL_WINDOW_MINIMIZED)
					{
						u32 time = (u32)(delay * 1000 / SDL_GetPerformanceFrequency());
						if(time >= 10)
							SDL_Delay(time);
					}
				}
			}
		}
	}

#endif

	studioClose();

	if(platform.audio.cvt.buf)
		SDL_free(platform.audio.cvt.buf);

	// if(platform.mouse.texture)
		// SDL_DestroyTexture(platform.mouse.texture);

	SDL_DestroyTexture(platform.texture);
	SDL_DestroyRenderer(platform.renderer);
	SDL_DestroyWindow(platform.window);

	SDL_CloseAudioDevice(platform.audio.device);

	return 0;
}
