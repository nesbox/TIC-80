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

#include "console.h"
#include "fs.h"
#include "config.h"
#include "net.h"
#include "ext/gif.h"
#include "ext/file_dialog.h"

#include <zlib.h>

#define CONSOLE_CURSOR_COLOR ((tic_color_red))
#define CONSOLE_BACK_TEXT_COLOR ((tic_color_dark_gray))
#define CONSOLE_FRONT_TEXT_COLOR ((tic_color_white))
#define CONSOLE_ERROR_TEXT_COLOR ((tic_color_red))
#define CONSOLE_CURSOR_BLINK_PERIOD (TIC_FRAMERATE)
#define CONSOLE_CURSOR_DELAY (TIC_FRAMERATE / 2)
#define CONSOLE_BUFFER_WIDTH (STUDIO_TEXT_BUFFER_WIDTH)
#define CONSOLE_BUFFER_HEIGHT (STUDIO_TEXT_BUFFER_HEIGHT)
#define CONSOLE_BUFFER_SCREENS 64
#define CONSOLE_BUFFER_SIZE (CONSOLE_BUFFER_WIDTH * CONSOLE_BUFFER_HEIGHT * CONSOLE_BUFFER_SCREENS)

#if defined(__WINDOWS__) || defined(__LINUX__) || defined(__MACOSX__)
#define CAN_EXPORT 1
#endif

#if defined(__WINDOWS__)
static const char* ExeExt = ".exe";
#endif

static struct
{
	char prefix[32];
	bool yes;
	bool menu;
	tic_cartridge file;
} embed =
{
	.prefix = "C8B39163816B47209E721136D37B8031",
	.yes = false,
};

static const char DefaultLuaTicPath[] = TIC_LOCAL "default.tic";
static const char DefaultMoonTicPath[] = TIC_LOCAL "default_moon.tic";
static const char DefaultJSTicPath[] = TIC_LOCAL "default_js.tic";
static const char DefaultBFTicPath[] = TIC_LOCAL "default_bf.tic";

static const char* getName(const char* name, const char* ext)
{
	static char path[FILENAME_MAX];

	strcpy(path, name);

	size_t ps = strlen(path);
	size_t es = strlen(ext);

	if(!(ps > es && strstr(path, ext) + es == path + ps))
		strcat(path, ext);

	return path;
}

static const char* getCartName(const char* name)
{
	return getName(name, CART_EXT);
}

static void scrollBuffer(char* buffer)
{
	memmove(buffer, buffer + CONSOLE_BUFFER_WIDTH, CONSOLE_BUFFER_SIZE - CONSOLE_BUFFER_WIDTH);
	memset(buffer + CONSOLE_BUFFER_SIZE - CONSOLE_BUFFER_WIDTH, 0, CONSOLE_BUFFER_WIDTH);
}

static void scrollConsole(Console* console)
{
	while(console->cursor.y >= CONSOLE_BUFFER_HEIGHT * CONSOLE_BUFFER_SCREENS)
	{
		scrollBuffer(console->buffer);
		scrollBuffer((char*)console->colorBuffer);

		console->cursor.y--;
	}

	s32 minScroll = console->cursor.y - CONSOLE_BUFFER_HEIGHT + 1;
	if(console->scroll.pos < minScroll)
		console->scroll.pos = minScroll;
}

static void consolePrint(Console* console, const char* text, u8 color)
{
	printf("%s", text);

	const char* textPointer = text;
	const char* endText = textPointer + strlen(text);

	while(textPointer != endText)
	{
		char symbol = *textPointer++;

		scrollConsole(console);

		if(symbol == '\n')
		{
			console->cursor.x = 0;
			console->cursor.y++;
		}
		else
		{
			s32 offset = console->cursor.x + console->cursor.y * CONSOLE_BUFFER_WIDTH;
			*(console->buffer + offset) = symbol;
			*(console->colorBuffer + offset) = color;

			console->cursor.x++;

			if(console->cursor.x >= CONSOLE_BUFFER_WIDTH)
			{
				console->cursor.x = 0;
				console->cursor.y++;
			}
		}

	}
}

static void printBack(Console* console, const char* text)
{
	consolePrint(console, text, CONSOLE_BACK_TEXT_COLOR);
}

static void printFront(Console* console, const char* text)
{
	consolePrint(console, text, CONSOLE_FRONT_TEXT_COLOR);
}

static void printError(Console* console, const char* text)
{
	consolePrint(console, text, CONSOLE_ERROR_TEXT_COLOR);
}

static void printLine(Console* console)
{
	consolePrint(console, "\n", 0);
}

static void commandDoneLine(Console* console, bool newLine)
{
	if(newLine)
		printLine(console);

	if(strlen(fsGetDir(console->fs)))
		printBack(console, fsGetDir(console->fs));

	printFront(console, ">");
}

static void commandDone(Console* console)
{
	commandDoneLine(console, true);
}

static void drawCursor(Console* console, s32 x, s32 y, u8 symbol)
{
	bool inverse = console->cursor.delay || console->tickCounter % CONSOLE_CURSOR_BLINK_PERIOD < CONSOLE_CURSOR_BLINK_PERIOD / 2;

	if(inverse)
		console->tic->api.rect(console->tic, x-1, y-1, TIC_FONT_WIDTH+1, TIC_FONT_HEIGHT+1, CONSOLE_CURSOR_COLOR);

	console->tic->api.draw_char(console->tic, symbol, x, y, inverse ? TIC_COLOR_BG : CONSOLE_FRONT_TEXT_COLOR);
}

static void drawConsoleText(Console* console)
{
	char* pointer = console->buffer + console->scroll.pos * CONSOLE_BUFFER_WIDTH;
	u8* colorPointer = console->colorBuffer + console->scroll.pos * CONSOLE_BUFFER_WIDTH;

	const char* end = console->buffer + CONSOLE_BUFFER_SIZE;
	s32 x = 0;
	s32 y = 0;

	while(pointer < end)
	{
		char symbol = *pointer++;
		u8 color = *colorPointer++;

		if(symbol)
			console->tic->api.draw_char(console->tic, symbol, x * STUDIO_TEXT_WIDTH, y * STUDIO_TEXT_HEIGHT, color);

		if(++x == CONSOLE_BUFFER_WIDTH)
		{
			y++;
			x = 0;
		}
	}
}

static void drawConsoleInputText(Console* console)
{
	s32 x = console->cursor.x * STUDIO_TEXT_WIDTH;
	s32 y = (console->cursor.y - console->scroll.pos) * STUDIO_TEXT_HEIGHT;

	const char* pointer = console->inputBuffer;
	const char* end = pointer + strlen(console->inputBuffer);
	s32 index = 0;

	while(pointer != end)
	{
		char symbol = *pointer++;

		if(console->inputPosition == index)
			drawCursor(console, x, y, symbol);
		else
			console->tic->api.draw_char(console->tic, symbol, x, y, CONSOLE_FRONT_TEXT_COLOR);

		index++;

		x += STUDIO_TEXT_WIDTH;
		if(x == (CONSOLE_BUFFER_WIDTH * STUDIO_TEXT_WIDTH))
		{
			y += STUDIO_TEXT_HEIGHT;
			x = 0;
		}

	}

	if(console->inputPosition == index)
		drawCursor(console, x, y, ' ');


}

static void processConsoleHome(Console* console)
{
	console->inputPosition = 0;
}

static void processConsoleEnd(Console* console)
{
	console->inputPosition = strlen(console->inputBuffer);
}

static void processConsoleDel(Console* console)
{
	char* pos = console->inputBuffer + console->inputPosition;
	size_t size = strlen(pos);
	memmove(pos, pos + 1, size);
}

static void processConsoleBackspace(Console* console)
{
	if(console->inputPosition > 0)
	{
		console->inputPosition--;

		processConsoleDel(console);
	}
}

static void onConsoleHelpCommand(Console* console, const char* param);

static void onConsoleExitCommand(Console* console, const char* param)
{
	exitStudio();
	commandDone(console);
}

static s32 writeGifData(const tic_mem* tic, u8* dst, const u8* src, s32 width, s32 height)
{
	s32 size = 0;
	gif_color* palette = (gif_color*)SDL_malloc(sizeof(gif_color) * TIC_PALETTE_SIZE);

	if(palette)
	{
		const tic_rgb* pal = tic->cart.palette.colors;
		for(s32 i = 0; i < TIC_PALETTE_SIZE; i++, pal++)
			palette[i].r = pal->r, palette[i].g = pal->g, palette[i].b = pal->b;

		gif_write_data(dst, &size, width, height, src, palette, TIC_PALETTE_BPP);

		SDL_free(palette);
	}

	return size;
}

static void loadCart(tic_mem* tic, tic_cartridge* cart, const u8* buffer, s32 size, bool palette)
{
	tic->api.load(cart, buffer, size, palette);

	if(!palette)
		memcpy(cart->palette.data, tic->config.palette.data, sizeof(tic_palette));
}

static bool loadRom(tic_mem* tic, const void* data, s32 size, bool palette)
{
	loadCart(tic, &tic->cart, data, size, palette);
	tic->api.reset(tic);

	return true;
}

static bool onConsoleLoadSectionCommand(Console* console, const char* param)
{
	bool result = false;

	if(param)
	{
		static const char* Sections[] =
		{
			"sprites",
			"map",
			"cover",
			"code",
			"sfx",
			"music",
			"palette",
		};

		char buf[64] = {0};

		for(s32 i = 0; i < COUNT_OF(Sections); i++)
		{
			sprintf(buf, "%s %s", CART_EXT, Sections[i]);
			char* pos = SDL_strstr(param, buf);

			if(pos)
			{
				pos[sizeof(CART_EXT) - 1] = 0;
				const char* name = getCartName(param);
				s32 size = 0;
				void* data = fsLoadFile(console->fs, name, &size);

				if(data)
				{
					tic_cartridge* cart = (tic_cartridge*)SDL_malloc(sizeof(tic_cartridge));

					if(cart)
					{
						loadCart(console->tic, cart, data, size, true);
						tic_mem* tic = console->tic;

						switch(i)
						{
						case 0: memcpy(&tic->cart.bank.tiles, 	&cart->bank.tiles, 	sizeof(tic_tiles)*2); break;
						case 1: memcpy(&tic->cart.bank.map, 	&cart->bank.map, 	sizeof(tic_map)); break;
						case 2: memcpy(&tic->cart.cover, 		&cart->cover, 		sizeof cart->cover); break;
						case 3: memcpy(&tic->cart.code, 		&cart->code, 		sizeof cart->code); break;
						case 4: memcpy(&tic->cart.bank.sfx, 	&cart->bank.sfx, 	sizeof(tic_sfx)); break;
						case 5: memcpy(&tic->cart.bank.music, 	&cart->bank.music, 	sizeof(tic_music)); break;
						case 6: memcpy(&tic->cart.palette, 		&cart->palette, 	sizeof(tic_palette)); break;
						}

						studioRomLoaded();

						printLine(console);
						printFront(console, Sections[i]);
						printBack(console, " loaded from ");
						printFront(console, name);
						printLine(console);

						SDL_free(cart);

						result = true;
					}

					SDL_free(data);
				}
				else printBack(console, "\ncart loading error");

				commandDone(console);
			}
		}
	}

	return result;
}

static void* getDemoCart(Console* console, tic_script_lang script, s32* size)
{
	char path[FILENAME_MAX] = {0};

	{
		switch(script)
		{
		case tic_script_lua:
			strcpy(path, DefaultLuaTicPath);
			break;
		case tic_script_moon:
			strcpy(path, DefaultMoonTicPath);
			break;
		case tic_script_js:
			strcpy(path, DefaultJSTicPath);
			break;
		case tic_script_bf:
			strcpy(path, DefaultBFTicPath);
			break;
		}

		void* data = fsLoadRootFile(console->fs, path, size);

		if(data && *size)
			return data;
	}

	static const u8 LuaDemoRom[] =
	{
		#include "../bin/assets/luademo.tic.dat"
	};

	static const u8 JsDemoRom[] =
	{
		#include "../bin/assets/jsdemo.tic.dat"
	};

	static const u8 MoonDemoRom[] =
	{
		#include "../bin/assets/moondemo.tic.dat"
	};

	static const u8 BfDemoRom[] =
	{
		#include "../bin/assets/bfdemo.tic.dat"
	};

	const u8* demo = NULL;
	s32 romSize = 0;

	switch(script)
	{
	case tic_script_lua:
		demo = LuaDemoRom;
		romSize = sizeof LuaDemoRom;
		break;
	case tic_script_moon:
		demo = MoonDemoRom;
		romSize = sizeof MoonDemoRom;
		break;
	case tic_script_js:
		demo = JsDemoRom;
		romSize = sizeof JsDemoRom;
		break;
	case tic_script_bf:
		demo = BfDemoRom;
		romSize = sizeof BfDemoRom;
		break;
	}

	u8* data = NULL;
	*size = unzip(&data, demo, romSize);

	if(data)
	{
		fsSaveRootFile(console->fs, path, data, *size, false);
	}

	return data;
}

static void setCartName(Console* console, const char* name)
{
	if(name != console->romName)
		strcpy(console->romName, name);
}

static void onConsoleLoadDemoCommandConfirmed(Console* console, const char* param)
{
	void* data = NULL;
	s32 size = 0;

	console->showGameMenu = false;

	if(strcmp(param, DefaultLuaTicPath) == 0)
		data = getDemoCart(console, tic_script_lua, &size);
	else if(strcmp(param, DefaultMoonTicPath) == 0)
		data = getDemoCart(console, tic_script_moon, &size);
	else if(strcmp(param, DefaultJSTicPath) == 0)
		data = getDemoCart(console, tic_script_js, &size);
	else if(strcmp(param, DefaultBFTicPath) == 0)
		data = getDemoCart(console, tic_script_bf, &size);

	const char* name = getCartName(param);

	setCartName(console, name);

	loadRom(console->tic, data, size, true);

	studioRomLoaded();

	printBack(console, "\ncart ");
	printFront(console, console->romName);
	printBack(console, " loaded!\n");

	SDL_free(data);
}

static void onCartLoaded(Console* console, const char* name)
{
	setCartName(console, name);

	studioRomLoaded();

	printBack(console, "\ncart ");
	printFront(console, console->romName);
	printBack(console, " loaded!\nuse ");
	printFront(console, "RUN");
	printBack(console, " command to run it\n");

}

static bool hasExt(const char* name, const char* ext)
{
	return strcmp(name + strlen(name) - strlen(ext), ext) == 0;
}

#if defined(TIC80_PRO)

static bool hasProjectExt(const char* name)
{
	return hasExt(name, PROJECT_LUA_EXT) || hasExt(name, PROJECT_MOON_EXT) || hasExt(name, PROJECT_JS_EXT);
}

static const char* projectComment(const char* name)
{
	return hasExt(name, PROJECT_JS_EXT) ? "//" : "--";
}

static void buf2str(const void* data, s32 size, char* ptr, bool flip)
{
	enum {Len = 2};

	for(s32 i = 0; i < size; i++, ptr+=Len)
	{
		sprintf(ptr, "%02x", ((u8*)data)[i]);

		if(flip)
		{
			char tmp = ptr[0];
			ptr[0] = ptr[1];
			ptr[1] = tmp;
		}
	}
}

static bool bufferEmpty(const u8* data, s32 size)
{
	for(s32 i = 0; i < size; i++)
		if(*data++)
			return false;

	return true;
}

static char* saveTextSection(char* ptr, const char* data)
{
	if(strlen(data) == 0)
		return ptr;

	sprintf(ptr, "%s\n", data);
	ptr += strlen(ptr);

	return ptr;
}

static char* saveBinaryBuffer(char* ptr, const char* comment, const void* data, s32 size, s32 row, bool flip)
{
	if(bufferEmpty(data, size)) 
		return ptr;

	sprintf(ptr, "%s %03i:", comment, row);
	ptr += strlen(ptr);

	buf2str(data, size, ptr, flip);
	ptr += strlen(ptr);

	sprintf(ptr, "\n");
	ptr += strlen(ptr);

	return ptr;
}

static char* saveBinarySection(char* ptr, const char* comment, const char* tag, s32 count, const void* data, s32 size, bool flip)
{
	if(bufferEmpty(data, size * count)) 
		return ptr;

	sprintf(ptr, "%s <%s>\n", comment, tag);
	ptr += strlen(ptr);

	for(s32 i = 0; i < count; i++, data = (u8*)data + size)
		ptr = saveBinaryBuffer(ptr, comment, data, size, i, flip);

	sprintf(ptr, "%s </%s>\n\n", comment, tag);
	ptr += strlen(ptr);

	return ptr;
}

typedef struct {char* tag; s32 count; s32 offset; s32 size; bool flip;} BinarySection;
static const BinarySection BinarySections[] = 
{
	{"PALETTE", 	1, 					offsetof(tic_cartridge, palette.data), 			sizeof(tic_palette), false},
	{"TILES", 		TIC_BANK_SPRITES, 	offsetof(tic_cartridge, bank.tiles), 			sizeof(tic_tile), true},
	{"SPRITES", 	TIC_BANK_SPRITES, 	offsetof(tic_cartridge, bank.sprites), 			sizeof(tic_tile), true},
	{"MAP", 		TIC_MAP_HEIGHT, 	offsetof(tic_cartridge, bank.map), 				TIC_MAP_WIDTH, true},
	{"WAVES", 		ENVELOPES_COUNT, 	offsetof(tic_cartridge, bank.sfx.waveform.envelopes), sizeof(tic_waveform), true},
	{"SFX", 		SFX_COUNT, 			offsetof(tic_cartridge, bank.sfx.data), 		sizeof(tic_sound_effect), true},
	{"PATTERNS", 	MUSIC_PATTERNS, 	offsetof(tic_cartridge, bank.music.patterns), 	sizeof(tic_track_pattern), true},
	{"TRACKS", 		MUSIC_TRACKS, 		offsetof(tic_cartridge, bank.music.tracks), 	sizeof(tic_track), true},
};

static s32 saveProject(Console* console, void* buffer, const char* comment)
{
	tic_mem* tic = console->tic;

	char* stream = buffer;
	char* ptr = saveTextSection(stream, tic->cart.code.data);

	for(s32 i = 0; i < COUNT_OF(BinarySections); i++)
	{
		const BinarySection* section = &BinarySections[i];
		ptr = saveBinarySection(ptr, comment, section->tag, section->count, (u8*)&tic->cart + section->offset, section->size, section->flip);
	}

	saveBinarySection(ptr, comment, "COVER", 1, &tic->cart.cover, tic->cart.cover.size + sizeof(s32), true);

	return strlen(stream);
}

static bool loadTextSection(const char* project, const char* comment, void* dst, s32 size)
{
	bool done = false;

	const char* start = project;
	const char* end = project + strlen(project);

	{
		char tagbuf[64];

		for(s32 i = 0; i < COUNT_OF(BinarySections); i++)
		{
			sprintf(tagbuf, "\n%s <%s>\n", comment, BinarySections[i].tag);

			const char* ptr = SDL_strstr(project, tagbuf);

			if(ptr && ptr < end)
				end = ptr;
		}		
	}

	if(end > start)
	{
		SDL_memcpy(dst, start, SDL_min(size, end - start));
		done = true;
	}

	return done;
}

static bool loadBinarySection(const char* project, const char* comment, const char* tag, s32 count, void* dst, s32 size, bool flip)
{
	char tagbuf[64];
	sprintf(tagbuf, "%s <%s>\n", comment, tag);

	const char* start = SDL_strstr(project, tagbuf);
	bool done = false;

	if(start)
	{
		start += strlen(tagbuf);

		sprintf(tagbuf, "\n%s </%s>", comment, tag);
		const char* end = SDL_strstr(start, tagbuf);

		if(end > start)
		{
			const char* ptr = start;

			if(size > 0)
			{
				while(ptr < end)
				{
					static char lineStr[] = "999";
					memcpy(lineStr, ptr + sizeof("-- ") - 1, sizeof lineStr - 1);

					s32 index = SDL_atoi(lineStr);
					
					if(index < count)
					{
						ptr += sizeof("-- 999:") - 1;
						str2buf(ptr, size*2, (u8*)dst + size*index, flip);
						ptr += size*2 + 1;
					}
					else break;
				}				
			}
			else
			{
				ptr += sizeof("-- 999:") - 1;
				str2buf(ptr, end - ptr, (u8*)dst, flip);
			}

			done = true;
		}
	}

	return done;
}

static bool loadProject(Console* console, const char* name, const char* data, s32 size, tic_cartridge* dst)
{
	tic_mem* tic = console->tic;

	char* project = (char*)SDL_malloc(size+1);

	bool done = false;

	if(project)
	{
		SDL_memcpy(project, data, size);
		project[size] = '\0';

		// remove all the '\r' chars
		{
			char *s, *d;
			for(s = d = project; (*d = *s); d += (*s++ != '\r'));
		}

		tic_cartridge* cart = (tic_cartridge*)SDL_malloc(sizeof(tic_cartridge));

		if(cart)
		{
			SDL_memset(cart, 0, sizeof(tic_cartridge));
			SDL_memcpy(&cart->palette, &tic->config.palette.data, sizeof(tic_palette));

			const char* comment = projectComment(name);

			if(loadTextSection(project, comment, cart->code.data, sizeof(tic_code)))
				done = true;

			for(s32 i = 0; i < COUNT_OF(BinarySections); i++)
			{
				const BinarySection* section = &BinarySections[i];
				if(loadBinarySection(project, comment, section->tag, section->count, (u8*)cart + section->offset, section->size, section->flip))
					done = true;
			}

			if(loadBinarySection(project, comment, "COVER", 1, &cart->cover, -1, true))
				done = true;
			
			SDL_memcpy(dst, cart, sizeof(tic_cartridge));

			SDL_free(cart);
		}

		SDL_free(project);
	}

	return done;
}

static void updateProject(Console* console)
{
	tic_mem* tic = console->tic;

	if(strlen(console->romName) && hasProjectExt(console->romName))
	{
		s32 size = 0;
		void* data = fsLoadFile(console->fs, console->romName, &size);

		if(data)
		{
			tic_cartridge* cart = SDL_malloc(sizeof(tic_cartridge));

			if(cart)
			{
				if(loadProject(console, console->romName, data, size, cart))
				{
					SDL_memcpy(&tic->cart, cart, sizeof(tic_cartridge));

					studioRomLoaded();
				}
				
				SDL_free(cart);
			}
			SDL_free(data);

		}		
	}
}

#endif

static void onConsoleLoadCommandConfirmed(Console* console, const char* param)
{
	if(onConsoleLoadSectionCommand(console, param)) return;

	if(param)
	{
		s32 size = 0;
		const char* name = getCartName(param);

		void* data = strcmp(name, CONFIG_TIC_PATH) == 0
			? fsLoadRootFile(console->fs, name, &size)
			: fsLoadFile(console->fs, name, &size);

		if(data)
		{
			console->showGameMenu = fsIsInPublicDir(console->fs);

			loadRom(console->tic, data, size, true);

			onCartLoaded(console, name);

			SDL_free(data);
		}
		else
		{
#if defined(TIC80_PRO)
			const char* name = getName(param, PROJECT_LUA_EXT);

			if(!fsExistsFile(console->fs, name))
				name = getName(param, PROJECT_MOON_EXT);

			if(!fsExistsFile(console->fs, name))
				name = getName(param, PROJECT_JS_EXT);

			void* data = fsLoadFile(console->fs, name, &size);

			if(data)
			{
				loadProject(console, name, data, size, &console->tic->cart);
				onCartLoaded(console, name);

				SDL_free(data);
			}
			else
			{
				printBack(console, "\ncart loading error");
			}
#else
			printBack(console, "\ncart loading error");
#endif
		}
	}
	else printBack(console, "\ncart name is missing");

	commandDone(console);
}

typedef void(*ConfirmCallback)(Console* console, const char* param);

typedef struct
{
	Console* console;
	char* param;
	ConfirmCallback callback;

} CommandConfirmData;

static void onConfirm(bool yes, void* data)
{
	CommandConfirmData* confirmData = (CommandConfirmData*)data;

	if(yes)
	{
		confirmData->callback(confirmData->console, confirmData->param);
	}
	else commandDone(confirmData->console);

	if(confirmData->param)
		SDL_free(confirmData->param);

	SDL_free(confirmData);
}

static void confirmCommand(Console* console, const char** text, s32 rows, const char* param, ConfirmCallback callback)
{
	CommandConfirmData* data = SDL_malloc(sizeof(CommandConfirmData));
	data->console = console;
	data->param = param ? SDL_strdup(param) : NULL;
	data->callback = callback;

	showDialog(text, rows, onConfirm, data);
}

static void onConsoleLoadDemoCommand(Console* console, const char* param)
{
	if(studioCartChanged())
	{
		static const char* Rows[] =
		{
			"YOU HAVE",
			"UNSAVED CHANGES",
			"",
			"DO YOU REALLY WANT",
			"TO LOAD CART?",
		};

		confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleLoadDemoCommandConfirmed);
	}
	else
	{
		onConsoleLoadDemoCommandConfirmed(console, param);
	}
}

static void onConsoleLoadCommand(Console* console, const char* param)
{
	if(studioCartChanged())
	{
		static const char* Rows[] =
		{
			"YOU HAVE",
			"UNSAVED CHANGES",
			"",
			"DO YOU REALLY WANT",
			"TO LOAD CART?",
		};

		confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleLoadCommandConfirmed);
	}
	else
	{
		onConsoleLoadCommandConfirmed(console, param);
	}
}

static void loadDemo(Console* console, tic_script_lang script)
{
	s32 size = 0;
	u8* data = getDemoCart(console, script, &size);

	if(data)
	{
		loadRom(console->tic, data, size, false);

		SDL_free(data);
	}

	SDL_memset(console->romName, 0, sizeof console->romName);

	studioRomLoaded();
}

static void onConsoleNewCommandConfirmed(Console* console, const char* param)
{
	if(param && strlen(param))
	{
		if(strcmp(param, "lua") == 0)
			loadDemo(console, tic_script_lua);
		else if(strcmp(param, "moon") == 0 || strcmp(param, "moonscript") == 0)
			loadDemo(console, tic_script_moon);
		else if(strcmp(param, "js") == 0 || strcmp(param, "javascript") == 0)
			loadDemo(console, tic_script_js);
		else if(strcmp(param, "bf") == 0 || strcmp(param, "brainfuck") == 0)
			loadDemo(console, tic_script_bf);
		else
		{
			printError(console, "\nunknown parameter: ");
			printError(console, param);
			commandDone(console);
			return;
		}
	}
	else loadDemo(console, tic_script_lua);

	printBack(console, "\nnew cart is created");
	commandDone(console);
}

static void onConsoleNewCommand(Console* console, const char* param)
{
	if(studioCartChanged())
	{
		static const char* Rows[] =
		{
			"YOU HAVE",
			"UNSAVED CHANGES",
			"",
			"DO YOU REALLY WANT",
			"TO CREATE NEW CART?",
		};

		confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleNewCommandConfirmed);
	}
	else
	{
		onConsoleNewCommandConfirmed(console, param);
	}
}

typedef struct
{
	s32 count;
	Console* console;
} PrintFileNameData;

static bool printFilename(const char* name, const char* info, s32 id, void* data, bool dir)
{

	PrintFileNameData* printData = data;
	Console* console = printData->console;

	printLine(console);

	if(dir)
	{

		printBack(console, "[");
		printBack(console, name);
		printBack(console, "]");
	}
	else printFront(console, name);

	if(!dir)
		printData->count++;

	return true;
}

static void onConsoleChangeDirectory(Console* console, const char* param)
{
	if(param && strlen(param))
	{
		if(strcmp(param, "/") == 0)
		{
			fsHomeDir(console->fs);
		}
		else if(strcmp(param, "..") == 0)
		{
			fsDirBack(console->fs);
		}
		else if(fsIsDir(console->fs, param))
		{
			fsChangeDir(console->fs, param);
		}
		else printBack(console, "\ndir doesn't exist");
	}
	else printBack(console, "\ninvalid dir name");

	commandDone(console);
}

static void onConsoleMakeDirectory(Console* console, const char* param)
{
	if(param && strlen(param))
		fsMakeDir(console->fs, param);
	else printBack(console, "\ninvalid dir name");

	commandDone(console);
}

static void onConsoleDirCommand(Console* console, const char* param)
{
	PrintFileNameData data = {0, console};

	printLine(console);

	fsEnumFiles(console->fs, printFilename, &data);

	if(data.count == 0)
	{
		printBack(console, "\n\nuse ");
		printFront(console, "ADD");
		printBack(console, " or ");
		printFront(console, "DEMO");
		printBack(console, " command to add carts");
	}

	printLine(console);
	commandDone(console);
}

#ifdef CAN_EXPORT

static void onConsoleFolderCommand(Console* console, const char* param)
{
	fsOpenWorkingFolder(console->fs);

	commandDone(console);
}

#endif

static void onConsoleClsCommand(Console* console, const char* param)
{
	memset(console->buffer, 0, CONSOLE_BUFFER_SIZE);
	memset(console->colorBuffer, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);
	console->scroll.pos = 0;
	console->cursor.x = console->cursor.y = 0;

	commandDoneLine(console, false);
}

static void installDemoCart(FileSystem* fs, const char* name, const void* cart, s32 size)
{
	u8* data = NULL;
	s32 dataSize = unzip(&data, cart, size);
	fsSaveFile(fs, name, data, dataSize, true);
	SDL_free(data);
}

static void onConsoleInstallDemosCommand(Console* console, const char* param)
{
	static const u8 DemoFire[] =
	{
		#include "../bin/assets/fire.tic.dat"
	};

	static const u8 DemoP3D[] =
	{
		#include "../bin/assets/p3d.tic.dat"
	};

	static const u8 DemoSFX[] =
	{
		#include "../bin/assets/sfx.tic.dat"
	};

	static const u8 DemoPalette[] =
	{
		#include "../bin/assets/palette.tic.dat"
	};

	static const u8 DemoFont[] =
	{
		#include "../bin/assets/font.tic.dat"
	};

	static const u8 DemoMusic[] =
	{
		#include "../bin/assets/music.tic.dat"
	};

	static const u8 GameQuest[] =
	{
		#include "../bin/assets/quest.tic.dat"
	};

	static const u8 GameTetris[] =
	{
		#include "../bin/assets/tetris.tic.dat"
	};

	static const u8 Benchmark[] =
	{
		#include "../bin/assets/benchmark.tic.dat"
	};

	FileSystem* fs = console->fs;

	static const struct {const char* name; const u8* data; s32 size;} Demos[] =
	{
		{"fire.tic", 		DemoFire, 		sizeof DemoFire},
		{"font.tic", 		DemoFont, 		sizeof DemoFont},
		{"music.tic", 		DemoMusic, 		sizeof DemoMusic},
		{"p3d.tic", 		DemoP3D, 		sizeof DemoP3D},
		{"palette.tic", 	DemoPalette, 	sizeof DemoPalette},
		{"quest.tic", 		GameQuest, 		sizeof GameQuest},
		{"sfx.tic", 		DemoSFX, 		sizeof DemoSFX},
		{"tetris.tic", 		GameTetris, 	sizeof GameTetris},
		{"benchmark.tic", 	Benchmark, 		sizeof Benchmark},
	};

	printBack(console, "\nadded carts:\n\n");

	for(s32 i = 0; i < COUNT_OF(Demos); i++)
	{
		installDemoCart(fs, Demos[i].name, Demos[i].data, Demos[i].size);
		printFront(console, Demos[i].name);
		printFront(console, "\n");
	}

	commandDone(console);
}

static void onConsoleSurfCommand(Console* console, const char* param)
{
	gotoSurf();

	commandDone(console);
}

static void onConsoleCodeCommand(Console* console, const char* param)
{
	gotoCode();
	commandDone(console);
}


static void onConsoleKeymapCommand(Console* console, const char* param)
{
	setStudioMode(TIC_KEYMAP_MODE);
	commandDone(console);
}

static void onConsoleVersionCommand(Console* console, const char* param)
{
	printBack(console, "\n");
	consolePrint(console, TIC_VERSION_LABEL, CONSOLE_BACK_TEXT_COLOR);
	commandDone(console);
}

static void onConsoleConfigCommand(Console* console, const char* param)
{
	if(param == NULL)
	{
		onConsoleLoadCommand(console, CONFIG_TIC_PATH);
		return;
	}
	else if(strcmp(param, "reset") == 0)
	{
		console->config->reset(console->config);
		printBack(console, "\nconfiguration reset :)");
	}
	else if(strcmp(param, "default") == 0 || strcmp(param, "default lua") == 0)
	{
		onConsoleLoadDemoCommand(console, DefaultLuaTicPath);
	}
	else if(strcmp(param, "default moon") == 0 || strcmp(param, "default moonscript") == 0)
	{
		onConsoleLoadDemoCommand(console, DefaultMoonTicPath);
	}
	else if(strcmp(param, "default js") == 0)
	{
		onConsoleLoadDemoCommand(console, DefaultJSTicPath);
	}
	else if(strcmp(param, "default bf") == 0 || strcmp(param, "default brainfuck") == 0)
	{
		onConsoleLoadDemoCommand(console, DefaultBFTicPath);
	}
	else
	{
		printError(console, "\nunknown parameter: ");
		printError(console, param);
	}

	commandDone(console);
}

static void onFileDownloaded(GetResult result, void* data)
{
	Console* console = (Console*)data;

	if(result == FS_FILE_NOT_DOWNLOADED)
		printBack(console, "\nfile not downloaded :|");
	else if (result == FS_FILE_DOWNLOADED)
		printBack(console, "\nfile downloaded :)");

	commandDone(console);
}

static void onImportCover(const char* name, const void* buffer, size_t size, void* data)
{
	Console* console = (Console*)data;

	if(name)
	{
		static const char GifExt[] = ".gif";

		const char* pos = strstr(name, GifExt);

		if(pos && strcmp(pos, GifExt) == 0)
		{
			gif_image* image = gif_read_data(buffer, (s32)size);

			if (image)
			{
				enum
				{
					Width = TIC80_WIDTH,
					Height = TIC80_HEIGHT,
					Size = Width * Height,
				};

				if(image->width == Width && image->height == Height)
				{
					if(size <= sizeof console->tic->cart.cover.data)
					{
						console->tic->cart.cover.size = size;
						SDL_memcpy(console->tic->cart.cover.data, buffer, size);

						printLine(console);
						printBack(console, name);
						printBack(console, " successfully imported");
					}
					else printError(console, "\ncover image too big :(");
				}
				else printError(console, "\ncover image must be 240x136 :(");

				gif_close(image);
			}
			else printError(console, "\nfile importing error :(");
		}
		else printBack(console, "\nonly .gif files can be imported :|");
	}
	else printBack(console, "\nfile not imported :|");

	commandDone(console);
}

static void onImportSprites(const char* name, const void* buffer, size_t size, void* data)
{
	Console* console = (Console*)data;

	if(name)
	{
		static const char GifExt[] = ".gif";

		const char* pos = strstr(name, GifExt);

		if(pos && strcmp(pos, GifExt) == 0)
		{
			gif_image* image = gif_read_data(buffer, (s32)size);

			if (image)
			{
				enum
				{
					Width = TIC_SPRITESHEET_SIZE,
					Height = TIC_SPRITESHEET_SIZE*2,
				};

				s32 w = SDL_min(Width, image->width);
				s32 h = SDL_min(Height, image->height);

				for (s32 y = 0; y < h; y++)
					for (s32 x = 0; x < w; x++)
					{
						u8 src = image->buffer[x + y * image->width];
						const gif_color* c = &image->palette[src];
						tic_rgb rgb = {c->r, c->g, c->b};
						u8 color = tic_tool_find_closest_color(console->tic->cart.palette.colors, &rgb);

						setSpritePixel(console->tic->cart.bank.tiles.data, x, y, color);
					}

				gif_close(image);

				printLine(console);
				printBack(console, name);
				printBack(console, " successfully imported");
			}
			else printError(console, "\nfile importing error :(");
		}
		else printBack(console, "\nonly .gif files can be imported :|");
	}
	else printBack(console, "\nfile not imported :|");

	commandDone(console);
}

static void injectMap(Console* console, const void* buffer, s32 size)
{
	enum {Size = sizeof(tic_map)};

	SDL_memset(&console->tic->cart.bank.map, 0, Size);
	SDL_memcpy(&console->tic->cart.bank.map, buffer, SDL_min(size, Size));
}

static void onImportMap(const char* name, const void* buffer, size_t size, void* data)
{
	Console* console = (Console*)data;

	if(name && buffer && size <= sizeof(tic_map))
	{
		injectMap(console, buffer, size);

		printLine(console);
		printBack(console, "map successfully imported");
	}
	else printBack(console, "\nfile not imported :|");

	commandDone(console);
}

static void onConsoleImportCommand(Console* console, const char* param)
{
	if(param == NULL)
	{
		printBack(console, "\nusage: import sprites|cover|map");
		commandDone(console);
	}
	else if(param && strcmp(param, "sprites") == 0)
		fsOpenFileData(onImportSprites, console);
	else if(param && strcmp(param, "map") == 0)
		fsOpenFileData(onImportMap, console);
	else if(param && strcmp(param, "cover") == 0)
		fsOpenFileData(onImportCover, console);
	else
	{
		printError(console, "\nunknown parameter: ");
		printError(console, param);
		commandDone(console);
	}
}

static void onSpritesExported(GetResult result, void* data)
{
	Console* console = (Console*)data;

	if(result == FS_FILE_NOT_DOWNLOADED)
		printBack(console, "\nsprites not exported :|");
	else if (result == FS_FILE_DOWNLOADED)
		printBack(console, "\nsprites successfully exported :)");

	commandDone(console);
}

static void onCoverExported(GetResult result, void* data)
{
	Console* console = (Console*)data;

	if(result == FS_FILE_NOT_DOWNLOADED)
		printBack(console, "\ncover image not exported :|");
	else if (result == FS_FILE_DOWNLOADED)
		printBack(console, "\ncover image successfully exported :)");

	commandDone(console);
}

static void exportCover(Console* console)
{
	tic_cover_image* cover = &console->tic->cart.cover;

	if(cover->size)
	{
		void* data = SDL_malloc(cover->size);
		memcpy(data, cover->data, cover->size);
		fsGetFileData(onCoverExported, "cover.gif", data, cover->size, DEFAULT_CHMOD, console);
	}
	else
	{
		printBack(console, "\ncover image is empty, run game and\npress [F7] to assign cover image");
		commandDone(console);
	}
}

static void exportSprites(Console* console)
{
	enum
	{
		Width = TIC_SPRITESHEET_SIZE,
		Height = TIC_SPRITESHEET_SIZE*2,
	};

	enum{ Size = Width * Height * sizeof(gif_color)};
	u8* buffer = (u8*)SDL_malloc(Size);

	if(buffer)
	{
		u8* data = (u8*)SDL_malloc(Width * Height);

		if(data)
		{
			for (s32 y = 0; y < Height; y++)
				for (s32 x = 0; x < Width; x++)
					data[x + y * Width] = getSpritePixel(console->tic->cart.bank.tiles.data, x, y);

			s32 size = 0;
			if((size = writeGifData(console->tic, buffer, data, Width, Height)))
			{
				// buffer will be freed inside fsGetFileData
				fsGetFileData(onSpritesExported, "sprites.gif", buffer, size, DEFAULT_CHMOD, console);
			}
			else
			{
				printError(console, "\nsprite export error :(");
				commandDone(console);
				SDL_free(buffer);
			}

			SDL_free(data);
		}
	}
}

static void onMapExported(GetResult result, void* data)
{
	Console* console = (Console*)data;

	if(result == FS_FILE_NOT_DOWNLOADED)
		printBack(console, "\nmap not exported :|");
	else if (result == FS_FILE_DOWNLOADED)
		printBack(console, "\nmap successfully exported :)");

	commandDone(console);
}

static void exportMap(Console* console)
{
	enum{Size = sizeof(tic_map)};

	void* buffer = SDL_malloc(Size);

	if(buffer)
	{
		SDL_memcpy(buffer, console->tic->cart.bank.map.data, Size);
		fsGetFileData(onMapExported, "world.map", buffer, Size, DEFAULT_CHMOD, console);
	}
}

#if defined(__EMSCRIPTEN__)

static void onConsoleExportCommand(Console* console, const char* param)
{
	if(param == NULL || (param && strcmp(param, "native") == 0) || (param && strcmp(param, "html") == 0))
	{
		printBack(console, "\nweb/arm version doesn't support html or\nnative export");
		printBack(console, "\nusage: export sprites|cover|map");
		commandDone(console);
	}
	else if(param && strcmp(param, "sprites") == 0)
		exportSprites(console);
	else if(param && strcmp(param, "map") == 0)
		exportMap(console);
	else if(param && strcmp(param, "cover") == 0)
		exportCover(console);
	else
	{
		printError(console, "\nunknown parameter: ");
		printError(console, param);
		commandDone(console);
	}
}

#else

#if !defined(__ANDROID__) && !defined(__MACOSX__) && !defined(__LINUX__)

static void *memmem(const void* haystack, size_t hlen, const void* needle, size_t nlen)
{
	const u8* p = haystack;
	size_t plen = hlen;

	if(!nlen) return NULL;

	s32 needle_first = *(u8*)needle;

	while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
	{
		if (!memcmp(p, needle, nlen))
		return (void *)p;

		p++;
		plen = hlen - (p - (const u8*)haystack);
	}

	return NULL;
}

#endif

typedef struct
{
	Console* console;
	const char* cartName;
} AppFileReadParam;

typedef struct
{
	u8* data;
	size_t size;
} MemoryBuffer;

static void writeMemoryData(MemoryBuffer* memory, const u8* src, size_t size)
{
	memcpy(memory->data + memory->size, src, size);
	memory->size += size;
}

static void writeMemoryString(MemoryBuffer* memory, const char* str)
{
	size_t size = strlen(str);
	memcpy(memory->data + memory->size, str, size);
	memory->size += size;
}

static void onConsoleExportHtmlCommand(Console* console, const char* name)
{
	tic_mem* tic = console->tic;

	char cartName[FILENAME_MAX];
	strcpy(cartName, name);

	{
		static const char HtmlExt[] = ".html";
		const char* pos = NULL;

		if((pos = strstr(name, HtmlExt)) && strcmp(pos, HtmlExt) == 0);
		else strcat(cartName, HtmlExt);
	}

	extern const u8 EmbedIndexZip[];
	extern const s32 EmbedIndexZipSize;
	extern const u8 EmbedTicJsZip[];
	extern const s32 EmbedTicJsZipSize;

	static const char Placeholder[] = "<script async type=\"text/javascript\" src=\"tic.js\"></script>";

	u8* EmbedIndex = NULL;
	u32 EmbedIndexSize = unzip(&EmbedIndex, EmbedIndexZip, EmbedIndexZipSize);

	u8* EmbedTicJs = NULL;
	u32 EmbedTicJsSize = unzip(&EmbedTicJs, EmbedTicJsZip, EmbedTicJsZipSize);

	u8* ptr = memmem(EmbedIndex, EmbedIndexSize, Placeholder, sizeof(Placeholder)-1);

	if(ptr)
	{
		MemoryBuffer output = {(u8*)SDL_malloc(EmbedTicJsSize * 2), 0};

		if(output.data)
		{
			writeMemoryData(&output, EmbedIndex, ptr - EmbedIndex);
			writeMemoryString(&output, "<script type='text/javascript'>\n");

			u8* buffer = (u8*)SDL_malloc(sizeof(tic_cartridge));

			if(buffer)
			{
				writeMemoryString(&output, "var cartridge = [");

				s32 size = tic->api.save(&tic->cart, buffer);

				if(size)
				{
					// zip buffer
					{
						unsigned long outSize = sizeof(tic_cartridge);
						u8* output = (u8*)SDL_malloc(outSize);

						compress2(output, &outSize, buffer, size, Z_BEST_COMPRESSION);
						SDL_free(buffer);

						buffer = output;
						size = outSize;
					}

					{
						u8* ptr = buffer;
						u8* end = ptr + size;

						char value[] = "999,";
						while(ptr != end)
						{
							sprintf(value, "%i,", *ptr++);
							writeMemoryString(&output, value);
						}
					}

					SDL_free(buffer);

					writeMemoryString(&output, "];\n");

					writeMemoryData(&output, EmbedTicJs, EmbedTicJsSize);
					writeMemoryString(&output, "</script>\n");

					ptr += sizeof(Placeholder)-1;
					writeMemoryData(&output, ptr, EmbedIndexSize - (ptr - EmbedIndex));

					fsGetFileData(onFileDownloaded, cartName, output.data, output.size, DEFAULT_CHMOD, console);					
				}
			}
		}
	}

	SDL_free(EmbedIndex);
	SDL_free(EmbedTicJs);
}

#ifdef CAN_EXPORT

static void* embedCart(Console* console, s32* size)
{
	tic_mem* tic = console->tic;

	void* data = fsReadFile(console->appPath, size);

	if(data)
	{
		void* start = memmem(data, *size, embed.prefix, sizeof(embed.prefix));

		if(start)
		{
			embed.yes = true;
			SDL_memcpy(&embed.file, &tic->cart, sizeof(tic_cartridge));
			SDL_memcpy(start, &embed, sizeof(embed));
			embed.yes = false;
		}

		return data;
	}
	
	return NULL;
}

#if defined(__WINDOWS__)

static const char* getFileFolder(const char* path)
{
	static char folder[FILENAME_MAX];

	const char* pos = strrchr(path, '\\');

	if(!pos)
		pos = strrchr(path, '/');

	if(pos)
	{
		s32 size = pos - path;
		memcpy(folder, path, size);
		folder[size] = 0;

		return folder;
	}

	return NULL;
}

static bool exportToFolder(Console* console, const char* folder, const char* file)
{
	const char* workFolder = getFileFolder(console->appPath);

	if(workFolder)
	{
		char src[FILENAME_MAX];
		strcpy(src, workFolder);
		strcat(src, file);

		char dst[FILENAME_MAX];
		strcpy(dst, folder);
		strcat(dst, file);

		return fsCopyFile(src, dst);
	}

	return NULL;
}

static void onConsoleExportNativeCommand(Console* console, const char* cartName)
{
	const char* folder = folder_dialog(console);
	bool done = false;

	if(folder)
	{
		s32 size = 0;

		void* data = embedCart(console, &size);

		if(data)
		{
			char path[FILENAME_MAX];
			strcpy(path, folder);
			strcat(path, "\\game.exe");

			done = fsWriteFile(path, data, size);

			SDL_free(data);
		}
		else
		{
			printBack(console, "\ngame exporting error :(");
		}
	}

	if(done && exportToFolder(console, folder, "\\tic80.dll") &&
		exportToFolder(console, folder, "\\SDL2.dll"))
		printBack(console, "\ngame exported :)");
	else printBack(console, "\ngame not exported :|");

	commandDone(console);
}

#else

static void onConsoleExportNativeCommand(Console* console, const char* cartName)
{
	s32 size = 0;
	void* data = embedCart(console, &size);

	if(data)
	{
		fsGetFileData(onFileDownloaded, cartName, data, size, DEFAULT_CHMOD, console);
	}
	else
	{
		onFileDownloaded(FS_FILE_NOT_DOWNLOADED, console);
	}
}

#endif


#endif

static const char* getExportName(Console* console, bool html)
{
	static char name[FILENAME_MAX];

	if(strlen(console->romName))
	{
		memset(name, 0, sizeof name);
		memcpy(name, console->romName, strstr(console->romName, ".tic") - console->romName);
	}
	else
	{
		strcpy(name, "game");
	}


	if(html)
		strcat(name, ".html");
	else
	{
#if defined(__WINDOWS__)
		strcat(name, ExeExt);
#endif
	}

	return name;
}

static void onConsoleExportCommand(Console* console, const char* param)
{
	if(param)
	{
		if(strcmp(param, "html") == 0) onConsoleExportHtmlCommand(console, getExportName(console, true));
		else
		{
			if(strcmp(param, "native") == 0)
			{
#ifdef CAN_EXPORT
				onConsoleExportNativeCommand(console, getExportName(console, false));
#else
				printBack(console, "\nnative export isn't supported on this platform\n");
				commandDone(console);
#endif
			}
			else if(strcmp(param, "sprites") == 0)
			{
				exportSprites(console);
			}
			else if(strcmp(param, "map") == 0)
			{
				exportMap(console);
			}
			else if(strcmp(param, "cover") == 0)
			{
				exportCover(console);
			}
			else
			{
				printError(console, "\nunknown parameter: ");
				printError(console, param);
				commandDone(console);
			}
		}
	}
	else
	{
		onConsoleExportHtmlCommand(console, getExportName(console, true));
	}
}

#endif

static CartSaveResult saveCartName(Console* console, const char* name)
{
	tic_mem* tic = console->tic;

	bool success = false;

	if(name && strlen(name))
	{
		u8* buffer = (u8*)SDL_malloc(sizeof(tic_cartridge) * 3);

		if(buffer)
		{
			if(strcmp(name, CONFIG_TIC_PATH) == 0)
			{
				console->config->save(console->config);
				studioRomSaved();

				return CART_SAVE_OK;
			}
			else
			{
				s32 size = 0;

#if defined(TIC80_PRO)
				if(hasProjectExt(name))
				{
					size = saveProject(console, buffer, projectComment(name));
				}
				else
#endif
				{
					name = getCartName(name);
					size = tic->api.save(&tic->cart, buffer);
				}

				if(size && fsSaveFile(console->fs, name, buffer, size, true))
				{
					setCartName(console, name);
					success = true;
					studioRomSaved();
				}
			}

			SDL_free(buffer);
		}
	}
	else if (strlen(console->romName))
	{
		return saveCartName(console, console->romName);
	}
	else return CART_SAVE_MISSING_NAME;

	return success ? CART_SAVE_OK : CART_SAVE_ERROR;
}

static CartSaveResult saveCart(Console* console)
{
	return saveCartName(console, NULL);
}

static void onConsoleSaveCommandConfirmed(Console* console, const char* param)
{
	CartSaveResult rom = saveCartName(console, param);

	if(rom == CART_SAVE_OK)
	{
		printBack(console, "\ncart ");
		printFront(console, console->romName);
		printBack(console, " saved!\n");
	}
	else if(rom == CART_SAVE_MISSING_NAME)
		printBack(console, "\ncart name is missing\n");
	else
		printBack(console, "\ncart saving error");

	commandDone(console);
}

static void onConsoleSaveCommand(Console* console, const char* param)
{
	if(param && strlen(param) && fsExistsFile(console->fs, getCartName(param)))
	{
		static const char* Rows[] =
		{
			"THE CART",
			"ALREADY EXISTS",
			"",
			"DO YOU WANT TO",
			"OVERWRITE IT?",
		};

		confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleSaveCommandConfirmed);
	}
	else
	{
		onConsoleSaveCommandConfirmed(console, param);
	}
}

static void onConsoleRunCommand(Console* console, const char* param)
{
	commandDone(console);

	console->tic->api.reset(console->tic);

	setStudioMode(TIC_RUN_MODE);
}

static void onConsoleResumeCommand(Console* console, const char* param)
{
	commandDone(console);

	console->tic->api.resume(console->tic);
	console->tic->api.sync(console->tic, false);

	setStudioMode(TIC_RUN_MODE);
}

static void onAddFile(const char* name, AddResult result, void* data)
{
	Console* console = (Console*)data;

	printLine(console);

	switch(result)
	{
	case FS_FILE_EXISTS:
		printBack(console, "file ");
		printFront(console, name);
		printBack(console, " already exists :|");
		break;
	case FS_FILE_ADDED:
		printBack(console, "file ");
		printFront(console, name);
		printBack(console, " is successfully added :)");
		break;
	default:
		printBack(console, "file not added :(");
		break;
	}

	commandDone(console);
}

static void onConsoleAddCommand(Console* console, const char* param)
{
	fsAddFile(console->fs, onAddFile, console);
}

static void onConsoleGetCommand(Console* console, const char* param)
{
	if(param)
	{
		fsGetFile(console->fs, onFileDownloaded, param, console);
		return;
	}
	else printBack(console, "\nfile name is missing");

	commandDone(console);
}

static void onConsoleDelCommandConfirmed(Console* console, const char* param)
{
	if(param && strlen(param))
	{
		if(fsIsDir(console->fs, param))
		{
			printBack(console, fsDeleteDir(console->fs, param)
				? "\ndir not deleted"
				: "\ndir successfully deleted");
		}
		else
		{
			printBack(console, fsDeleteFile(console->fs, param)
				? "\nfile not deleted"
				: "\nfile successfully deleted");
		}
	}
	else printBack(console, "\nname is missing");

	commandDone(console);
}

static void onConsoleDelCommand(Console* console, const char* param)
{
	static const char* Rows[] =
	{
		"", "",
		"DO YOU REALLY WANT",
		"TO DELETE FILE?",
	};

	confirmCommand(console, Rows, COUNT_OF(Rows), param, onConsoleDelCommandConfirmed);
}

static void printTable(Console* console, const char* text)
{
	printf("%s", text);

	const char* textPointer = text;
	const char* endText = textPointer + strlen(text);

	while(textPointer != endText)
	{
		char symbol = *textPointer++;

		scrollConsole(console);

		if(symbol == '\n')
		{
			console->cursor.x = 0;
			console->cursor.y++;
		}
		else
		{
			s32 offset = console->cursor.x + console->cursor.y * CONSOLE_BUFFER_WIDTH;
			*(console->buffer + offset) = symbol;

			u8 color = 0;

			switch(symbol)
			{
			case '+':
			case '|':
			case '-':
				color = (tic_color_gray);
				break;
			default:
				color = (tic_color_white);
			}

			*(console->colorBuffer + offset) = color;

			console->cursor.x++;

			if(console->cursor.x >= CONSOLE_BUFFER_WIDTH)
			{
				console->cursor.x = 0;
				console->cursor.y++;
			}
		}

	}
}

static void printRamInfo(Console* console, s32 addr, const char* name, s32 size)
{
	char buf[STUDIO_TEXT_BUFFER_WIDTH];
	sprintf(buf, "\n| %05X | %-17s | %-5i |", addr, name, size);
	printTable(console, buf);
}

static void onConsoleRamCommand(Console* console, const char* param)
{
	printLine(console);

	printTable(console, "\n+-----------------------------------+" \
						"\n|           80K RAM LAYOUT          |" \
						"\n+-------+-------------------+-------+" \
						"\n| ADDR  | INFO              | SIZE  |" \
						"\n+-------+-------------------+-------+");

	static const struct{s32 addr; const char* info;} Layout[] =
	{
		{offsetof(tic_ram, vram.screen), 				"SCREEN"},
		{offsetof(tic_ram, vram.palette), 				"PALETTE"},
		{offsetof(tic_ram, vram.mapping), 				"PALETTE MAP"},
		{offsetof(tic_ram, vram.vars.colors), 			"BORDER"},
		{offsetof(tic_ram, vram.vars.offset), 			"SCREEN OFFSET"},
		{offsetof(tic_ram, vram.vars.mask), 			"GAMEPAD MASK"},
		{offsetof(tic_ram, vram.input.gamepad), 		"GAMEPAD"},
		{offsetof(tic_ram, vram.input.reserved), 		"..."},
		{offsetof(tic_ram, tiles), 						"TILES"},
		{offsetof(tic_ram, sprites), 					"SPRITES"},
		{offsetof(tic_ram, map), 						"MAP"},
		{offsetof(tic_ram, persistent), 				"PERSISTENT MEMORY"},
		{offsetof(tic_ram, registers), 					"SOUND REGISTERS"},
		{offsetof(tic_ram, sfx.waveform), 				"WAVEFORMS"},
		{offsetof(tic_ram, sfx.data), 					"SFX"},
		{offsetof(tic_ram, music.patterns.data), 		"MUSIC PATTERNS"},
		{offsetof(tic_ram, music.tracks.data), 			"MUSIC TRACKS"},
		{offsetof(tic_ram, music_pos), 					"MUSIC POS"},
		{TIC_RAM_SIZE, 									"..."},
	};

	enum{Last = COUNT_OF(Layout)-1};

	for(s32 i = 0; i < Last; i++)
		printRamInfo(console, Layout[i].addr, Layout[i].info, Layout[i+1].addr-Layout[i].addr);

	printRamInfo(console, Layout[Last].addr, Layout[Last].info, 0);

	printTable(console, "\n+-------+-------------------+-------+");

	printLine(console);
	commandDone(console);
}

static const struct
{
	const char* command;
	const char* alt;
	const char* info;
	void(*handler)(Console*, const char*);

} AvailableConsoleCommands[] =
{
	{"help", 	NULL, "show this info", 			onConsoleHelpCommand},
	{"ram", 	NULL, "show memory info", 			onConsoleRamCommand},
	{"exit", 	"quit", "exit the application", 	onConsoleExitCommand},
	{"new", 	NULL, "create new cart",			onConsoleNewCommand},
	{"load", 	NULL, "load cart", 					onConsoleLoadCommand},
	{"save", 	NULL, "save cart",	 				onConsoleSaveCommand},
	{"run",		NULL, "run loaded cart",			onConsoleRunCommand},
	{"resume",	NULL, "resume run cart",			onConsoleResumeCommand},
	{"dir",		"ls", "show list of files", 		onConsoleDirCommand},
	{"cd",		NULL, "change directory", 			onConsoleChangeDirectory},
	{"mkdir",	NULL, "make directory", 			onConsoleMakeDirectory},
#ifdef CAN_EXPORT
	{"folder",	NULL, "open working folder in OS", 	onConsoleFolderCommand},
#endif
	{"add",		NULL, "add file", 					onConsoleAddCommand},
	{"get",		NULL, "download file", 				onConsoleGetCommand},
	{"export",	NULL, "export html or native game",	onConsoleExportCommand},
	{"import",	NULL, "import sprites from .gif",	onConsoleImportCommand},
	{"del",		NULL, "delete file or dir",			onConsoleDelCommand},
	{"cls",		NULL, "clear screen",				onConsoleClsCommand},
	{"demo",	NULL, "install demo carts",			onConsoleInstallDemosCommand},
	{"config",	NULL, "edit TIC config",			onConsoleConfigCommand},
	{"keymap",	NULL, "configure keyboard mapping",	onConsoleKeymapCommand},
	{"version",	NULL, "show the current version",	onConsoleVersionCommand},
	{"edit",	NULL, "open cart editor",			onConsoleCodeCommand},
	{"surf",	NULL, "open carts browser",			onConsoleSurfCommand},
};

static bool predictFilename(const char* name, const char* info, s32 id, void* data, bool dir)
{
	char* buffer = (char*)data;

	if(strstr(name, buffer) == name)
	{
		strcpy(buffer, name);
		return false;
	}

	return true;
}

static void processConsoleTab(Console* console)
{
	char* input = console->inputBuffer;

	if(strlen(input))
	{
		char* param = SDL_strchr(input, ' ');

		if(param && strlen(++param))
		{
			fsEnumFiles(console->fs, predictFilename, param);
			console->inputPosition = strlen(input);
		}
		else
		{
			for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
			{
				const char* command = AvailableConsoleCommands[i].command;

				if(strstr(command, input) == command)
				{
					strcpy(input, command);
					console->inputPosition = strlen(input);
					break;
				}
			}
		}
	}
}

static void toUpperStr(char* str)
{
	while(*str)
	{
		*str = SDL_toupper(*str);
		str++;
	}
}

static void onConsoleHelpCommand(Console* console, const char* param)
{
	printBack(console, "\navailable commands:\n\n");

	size_t maxName = 0;
	for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
	{
		size_t len = strlen(AvailableConsoleCommands[i].command);

		{
			const char* alt = AvailableConsoleCommands[i].alt;
			if(alt)
				len += strlen(alt) + 1;			
		}

		if(len > maxName) maxName = len;
	}

	char upName[64];

	for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
	{
		const char* command = AvailableConsoleCommands[i].command;

		{
			strcpy(upName, command);
			toUpperStr(upName);
			printFront(console, upName);
		}

		const char* alt = AvailableConsoleCommands[i].alt;

		if(alt)
		{
			strcpy(upName, alt);
			toUpperStr(upName);
			printBack(console, "/");
			printFront(console, upName);
		}

		size_t len = maxName - strlen(command) - (alt ? strlen(alt) : -1);
		while(len--) printBack(console, " ");

		printBack(console, AvailableConsoleCommands[i].info);
		printLine(console);
	}

	printBack(console, "\npress ");

#if defined(__ANDROID__)
	printFront(console, "BACK");
#else
	printFront(console, "ESC");
#endif

	printBack(console, " to enter UI mode\n");

	commandDone(console);
}

static s32 tic_strcasecmp(const char *str1, const char *str2)
{
	char a = 0;
	char b = 0;
	while (*str1 && *str2) {
		a = SDL_toupper((unsigned char) *str1);
		b = SDL_toupper((unsigned char) *str2);
		if (a != b)
			break;
		++str1;
		++str2;
	}
	a = SDL_toupper(*str1);
	b = SDL_toupper(*str2);
	return (int) ((unsigned char) a - (unsigned char) b);
}

static void processCommand(Console* console, const char* command)
{
	while(*command == ' ')
		command++;

	char* end = (char*)command + strlen(command) - 1;

	while(*end == ' ' && end > command)
		*end-- = '\0';

	char* param = SDL_strchr(command, ' ');

	if(param)
		*param++ = '\0';

	if(param && !strlen(param)) param = NULL;

	for(s32 i = 0; i < COUNT_OF(AvailableConsoleCommands); i++)
		if(tic_strcasecmp(command, AvailableConsoleCommands[i].command) == 0 ||
			(AvailableConsoleCommands[i].alt && tic_strcasecmp(command, AvailableConsoleCommands[i].alt) == 0))
		{
			if(AvailableConsoleCommands[i].handler)
			{
				AvailableConsoleCommands[i].handler(console, param);
				command = NULL;
				break;
			}
		}

	if(command)
	{
		printLine(console);
		printError(console, "unknown command:");
		printError(console, console->inputBuffer);
		commandDone(console);
	}
}

static void fillInputBufferFromHistory(Console* console)
{
	memset(console->inputBuffer, 0, sizeof(console->inputBuffer));
	strcpy(console->inputBuffer, console->history->value);
	processConsoleEnd(console);
}

static void onHistoryUp(Console* console)
{
	if(console->history)
	{
		if(console->history->next)
			console->history = console->history->next;
	}
	else console->history = console->historyHead;

	if(console->history)
		fillInputBufferFromHistory(console);
}

static void onHistoryDown(Console* console)
{
	if(console->history)
	{
		if(console->history->prev)
			console->history = console->history->prev;

		fillInputBufferFromHistory(console);
	}
}

static void appendHistory(Console* console, const char* value)
{
	HistoryItem* item = (HistoryItem*)SDL_malloc(sizeof(HistoryItem));
	item->value = SDL_strdup(value);
	item->next = NULL;
	item->prev = NULL;

	if(console->historyHead == NULL)
	{
		console->historyHead = item;
		return;
	}

	console->historyHead->prev = item;
	item->next = console->historyHead;
	console->historyHead = item;
	console->history = NULL;
}

static void processConsoleCommand(Console* console)
{
	console->inputPosition = 0;

	size_t commandSize = strlen(console->inputBuffer);

	if(commandSize)
	{
		printFront(console, console->inputBuffer);
		appendHistory(console, console->inputBuffer);

		processCommand(console, console->inputBuffer);

		memset(console->inputBuffer, 0, sizeof(console->inputBuffer));
	}
	else commandDone(console);
}

static void error(Console* console, const char* info)
{
	consolePrint(console, info ? info : "unknown error", (tic_color_red));
	commandDone(console);
}

static void trace(Console* console, const char* text, u8 color)
{
	consolePrint(console, text, color);
	commandDone(console);
}

static void setScroll(Console* console, s32 val)
{
	if(console->scroll.pos != val)
	{
		console->scroll.pos = val;

		if(console->scroll.pos < 0) console->scroll.pos = 0;
		if(console->scroll.pos > console->cursor.y) console->scroll.pos = console->cursor.y;
	}
}

static void processGesture(Console* console)
{
	SDL_Point point = {0, 0};

	if(getGesturePos(&point))
	{
		if(console->scroll.active)
			setScroll(console, (console->scroll.start - point.y) / STUDIO_TEXT_HEIGHT);
		else
		{
			console->scroll.start = point.y + console->scroll.pos * STUDIO_TEXT_HEIGHT;
			console->scroll.active = true;
		}
	}
	else console->scroll.active = false;
}

static void checkNewVersion(Console* console)
{
	Net* net = createNet();

	if(net)
	{
		NetVersion version = netVersionRequest(net);
		SDL_free(net);

		if((version.major > TIC_VERSION_MAJOR) ||
			(version.major == TIC_VERSION_MAJOR && version.minor > TIC_VERSION_MINOR) ||
			(version.major == TIC_VERSION_MAJOR && version.minor == TIC_VERSION_MINOR && version.patch > TIC_VERSION_PATCH))
		{
			char msg[FILENAME_MAX] = {0};
			sprintf(msg, "\n A new version %i.%i.%i is available.\n", version.major, version.minor, version.patch);
			consolePrint(console, msg, (tic_color_light_green));
		}
	}
}

static void tick(Console* console)
{
	SDL_Event* event = NULL;
	while ((event = pollEvent()))
	{
		switch(event->type)
		{
		case SDL_MOUSEWHEEL:
			{
				enum{Scroll = 3};
				s32 delta = event->wheel.y > 0 ? -Scroll : Scroll;
				setScroll(console, console->scroll.pos + delta);
			}
			break;
		case SDL_KEYDOWN:
			{
				switch(event->key.keysym.sym)
				{
				case SDLK_UP:
					onHistoryUp(console);
					break;
				case SDLK_DOWN:
					onHistoryDown(console);
					break;
				case SDLK_RIGHT:
					{
						console->inputPosition++;
						size_t len = strlen(console->inputBuffer);
						if(console->inputPosition > len)
							console->inputPosition = len;
					}
					break;
				case SDLK_LEFT:
					{
						if(console->inputPosition > 0)
							console->inputPosition--;
					}
					break;
				case SDLK_RETURN: 		processConsoleCommand(console); break;
				case SDLK_BACKSPACE: 	processConsoleBackspace(console); break;
				case SDLK_DELETE: 		processConsoleDel(console); break;
				case SDLK_HOME: 		processConsoleHome(console); break;
				case SDLK_END: 			processConsoleEnd(console); break;
				case SDLK_TAB: 			processConsoleTab(console); break;

				default: break;
				}

				scrollConsole(console);
				console->cursor.delay = CONSOLE_CURSOR_DELAY;
			}
			break;
		case SDL_TEXTINPUT:
			{
				const char* symbol = event->text.text;

				if(strlen(symbol) == 1)
				{
					char sym = *symbol;

					size_t size = strlen(console->inputBuffer);

					if(size < sizeof(console->inputBuffer))
					{
						char* pos = console->inputBuffer + console->inputPosition;
						memmove(pos + 1, pos, strlen(pos));

						*(console->inputBuffer + console->inputPosition) = sym;
						console->inputPosition++;
					}
				}

				console->cursor.delay = CONSOLE_CURSOR_DELAY;
			}
			break;
		}
	}

	processGesture(console);

	if(console->tickCounter == 0)
	{
		if(!embed.yes)
		{
			loadDemo(console, tic_script_lua);

			printBack(console, "\n hello! type ");
			printFront(console, "help");
			printBack(console, " for help\n");

			if(getConfig()->checkNewVersion)
				checkNewVersion(console);

			commandDone(console);
		}
		else printBack(console, "\n loading cart...");
	}

	console->tic->api.clear(console->tic, TIC_COLOR_BG);
	drawConsoleText(console);

	if(embed.yes)
	{
		if(console->tickCounter >= (u32)(console->skipStart ? 1 : TIC_FRAMERATE))
		{
			if(!console->skipStart)
				console->showGameMenu = true;

			memcpy(&console->tic->cart, &embed.file, sizeof(tic_cartridge));
			setStudioMode(TIC_RUN_MODE);
			embed.yes = false;
			console->skipStart = false;
			studioRomLoaded();

			console->tic->api.reset(console->tic);

			printLine(console);
			commandDone(console);
			console->active = true;

			return;
		}
	}
	else
	{	
		if(console->cursor.delay)
			console->cursor.delay--;

		if(getStudioMode() != TIC_CONSOLE_MODE) return;

		drawConsoleInputText(console);
	}

	console->tickCounter++;

	if(console->startSurf)
	{
		console->startSurf = false;
		gotoSurf();
	}
}

static bool cmdLoadCart(Console* console, const char* name)
{
	bool done = false;

	s32 size = 0;
	void* data = fsReadFile(name, &size);

	if(data)
	{
#if defined(TIC80_PRO)
		if(hasProjectExt(name))
		{
			loadProject(console, name, data, size, &embed.file);
			setCartName(console, fsFilename(name));
			embed.yes = true;
			console->skipStart = true;
			done = true;
		}
		else
#endif

		if(hasExt(name, CART_EXT))
		{
			loadCart(console->tic, &embed.file, data, size, true);			
			setCartName(console, fsFilename(name));
			embed.yes = true;
			done = true;
		}
		
		SDL_free(data);
	}

	return done;
}

static bool loadFileIntoBuffer(Console* console, char* buffer, const char* fileName)
{
	s32 size = 0;
	void* contents = fsReadFile(fileName, &size);

	if(contents)
	{
		memset(buffer, 0, TIC_CODE_SIZE);

		if(size > TIC_CODE_SIZE)
		{
			char messageBuffer[256];
			sprintf(messageBuffer, "\n code is larger than %i symbols\n", TIC_CODE_SIZE);

			printError(console, messageBuffer);
		}

		memcpy(buffer, contents, SDL_min(size, TIC_CODE_SIZE-1));
		SDL_free(contents);

		return true;
	}

	return false;
}

static void tryReloadCode(Console* console, char* codeBuffer)
{
	if(console->codeLiveReload.active)
	{
		const char* fileName = console->codeLiveReload.fileName;
		loadFileIntoBuffer(console, codeBuffer, fileName);
	}
}

static bool cmdInjectCode(Console* console, const char* param, const char* name)
{
	bool done = false;

	bool watch = strcmp(param, "-code-watch") == 0;
	if(watch || strcmp(param, "-code") == 0)
	{
		bool loaded = loadFileIntoBuffer(console, embed.file.code.data, name);

		if(loaded)
		{
			embed.yes = true;
			console->skipStart = true;
			done = true;

			if(watch)
			{
				console->codeLiveReload.active = true;
				strcpy(console->codeLiveReload.fileName, name);
			}
		}
	}

	return done;
}

static bool cmdInjectSprites(Console* console, const char* param, const char* name)
{
	bool done = false;

	if(strcmp(param, "-sprites") == 0)
	{
		s32 size = 0;
		void* sprites = fsReadFile(name, &size);

		if(sprites)
		{
			gif_image* image = gif_read_data(sprites, size);

			if (image)
			{
				enum
				{
					Width = TIC_SPRITESHEET_SIZE,
					Height = TIC_SPRITESHEET_SIZE*2,
				};

				s32 w = SDL_min(Width, image->width);
				s32 h = SDL_min(Height, image->height);

				for (s32 y = 0; y < h; y++)
					for (s32 x = 0; x < w; x++)
					{
						u8 src = image->buffer[x + y * image->width];
						const gif_color* c = &image->palette[src];
						tic_rgb rgb = {c->r, c->g, c->b};
						u8 color = tic_tool_find_closest_color(embed.file.palette.colors, &rgb);

						setSpritePixel(embed.file.bank.tiles.data, x, y, color);
					}

				gif_close(image);
			}

			SDL_free(sprites);

			embed.yes = true;
			console->skipStart = true;
			done = true;
		}

	}

	return done;
}

static bool cmdInjectMap(Console* console, const char* param, const char* name)
{
	bool done = false;

	if(strcmp(param, "-map") == 0)
	{
		s32 size = 0;
		void* map = fsReadFile(name, &size);

		if(map)
		{
			if(size <= sizeof(tic_map))
			{
				injectMap(console, map, size);

				embed.yes = true;
				console->skipStart = true;
				done = true;
			}

			SDL_free(map);
		}
	}

	return done;
}

void initConsole(Console* console, tic_mem* tic, FileSystem* fs, Config* config, s32 argc, char **argv)
{
	if(!console->buffer) console->buffer = SDL_malloc(CONSOLE_BUFFER_SIZE);
	if(!console->colorBuffer) console->colorBuffer = SDL_malloc(CONSOLE_BUFFER_SIZE);

	*console = (Console)
	{
		.tic = tic,
		.config = config,
		.load = onConsoleLoadCommandConfirmed,

#if defined(TIC80_PRO)
		.loadProject = loadProject,
		.updateProject = updateProject,
#else
		.loadProject = NULL,
		.updateProject = NULL,
#endif
		.error = error,
		.trace = trace,
		.tick = tick,
		.save = saveCart,
		.cursor = {.x = 0, .y = 0, .delay = 0},
		.scroll =
		{
			.pos = 0,
			.start = 0,
			.active = false,
		},
		.codeLiveReload =
		{
			.active = false,
			.reload = tryReloadCode,
		},
		.inputPosition = 0,
		.history = NULL,
		.historyHead = NULL,
		.tickCounter = 0,
		.active = false,
		.buffer = console->buffer,
		.colorBuffer = console->colorBuffer,
		.fs = fs,
		.showGameMenu = false,
		.startSurf = false,
		.skipStart = false,
		.goFullscreen = false,
	};

	memset(console->buffer, 0, CONSOLE_BUFFER_SIZE);
	memset(console->colorBuffer, TIC_COLOR_BG, CONSOLE_BUFFER_SIZE);

	memset(console->codeLiveReload.fileName, 0, FILENAME_MAX);

	if(argc)
	{
		strcpy(console->appPath, argv[0]);

#if defined(__WINDOWS__)
		if(!strstr(console->appPath, ExeExt))
			strcat(console->appPath, ExeExt);
#endif
	}

	printFront(console, "\n " TIC_NAME_FULL "");
	printBack(console, " " TIC_VERSION_LABEL "\n");
	printBack(console, " " TIC_COPYRIGHT "\n");

	if(argc > 1)
	{
		memcpy(embed.file.palette.data, tic->config.palette.data, sizeof(tic_palette));

		u32 argp = 1;

		// trying to load cart
		for (s32 i = 1; i < argc; i++)
		{
			if(cmdLoadCart(console, argv[i]))
			{
				argp |= 1 << i;
				break;
			}
		}

		// process '-key val' params
		for (s32 i = 1; i < argc-1; i++)
		{
			s32 mask = 0b11 << i;

			if(~argp & mask)
			{
				const char* first = argv[i];
				const char* second = argv[i + 1];

				if(cmdInjectCode(console, first, second)
					|| cmdInjectSprites(console, first, second)
					|| cmdInjectMap(console, first, second))
					argp |= mask;				
			}
		}

		// proccess single params
		for (s32 i = 1; i < argc; i++)
		{
			if(~argp & 1 << i)
			{
				const char* arg = argv[i];

				if(strcmp(arg, "-nosound") == 0)
					config->data.noSound = true;

				else if(strcmp(arg, "-surf") == 0)
					console->startSurf = true;

				else if(strcmp(arg, "-fullscreen") == 0)
					console->goFullscreen = true;

				else if(strcmp(arg, "-skip") == 0)
					console->skipStart = true;

				else continue;

				argp |= 0b1 << i;
			}
		}

		for (s32 i = 1; i < argc; i++)
		{
			if(~argp & 1 << i)
			{
				char buf[256];
				sprintf(buf, "parameter or file not processed: %s\n", argv[i]);
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", buf, NULL);
			}
		}
	}

#if defined(__EMSCRIPTEN__)

	if(!embed.yes)
	{
		void* cartPtr = (void*)EM_ASM_INT_V
		(
			if(typeof cartridge != 'undefined' && cartridge.length)
			{
				var ptr = Module._malloc(cartridge.length);

				Module.writeArrayToMemory(cartridge, ptr);

				return ptr;
			}
			else return 0;
		);

		if(cartPtr)
		{
			embed.yes = true;

			s32 cartSize = EM_ASM_INT_V(return cartridge.length;);

			u8* data = NULL;
			s32 size = unzip(&data, cartPtr, cartSize);
			loadCart(tic, &embed.file, data, size, true);

			SDL_free(data);

			EM_ASM_({Module._free($0);}, cartPtr);
		}
	}

#endif

	console->active = !embed.yes;
}