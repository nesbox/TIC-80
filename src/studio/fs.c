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

#include "studio/paths.h"
#include "studio/system.h"
#include "fs.h"
#include "fs_internal.h"
#include "net.h"
#include "ext/json.h"
#include "cart.h"
#include "tools.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(TIC80_PRO)
#include "studio/project.h"
#endif

#if defined(BAREMETALPI)
#ifdef EN_DEBUG
#define dbg(...) printf(__VA_ARGS__)
#else
#define dbg(...)
#endif
#endif

#if defined(BAREMETALPI)
#include "../../circle-stdlib/libs/circle/addon/fatfs/ff.h"
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined(__TIC_WINDOWS__)
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(__TIC_WINDOWS__)
#define SLASH_SYMBOL ('\\')
#else
#define SLASH_SYMBOL ('/')
#endif

#if defined(__TIC_MACOSX__)
#include <mach-o/dyld.h>
#endif

static const char* PublicDir = TIC_HOST;

// One entry of the site's last listing: the name, display title and fetch hash.
typedef struct
{
    char name[TICNAME_MAX];
    char title[TICNAME_MAX];
    char hash[TICNAME_MAX];
    bool dir;
} SiteEntry;

struct tic_fs
{
    char dir[TICNAME_MAX];
    char work[TICNAME_MAX];
    tic_net* net;

    const fs_mount_ops* mount; // the contract test's fake provider

    struct
    {
        bool valid;
        char dir[TICNAME_MAX];
        SiteEntry* items;
        s32 count;
    } site;
};

#if defined(__EMSCRIPTEN__)
void syncfs()
{
    // clang-format off
    EM_ASM({Module.syncFSRequests++;});
    // clang-format on
}
#endif

const char* tic_fs_pathroot(tic_fs* fs, const char* name)
{
    static char path[TICNAME_MAX * 2];

    snprintf(path, sizeof path, "%s%s", fs->dir, name);

#if defined(__TIC_WINDOWS__)
    char* ptr = path;
    while(*ptr)
    {
        if(*ptr == '/') *ptr = SLASH_SYMBOL;
        ptr++;
    }
#endif

    return path;
}

const char* tic_fs_path(tic_fs* fs, const char* name)
{
    static char path[TICNAME_MAX + 1];

    if(*name == '/')
        snprintf(path, sizeof path, "%s", name + 1);
    else if(strlen(fs->work))
        snprintf(path, sizeof path, "%s/%s", fs->work, name);
    else
        snprintf(path, sizeof path, "%s", name);

    return tic_fs_pathroot(fs, path);
}

static bool isRoot(tic_fs* fs)
{
    return fs->work[0] == '\0';
}

bool tic_fs_isroot(tic_fs* fs)
{
    return isRoot(fs);
}

static bool isPublicRoot(tic_fs* fs)
{
    return strcmp(fs->work, PublicDir) == 0;
}

static bool isPublic(tic_fs* fs)
{
    return memcmp(fs->work, PublicDir, STRLEN(PublicDir)) == 0;
}

#if defined(__TIC_WINDOWS__)

typedef wchar_t FsString;

#define __S(x) L##x
#define _S(x)  __S(x)

static const FsString* utf8ToString(const char* str)
{
    FsString* wstr = malloc(TICNAME_MAX * sizeof(FsString));

    MultiByteToWideChar(CP_UTF8, 0, str, TICNAME_MAX, wstr, TICNAME_MAX);

    return wstr;
}

static const char* stringToUtf8(const FsString* wstr)
{
    char* str = malloc(TICNAME_MAX * sizeof(char));

    WideCharToMultiByte(CP_UTF8, 0, wstr, TICNAME_MAX, str, TICNAME_MAX, 0, 0);

    return str;
}

#if defined(__TIC_WIN7__)

time_t FileTimeToTimeT(FILETIME* ft)
{
    ULARGE_INTEGER ull;
    ull.LowPart = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;
    return (time_t)((ull.QuadPart / 10000000ULL) - 11644473600ULL);
}

// There is a bug in the toolchain when compiling with v141_xp
// This shim is a workaround for that, using GetFileAttributesEx
// see https://stackoverflow.com/questions/32452777/visual-c-2015-express-stat-not-working-on-windows-xp
static int _wstat_win32_shim(const wchar_t* path, struct _stat* buffer)
{
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if(!GetFileAttributesExW(path, GetFileExInfoStandard, &fileInfo))
    {
        return -1;
    }

    memset(buffer, 0, sizeof(struct _stat));

    buffer->st_mode = _S_IREAD;
    if(!(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_READONLY))
    {
        buffer->st_mode |= _S_IWRITE;
    }
    if(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        buffer->st_mode |= _S_IFDIR;
    }
    else
    {
        buffer->st_mode |= _S_IFREG;
    }

    // Set the file size
    buffer->st_size = ((_off_t)fileInfo.nFileSizeHigh << 32) | fileInfo.nFileSizeLow;

    // Set the file times
    buffer->st_mtime = FileTimeToTimeT(&fileInfo.ftLastWriteTime);
    buffer->st_atime = FileTimeToTimeT(&fileInfo.ftLastAccessTime);
    buffer->st_ctime = FileTimeToTimeT(&fileInfo.ftCreationTime);
    return 0;
}

#endif

#define freeString(S) free((void*)S)

#define TIC_DIR         _WDIR
#define tic_dirent      _wdirent
#define tic_stat_struct _stat

#define tic_opendir  _wopendir
#define tic_readdir  _wreaddir
#define tic_closedir _wclosedir
#define tic_rmdir    _wrmdir

// use the shim (see above) if we're targeting Windows XP
#if defined(__TIC_WIN7__)
#define tic_stat _wstat_win32_shim
#else
#define tic_stat _wstat
#endif
#define tic_remove      _wremove
#define tic_fopen       _wfopen
#define tic_mkdir(name) _wmkdir(name)
#define tic_strncpy     wcsncpy
#define tic_strncat     wcsncat
#define tic_strlen      wcslen

#else

typedef char FsString;

#define _S(x) (x)

#define utf8ToString(S) (S)
#define stringToUtf8(S) (S)

#define freeString(S)

#define TIC_DIR         DIR
#define tic_dirent      dirent
#define tic_stat_struct stat

#define tic_opendir     opendir
#define tic_readdir     readdir
#define tic_closedir    closedir
#define tic_rmdir       rmdir
#define tic_stat        stat
#define tic_remove      remove
#define tic_fopen       fopen
#define tic_mkdir(name) mkdir(name, 0777)
#define tic_strncpy     strncpy
#define tic_strncat     strncat
#define tic_strlen      strlen

#endif

static const char* baseName(const char* path)
{
    const char* ptr = path + strlen(path);

    while(ptr > path && ptr[-1] != '/' && ptr[-1] != '\\')
        --ptr;

    return ptr;
}

static fs_result localError(fs_code code)
{
    switch(code)
    {
    case FS_NOT_FOUND: return (fs_result){code, "not found"};
    case FS_DENIED: return (fs_result){code, "access denied"};
    case FS_QUOTA: return (fs_result){code, "no space left"};
    case FS_UNSUPPORTED: return (fs_result){code, "not supported"};
    default: return (fs_result){code, "I/O error"};
    }
}

#if !defined(BAREMETALPI)
static fs_code errorCode(int error)
{
    switch(error)
    {
    case ENOENT: return FS_NOT_FOUND;
    case EACCES:
    case EPERM: return FS_DENIED;
    case ENOSPC: return FS_QUOTA;
    default: return FS_IO;
    }
}
#endif

// Every unified path is the mount's own: a host path never reaches outside it.
static const char* localPath(tic_fs* fs, const char* path)
{
    return tic_fs_path(fs, path);
}

static void* localRead(const char* path, s32* size, fs_result* result)
{
#if defined(BAREMETALPI)
    FILINFO info;
    FRESULT res = f_stat(path, &info);

    if(res != FR_OK)
    {
        *result = localError(FS_NOT_FOUND);
        return NULL;
    }

    FIL file;
    res = f_open(&file, path, FA_READ | FA_OPEN_EXISTING);

    if(res != FR_OK)
    {
        *result = localError(FS_IO);
        return NULL;
    }

    u8* buffer = malloc(info.fsize ? info.fsize : 1);
    UINT read = 0;

    if(buffer)
        res = f_read(&file, buffer, info.fsize, &read);

    f_close(&file);

    if(!buffer || res != FR_OK || read != info.fsize)
    {
        free(buffer);
        *result = localError(FS_IO);
        return NULL;
    }

    *size = (s32)info.fsize;
    *result = (fs_result){FS_OK, NULL};
    return buffer;
#else
    errno = 0;

    const FsString* pathString = utf8ToString(path);
    FILE* file = tic_fopen(pathString, _S("rb"));
    freeString(pathString);

    if(!file)
    {
        *result = localError(errorCode(errno));
        return NULL;
    }

    void* buffer = NULL;
    long end = -1;

    if(fseek(file, 0, SEEK_END) == 0)
        end = ftell(file);

    if(end >= 0 && fseek(file, 0, SEEK_SET) == 0)
    {
        buffer = malloc(end ? (size_t)end : 1);

        if(buffer && end && fread(buffer, 1, (size_t)end, file) != (size_t)end)
        {
            free(buffer);
            buffer = NULL;
        }
    }

    fclose(file);

    if(!buffer)
    {
        *result = localError(errorCode(errno));
        return NULL;
    }

    *size = (s32)end;
    *result = (fs_result){FS_OK, NULL};
    return buffer;
#endif
}

static fs_result localWrite(const char* path, const void* bytes, s32 size)
{
#if defined(BAREMETALPI)
    FIL file;
    FRESULT res = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);

    if(res != FR_OK)
        return localError(FS_IO);

    UINT written = 0;
    res = f_write(&file, bytes, size, &written);
    f_close(&file);

    return res == FR_OK && written == (UINT)size ? (fs_result){FS_OK, NULL} : localError(FS_IO);
#else
    errno = 0;

    const FsString* pathString = utf8ToString(path);
    FILE* file = tic_fopen(pathString, _S("wb"));
    freeString(pathString);

    if(!file)
        return localError(errorCode(errno));

    if(size > 0 && fwrite(bytes, 1, (size_t)size, file) != (size_t)size)
    {
        int error = errno;
        fclose(file);
        return localError(errorCode(error));
    }

    if(fclose(file) != 0)
        return localError(errorCode(errno));

#if defined(__EMSCRIPTEN__)
    syncfs();
#endif

    return (fs_result){FS_OK, NULL};
#endif
}

typedef struct
{
    fs_list_item_cb onItem;
    void* data;
} LocalListData;

static bool localListItem(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    (void)title;
    (void)hash;
    (void)id;

    LocalListData* list = data;
    fs_item item = {name, NULL, dir, -1, 0};

    return list->onItem(&item, list->data);
}

static void localList(tic_fs* fs, const char* path, fs_list_item_cb onItem, fs_list_done_cb onDone, void* data)
{
    // A copy: a callback starting another operation must not move the path being read.
    char resolved[TICNAME_MAX];
    snprintf(resolved, sizeof resolved, "%s", localPath(fs, path));
    bool root = isRoot(fs) && (!*path || (path[0] == '/' && !path[1]));

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    if(root)
    {
        fs_item item = {PublicDir, NULL, true, -1, 0};

        if(!onItem(&item, data))
        {
            onDone(NULL, (fs_result){FS_OK, NULL}, data);
            return;
        }
    }
#else
    (void)root;
#endif

    if(!fs_isdir(resolved))
    {
        onDone(NULL, localError(FS_NOT_FOUND), data);
        return;
    }

    LocalListData list = {onItem, data};
    fs_enum(resolved, localListItem, &list);

    onDone(NULL, (fs_result){FS_OK, NULL}, data);
}

static void localLoad(tic_fs* fs, const char* path, fs_load_done_cb onDone, void* data)
{
    char resolved[TICNAME_MAX];
    snprintf(resolved, sizeof resolved, "%s", localPath(fs, path));

    s32 size = 0;
    fs_result result;
    void* bytes = localRead(resolved, &size, &result);

    if(!bytes)
    {
        onDone(NULL, NULL, 0, result, data);
        return;
    }

    // The name as asked: a nested cart keeps its folder, so a save follows it.
    char name[TICNAME_MAX];
    snprintf(name, sizeof name, "%s", path);

    onDone(name, bytes, size, (fs_result){FS_OK, NULL}, data);
    free(bytes);
}

static void localSave(tic_fs* fs, const char* path, const void* bytes, s32 size, fs_save_done_cb onDone, void* data)
{
    char resolved[TICNAME_MAX];
    snprintf(resolved, sizeof resolved, "%s", localPath(fs, path));

    char name[TICNAME_MAX];
    snprintf(name, sizeof name, "%s", baseName(path));

    onDone(name, localWrite(resolved, bytes, size), data);
}

static void localDelete(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    char resolved[TICNAME_MAX];
    snprintf(resolved, sizeof resolved, "%s", localPath(fs, path));

    if(!fs_exists(resolved))
    {
        onDone(localError(FS_NOT_FOUND), data);
        return;
    }

    fs_result result = (fs_result){FS_OK, NULL};

#if defined(BAREMETALPI)
    if(f_unlink(resolved) != FR_OK)
        result = localError(FS_IO);
#else
    errno = 0;

#if defined(__TIC_WINDOWS__)
    const FsString* pathString = utf8ToString(resolved);
    int ret = fs_isdir(resolved) ? tic_rmdir(pathString) : tic_remove(pathString);
    freeString(pathString);
#else
    int ret = fs_isdir(resolved) ? tic_rmdir(resolved) : tic_remove(resolved);
#endif

    if(ret != 0)
        result = localError(errorCode(errno));

#if defined(__EMSCRIPTEN__)
    syncfs();
#endif
#endif

    onDone(result, data);
}

static void localMkdir(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    char resolved[TICNAME_MAX];
    snprintf(resolved, sizeof resolved, "%s", localPath(fs, path));

    fs_result result = (fs_result){FS_OK, NULL};

#if defined(BAREMETALPI)
    if(f_mkdir(resolved) != FR_OK)
        result = localError(FS_IO);
#else
    errno = 0;

    const FsString* pathString = utf8ToString(resolved);
    int ret = tic_mkdir(pathString);
    int error = errno;
    freeString(pathString);

    if(ret != 0)
        result = error == EEXIST ? (fs_result){FS_IO, "the folder exists"} : localError(errorCode(error));

#if defined(__EMSCRIPTEN__)
    syncfs();
#endif
#endif

    onDone(result, data);
}

static void localOpen(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    tic_sys_open_path(localPath(fs, path));
    onDone((fs_result){FS_OK, NULL}, data);
}

static void localStat(tic_fs* fs, const char* path, fs_stat_done_cb onDone, void* data)
{
    char resolved[TICNAME_MAX];
    snprintf(resolved, sizeof resolved, "%s", localPath(fs, path));

    if(!*path || (path[0] == '/' && !path[1]))
    {
        fs_item item = {"", NULL, true, -1, 0};
        onDone(&item, (fs_result){FS_OK, NULL}, data);
        return;
    }

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    if(isRoot(fs) && strcmp(path, PublicDir) == 0)
    {
        fs_item item = {PublicDir, NULL, true, -1, 0};
        onDone(&item, (fs_result){FS_OK, NULL}, data);
        return;
    }
#endif

#if defined(BAREMETALPI)
    FILINFO info;
    FRESULT res = f_stat(resolved, &info);

    if(res != FR_OK)
    {
        onDone(NULL, localError(FS_NOT_FOUND), data);
        return;
    }

    fs_item item = {baseName(path), NULL, (info.fattrib & AM_DIR) != 0, -1, 0};
    onDone(&item, (fs_result){FS_OK, NULL}, data);
#else
    errno = 0;

    struct tic_stat_struct info;
    const FsString* pathString = utf8ToString(resolved);
    int ret = tic_stat(pathString, &info);
    int error = errno;
    freeString(pathString);

    if(ret != 0)
    {
        onDone(NULL, localError(errorCode(error)), data);
        return;
    }

    bool dir = S_ISDIR(info.st_mode);
    fs_item item = {baseName(path), NULL, dir, dir ? -1 : (s64)info.st_size, (s64)info.st_mtime};
    onDone(&item, (fs_result){FS_OK, NULL}, data);
#endif
}

// A cover is a GIF: the site serves one, the local mount renders the cart's screen.
typedef struct
{
    u8* data;
    s32 size;
    s32 capacity;
} ByteBuffer;

static bool bufferByte(ByteBuffer* buffer, u8 byte)
{
    if(buffer->size >= buffer->capacity)
    {
        s32 capacity = buffer->capacity ? buffer->capacity * 2 : 1024;
        u8* data = realloc(buffer->data, capacity);

        if(!data)
            return false;

        buffer->data = data;
        buffer->capacity = capacity;
    }

    buffer->data[buffer->size++] = byte;
    return true;
}

static bool bufferWord(ByteBuffer* buffer, s32 value)
{
    return bufferByte(buffer, (u8)(value & 0xff)) && bufferByte(buffer, (u8)((value >> 8) & 0xff));
}

typedef struct
{
    ByteBuffer* bytes;
    u32 bits;
    s32 count;
    bool ok;
} BitWriter;

static void bufferBits(BitWriter* writer, s32 code, s32 size)
{
    writer->bits |= (u32)code << writer->count;
    writer->count += size;

    while(writer->count >= 8)
    {
        if(!bufferByte(writer->bytes, (u8)(writer->bits & 0xff)))
            writer->ok = false;

        writer->bits >>= 8;
        writer->count -= 8;
    }
}

static u8* gifCover(const u8* screen, const tic_rgb* palette, s32* size)
{
    enum
    {
        MinCodeSize = 4,
        ClearCode = 1 << MinCodeSize,
        EndCode = ClearCode + 1,
        Pixels = TIC80_WIDTH * TIC80_HEIGHT,
    };

    ByteBuffer gif = {0};
    ByteBuffer lzw = {0};
    BitWriter writer = {&lzw, 0, 0, true};

    {
        static const char Header[] = "GIF89a";

        for(s32 i = 0; Header[i]; i++)
            if(!bufferByte(&gif, (u8)Header[i]))
                goto fail;
    }

    if(!bufferWord(&gif, TIC80_WIDTH) || !bufferWord(&gif, TIC80_HEIGHT))
        goto fail;

    // A global color table of 16 entries, 8 bits per primary.
    if(!bufferByte(&gif, 0xf3) || !bufferByte(&gif, 0) || !bufferByte(&gif, 0))
        goto fail;

    for(s32 i = 0; i < TIC_PALETTE_SIZE; i++)
        if(!bufferByte(&gif, palette[i].r) || !bufferByte(&gif, palette[i].g) || !bufferByte(&gif, palette[i].b))
            goto fail;

    // One image, no local table, not interlaced.
    if(!bufferByte(&gif, 0x2c) || !bufferWord(&gif, 0) || !bufferWord(&gif, 0) ||
       !bufferWord(&gif, TIC80_WIDTH) || !bufferWord(&gif, TIC80_HEIGHT) || !bufferByte(&gif, 0))
        goto fail;

    if(!bufferByte(&gif, MinCodeSize))
        goto fail;

    {
        s32 codeSize = MinCodeSize + 1;
        s32 next = ClearCode + 2;
        bool first = true;

        bufferBits(&writer, ClearCode, codeSize);

        for(s32 i = 0; i < Pixels; i++)
        {
            if(next >= 4096)
            {
                bufferBits(&writer, ClearCode, codeSize);
                codeSize = MinCodeSize + 1;
                next = ClearCode + 2;
                first = true;
            }

            bufferBits(&writer, tic_tool_peek4(screen, i), codeSize);

            if(first)
                first = false;
            else
                next++;

            if(next == (1 << codeSize) && codeSize < 12)
                codeSize++;
        }

        bufferBits(&writer, EndCode, codeSize);

        if(writer.count > 0 && !bufferByte(&lzw, (u8)(writer.bits & 0xff)))
            writer.ok = false;
    }

    if(!writer.ok)
        goto fail;

    for(s32 pos = 0; pos < lzw.size;)
    {
        s32 chunk = MIN(lzw.size - pos, 255);

        if(!bufferByte(&gif, (u8)chunk))
            goto fail;

        for(s32 i = 0; i < chunk; i++)
            if(!bufferByte(&gif, lzw.data[pos + i]))
                goto fail;

        pos += chunk;
    }

    if(!bufferByte(&gif, 0) || !bufferByte(&gif, 0x3b))
        goto fail;

    free(lzw.data);
    *size = gif.size;
    return gif.data;

fail:
    free(lzw.data);
    free(gif.data);
    return NULL;
}

static void localCover(tic_fs* fs, const char* path, fs_cover_done_cb onDone, void* data)
{
    char resolved[TICNAME_MAX];
    snprintf(resolved, sizeof resolved, "%s", localPath(fs, path));

    s32 size = 0;
    fs_result result;
    void* bytes = localRead(resolved, &size, &result);

    if(!bytes)
    {
        onDone(NULL, 0, result, data);
        return;
    }

    tic_cartridge* cart = calloc(1, sizeof(tic_cartridge));
    bool loaded = false;

    if(cart)
    {
        if(tic_tool_has_ext(path, PNG_EXT))
        {
            tic_cartridge* png = loadPngCart((png_buffer){bytes, size});

            if(png)
            {
                memcpy(cart, png, sizeof(tic_cartridge));
                free(png);
                loaded = true;
            }
        }
#if defined(TIC80_PRO)
        else if(tic_tool_is_project(path))
            loaded = tic_project_load(path, bytes, size, cart);
#endif
        else
        {
            tic_cart_load(cart, bytes, size);
            loaded = true;
        }
    }

    free(bytes);

    if(!loaded || EMPTY(cart->bank0.screen.data) || EMPTY(cart->bank0.palette.vbank0.data))
    {
        free(cart);
        onDone(NULL, 0, localError(FS_NOT_FOUND), data);
        return;
    }

    s32 gifSize = 0;
    u8* gif = gifCover(cart->bank0.screen.data, cart->bank0.palette.vbank0.colors, &gifSize);
    free(cart);

    if(!gif)
    {
        onDone(NULL, 0, localError(FS_IO), data);
        return;
    }

    onDone(gif, gifSize, (fs_result){FS_OK, NULL}, data);
    free(gif);
}

static const fs_mount_ops LocalOps = {
    .list = localList,
    .load = localLoad,
    .save = localSave,
    .delete = localDelete,
    .stat = localStat,
    .mkdir = localMkdir,
    .openfolder = localOpen,
    .cover = localCover,
};

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
static void siteCacheClear(tic_fs* fs)
{
    free(fs->site.items);
    fs->site.items = NULL;
    fs->site.count = 0;
    fs->site.valid = false;
    fs->site.dir[0] = '\0';
}

static bool siteAddEntry(SiteEntry** items, s32* count, const SiteEntry* entry)
{
    SiteEntry* list = realloc(*items, sizeof(SiteEntry) * (*count + 1));

    if(!list)
        return false;

    *items = list;
    list[*count] = *entry;
    (*count)++;

    return true;
}

static bool siteParseListing(tic_fs* fs, const char* dir, const char* json, s32 size, SiteEntry** items, s32* count)
{
    *items = NULL;
    *count = 0;

    if(!json_parse((char*)json, size))
        return false;

    SiteEntry* list = NULL;
    s32 listCount = 0;
    bool ok = true;

    {
        s32 folders = json_array("folders", 0);

        for(s32 i = 0, n = json_array_size(folders); ok && i < n; i++)
        {
            char name[TICNAME_MAX];

            if(json_string("name", json_array_item(folders, i), name, sizeof name))
            {
                SiteEntry entry = {0};
                snprintf(entry.name, sizeof entry.name, "%s", name);
                entry.dir = true;
                ok = siteAddEntry(&list, &listCount, &entry);
            }
        }
    }

    {
        s32 files = json_array("files", 0);

        for(s32 i = 0, n = json_array_size(files); ok && i < n; i++)
        {
            char name[TICNAME_MAX];
            char hash[TICNAME_MAX];
            char filename[TICNAME_MAX];
            s32 item = json_array_item(files, i);

            if(json_string("name", item, name, sizeof name) && json_string("hash", item, hash, sizeof hash) &&
               json_string("filename", item, filename, sizeof filename))
            {
                SiteEntry entry = {0};
                snprintf(entry.name, sizeof entry.name, "%s", filename);
                snprintf(entry.title, sizeof entry.title, "%s", name);
                snprintf(entry.hash, sizeof entry.hash, "%s", hash);
                ok = siteAddEntry(&list, &listCount, &entry);
            }
        }
    }

    if(!ok)
    {
        free(list);
        return false;
    }

    siteCacheClear(fs);
    fs->site.items = malloc(sizeof(SiteEntry) * (listCount ? listCount : 1));

    if(fs->site.items)
    {
        if(listCount)
            memcpy(fs->site.items, list, sizeof(SiteEntry) * listCount);

        fs->site.count = listCount;
        snprintf(fs->site.dir, sizeof fs->site.dir, "%s", dir);
        fs->site.valid = true;
    }

    *items = list;
    *count = listCount;

    return true;
}

static fs_result siteError(const net_get_data* netData)
{
    if(netData->type != net_get_error)
        return (fs_result){FS_IO, "the site did not answer"};

    s32 code = netData->error.code;

    switch(code)
    {
    case 401: return (fs_result){FS_SESSION, "the session ended"};
    case 403: return (fs_result){FS_DENIED, "access denied"};
    case 404: return (fs_result){FS_NOT_FOUND, "not found"};
    case 413: return (fs_result){FS_QUOTA, "the site's limit refused it"};
    default: return code <= 0 ? (fs_result){FS_OFFLINE, "no network"} : (fs_result){FS_IO, "the site did not answer"};
    }
}

// The directory a path names, relative to the site's root.
static void siteDirOf(tic_fs* fs, const char* path, char* out, s32 size)
{
    const char* cwd = fs->work + strlen(PublicDir);

    if(*cwd == '/')
        cwd++;

    char rel[TICNAME_MAX];
    snprintf(rel, sizeof rel, "%s", path);

    const char* ptr = rel;

    while(*ptr == '/')
        ptr++;

    if(*cwd && *ptr)
        snprintf(out, size, "%s/%s", cwd, ptr);
    else
        snprintf(out, size, "%s%s", *cwd ? cwd : "", ptr);
}

static void siteSplit(tic_fs* fs, const char* path, char* dir, s32 dirSize, char* name, s32 nameSize)
{
    char rel[TICNAME_MAX];
    snprintf(rel, sizeof rel, "%s", path);

    char* ptr = rel;

    while(*ptr == '/')
        ptr++;

    char* slash = strrchr(ptr, '/');

    if(slash)
    {
        *slash = '\0';
        siteDirOf(fs, ptr, dir, dirSize);
        snprintf(name, nameSize, "%s", slash + 1);
    }
    else
    {
        siteDirOf(fs, "", dir, dirSize);
        snprintf(name, nameSize, "%s", ptr);
    }
}

typedef void (*SiteListingDone)(tic_fs* fs, SiteEntry* items, s32 count, fs_result result, void* data);

typedef struct
{
    tic_fs* fs;
    char dir[TICNAME_MAX];
    SiteListingDone done;
    void* data;
} SiteListingRequest;

static void onSiteListing(const net_get_data* netData)
{
    SiteListingRequest* request = netData->calldata;

    if(netData->type == net_get_progress)
        return;

    SiteEntry* items = NULL;
    s32 count = 0;
    fs_result result;

    if(netData->type == net_get_done)
    {
        result = siteParseListing(request->fs, request->dir, (const char*)netData->done.data, netData->done.size, &items, &count)
                     ? (fs_result){FS_OK, NULL}
                     : (fs_result){FS_IO, "invalid listing"};
    }
    else
        result = siteError(netData);

    request->done(request->fs, items, count, result, request->data);
    free(request);
}

static void siteFetchListing(tic_fs* fs, const char* dir, SiteListingDone done, void* data)
{
    SiteListingRequest* request = calloc(1, sizeof(SiteListingRequest));

    if(!request)
    {
        done(fs, NULL, 0, (fs_result){FS_IO, "out of memory"}, data);
        return;
    }

    request->fs = fs;
    snprintf(request->dir, sizeof request->dir, "%s", dir);
    request->done = done;
    request->data = data;

    char url[TICNAME_MAX + 16];
    snprintf(url, sizeof url, "/json?fn=dir&path=%s", dir);

    tic_net_get(fs->net, url, onSiteListing, request);
}

typedef struct
{
    tic_fs* fs;
    fs_list_item_cb onItem;
    fs_list_done_cb onDone;
    void* data;
} SiteListRequest;

static void onSiteList(tic_fs* fs, SiteEntry* items, s32 count, fs_result result, void* data)
{
    (void)fs;

    SiteListRequest* request = data;

    if(result.code == FS_OK)
    {
        for(s32 i = 0; i < count; i++)
        {
            fs_item item = {items[i].name, items[i].dir ? NULL : items[i].title, items[i].dir, -1, 0};

            if(!request->onItem(&item, request->data))
                break;
        }
    }

    request->onDone(NULL, result, request->data);

    free(items);
    free(request);
}

static void siteList(tic_fs* fs, const char* path, fs_list_item_cb onItem, fs_list_done_cb onDone, void* data)
{
    SiteListRequest* request = calloc(1, sizeof(SiteListRequest));

    if(!request)
    {
        onDone(NULL, (fs_result){FS_IO, "out of memory"}, data);
        return;
    }

    request->fs = fs;
    request->onItem = onItem;
    request->onDone = onDone;
    request->data = data;

    char dir[TICNAME_MAX];
    siteDirOf(fs, path, dir, sizeof dir);

    siteFetchListing(fs, dir, onSiteList, request);
}

// A request resolves a name to its listing entry, then loads, stats or covers it.
typedef struct SiteRequest
{
    tic_fs* fs;
    char dir[TICNAME_MAX];
    char name[TICNAME_MAX];
    char hash[TICNAME_MAX];
    char title[TICNAME_MAX];
    bool dirEntry;
    bool allowDir;

    fs_load_done_cb loadDone;
    fs_cover_done_cb coverDone;
    fs_stat_done_cb statDone;
    void* data;
} SiteRequest;

typedef struct
{
    SiteRequest* request;
    void (*found)(SiteRequest* request, fs_result result);
} SiteResolve;

static void siteTake(SiteRequest* request, const SiteEntry* entry)
{
    snprintf(request->hash, sizeof request->hash, "%s", entry->hash);
    snprintf(request->title, sizeof request->title, "%s", entry->title);
    request->dirEntry = entry->dir;
}

static bool siteFindEntry(const SiteEntry* items, s32 count, const char* name, bool allowDir, SiteEntry* entry)
{
    for(s32 i = 0; i < count; i++)
    {
        if(strcmp(items[i].name, name) == 0 && (allowDir || !items[i].dir))
        {
            *entry = items[i];
            return true;
        }
    }

    return false;
}

static void onSiteResolve(tic_fs* fs, SiteEntry* items, s32 count, fs_result result, void* data)
{
    (void)fs;

    SiteResolve* resolve = data;
    SiteRequest* request = resolve->request;

    if(result.code == FS_OK)
    {
        SiteEntry entry;

        if(siteFindEntry(items, count, request->name, request->allowDir, &entry))
        {
            siteTake(request, &entry);
            result = (fs_result){FS_OK, NULL};
        }
        else
            result = (fs_result){FS_NOT_FOUND, "not found"};
    }

    resolve->found(request, result);

    free(items);
    free(resolve);
}

static void siteResolve(SiteRequest* request, void (*found)(SiteRequest*, fs_result))
{
    if(request->fs->site.valid && strcmp(request->fs->site.dir, request->dir) == 0)
    {
        SiteEntry entry;

        if(siteFindEntry(request->fs->site.items, request->fs->site.count, request->name, request->allowDir, &entry))
        {
            siteTake(request, &entry);

            // A load consumes the listing it resolved from: the next load
            // lists again, so an update the last listing predates is seen.
            if(request->loadDone)
                siteCacheClear(request->fs);

            found(request, (fs_result){FS_OK, NULL});
            return;
        }

        // A name the cache predates: list the folder again before refusing.
    }

    SiteResolve* resolve = calloc(1, sizeof(SiteResolve));

    if(!resolve)
    {
        found(request, (fs_result){FS_IO, "out of memory"});
        return;
    }

    resolve->request = request;
    resolve->found = found;

    siteFetchListing(request->fs, request->dir, onSiteResolve, resolve);
}

static void siteStatFound(SiteRequest* request, fs_result result)
{
    if(result.code == FS_OK)
    {
        fs_item item = {request->name, NULL, request->dirEntry, -1, 0};
        request->statDone(&item, result, request->data);
    }
    else
        request->statDone(NULL, result, request->data);

    free(request);
}

static void onSiteCart(const net_get_data* netData)
{
    SiteRequest* request = netData->calldata;

    if(netData->type == net_get_progress)
        return;

    if(netData->type == net_get_done)
    {
        char cachePath[TICNAME_MAX];
        snprintf(cachePath, sizeof cachePath, TIC_CACHE "%s.tic", request->hash);

        tic_fs_saveroot(request->fs, cachePath, netData->done.data, netData->done.size, false);

        request->loadDone(request->name, netData->done.data, netData->done.size, (fs_result){FS_OK, NULL}, request->data);
    }
    else
        request->loadDone(NULL, NULL, 0, siteError(netData), request->data);

    free(request);
}

static void siteLoadFound(SiteRequest* request, fs_result result)
{
    if(result.code != FS_OK)
    {
        request->loadDone(NULL, NULL, 0, result, request->data);
        free(request);
        return;
    }

    char cachePath[TICNAME_MAX];
    snprintf(cachePath, sizeof cachePath, TIC_CACHE "%s.tic", request->hash);

    s32 size = 0;
    void* cached = tic_fs_loadroot(request->fs, cachePath, &size);

    if(cached)
    {
        request->loadDone(request->name, cached, size, (fs_result){FS_OK, NULL}, request->data);
        free(cached);
        free(request);
        return;
    }

    char url[TICNAME_MAX * 2];
    snprintf(url, sizeof url, "/cart/%s/%s", request->hash, request->name);

    tic_net_get(request->fs->net, url, onSiteCart, request);
}

static void onSiteCover(const net_get_data* netData)
{
    SiteRequest* request = netData->calldata;

    if(netData->type == net_get_progress)
        return;

    if(netData->type == net_get_done)
    {
        char cachePath[TICNAME_MAX];
        snprintf(cachePath, sizeof cachePath, TIC_CACHE "%s.gif", request->hash);

        tic_fs_saveroot(request->fs, cachePath, netData->done.data, netData->done.size, false);

        request->coverDone(netData->done.data, netData->done.size, (fs_result){FS_OK, NULL}, request->data);
    }
    else
        request->coverDone(NULL, 0, siteError(netData), request->data);

    free(request);
}

static void siteCoverFound(SiteRequest* request, fs_result result)
{
    if(result.code != FS_OK)
    {
        request->coverDone(NULL, 0, result, request->data);
        free(request);
        return;
    }

    char cachePath[TICNAME_MAX];
    snprintf(cachePath, sizeof cachePath, TIC_CACHE "%s.gif", request->hash);

    s32 size = 0;
    void* cached = tic_fs_loadroot(request->fs, cachePath, &size);

    if(cached)
    {
        request->coverDone(cached, size, (fs_result){FS_OK, NULL}, request->data);
        free(cached);
        free(request);
        return;
    }

    char url[TICNAME_MAX * 2];
    snprintf(url, sizeof url, "/cart/%s/cover.gif", request->hash);

    tic_net_get(request->fs->net, url, onSiteCover, request);
}

static void siteLoad(tic_fs* fs, const char* path, fs_load_done_cb onDone, void* data)
{
    SiteRequest* request = calloc(1, sizeof(SiteRequest));

    if(!request)
    {
        onDone(NULL, NULL, 0, (fs_result){FS_IO, "out of memory"}, data);
        return;
    }

    request->fs = fs;
    request->loadDone = onDone;
    request->data = data;

    siteSplit(fs, path, request->dir, sizeof request->dir, request->name, sizeof request->name);
    siteResolve(request, siteLoadFound);
}

static void siteCover(tic_fs* fs, const char* path, fs_cover_done_cb onDone, void* data)
{
    SiteRequest* request = calloc(1, sizeof(SiteRequest));

    if(!request)
    {
        onDone(NULL, 0, (fs_result){FS_IO, "out of memory"}, data);
        return;
    }

    request->fs = fs;
    request->coverDone = onDone;
    request->data = data;

    siteSplit(fs, path, request->dir, sizeof request->dir, request->name, sizeof request->name);
    siteResolve(request, siteCoverFound);
}

static void siteStat(tic_fs* fs, const char* path, fs_stat_done_cb onDone, void* data)
{
    // The working directory is a directory on every mount.
    if(!*path)
    {
        fs_item item = {"", NULL, true, -1, 0};
        onDone(&item, (fs_result){FS_OK, NULL}, data);
        return;
    }

    SiteRequest* request = calloc(1, sizeof(SiteRequest));

    if(!request)
    {
        onDone(NULL, (fs_result){FS_IO, "out of memory"}, data);
        return;
    }

    request->fs = fs;
    request->allowDir = true;
    request->statDone = onDone;
    request->data = data;

    siteSplit(fs, path, request->dir, sizeof request->dir, request->name, sizeof request->name);
    siteResolve(request, siteStatFound);
}

static void siteSave(tic_fs* fs, const char* path, const void* bytes, s32 size, fs_save_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;
    (void)bytes;
    (void)size;

    onDone(NULL, (fs_result){FS_DENIED, "access denied"}, data);
}

static void siteDelete(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    onDone((fs_result){FS_DENIED, "access denied"}, data);
}

static void siteMkdir(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    onDone((fs_result){FS_UNSUPPORTED, "not supported on the site"}, data);
}

static void siteOpen(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    (void)fs;
    (void)path;

    onDone((fs_result){FS_UNSUPPORTED, "not supported on the site"}, data);
}

static const fs_mount_ops SiteOps = {
    .list = siteList,
    .load = siteLoad,
    .save = siteSave,
    .delete = siteDelete,
    .stat = siteStat,
    .mkdir = siteMkdir,
    .openfolder = siteOpen,
    .cover = siteCover,
};
#endif

static const fs_mount_ops* fsMount(tic_fs* fs)
{
    if(fs->mount)
        return fs->mount;

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    if(isPublic(fs))
        return &SiteOps;
#endif

    return &LocalOps;
}

void tic_fs_set_mount(tic_fs* fs, const fs_mount_ops* mount)
{
    fs->mount = mount;
}

void tic_fs_list(tic_fs* fs, const char* path, fs_list_item_cb onItem, fs_list_done_cb onDone, void* data)
{
    fsMount(fs)->list(fs, path, onItem, onDone, data);
}

void tic_fs_load(tic_fs* fs, const char* path, fs_load_done_cb onDone, void* data)
{
    fsMount(fs)->load(fs, path, onDone, data);
}

void tic_fs_save(tic_fs* fs, const char* path, const void* bytes, s32 size, fs_save_done_cb onDone, void* data)
{
    fsMount(fs)->save(fs, path, bytes, size, onDone, data);
}

void tic_fs_delete(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    fsMount(fs)->delete(fs, path, onDone, data);
}

void tic_fs_stat(tic_fs* fs, const char* path, fs_stat_done_cb onDone, void* data)
{
    fsMount(fs)->stat(fs, path, onDone, data);
}

void tic_fs_mkdir(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    fsMount(fs)->mkdir(fs, path, onDone, data);
}

void tic_fs_openfolder(tic_fs* fs, const char* path, fs_done_cb onDone, void* data)
{
    fsMount(fs)->openfolder(fs, path, onDone, data);
}

void tic_fs_cover(tic_fs* fs, const char* path, fs_cover_done_cb onDone, void* data)
{
    fsMount(fs)->cover(fs, path, onDone, data);
}

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
typedef struct
{
    fs_list_callback item;
    fs_done_callback done;
    void* data;
} NetDirData;

static void onDirResponse(const net_get_data* netData)
{
    NetDirData* netDirData = (NetDirData*)netData->calldata;

    if(netData->type == net_get_done)
    {
        if(json_parse((char*)netData->done.data, netData->done.size))
        {
            typedef char string[TICNAME_MAX];

            string name, hash, filename;

            s32 folders = json_array("folders", 0);
            for(s32 i = 0, size = json_array_size(folders); i < size; i++)
            {
                if(json_string("name", json_array_item(folders, i), name, sizeof name))
                    netDirData->item(name, NULL, NULL, 0, netDirData->data, true);
            }

            s32 files = json_array("files", 0);
            for(s32 i = 0, size = json_array_size(files); i < size; i++)
            {
                s32 item = json_array_item(files, i);

                if(json_string("name", item, name, sizeof name) && json_string("hash", item, hash, sizeof hash) && json_string("filename", item, filename, sizeof filename))
                    netDirData->item(filename, name, hash, json_int("id", item), netDirData->data, false);
            }
        }
    }

    switch(netData->type)
    {
        case net_get_done:
        case net_get_error:
            netDirData->done(netDirData->data);
            free(netDirData);
            break;
        default: break;
    }
}
#endif

void fs_enum(const char* path, fs_list_callback callback, void* data)
{
#if defined(BAREMETALPI)
    dbg("fs_enum %s", path);

    if(path && *path)
    {
        // ok
    }
    else
    {
        return;
    }

    static char path2[TICNAME_MAX];
    strncpy(path2, path, sizeof path2);

    if(path2[strlen(path2) - 1] == '/') // one character
        path2[strlen(path2) - 1] = 0;

    dbg("fs_enum Real %s", path2);

    DIR Directory;
    FILINFO FileInfo;
    FRESULT Result = f_findfirst(&Directory, &FileInfo, path2, "*");
    dbg("enumFilesRes %d", Result);

    for(unsigned i = 0; Result == FR_OK && FileInfo.fname[0]; i++)
    {
        if(!(FileInfo.fattrib & (AM_HID | AM_SYS)))
        {
            bool result = callback(FileInfo.fname, NULL, NULL, 0, data, FileInfo.fattrib & AM_DIR);

            if(!result)
            {
                break;
            }
        }

        Result = f_findnext(&Directory, &FileInfo);
    }

#else
    TIC_DIR* dir = NULL;
    struct tic_dirent* ent = NULL;

    const FsString* pathString = utf8ToString(path);

    if((dir = tic_opendir(pathString)) != NULL)
    {
        FsString fullPath[TICNAME_MAX] = {0};
        struct tic_stat_struct s;

        while((ent = tic_readdir(dir)) != NULL)
        {
            if(*ent->d_name != _S('.'))
            {
                size_t pathLen = tic_strlen(pathString);
                size_t nameLen = tic_strlen(ent->d_name);

                if(pathLen + nameLen < COUNT_OF(fullPath))
                {
                    tic_strncpy(fullPath, pathString, COUNT_OF(fullPath));
                    tic_strncpy(fullPath + pathLen, ent->d_name, nameLen + 1);
                }

                if(tic_stat(fullPath, &s) == 0)
                {
                    const char* name = stringToUtf8(ent->d_name);
                    bool result = callback(name, NULL, NULL, 0, data, S_ISDIR(s.st_mode));
                    freeString(name);

                    if(!result) break;
                }
            }
        }

        tic_closedir(dir);
    }

    freeString(pathString);
#endif
}

void tic_fs_enum(tic_fs* fs, fs_list_callback onItem, fs_done_callback onDone, void* data)
{
#ifndef BAREMETALPI
    if(isRoot(fs) && !onItem(PublicDir, NULL, NULL, 0, data, true))
    {
        onDone(data);
        return;
    }

#if defined(BUILD_EDITORS) || defined(BUILD_SURF)
    if(isPublic(fs))
    {
        // An old-style listing still invalidates the hash cache it could contradict.
        siteCacheClear(fs);

        char request[TICNAME_MAX];
        snprintf(request, sizeof request, "/json?fn=dir&path=%s", fs->work + sizeof(TIC_HOST));

        NetDirData netDirData = {onItem, onDone, data};
        tic_net_get(fs->net, request, onDirResponse, MOVE(netDirData));

        return;
    }
#endif

#endif

    const char* path = tic_fs_path(fs, "");

    fs_enum(path, onItem, data);

    onDone(data);
}

bool tic_fs_deldir(tic_fs* fs, const char* name)
{
#if defined(BAREMETALPI)
    const char* path = tic_fs_path(fs, name);
    const FsString* pathString = utf8ToString(path);
    bool result = (f_unlink(pathString) != FR_OK);
    freeString(pathString);

    return result;
#else
#if defined(__TIC_WINDOWS__)
    const char* path = tic_fs_path(fs, name);

    const FsString* pathString = utf8ToString(path);
    bool result = tic_rmdir(pathString);
    freeString(pathString);

#else
    bool result = rmdir(tic_fs_path(fs, name));
#endif

#if defined(__EMSCRIPTEN__)
    syncfs();
#endif

    return result;
#endif
}

bool tic_fs_delfile(tic_fs* fs, const char* name)
{
    const char* path = tic_fs_path(fs, name);

    const FsString* pathString = utf8ToString(path);
#if defined(BAREMETALPI)
    bool result = (f_unlink(pathString) != FR_OK);
#else
    bool result = tic_remove(pathString);
#endif
    freeString(pathString);

#if defined(__EMSCRIPTEN__)
    syncfs();
#endif

    return result;
}

void tic_fs_homedir(tic_fs* fs)
{
    memset(fs->work, 0, sizeof fs->work);
}

void tic_fs_dirback(tic_fs* fs)
{
    if(isPublicRoot(fs))
    {
        tic_fs_homedir(fs);
        return;
    }

    char* start = fs->work;
    char* ptr = start + strlen(fs->work);

    while(ptr > start && *ptr != '/')
        ptr--;
    while(*ptr)
        *ptr++ = '\0';
}

void tic_fs_dir(tic_fs* fs, char* dir)
{
    snprintf(dir, TICNAME_MAX, "%s", fs->work);
}

void tic_fs_changedir(tic_fs* fs, const char* dir)
{
    char temp[TICNAME_MAX + 1];

    if(strlen(fs->work) > 0)
    {
        snprintf(temp, TICNAME_MAX + 1, "%s/%s", fs->work, dir);
    }
    else
    {
        snprintf(temp, TICNAME_MAX, "%s", dir);
    }

    snprintf(fs->work, sizeof fs->work, "%.*s", (int)(sizeof fs->work - 1), temp);

#if defined(__TIC_WINDOWS__)
    for(char *ptr = fs->work, *end = ptr + strlen(ptr); ptr < end; ptr++)
        if(*ptr == SLASH_SYMBOL)
            *ptr = '/';
#endif
}

typedef struct
{
    char* name;
    bool found;
    fs_isdir_callback done;
    void* data;

} EnumPublicDirsData;

static bool onEnumPublicDirs(const char* name, const char* title, const char* hash, s32 id, void* data, bool dir)
{
    (void)dir;
    (void)hash;
    (void)id;
    (void)title;
    EnumPublicDirsData* enumPublicDirsData = (EnumPublicDirsData*)data;

    if(strcmp(name, enumPublicDirsData->name) == 0)
    {
        enumPublicDirsData->found = true;
        return false;
    }

    return true;
}

static void onEnumPublicDirsDone(void* data)
{
    EnumPublicDirsData* enumPublicDirsData = data;
    enumPublicDirsData->done(enumPublicDirsData->found, enumPublicDirsData->data);
    free(enumPublicDirsData->name);
    free(enumPublicDirsData);
}

bool fs_isdir(const char* path)
{
#if defined(BAREMETALPI)
    FILINFO s;
    FRESULT res = f_stat(path, &s);
    if(res != FR_OK) return false;
    return s.fattrib & AM_DIR;
#else
    struct tic_stat_struct s;
    const FsString* pathString = utf8ToString(path);
    bool isdir = tic_stat(pathString, &s) == 0 && S_ISDIR(s.st_mode);
    freeString(pathString);
    return isdir;
#endif
}

bool tic_fs_isdir(tic_fs* fs, const char* name)
{
    if(*name == '.') return false;
    if(isRoot(fs) && strcmp(name, PublicDir) == 0) return true;

#if defined(BAREMETALPI)
    dbg("fsIsDirSync %s\n", name);
    FILINFO s;
    const char* path = tic_fs_path(fs, name);
    FRESULT res = f_stat(path, &s);
    if(res != FR_OK) return false;

    return s.fattrib & AM_DIR;
#else
    return fs_isdir(tic_fs_path(fs, name));
#endif
}

void tic_fs_isdir_async(tic_fs* fs, const char* name, fs_isdir_callback callback, void* data)
{
    if(isPublic(fs))
    {
        EnumPublicDirsData enumPublicDirsData = {strdup(name), false, callback, data};
        tic_fs_enum(fs, onEnumPublicDirs, onEnumPublicDirsDone, MOVE(enumPublicDirsData));
        return;
    }

    callback(tic_fs_isdir(fs, name), data);
}

bool fs_write(const char* name, const void* buffer, s32 size)
{
#if defined(BAREMETALPI)
    dbg("fs_write %s\n", name);
    FIL File;
    FRESULT res = f_open(&File, name, FA_WRITE | FA_CREATE_ALWAYS);
    if(res != FR_OK)
    {
        return false;
    }

    UINT written = 0;

    res = f_write(&File, buffer, size, &written);

    f_close(&File);
    if(res != FR_OK)
    {
        return false;
    }
    if(written != size)
    {
        dbg("Write size diff %d %d", size, written);
        return false;
    }
    return true;
#else
    const FsString* pathString = utf8ToString(name);
    FILE* file = tic_fopen(pathString, _S("wb"));
    freeString(pathString);

    if(file)
    {
        fwrite(buffer, 1, size, file);
        fclose(file);

#if defined(__EMSCRIPTEN__)
        syncfs();
#endif

        return true;
    }

    return false;
#endif
}

void* fs_read(const char* path, s32* size)
{
#if defined(BAREMETALPI)
    dbg("fs_read %s\n", path);
    FILINFO fi;
    FRESULT res = f_stat(path, &fi);
    if(res != FR_OK) return NULL;
    FIL file;
    res = f_open(&file, path, FA_READ | FA_OPEN_EXISTING);
    if(res != FR_OK) return NULL;
    *size = fi.fsize; // size is in output!
    void* buffer = malloc(*size);
    UINT read = 0;
    res = f_read(&file, buffer, fi.fsize, &read);

    f_close(&file);
    if(read != (*size)) return NULL;

    return buffer;

#else
    const FsString* pathString = utf8ToString(path);
    FILE* file = tic_fopen(pathString, _S("rb"));
    freeString(pathString);

    void* buffer = NULL;

    if(file)
    {

        fseek(file, 0, SEEK_END);
        *size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if((buffer = malloc(*size)) && fread(buffer, *size, 1, file))
        {
        }

        fclose(file);
    }

    return buffer;

#endif
}

bool fs_exists(const char* name)
{
#if defined(BAREMETALPI)
    dbg("fs_exists %s\n", name);
    FILINFO s;

    FRESULT res = f_stat(name, &s);
    return res == FR_OK;
#else
    struct tic_stat_struct s;

    const FsString* pathString = utf8ToString(name);
    bool ret = tic_stat(pathString, &s) == 0;
    freeString(pathString);

    return ret;
#endif
}

bool tic_fs_exists(tic_fs* fs, const char* name)
{
    return fs_exists(tic_fs_path(fs, name));
}

u64 fs_date(const char* path)
{
#if defined(BAREMETALPI)
    dbg("fs_date %s\n", path);
    // TODO BAREMETALPI
    return 0;
#else
    struct tic_stat_struct s;

    const FsString* pathString = utf8ToString(path);
    s32 ret = tic_stat(pathString, &s);
    freeString(pathString);

    if(ret == 0 && S_ISREG(s.st_mode))
    {
        return s.st_mtime;
    }

    return 0;
#endif
}

bool tic_fs_saveroot(tic_fs* fs, const char* name, const void* data, s32 size, bool overwrite)
{
    const char* path = tic_fs_pathroot(fs, name);

    if(!overwrite)
    {
        if(fs_exists(path))
            return false;
    }

    return fs_write(path, data, size);
}

void* tic_fs_loadroot(tic_fs* fs, const char* name, s32* size)
{
    return fs_read(tic_fs_pathroot(fs, name), size);
}

bool tic_fs_makedir(tic_fs* fs, const char* name)
{
#if defined(BAREMETALPI)
    // TODO BAREMETALPI
    dbg("makeDir %s\n", name);

    const FsString* pathString = tic_fs_path(fs, name);
    char* path = strdup(pathString);
    if(path && *path)
    {                                     // make sure result has at least
        if(path[strlen(path) - 1] == '/') // one character
            path[strlen(path) - 1] = 0;
    }

    FRESULT res = f_mkdir(path);
    if(res != FR_OK)
    {
        dbg("Could not mkdir %s\n", name);
    }
    free(path);
    return (res != FR_OK);
#else

    const FsString* pathString = utf8ToString(tic_fs_path(fs, name));
    int result = tic_mkdir(pathString);
    freeString(pathString);

#if defined(__EMSCRIPTEN__)
    syncfs();
#endif
    return result;
#endif
}

#if defined(__TIC_WINDOWS__)
#define SEP "\\"
#else
#define SEP "/"
#endif

tic_fs* tic_fs_create(const char* path, tic_net* net)
{
    tic_fs* fs = (tic_fs*)calloc(1, sizeof(tic_fs));

    snprintf(fs->dir, sizeof fs->dir, "%s", path);

    if(path[strlen(path) - 1] != SEP[0])
        strcat(fs->dir, SEP);

    fs->net = net;

    return fs;
}

const char* fs_apppath()
{
    static char apppath[TICNAME_MAX];

#if defined(__TIC_WINDOWS__)
    {
        wchar_t wideAppPath[TICNAME_MAX];
        GetModuleFileNameW(NULL, wideAppPath, sizeof wideAppPath);
        WideCharToMultiByte(CP_UTF8, 0, wideAppPath, COUNT_OF(wideAppPath), apppath, COUNT_OF(apppath), 0, 0);
    }
#elif defined(__TIC_LINUX__)
    s32 size = readlink("/proc/self/exe", apppath, sizeof apppath);
    apppath[size] = '\0';
#elif defined(__TIC_MACOSX__)
    u32 size = sizeof apppath;
    _NSGetExecutablePath(apppath, &size);
#endif

    return apppath;
}

const char* fs_appfolder()
{
    static char appfolder[TICNAME_MAX];
    strcpy(appfolder, fs_apppath());

    char* pos = strrchr(appfolder, SLASH_SYMBOL);

    if(pos && pos[1])
        pos[1] = '\0';

    return appfolder;
}
