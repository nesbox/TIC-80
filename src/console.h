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

#pragma once

#include "studio.h"

typedef enum
{
	CART_SAVE_OK,
	CART_SAVE_ERROR,
	CART_SAVE_MISSING_NAME,
} CartSaveResult;

typedef struct HistoryItem HistoryItem;

struct HistoryItem
{
	char* value;
	HistoryItem* prev;
	HistoryItem* next;
};

typedef struct Console Console;

struct Console
{
	struct Config* config;

	struct
	{
		s32 x;
		s32 y;
		s32 delay;
	} cursor;

	struct
	{
		s32 pos;
		s32 start;

		bool active;
	} scroll;

	struct
	{
		char fileName[FILENAME_MAX];
		bool active;

		void(*reload)(Console*, char*);
	} codeLiveReload;

	struct
	{
		bool yes;
		tic_cartridge* file;
	} embed;

	char* buffer;
	u8* colorBuffer;

	char inputBuffer[STUDIO_TEXT_BUFFER_WIDTH * STUDIO_TEXT_BUFFER_HEIGHT];
	size_t inputPosition;

	tic_mem* tic;

	struct FileSystem* fs;

	char romName[FILENAME_MAX];
	char appPath[FILENAME_MAX];

	HistoryItem* history;
	HistoryItem* historyHead;

	u32 tickCounter;

	struct
	{
		bool active;
		bool showGameMenu;
		bool startSurf;
		bool skipStart;
		bool goFullscreen;
		bool crtMonitor;
	};

	void(*load)(Console*, const char* path, const char* hash);
	bool(*loadProject)(Console*, const char* name, const char* data, s32 size, tic_cartridge* dst);
	void(*updateProject)(Console*);
	void(*error)(Console*, const char*);
	void(*trace)(Console*, const char*, u8 color);
	void(*tick)(Console*);

	CartSaveResult(*save)(Console*);
};

void initConsole(Console*, tic_mem*, struct FileSystem* fs, struct Config* config, s32 argc, char **argv);