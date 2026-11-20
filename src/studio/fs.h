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

typedef bool (*fs_list_callback)(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir);
typedef void (*fs_done_callback)(void* data);
typedef void (*fs_isdir_callback)(bool dir, void* data);

typedef struct tic_fs tic_fs;
struct tic_net;

// The unified interface: path-based calls for whichever mount serves them.
// A local mount answers inline; a remote one later, through the same callbacks.
typedef enum
{
    FS_OK,
    FS_NOT_FOUND,
    FS_DENIED,      // the mount refuses the operation
    FS_SESSION,     // the session ended
    FS_OFFLINE,     // no transport
    FS_QUOTA,       // a storage limit refused the write
    FS_UNSUPPORTED, // the mount has no such operation
    FS_IO,          // an I/O error
} fs_code;

typedef struct
{
    fs_code code;
    const char* message; // human text, valid during the completion
} fs_result;

typedef struct
{
    const char* name;  // the file's name in this directory
    const char* title; // display title, NULL when the name is the title
    bool dir;
    s64 size;  // -1 when unknown
    s64 date;  // 0 when unknown
    s32 token; // the mount's own handle for the item, opaque to the caller; 0 when none
} fs_item;

typedef struct
{
    bool has_usage;
    s32 count;
    s32 max_count;
    s64 bytes;
    s64 max_bytes;
} fs_usage;

// A byte count the member reads: bytes below 1 KB, then KB, MB or GB.
const char* tic_fs_size_text(s64 bytes, char* out, s32 size);

typedef bool (*fs_list_item_cb)(const fs_item* item, void* data);
typedef void (*fs_list_done_cb)(const fs_usage* usage, fs_result result, void* data);
typedef void (*fs_load_done_cb)(const char* name, const u8* bytes, s32 size, fs_result result, void* data);
typedef void (*fs_save_done_cb)(const char* name, fs_result result, void* data);
typedef void (*fs_done_cb)(fs_result result, void* data);
typedef void (*fs_stat_done_cb)(const fs_item* item, fs_result result, void* data);
typedef void (*fs_cover_done_cb)(const u8* bytes, s32 size, fs_result result, void* data);

void tic_fs_list(tic_fs* fs, const char* path, fs_list_item_cb onItem, fs_list_done_cb onDone, void* data);
void tic_fs_load(tic_fs* fs, const char* path, fs_load_done_cb onDone, void* data);
void tic_fs_save(tic_fs* fs, const char* path, const void* bytes, s32 size, fs_save_done_cb onDone, void* data);
void tic_fs_delete(tic_fs* fs, const char* path, fs_done_cb onDone, void* data);
void tic_fs_stat(tic_fs* fs, const char* path, fs_stat_done_cb onDone, void* data);
void tic_fs_mkdir(tic_fs* fs, const char* path, fs_done_cb onDone, void* data);
void tic_fs_openfolder(tic_fs* fs, const char* path, fs_done_cb onDone, void* data);
void tic_fs_cover(tic_fs* fs, const char* path, fs_cover_done_cb onDone, void* data);

tic_fs* tic_fs_create(const char* path, struct tic_net* net);

// The shelf is the account's remote mount, always listed at the root.
#define TIC_SHELF_DIR "shelf"

// The remote mounts' inputs: the account, injected so a test can drive a
// remote mount with no studio, no socket and no account. A NULL signed_in is
// signed out.
typedef struct
{
    bool (*signed_in)(void* data);
    const char* (*token)(void* data);
    // A 401 from the shelf carries the token that request used: the account
    // clears its token only when it is still the current one, so an answer to
    // an old request cannot sign out a newer session.
    void (*unauthorized)(void* data, const char* token);
    void* data;
} tic_fs_remote;

void tic_fs_set_remote(tic_fs* fs, const tic_fs_remote* remote);
bool tic_fs_signedin(tic_fs* fs);

// App storage: config, options, cache, persistent data and the token; local and sync.
const char* tic_fs_path(tic_fs* fs, const char* name);
const char* tic_fs_pathroot(tic_fs* fs, const char* name);
void* tic_fs_loadroot(tic_fs* fs, const char* name, s32* size);
bool tic_fs_saveroot(tic_fs* fs, const char* name, const void* data, s32 size, bool overwrite);

// Working directory state and the legacy per-mount calls, until their callers move.
void tic_fs_enum(tic_fs* fs, fs_list_callback onItem, fs_done_callback onDone, void* data);
void tic_fs_isdir_async(tic_fs* fs, const char* name, fs_isdir_callback callback, void* data);
bool tic_fs_delfile(tic_fs* fs, const char* name);
bool tic_fs_deldir(tic_fs* fs, const char* name);
bool tic_fs_makedir(tic_fs* fs, const char* name);
bool tic_fs_exists(tic_fs* fs, const char* name);
bool tic_fs_isdir(tic_fs* fs, const char* dir);
bool tic_fs_isroot(tic_fs* fs);
void tic_fs_changedir(tic_fs* fs, const char* dir);
void tic_fs_dir(tic_fs* fs, char* out);
void tic_fs_dirback(tic_fs* fs);
void tic_fs_homedir(tic_fs* fs);

u64 fs_date(const char* name);
bool fs_exists(const char* name);
bool fs_isdir(const char* path);
void* fs_read(const char* path, s32* size);
bool fs_write(const char* path, const void* data, s32 size);
void fs_enum(const char* path, fs_list_callback callback, void* data);

const char* fs_apppath();
const char* fs_appfolder();
