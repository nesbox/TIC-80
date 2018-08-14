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

#include "surf.h"
#include "fs.h"
#include "console.h"

#include "ext/gif.h"

#include <string.h>

#define MAIN_OFFSET 4
#define MENU_HEIGHT 10
#define MAX_CARTS 256
#define ANIM 10
#define COVER_WIDTH 140
#define COVER_HEIGHT 116
#define COVER_Y 5
#define COVER_X (TIC80_WIDTH - COVER_WIDTH - COVER_Y)

#if defined(__TIC_WINDOWS__) || defined(__TIC_LINUX__) || defined(__TIC_MACOSX__)
#define CAN_OPEN_URL 1
#endif

typedef struct
{
	s32 start;
	s32 end;
	s32 duration;

	s32* val;
} Anim;

typedef struct Movie Movie;

struct Movie
{
	Anim** items;

	s32 time;
	s32 duration;
	s32 count;

	Movie* next;
	void (*done)(Surf* surf);
};

static struct
{
	s32 topBarY;
	s32 bottomBarY;
	s32 menuX;
	s32 menuHeight;
} AnimVar;

static Anim topBarShowAnim = {0, MENU_HEIGHT, ANIM, &AnimVar.topBarY};
static Anim bottomBarShowAnim = {0, MENU_HEIGHT, ANIM, &AnimVar.bottomBarY};

static Anim topBarHideAnim = {MENU_HEIGHT, 0, ANIM, &AnimVar.topBarY};
static Anim bottomBarHideAnim = {MENU_HEIGHT, 0, ANIM, &AnimVar.bottomBarY};

static Anim menuLeftHideAnim = {0, -240, ANIM, &AnimVar.menuX};
static Anim menuRightHideAnim = {0, 240, ANIM, &AnimVar.menuX};
static Anim menuHideAnim = {MENU_HEIGHT, 0, ANIM, &AnimVar.menuHeight};

static Anim menuLeftShowAnim = {240, 0, ANIM, &AnimVar.menuX};
static Anim menuRightShowAnim = {-240, 0, ANIM, &AnimVar.menuX};
static Anim menuShowAnim = {0, MENU_HEIGHT, ANIM, &AnimVar.menuHeight};

static Anim* MenuModeShowMovieItems[] = 
{
	&topBarShowAnim,
	&bottomBarShowAnim,
	&menuRightShowAnim,
	&menuShowAnim,
};

static Anim* MenuModeHideMovieItems[] = 
{
	&topBarHideAnim,
	&bottomBarHideAnim,
	&menuLeftHideAnim,
	&menuHideAnim,
};

static Anim* MenuLeftHideMovieItems[] = 
{
	&menuLeftHideAnim,
	&menuHideAnim,
};

static Anim* MenuRightHideMovieItems[] = 
{
	&menuRightHideAnim,
	&menuHideAnim,
};

static Anim* MenuLeftShowMovieItems[] = 
{
	&menuLeftShowAnim,
	&menuShowAnim,
};

static Anim* MenuRightShowMovieItems[] = 
{
	&menuRightShowAnim,
	&menuShowAnim,
};

static Movie EmptyState;
static Movie MenuModeState;

#define DECLARE_MOVIE(NAME, NEXT) static Movie NAME ## State = \
{ \
	.items = NAME ## MovieItems, \
	.count = COUNT_OF(NAME ## MovieItems), \
	.duration = ANIM, \
	.next = & NEXT ## State, \
}

DECLARE_MOVIE(MenuModeShow, MenuMode);
DECLARE_MOVIE(MenuModeHide, Empty);
DECLARE_MOVIE(MenuLeftShow,  MenuMode);
DECLARE_MOVIE(MenuRightShow, MenuMode);
DECLARE_MOVIE(MenuLeftHide, MenuLeftShow);
DECLARE_MOVIE(MenuRightHide, MenuRightShow);

typedef struct MenuItem MenuItem;

struct MenuItem
{
	char* label;
	const char* name;
	const char* hash;
	s32 id;
	tic_screen* cover;
	bool dir;
	bool project;
};

typedef struct
{
	MenuItem* items;
	s32 count;
	Surf* surf;
} AddMenuItem;

static void resetMovie(Surf* surf, Movie* movie, void (*done)(Surf* surf))
{
	surf->state = movie;

	movie->time = 0;
	for(s32 i = 0; i < movie->count; i++)
	{
		Anim* anim = movie->items[i];
		*anim->val = anim->start;
	}

	movie->time = 0;
	movie->done = done;
}

static void drawTopToolbar(Surf* surf, s32 x, s32 y)
{
	tic_mem* tic = surf->tic;

	enum{Height = MENU_HEIGHT};

	tic->api.rect(tic, x, y, TIC80_WIDTH, Height, tic_color_blue);
	tic->api.rect(tic, x, y + Height, TIC80_WIDTH, 1, tic_color_black);
	{
		char label[FILENAME_MAX];

		sprintf(label, "%s", "TIC-80 SURF");

		s32 xl = x + MAIN_OFFSET;
		s32 yl = y + (Height - TIC_FONT_HEIGHT)/2;
		tic->api.text(tic, label, xl, yl+1, tic_color_black, false);
		tic->api.text(tic, label, xl, yl, tic_color_white, false);
	}

	enum{Gap = 10, TipX = 150, SelectWidth = 54};

	u8 colorkey = 0;
	tic->api.sprite_ex(tic, &getConfig()->cart->bank0.tiles, 12, TipX, y+1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
	{
		static const char Label[] = "SELECT";
		tic->api.text(tic, Label, TipX + Gap, y+3, tic_color_black, false);
		tic->api.text(tic, Label, TipX + Gap, y+2, tic_color_white, false);
	}

	tic->api.sprite_ex(tic, &getConfig()->cart->bank0.tiles, 13, TipX + SelectWidth, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
	{
		static const char Label[] = "BACK";
		tic->api.text(tic, Label, TipX + Gap + SelectWidth, y +3, tic_color_black, false);
		tic->api.text(tic, Label, TipX + Gap + SelectWidth, y +2, tic_color_white, false);
	}
}

static void drawBottomToolbar(Surf* surf, s32 x, s32 y)
{
	tic_mem* tic = surf->tic;

	enum{Height = MENU_HEIGHT};

	tic->api.rect(tic, x, y, TIC80_WIDTH, Height, tic_color_blue);
	tic->api.rect(tic, x, y + Height, TIC80_WIDTH, 1, tic_color_black);
	{
		char label[FILENAME_MAX];
		char dir[FILENAME_MAX];
		fsGetDir(surf->fs, dir);

		sprintf(label, "/%s", dir);
		s32 xl = x + MAIN_OFFSET;
		s32 yl = y + (Height - TIC_FONT_HEIGHT)/2;
		tic->api.text(tic, label, xl, yl+1, tic_color_black, false);
		tic->api.text(tic, label, xl, yl, tic_color_white, false);
	}

#ifdef CAN_OPEN_URL	

	if(surf->menu.items[surf->menu.pos].hash)
	{
		enum{Gap = 10, TipX = 134, SelectWidth = 54};

		u8 colorkey = 0;

		tic->api.sprite_ex(tic, &getConfig()->cart->bank0.tiles, 15, TipX + SelectWidth, y + 1, 1, 1, &colorkey, 1, 1, tic_no_flip, tic_no_rotate);
		{
			static const char Label[] = "WEBSITE";
			tic->api.text(tic, Label, TipX + Gap + SelectWidth, y +3, tic_color_black, false);
			tic->api.text(tic, Label, TipX + Gap + SelectWidth, y +2, tic_color_white, false);
		}
	}
#endif

}

static void drawCover(Surf* surf, s32 pos, s32 x, s32 y)
{
	if(!surf->menu.items[surf->menu.pos].cover)
		return;

	tic_mem* tic = surf->tic;

	enum{Width = TIC80_WIDTH, Height = TIC80_HEIGHT};

	tic_screen* cover = surf->menu.items[pos].cover;

	if(cover)
	{
		for(s32 yc = 0; yc < Height; yc++)
			memcpy(tic->ram.vram.screen.data + (yc * TIC80_WIDTH)/2, cover->data + (yc * Width)/2, Width/2);
	}
}

static void drawInverseRect(tic_mem* tic, s32 x, s32 y, s32 w, s32 h)
{
	if(x < 0)
	{
		w += x;
		x = 0;
	}

	if(y < 0)
	{
		h += y;
		y = 0;
	}

	w += x;
	h += y;

	if(w > TIC80_WIDTH)
		w = TIC80_WIDTH;

	if(h > TIC80_HEIGHT)
		h = TIC80_HEIGHT;

	for(s32 j = y; j < h; j++)
	{
		for(s32 i = x; i < w; i++)
		{
			s32 index = i + j*TIC80_WIDTH;
			u8 color = tic_tool_peek4(tic->ram.vram.screen.data, index);
			tic_tool_poke4(tic->ram.vram.screen.data, index, color % 4);
		}            
	}
}

static void drawMenu(Surf* surf, s32 x, s32 y, bool bg)
{
	tic_mem* tic = surf->tic;

	enum {Height = MENU_HEIGHT};

	if(bg)
	{
		if(AnimVar.menuHeight)
			drawInverseRect(tic, 0, y + (MENU_HEIGHT - AnimVar.menuHeight)/2 - 1, TIC80_WIDTH, AnimVar.menuHeight+2);    
	}
	else
	{
		tic->api.rect(tic, 0, y + (MENU_HEIGHT - AnimVar.menuHeight)/2, TIC80_WIDTH, AnimVar.menuHeight, tic_color_red);
	}

	for(s32 i = 0; i < surf->menu.count; i++)
	{
		const char* name = surf->menu.items[i].label;

		s32 ym = Height * i + y - surf->menu.pos*MENU_HEIGHT - surf->menu.anim + (MENU_HEIGHT - TIC_FONT_HEIGHT)/2;

		if(bg)
		{
			s32 size = tic->api.text(tic, name, 0, -TIC_FONT_HEIGHT, 0, false);

			drawInverseRect(tic, x + MAIN_OFFSET - 1, ym-1, size+1, TIC_FONT_HEIGHT+2);
		}
		else
		{
			tic->api.text(tic, name, x + MAIN_OFFSET, ym + 1, tic_color_black, false);
			tic->api.text(tic, name, x + MAIN_OFFSET, ym, tic_color_white, false);
		}
	}
}

static void drawBG(Surf* surf)
{
	tic_mem* tic = surf->tic;

	enum{Size = 16, Width = TIC80_WIDTH/Size+1, Height = TIC80_HEIGHT/Size+1};

	s32 offset = surf->ticks % Size;
	s32 counter = 0;

	for(s32 j = 0; j < Height + 1; j++)
		for(s32 i = 0; i < Width + 1; i++)
			if(counter++ % 2)
				tic->api.sprite_ex(tic, &getConfig()->cart->bank0.tiles, 34, i*Size - offset, j*Size - offset, 2, 2, 0, 0, 1, tic_no_flip, tic_no_rotate);
}

static void replace(char* src, const char* what, const char* with)
{
	while(true)
	{
		char* pos = strstr(src, what);

		if(pos)
		{
			strcpy(pos, pos + strlen(what) - strlen(with));
			memcpy(pos, with, strlen(with));
		}
		else break;     
	}
}

static bool hasExt(const char* name, const char* ext)
{
	return strcmp(name + strlen(name) - strlen(ext), ext) == 0;
}

static void cutExt(char* name, const char* ext)
{
	name[strlen(name)-strlen(ext)] = '\0';
}

static bool addMenuItem(const char* name, const char* info, s32 id, void* ptr, bool dir)
{
	AddMenuItem* data = (AddMenuItem*)ptr;

	static const char CartExt[] = CART_EXT;

	if(dir 
		|| hasExt(name, CartExt)
#if defined(TIC80_PRO)		
		|| hasExt(name, PROJECT_LUA_EXT)
		|| hasExt(name, PROJECT_MOON_EXT)
		|| hasExt(name, PROJECT_JS_EXT)
		|| hasExt(name, PROJECT_WREN_EXT)
#endif
		)
	{
		MenuItem* item = &data->items[data->count++];

		item->name = strdup(name);
		bool project = false;
		if(dir)
		{
			char folder[FILENAME_MAX];
			sprintf(folder, "[%s]", name);
			item->label = strdup(folder);
		}
		else
		{

			item->label = strdup(name);

			if(hasExt(name, CartExt))
				cutExt(item->label, CartExt);
			else
			{
				project = true;
			}


			replace(item->label, "&amp;", "&");
			replace(item->label, "&#39;", "'");
		}

		item->hash = info ? strdup(info) : NULL;
		item->id = id;
		item->dir = dir;
		item->cover = NULL;
		item->project = project;
	}

	return data->count < MAX_CARTS;
}

static void resetMenu(Surf* surf)
{
	if(surf->menu.items)
	{
		for(s32 i = 0; i < surf->menu.count; i++)
		{
			free((void*)surf->menu.items[i].name);

			const char* hash = surf->menu.items[i].hash;
			if(hash) free((void*)hash);

			tic_screen* cover = surf->menu.items[i].cover;
			if(cover) free(cover);

			const char* label = surf->menu.items[i].label;
			if(label) free((void*)label);
		}

		free(surf->menu.items);

		surf->menu.items = NULL;
		surf->menu.count = 0;
	}

	surf->menu.pos = 0;
	surf->menu.anim = 0;
}

static void* requestCover(Surf* surf, const char* hash, s32* size)
{
	char cachePath[FILENAME_MAX] = {0};
	sprintf(cachePath, TIC_CACHE "%s.gif", hash);

	{
		void* data = fsLoadRootFile(surf->fs, cachePath, size);

		if(data)
			return data;
	}

	char path[FILENAME_MAX] = {0};
	sprintf(path, "/cart/%s/cover.gif", hash);
	void* data = getSystem()->getUrlRequest(path, size);

	if(data)
	{
		fsSaveRootFile(surf->fs, cachePath, data, *size, false);
	}

	return data;
}

static void updateMenuItemCover(Surf* surf, const u8* cover, s32 size)
{
	MenuItem* item = &surf->menu.items[surf->menu.pos];

	item->cover = malloc(sizeof(tic_screen));

	gif_image* image = gif_read_data(cover, size);

	if(image)
	{
		if (image->width == TIC80_WIDTH && image->height == TIC80_HEIGHT)
		{
			enum { Size = TIC80_WIDTH * TIC80_HEIGHT };

			for (s32 i = 0; i < Size; i++)
			{
				const gif_color* c = &image->palette[image->buffer[i]];
				tic_rgb rgb = { c->r, c->g, c->b };
				u8 color = tic_tool_find_closest_color(getConfig()->cart->bank0.palette.colors, &rgb);
				tic_tool_poke4(item->cover->data, i, color);
			}
		}

		gif_close(image);
	}
}

static void loadCover(Surf* surf)
{
	tic_mem* tic = surf->tic;
	
	MenuItem* item = &surf->menu.items[surf->menu.pos];
	
	if(!fsIsInPublicDir(surf->fs))
	{

		s32 size = 0;
		void* data = fsLoadFile(surf->fs, item->name, &size);

		if(data)
		{
			tic_cartridge* cart = (tic_cartridge*)malloc(sizeof(tic_cartridge));

			if(cart)
			{
				if(hasExt(item->name, PROJECT_LUA_EXT))
					surf->console->loadProject(surf->console, item->name, data, size, cart);
				else
					tic->api.load(cart, data, size, true);

				if(cart->cover.size)
					updateMenuItemCover(surf, cart->cover.data, cart->cover.size);

				free(cart);
			}

			free(data);
		}
	}
	else if(item->hash && !item->cover)
	{
		s32 size = 0;

		u8* cover = requestCover(surf, item->hash, &size);

		if(cover)
		{
			updateMenuItemCover(surf, cover, size);
			free(cover);
		}       
	}
}

static void initMenu(Surf* surf)
{
	resetMenu(surf);

	// TODO: calc files count before
	enum{Count = MAX_CARTS, Size = sizeof(MenuItem) * Count};

	AddMenuItem data = 
	{
		.items = malloc(Size),
		.count = 0,
		.surf = surf,
	};

	char dir[FILENAME_MAX];
	fsGetDir(surf->fs, dir);

	if(strcmp(dir, "") != 0)
		addMenuItem("..", NULL, 0, &data, true);

	fsEnumFiles(surf->fs, addMenuItem, &data);

	surf->menu.items = data.items;
	surf->menu.count = data.count;
}

static void onGoBackDir(Surf* surf)
{
	char last[FILENAME_MAX];
	fsGetDir(surf->fs, last);

	fsDirBack(surf->fs);
	initMenu(surf);

	char current[FILENAME_MAX];
	fsGetDir(surf->fs, current);

	for(s32 i = 0; i < surf->menu.count; i++)
	{
		const MenuItem* item = &surf->menu.items[i];

		if(item->dir)
		{
			char path[FILENAME_MAX];

			if(strlen(current))
				sprintf(path, "%s/%s", current, item->name);
			else strcpy(path, item->name);

			if(strcmp(path, last) == 0)
			{
				surf->menu.pos = i;
				break;
			}
		}
	}
}

static void onGoToDir(Surf* surf)
{
	MenuItem* item = &surf->menu.items[surf->menu.pos];

	fsChangeDir(surf->fs, item->name);
	initMenu(surf);
}

static void changeDirectory(Surf* surf, const char* dir)
{
	if(strcmp(dir, "..") == 0)
	{
		char dir[FILENAME_MAX];
		fsGetDir(surf->fs, dir);

		if(strcmp(dir, "") != 0)
		{
			playSystemSfx(2);
			resetMovie(surf, &MenuRightHideState, onGoBackDir);
		}
	}
	else if(fsIsDir(surf->fs, dir))
	{
		playSystemSfx(2);
		resetMovie(surf, &MenuLeftHideState, onGoToDir);
	}
}

static void onPlayCart(Surf* surf)
{
	MenuItem* item = &surf->menu.items[surf->menu.pos];

	if(item->project)
	{
		tic_cartridge* cart = malloc(sizeof(tic_cartridge));

		if(cart)
		{
			s32 size = 0;
			void* data = fsLoadFile(surf->fs, item->name, &size);

			surf->console->loadProject(surf->console, item->name, data, size, cart);

			memcpy(&surf->tic->cart, cart, sizeof(tic_cartridge));

			studioRomLoaded();

			free(cart);
		}
	}
	else
		surf->console->load(surf->console, item->name, item->hash);

	runGameFromSurf();
}

static void loadCart(Surf* surf)
{
	resetMovie(surf, &MenuModeHideState, onPlayCart);
}

static void processAnim(Surf* surf)
{
	enum{Frames = MENU_HEIGHT};

	{
		if(surf->state->time > surf->state->duration)
		{
			if(surf->state->done)
				surf->state->done(surf);

			if(surf->state->next)
				resetMovie(surf, surf->state->next, NULL);
		}

		for(s32 i = 0; i < surf->state->count; i++)
		{
			Anim* anim = surf->state->items[i];

			if(surf->state->time < anim->duration)
			{
				*anim->val = anim->start + (anim->end - anim->start) * surf->state->time / anim->duration;
			}
			else
			{
				*anim->val = anim->end;
			}
		}

		surf->state->time++;

	}

	if(surf->menu.anim)
	{
		if(surf->menu.anim < 0) surf->menu.anim--;
		if(surf->menu.anim > 0) surf->menu.anim++;

		if(surf->menu.anim <= -Frames)
		{
			surf->menu.anim = 0;
			surf->menu.pos--;

			if(surf->menu.pos < 0)
				surf->menu.pos = surf->menu.count-1;
		}

		if(surf->menu.anim >= Frames)
		{
			surf->menu.anim = 0;
			surf->menu.pos++;

			if(surf->menu.pos >= surf->menu.count)
				surf->menu.pos = 0;
		}
	}
}

static void processGamepad(Surf* surf)
{
	tic_mem* tic = surf->tic;

	enum{Frames = MENU_HEIGHT};

	{
		enum{Hold = 20, Period = Frames};

		enum
		{
			Up, Down, Left, Right, A, B, X, Y
		};

		if(tic->api.btnp(tic, Up, Hold, Period))
		{
			surf->menu.anim = -1;

			playSystemSfx(2);
		}

		if(tic->api.btnp(tic, Down, Hold, Period))
		{
			surf->menu.anim = 1;

			playSystemSfx(2);
		}

		if(tic->api.btnp(tic, A, -1, -1))
		{
			MenuItem* item = &surf->menu.items[surf->menu.pos];
			item->dir ? changeDirectory(surf, item->name) : loadCart(surf);
		}

		if(tic->api.btnp(tic, B, -1, -1))
		{
			changeDirectory(surf, "..");
		}

#ifdef CAN_OPEN_URL

		if(tic->api.btnp(tic, Y, -1, -1))
		{
			MenuItem* item = &surf->menu.items[surf->menu.pos];

			if(!item->dir)
			{
				char url[FILENAME_MAX];
				sprintf(url, "https://" TIC_HOST "/play?cart=%i", item->id);
				getSystem()->openSystemPath(url);
			}
		}
#endif

	}

}

static void tick(Surf* surf)
{
	if(!surf->init)
	{
		initMenu(surf);

		resetMovie(surf, &MenuModeShowState, NULL);

		surf->init = true;
	}

	surf->ticks++;

	tic_mem* tic = surf->tic;
	tic->api.clear(tic, TIC_COLOR_BG);

	drawBG(surf);

	if(surf->menu.count > 0)
	{
		processAnim(surf);

		if(surf->state == &MenuModeState)
		{
			processGamepad(surf);
		}

		loadCover(surf);

		drawCover(surf, surf->menu.pos, 0, 0);

		if(surf->menu.items[surf->menu.pos].cover)
			drawMenu(surf, AnimVar.menuX, (TIC80_HEIGHT - MENU_HEIGHT)/2, true);

		drawMenu(surf, AnimVar.menuX, (TIC80_HEIGHT - MENU_HEIGHT)/2, false);

		drawTopToolbar(surf, 0, AnimVar.topBarY - MENU_HEIGHT);
		drawBottomToolbar(surf, 0, TIC80_HEIGHT - AnimVar.bottomBarY);
	}
	else
	{
		static const char Label[] = "You don't have any files...";
		s32 size = tic->api.text(tic, Label, 0, -TIC_FONT_HEIGHT, tic_color_white, false);
		tic->api.text(tic, Label, (TIC80_WIDTH - size) / 2, (TIC80_HEIGHT - TIC_FONT_HEIGHT)/2, tic_color_white, false);
	}
}

static void resume(Surf* surf)
{
	resetMovie(surf, &MenuModeShowState, NULL);
}

void initSurf(Surf* surf, tic_mem* tic, struct Console* console)
{
	*surf = (Surf)
	{
		.tic = tic,
		.console = console,
		.fs = console->fs,
		.tick = tick,
		.ticks = 0,
		.state = &EmptyState,
		.init = false,
		.resume = resume,
		.menu = 
		{
			.pos = 0,
			.anim = 0,
			.items = NULL,
			.count = 0,
		},
	};

	fsMakeDir(surf->fs, TIC_CACHE);
}
