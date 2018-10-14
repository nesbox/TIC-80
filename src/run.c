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

static const char* data2md5(const void* data, s32 length)
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
		enum{Size = 16};
		u8 digest[Size];
		MD5_Final(digest, &c);

		for (s32 n = 0; n < Size; ++n)
			snprintf(out + n*2, sizeof("ff"), "%02x", digest[n]);
	}

	return out;
}

static void initPMemName(Run* run)
{
	const char* data = strlen(run->tic->saveid) ? run->tic->saveid : run->tic->cart.code.data;
	const char* md5 = data2md5(data, strlen(data));
	strcpy(run->saveid, TIC_LOCAL);
	strcat(run->saveid, md5);
}

static void tick(Run* run)
{
	if (getStudioMode() != TIC_RUN_MODE)
		return;

	run->tic->api.tick(run->tic, &run->tickData);

	enum {Size = sizeof(tic_persistent)};

	if(run->tickData.syncPMEM)
	{		
		fsSaveRootFile(run->console->fs, run->saveid, &run->tic->persistent, Size, true);
		memcpy(&run->persistent, &run->tic->persistent, Size);
		run->tickData.syncPMEM = false;
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

	if (memcmp(tic->cart.code.data, DoFileTag, Size) == 0)
	{
		const char* start = tic->cart.code.data + Size;
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
					else memcpy(dst, buffer, size);
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


static bool forceExit(void* data)
{
	getSystem()->poll();

	tic_mem* tic = ((Run*)data)->tic;

	return tic->api.key(tic, tic_key_escape);
}

void initRun(Run* run, Console* console, tic_mem* tic)
{
	*run = (Run)
	{
		.tic = tic,
		.console = console,
		.tick = tick,
		.exit = false,
		.tickData = 
		{
			.error = onError,
			.trace = onTrace,
			.counter = getSystem()->getPerformanceCounter,
			.freq = getSystem()->getPerformanceFrequency,
			.start = 0,
			.data = run,
			.exit = onExit,
			.preprocessor = processDoFile,
			.forceExit = forceExit,
			.syncPMEM = false,
		},
	};

	{
		enum {Size = sizeof(tic_persistent)};
		memset(&run->tic->persistent, 0, Size);

		initPMemName(run);

		s32 size = 0;
		void* data = fsLoadRootFile(run->console->fs, run->saveid, &size);

		if(size > Size) size = Size;

		if(data)
		{
			memcpy(&run->tic->persistent, data, size);
			memcpy(&run->persistent, data, size);
		}

		if(data) free(data);
	}

	getSystem()->preseed();
}
