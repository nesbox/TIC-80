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

#include "config.h"
#include "fs.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static void readConfigVideoLength(Config* config, lua_State* lua)
{
	lua_getglobal(lua, "GIF_LENGTH");

	if(lua_isinteger(lua, -1))
		config->data.gifLength = (s32)lua_tointeger(lua, -1);

	lua_pop(lua, 1);
}

static void readConfigVideoScale(Config* config, lua_State* lua)
{
	lua_getglobal(lua, "GIF_SCALE");

	if(lua_isinteger(lua, -1))
		config->data.gifScale = (s32)lua_tointeger(lua, -1);

	lua_pop(lua, 1);
}

static void readConfigCheckNewVersion(Config* config, lua_State* lua)
{
	lua_getglobal(lua, "CHECK_NEW_VERSION");

	if(lua_isboolean(lua, -1))
		config->data.checkNewVersion = lua_toboolean(lua, -1);

	lua_pop(lua, 1);
}

static void readConfigNoSound(Config* config, lua_State* lua)
{
	lua_getglobal(lua, "NO_SOUND");

	if(lua_isboolean(lua, -1))
		config->data.noSound = lua_toboolean(lua, -1);

	lua_pop(lua, 1);
}

static void readCursorTheme(Config* config, lua_State* lua)
{
	lua_getfield(lua, -1, "CURSOR");

	if(lua_type(lua, -1) == LUA_TTABLE)
	{
		{
			lua_getfield(lua, -1, "SPRITE");

			if(lua_isinteger(lua, -1))
			{
				config->data.theme.cursor.sprite = (s32)lua_tointeger(lua, -1);
			}

			lua_pop(lua, 1);			
		}

		{
			lua_getfield(lua, -1, "PIXEL_PERFECT");
			if(lua_isboolean(lua, -1))
				config->data.theme.cursor.pixelPerfect = lua_toboolean(lua, -1);

			lua_pop(lua, 1);			
		}
	}

	lua_pop(lua, 1);
}

static void readCodeTheme(Config* config, lua_State* lua)
{
	lua_getfield(lua, -1, "CODE");

	if(lua_type(lua, -1) == LUA_TTABLE)
	{
		static const char* Fields[] = {"BG", "STRING", "NUMBER", "KEYWORD", "API", "COMMENT", "SIGN", "VAR", "OTHER", "SELECT", "CURSOR"};

		for(s32 i = 0; i < COUNT_OF(Fields); i++)
		{
			lua_getfield(lua, -1, Fields[i]);

			if(lua_isinteger(lua, -1))
				((u8*)&config->data.theme.code)[i] = (u8)lua_tointeger(lua, -1);

			lua_pop(lua, 1);
		}

		{
			lua_getfield(lua, -1, "SHADOW");

			if(lua_isboolean(lua, -1))
				config->data.theme.code.shadow = lua_toboolean(lua, -1);

			lua_pop(lua, 1);
		}
	}

	lua_pop(lua, 1);
}

static void readGamepadTheme(Config* config, lua_State* lua)
{
	lua_getfield(lua, -1, "GAMEPAD");

	if(lua_type(lua, -1) == LUA_TTABLE)
	{
        lua_getfield(lua, -1, "TOUCH");

		if(lua_type(lua, -1) == LUA_TTABLE)
		{
			lua_getfield(lua, -1, "ALPHA");

			if(lua_isinteger(lua, -1))
				config->data.theme.gamepad.touch.alpha = (u8)lua_tointeger(lua, -1);

			lua_pop(lua, 1);
		}

		lua_pop(lua, 1);
	}

	lua_pop(lua, 1);
}

static void readTheme(Config* config, lua_State* lua)
{
	lua_getglobal(lua, "THEME");

	if(lua_type(lua, -1) == LUA_TTABLE)
	{
		readCursorTheme(config, lua);
		readCodeTheme(config, lua);
		readGamepadTheme(config, lua);
	}

	lua_pop(lua, 1);
}

static void readConfig(Config* config)
{
	lua_State* lua = luaL_newstate();

	if(lua)
	{
		if(luaL_loadstring(lua, config->tic->config.code.data) == LUA_OK && lua_pcall(lua, 0, LUA_MULTRET, 0) == LUA_OK)
		{
			readConfigVideoLength(config, lua);
			readConfigVideoScale(config, lua);
			readConfigCheckNewVersion(config, lua);
			readConfigNoSound(config, lua);
			readTheme(config, lua);
		}

		lua_close(lua);
	}
}

static void update(Config* config, const u8* buffer, size_t size)
{
	config->tic->api.load((tic_cartridge*)&config->tic->config, sizeof(tic_bank), buffer, size, true);

	readConfig(config);
	studioConfigChanged();
}

static void setDefault(Config* config)
{
	SDL_memset(&config->data, 0, sizeof(StudioConfig));

	{
		static const u8 DefaultBiosZip[] = 
		{
			#include "../bin/assets/config.tic.dat"
		};

		u8* embedBios = NULL;
		s32 size = unzip((u8**)&embedBios, DefaultBiosZip, sizeof DefaultBiosZip);

		if(embedBios)
		{
			update(config, embedBios, size);

			SDL_free(embedBios);
		}
	}
}

static void saveConfig(Config* config, bool overwrite)
{
	u8* buffer = SDL_malloc(sizeof(tic_cartridge));

	if(buffer)
	{
		s32 size = config->tic->api.save((tic_cartridge*)&config->tic->config, buffer);

		fsSaveRootFile(config->fs, CONFIG_TIC_PATH, buffer, size, overwrite);

		SDL_free(buffer);
	}
}

static void reset(Config* config)
{
	setDefault(config);
	saveConfig(config, true);
}

static void save(Config* config)
{
	SDL_memcpy(&config->tic->config, &config->tic->cart, sizeof(tic_cartridge));
	readConfig(config);
	saveConfig(config, true);

	studioConfigChanged();
}

void initConfig(Config* config, tic_mem* tic, FileSystem* fs)
{
	*config = (Config)
	{
		.tic = tic,
		.save = save,
		.reset = reset,
		.fs = fs,
	};

	setDefault(config);

	s32 size = 0;
	u8* data = (u8*)fsLoadRootFile(fs, CONFIG_TIC_PATH, &size);

	if(data)
	{
		update(config, data, size);

		SDL_free(data);
	}
	else saveConfig(config, false);

	tic->api.reset(tic);
}
