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

#include "fs.h"

// The mount providers behind the interface; no caller sees this. The contract test installs a fake one.
typedef struct fs_mount_ops
{
    void (*list)(tic_fs* fs, const char* path, fs_list_item_cb onItem, fs_list_done_cb onDone, void* data);
    void (*load)(tic_fs* fs, const char* path, fs_load_done_cb onDone, void* data);
    void (*save)(tic_fs* fs, const char* path, const void* bytes, s32 size, fs_save_done_cb onDone, void* data);
    void (*delete)(tic_fs* fs, const char* path, fs_done_cb onDone, void* data);
    void (*stat)(tic_fs* fs, const char* path, fs_stat_done_cb onDone, void* data);
    void (*mkdir)(tic_fs* fs, const char* path, fs_done_cb onDone, void* data);
    void (*openfolder)(tic_fs* fs, const char* path, fs_done_cb onDone, void* data);
    void (*cover)(tic_fs* fs, const char* path, fs_cover_done_cb onDone, void* data);
} fs_mount_ops;

// The provider seam the contract test installs its fake remote mount through.
void tic_fs_set_mount(tic_fs* fs, const fs_mount_ops* mount);
