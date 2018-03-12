#include "../system.h"
#include "../net.h"
#include "../tools.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <SDL.h>

#define STUDIO_UI_SCALE 2
#define STUDIO_PIXEL_FORMAT SDL_PIXELFORMAT_ABGR8888
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
		tic80_gamepads joystick;
	} gamepad;

	struct
	{
		SDL_Texture* texture;
		const u8* src;
	} mouse;

	Net* net;

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

static void calcTextureRect(SDL_Rect* rect)
{
	SDL_GetWindowSize(platform.window, &rect->w, &rect->h);

	{
		enum{Width = TIC80_WIDTH, Height = TIC80_HEIGHT};

		s32 discreteHeight = rect->h - rect->h % Height;
		s32 discreteWidth = Width * discreteHeight / Height;

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
		#include "../keycodes.c"
	};

	tic80_input* input = &platform.studio->tic->ram.input;
	input->keyboard.data = 0;

	const u8* keyboard = SDL_GetKeyboardState(NULL);

	for(s32 i = 0, c = 0; i < COUNT_OF(KeyboardCodes) && c < COUNT_OF(input->keyboard.keys); i++)
		if(keyboard[i] && KeyboardCodes[i] > tic_key_unknown)
			input->keyboard.keys[c++] = KeyboardCodes[i];
}

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
	processJoysticks();
	
	{
		platform.studio->tic->ram.input.gamepads.data = 0;
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

static void tick()
{
	pollEvent();

	if(platform.studio->quit)
		return;

	SDL_RenderClear(platform.renderer);
	
	blitTexture();
	renderCursor();

	SDL_RenderPresent(platform.renderer);

	blitSound();
}

static const char* getAppFolder()
{
	static char appFolder[FILENAME_MAX];

	char* path = SDL_GetPrefPath(TIC_PACKAGE, TIC_NAME);
	strcpy(appFolder, path);
	SDL_free(path);

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
	SDL_SetWindowFullscreen(platform.window, SDL_GetWindowFlags(platform.window) & SDL_WINDOW_FULLSCREEN_DESKTOP ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
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

static void updateConfig() {}

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

s32 main(s32 argc, char **argv)
{
	const char* folder = getAppFolder();

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

	initSound();

	platform.net = createNet();

	platform.window = SDL_CreateWindow( TIC_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		(TIC80_FULLWIDTH) * STUDIO_UI_SCALE,
		(TIC80_FULLHEIGHT) * STUDIO_UI_SCALE,
		SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);

	platform.studio = studioInit(argc, argv, platform.audio.spec.freq, folder, &systemInterface);

	platform.renderer = SDL_CreateRenderer(platform.window, -1, SDL_RENDERER_SOFTWARE);
	platform.texture = SDL_CreateTexture(platform.renderer, STUDIO_PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, TEXTURE_SIZE, TEXTURE_SIZE);

	{
		u64 nextTick = SDL_GetPerformanceCounter();
		const u64 Delta = SDL_GetPerformanceFrequency() / TIC_FRAMERATE;

		while (!platform.studio->quit)
		{
			nextTick += Delta;

			tick();

			{
				s64 delay = nextTick - SDL_GetPerformanceCounter();

				if(delay < 0) nextTick -= delay;
				else SDL_Delay((u32)(delay * 1000 / SDL_GetPerformanceFrequency()));
			}
		}
	}

	platform.studio->close();

	closeNet(platform.net);

	if(platform.audio.cvt.buf)
		SDL_free(platform.audio.cvt.buf);

	if(platform.mouse.texture)
		SDL_DestroyTexture(platform.mouse.texture);

	SDL_DestroyTexture(platform.texture);
	SDL_DestroyRenderer(platform.renderer);
	SDL_DestroyWindow(platform.window);

	SDL_CloseAudioDevice(platform.audio.device);

	return 0;
}
