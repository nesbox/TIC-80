// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <string.h>
#include <stdio.h>
#include <SDL.h>
#include <tic80.h>

#define TIC80_WINDOW_SCALE 3
#define TIC80_WINDOW_TITLE "TIC-80"
#define TIC80_DEFAULT_CART "cart.tic"
#define TIC80_EXECUTABLE_NAME "player-sdl"

static struct
{
	bool quit;
} state =
{
	.quit = false,
};

static void onExit()
{
	state.quit = true;
}

s32 runCart(void* cart, s32 size)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	SDL_Window* window = SDL_CreateWindow(TIC80_WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, TIC80_FULLWIDTH * TIC80_WINDOW_SCALE, TIC80_FULLHEIGHT * TIC80_WINDOW_SCALE, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, TIC80_FULLWIDTH, TIC80_FULLHEIGHT);

	SDL_AudioDeviceID audioDevice = 0;
	SDL_AudioSpec audioSpec;
	SDL_AudioCVT cvt;
	bool audioStarted = false;
	s32 output = 0;

	{
		SDL_AudioSpec want =
		{
			.freq = TIC80_SAMPLERATE,
			.format = AUDIO_S16,
			.channels = 2,
			.userdata = NULL,
		};

		audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &audioSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);

		SDL_BuildAudioCVT(&cvt, want.format, want.channels, audioSpec.freq, audioSpec.format, audioSpec.channels, audioSpec.freq);

		if (cvt.needed)
		{
			cvt.len = audioSpec.freq * audioSpec.channels * sizeof(s16) / TIC80_FRAMERATE;
			cvt.buf = SDL_malloc(cvt.len * cvt.len_mult);
		}
	}

	tic80_input input;
	SDL_memset(&input, 0, sizeof input);

	tic80* tic = tic80_create(audioSpec.freq);
	tic->callback.exit = onExit;
	tic80_load(tic, cart, size);

	if(!tic)
	{
		fprintf(stderr, "Failed to load cart data.");
		output = 1;
	}
	else {
		u64 nextTick = SDL_GetPerformanceCounter();
		const u64 Delta = SDL_GetPerformanceFrequency() / TIC80_FRAMERATE;

		while(!state.quit)
		{
			SDL_Event event;

			while(SDL_PollEvent(&event))
			{
				switch(event.type)
				{
				case SDL_QUIT:
					state.quit = true;
					break;
				case SDL_KEYUP:
					// Quit when pressing the escape button.
					if(event.key.keysym.sym == SDLK_ESCAPE)
					{
						state.quit = true;
					}
					break;
				}
			}

			{
				input.gamepads.data = 0;
				const uint8_t* keyboard = SDL_GetKeyboardState(NULL);

				static const SDL_Scancode Keys[] =
				{
					SDL_SCANCODE_UP,
					SDL_SCANCODE_DOWN,
					SDL_SCANCODE_LEFT,
					SDL_SCANCODE_RIGHT,

					SDL_SCANCODE_Z,
					SDL_SCANCODE_X,
					SDL_SCANCODE_A,
					SDL_SCANCODE_S,
				};

				for (s32 i = 0; i < SDL_arraysize(Keys); i++)
				{
					if (keyboard[Keys[i]])
					{
						input.gamepads.first.data |= (1 << i);
					}
				}
			}

			nextTick += Delta;

			tic80_tick(tic, &input);

			if (!audioStarted && audioDevice)
				audioStarted = true;

			SDL_PauseAudioDevice(audioDevice, 0);

			{
				s32 size = tic->sound.count * sizeof(tic->sound.samples[0]);

				if (cvt.needed)
				{
					SDL_memcpy(cvt.buf, tic->sound.samples, size);
					SDL_ConvertAudio(&cvt);
					SDL_QueueAudio(audioDevice, cvt.buf, cvt.len_cvt);
				}
				else SDL_QueueAudio(audioDevice, tic->sound.samples, size);
			}

			SDL_RenderClear(renderer);

			{
				void* pixels = NULL;
				s32 pitch = 0;
				SDL_Rect destination;
				SDL_LockTexture(texture, NULL, &pixels, &pitch);
				SDL_memcpy(pixels, tic->screen, pitch * TIC80_FULLHEIGHT);
				SDL_UnlockTexture(texture);

				// Render the image in the proper aspect ratio.
				{
					s32 windowWidth, windowHeight;
					SDL_GetWindowSize(window, &windowWidth, &windowHeight);
					float widthRatio = (float)windowWidth / TIC80_FULLWIDTH;
					float heightRatio = (float)windowHeight / TIC80_FULLHEIGHT;
					float optimalSize = widthRatio < heightRatio ? widthRatio : heightRatio;
					destination.w = (s32)(TIC80_FULLWIDTH * optimalSize);
					destination.h = (s32)(TIC80_FULLHEIGHT * optimalSize);
					destination.x = windowWidth / 2 - destination.w / 2;
					destination.y = windowHeight / 2 - destination.h / 2;
				}

				SDL_RenderCopy(renderer, texture, NULL, &destination);
			}

			SDL_RenderPresent(renderer);

			{
				s64 delay = nextTick - SDL_GetPerformanceCounter();

				if (delay < 0)
					nextTick -= delay;
				else SDL_Delay((u32)(delay * 1000 / SDL_GetPerformanceFrequency()));
			}
		}

		tic80_delete(tic);
	}

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_CloseAudioDevice(audioDevice);

	SDL_free(cart);
	return output;
}

s32 main(s32 argc, char **argv)
{
	const char* executable = argc > 0 ? argv[0] : TIC80_EXECUTABLE_NAME;
	const char* input = (argc > 1) ? argv[1] : TIC80_DEFAULT_CART;

	// Display help message.
	if(strcmp(input, "--help") == 0 || strcmp(input, "-h") == 0)
	{
		printf("Usage: %s <file>\n", executable);
		return 0;
	}

	// Load the given file.
	FILE* file = fopen(input, "rb");
	if(!file)
	{
		fprintf(stderr, "Error: Could not load %s.\n\nUsage: %s <file>\n", input, argv[0]);
		return 1;
	}

	// Load the file data.
	fseek(file, 0, SEEK_END);
	s32 size = ftell(file);
	fseek(file, 0, SEEK_SET);

	// Read the data into usable memory.
	void* cart = SDL_malloc(size);
	if(cart) fread(cart, size, 1, file);
	fclose(file);

	if (!cart) {
		fprintf(stderr, "Error reading %s.\n", input);
		return 1;
	}

	return runCart(cart, size);
}
