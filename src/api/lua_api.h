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

#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <ctype.h>

s32 luaopen_lpeg(lua_State *lua);

extern void initLuaAPI(tic_core* core);
extern void callLuaTick(tic_mem* tic);
extern void callLuaBoot(tic_mem* tic);
extern void callLuaScanlineName(tic_mem* tic, s32 row, void* data, const char* name);
extern void callLuaScanline(tic_mem* tic, s32 row, void* data);
extern void callLuaBorder(tic_mem* tic, s32 row, void* data);
extern void callLuaOverline(tic_mem* tic, void* data);
extern void callLuaMenu(tic_mem* tic, s32 index, void* data);
extern void closeLua(tic_mem* tic);
extern void callLuaTick(tic_mem* tic);
extern void lua_open_builtins(lua_State *lua);
