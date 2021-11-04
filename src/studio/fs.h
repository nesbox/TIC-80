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

#include <tic80_types.h>
#include <string.h>

typedef bool(*fs_list_callback)(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir);
typedef void(*fs_done_callback)(void* data);
typedef void(*fs_isdir_callback)(bool dir, void* data);
typedef void(*fs_load_callback)(const u8* buffer, s32 size, void* data);

typedef struct tic_fs tic_fs;
struct tic_net;

tic_fs*     tic_fs_create   (const char* path, struct tic_net* net);
const char* tic_fs_path     (tic_fs* fs, const char* name);
const char* tic_fs_pathroot (tic_fs* fs, const char* name);

void    tic_fs_enum         (tic_fs* fs, fs_list_callback onItem, fs_done_callback onDone, void* data);
void    tic_fs_isdir_async  (tic_fs* fs, const char* name, fs_isdir_callback callback, void* data);
void    tic_fs_hashload     (tic_fs* fs, const char* name, const char* hash, fs_load_callback callback, void* data);
bool    tic_fs_delfile      (tic_fs* fs, const char* name);
bool    tic_fs_deldir       (tic_fs* fs, const char* name);
bool    tic_fs_save         (tic_fs* fs, const char* name, const void* data, s32 size, bool overwrite);
bool    tic_fs_saveroot     (tic_fs* fs, const char* name, const void* data, s32 size, bool overwrite);
void*   tic_fs_load         (tic_fs* fs, const char* name, s32* size);
void*   tic_fs_loadroot     (tic_fs* fs, const char* name, s32* size);
bool    tic_fs_makedir      (tic_fs* fs, const char* name);
bool    tic_fs_exists       (tic_fs* fs, const char* name);
void    tic_fs_openfolder   (tic_fs* fs);
bool    tic_fs_isdir        (tic_fs* fs, const char* dir);
bool    tic_fs_ispubdir     (tic_fs* fs);
void    tic_fs_changedir    (tic_fs* fs, const char* dir);
void    tic_fs_dir          (tic_fs* fs, char* out);
void    tic_fs_dirback      (tic_fs* fs);
void    tic_fs_homedir      (tic_fs* fs);

u64     fs_date     (const char* name);
bool    fs_exists   (const char* name);
void*   fs_read     (const char* path, s32* size);
bool    fs_write    (const char* path, const void* data, s32 size);
