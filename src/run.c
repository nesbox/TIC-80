// MIT License

// Copyright (c) 2017 Vadim Grigoruk @nesbox // grigoruk@gmail.com

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

#include "run.h"
#include "console.h"
#include "fs.h"
#include "ext/md5.h"
#include <time.h>

static void onTrace(void* data, const char* text, u8 color)
{
	Run* run = (Run*)data;

	run->console->trace(run->console, text, color);
}

static void onError(void* data, const char* info)
{
	Run* run = (Run*)data;

	setStudioMode(TIC_CONSOLE_MODE);
	run->console->error(run->console, info);
}

static void onExit(void* data)
{
	Run* run = (Run*)data;

	run->exit = true;
}

static char* data2md5(const void* data, s32 length)
{
	const char *str = data;
	MD5_CTX c;
	
	static char out[33];

	MD5_Init(&c);

	while (length > 0) 
	{
		MD5_Update(&c, str, length > 512 ? 512: length);

		length -= 512;
		str += 512;
	}

	{
		u8 digest[16];
		MD5_Final(digest, &c);

		for (s32 n = 0; n < 16; ++n)
			snprintf(&(out[n*2]), 16*2, "%02x", (u32)digest[n]);
	}

	return out;
}

static const char* getPMemName(Run* run)
{
	static char buffer[FILENAME_MAX];

	const char* data = strlen(run->tic->saveid) ? run->tic->saveid : run->tic->cart.bank0.code.data;
	char* md5 = data2md5(data, (s32)strlen(data));
	strcpy(buffer, TIC_LOCAL);
	strcat(buffer, md5);

	return buffer;
}

static void tick(Run* run)
{
	while(pollEvent());

	if (getStudioMode() != TIC_RUN_MODE)
		return;

	if(!run->init)
	{	
		run->tickData.start = run->tickData.counter(),
		run->init = true;
	}

	run->tic->api.tick(run->tic, &run->tickData);

	enum {Size = sizeof(tic_persistent)};

	if(SDL_memcmp(&run->tic->ram.persistent, run->persistent, Size) != 0)
	{		
		fsSaveRootFile(run->console->fs, getPMemName(run), &run->tic->ram.persistent, Size, true);

		SDL_memcpy(run->persistent, &run->tic->ram.persistent, Size);
	}

	if(run->exit)
		setStudioMode(TIC_CONSOLE_MODE);
}

static void processDoFile(void* data, char* dst)
{
	Run* run = (Run*)data;
	tic_mem* tic = run->tic;

	static const char DoFileTag[] = "dofile(";
	enum {Size = sizeof DoFileTag - 1};

	if (memcmp(tic->cart.bank0.code.data, DoFileTag, Size) == 0)
	{
		const char* start = tic->cart.bank0.code.data + Size;
		const char* end = strchr(start, ')');

		if(end && *start == *(end-1) && (*start == '"' || *start == '\''))
		{
			char filename[FILENAME_MAX] = {0};
			memcpy(filename, start + 1, end - start - 2);
			memset(dst, 0, sizeof(tic_code));

			s32 size = 0;
			void* buffer = fsReadFile(filename, &size);

			if(buffer)
			{
				if(size > 0)
				{
					if(size > TIC_CODE_SIZE)
					{
						char buffer[256];
						sprintf(buffer, "code is larger than %i symbols", TIC_CODE_SIZE);
						onError(run, buffer);

						return;
					}
					else SDL_memcpy(dst, buffer, size);
				}
			}
			else
			{
				char buffer[256];
				sprintf(buffer, "dofile: file '%s' not found", filename);
				onError(run, buffer);

				return;
			}
		}
	}

	return;
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

static bool forceExit(void* data)
{
	while(pollEvent());

	return getStudioMode() != TIC_RUN_MODE;
}

void initRun(Run* run, Console* console, tic_mem* tic)
{
	*run = (Run)
	{
		.tic = tic,
		.console = console,
		.tick = tick,
		.exit = false,
		.init = false,
		.tickData = 
		{
			.error = onError,
			.trace = onTrace,
			.counter = SDL_GetPerformanceCounter,
			.freq = SDL_GetPerformanceFrequency,
			.start = 0,
			.data = run,
			.exit = onExit,
			.preprocessor = processDoFile,
			.forceExit = forceExit,
		},
	};

	{
		enum {Size = sizeof(tic_persistent)};
		SDL_memset(&run->tic->ram.persistent, 0, Size);

		s32 size = 0;
		void* data = fsLoadRootFile(run->console->fs, getPMemName(run), &size);

		if(size == Size && data)
		{
			SDL_memcpy(&run->tic->ram.persistent, data, Size);
			SDL_memcpy(run->persistent, data, Size);
		}

		if(data) SDL_free(data);
	}

	preseed();
}
