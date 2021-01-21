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

typedef bool(*ListCallback)(const char* name, const char* info, s32 id, void* data, bool dir);
typedef void(*DoneCallback)(void* data);
typedef void(*IsDirCallback)(bool dir, void* data);
typedef void(*LoadCallback)(const u8* buffer, s32 size, void* data);

typedef struct FileSystem FileSystem;
struct Net;

FileSystem* createFileSystem(const char* path, struct Net* net);

void fsEnumFilesAsync(FileSystem* fs, ListCallback onItem, DoneCallback onDone, void* data);
void fsIsDirAsync(FileSystem* fs, const char* name, IsDirCallback callback, void* data);
void fsLoadFileByHashAsync(FileSystem* fs, const char* hash, LoadCallback callback, void* data);

bool fsDeleteFile(FileSystem* fs, const char* name);
bool fsDeleteDir(FileSystem* fs, const char* name);
bool fsSaveFile(FileSystem* fs, const char* name, const void* data, s32 size, bool overwrite);
bool fsSaveRootFile(FileSystem* fs, const char* name, const void* data, s32 size, bool overwrite);
void* fsLoadFile(FileSystem* fs, const char* name, s32* size);
void* fsLoadRootFile(FileSystem* fs, const char* name, s32* size);
const char* fsGetFilePath(FileSystem* fs, const char* name);
const char* fsGetRootFilePath(FileSystem* fs, const char* name);
void fsMakeDir(FileSystem* fs, const char* name);
bool fsExistsFile(FileSystem* fs, const char* name);

u64 fsMDate(const char* name);
void fsBasename(const char *path, char* out);
void fsFilename(const char *path, char* out);
bool fsExists(const char* name);
void* fsReadFile(const char* path, s32* size);
bool fsWriteFile(const char* path, const void* data, s32 size);
void fsOpenWorkingFolder(FileSystem* fs);
bool fsIsDir(FileSystem* fs, const char* dir);
bool fsIsInPublicDir(FileSystem* fs);
void fsChangeDir(FileSystem* fs, const char* dir);
void fsGetDir(FileSystem* fs, char* out);
void fsDirBack(FileSystem* fs);
void fsHomeDir(FileSystem* fs);