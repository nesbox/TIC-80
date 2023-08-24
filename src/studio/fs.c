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

#include "studio.h"
#include "fs.h"
#include "net.h"

#if defined(BAREMETALPI) || defined(_3DS)
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

#if defined(TIC_BUILD_WITH_LUA)

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

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

static const char* PublicDir = TIC_HOST;

struct tic_fs
{
    char dir[TICNAME_MAX];
    char work[TICNAME_MAX];
    tic_net* net;
};

#if defined(__EMSCRIPTEN__)
void syncfs()
{
    EM_ASM({Module.syncFSRequests++;});
}
#endif 

const char* tic_fs_pathroot(tic_fs* fs, const char* name)
{
    static char path[TICNAME_MAX];

    snprintf(path, sizeof path, "%s%s", fs->dir, name);

#if defined(__TIC_WINDOWS__)
    char* ptr = path;
    while (*ptr)
    {
        if (*ptr == '/') *ptr = '\\';
        ptr++;
    }
#endif

    return path;
}

const char* tic_fs_path(tic_fs* fs, const char* name)
{
    static char path[TICNAME_MAX];

    if(*name == '/')
        strncpy(path, name + 1, sizeof path);
    else if(strlen(fs->work))
        snprintf(path, sizeof path, "%s/%s", fs->work, name);
    else 
        strncpy(path, name, sizeof path);

    return tic_fs_pathroot(fs, path);
}

static bool isRoot(tic_fs* fs)
{
    return fs->work[0] == '\0';
}

static bool isPublicRoot(tic_fs* fs)
{
    return strcmp(fs->work, PublicDir) == 0;
}

static bool isPublic(tic_fs* fs)
{
    return memcmp(fs->work, PublicDir, STRLEN(PublicDir)) == 0;
}

bool tic_fs_ispubdir(tic_fs* fs)
{
    return isPublic(fs);
}

#if defined(__TIC_WINDOWS__)

typedef wchar_t FsString;

#define __S(x) L ## x 
#define _S(x) __S(x)

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

#define freeString(S) free((void*)S)


#define TIC_DIR _WDIR
#define tic_dirent _wdirent
#define tic_stat_struct _stat

#define tic_opendir _wopendir
#define tic_readdir _wreaddir
#define tic_closedir _wclosedir
#define tic_rmdir _wrmdir
#define tic_stat _wstat
#define tic_remove _wremove
#define tic_fopen _wfopen
#define tic_mkdir(name) _wmkdir(name)
#define tic_strncpy wcsncpy
#define tic_strncat wcsncat

#else

typedef char FsString;

#define _S(x) (x)

#define utf8ToString(S) (S)
#define stringToUtf8(S) (S)

#define freeString(S)

#define TIC_DIR DIR
#define tic_dirent dirent
#define tic_stat_struct stat

#define tic_opendir opendir
#define tic_readdir readdir
#define tic_closedir closedir
#define tic_rmdir rmdir
#define tic_stat stat
#define tic_remove remove
#define tic_fopen fopen
#define tic_mkdir(name) mkdir(name, 0777)
#define tic_strncpy strncpy
#define tic_strncat strncat

#endif

typedef struct
{
    fs_list_callback item;
    fs_done_callback done;
    void* data;
} NetDirData;

#if defined(TIC_BUILD_WITH_LUA)

static lua_State* netLuaInit(u8* buffer, s32 size)
{
    if (buffer && size)
    {
        char* script = calloc(1, size + 1);
        memcpy(script, buffer, size);

        lua_State* lua = luaL_newstate();

        if(lua)
        {
            if(luaL_loadstring(lua, script) == LUA_OK && lua_pcall(lua, 0, LUA_MULTRET, 0) == LUA_OK)
                return lua;

            else lua_close(lua);
        }

        free(script);
    }

    return NULL;
}

static void onDirResponse(const net_get_data* netData)
{
    NetDirData* netDirData = (NetDirData*)netData->calldata;

    if(netData->type == net_get_done)
    {
        lua_State* lua = netLuaInit(netData->done.data, netData->done.size);

        if (lua)
        {
            {
                lua_getglobal(lua, "folders");

                if (lua_type(lua, -1) == LUA_TTABLE)
                {
                    s32 count = (s32)lua_rawlen(lua, -1);

                    for (s32 i = 1; i <= count; i++)
                    {
                        lua_geti(lua, -1, i);

                        {
                            lua_getfield(lua, -1, "name");
                            if (lua_isstring(lua, -1))
                                netDirData->item(lua_tostring(lua, -1), NULL, NULL, 0, netDirData->data, true);

                            lua_pop(lua, 1);
                        }

                        lua_pop(lua, 1);
                    }
                }

                lua_pop(lua, 1);
            }

            {
                lua_getglobal(lua, "files");

                if (lua_type(lua, -1) == LUA_TTABLE)
                {
                    s32 count = (s32)lua_rawlen(lua, -1);

                    for (s32 i = 1; i <= count; i++)
                    {
                        lua_geti(lua, -1, i);

                        char hash[TICNAME_MAX];
                        char name[TICNAME_MAX];
                        char title[TICNAME_MAX];

                        {
                            lua_getfield(lua, -1, "hash");
                            if (lua_isstring(lua, -1))
                                strncpy(hash, lua_tostring(lua, -1), sizeof hash);

                            lua_pop(lua, 1);
                        }

                        {
                            lua_getfield(lua, -1, "filename");

                            if (lua_isstring(lua, -1))
                                strncpy(name, lua_tostring(lua, -1), sizeof name);

                            lua_pop(lua, 1);
                        }

                        {
                            lua_getfield(lua, -1, "name");

                            if (lua_isstring(lua, -1))
                                strncpy(title, lua_tostring(lua, -1), sizeof title);

                            lua_pop(lua, 1);
                        }

                        {
                            lua_getfield(lua, -1, "id");

                            if (lua_isinteger(lua, -1))
                                netDirData->item(name, title, hash, (s32)lua_tointeger(lua, -1), netDirData->data, false);

                            lua_pop(lua, 1);
                        }

                        lua_pop(lua, 1);
                    }
                }

                lua_pop(lua, 1);
            }

            lua_close(lua);
        }
    }

    switch (netData->type)
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

static void enumFiles(tic_fs* fs, const char* path, fs_list_callback callback, void* data)
{
#if defined(BAREMETALPI)
    dbg("enumFiles %s", path);

        if (path && *path) {
        // ok
    }
    else
    {
        return;
    }

    static char path2[TICNAME_MAX];
    strncpy(path2, path, sizeof path2);

    if (path2[strlen(path2) - 1] == '/')    // one character
                path2[strlen(path2) - 1] = 0;

    dbg("enumFiles Real %s", path2);


    DIR Directory;
    FILINFO FileInfo;
    FRESULT Result = f_findfirst (&Directory, &FileInfo, path2, "*");
    dbg("enumFilesRes %d", Result);

    for (unsigned i = 0; Result == FR_OK && FileInfo.fname[0]; i++)
    {
        if (!(FileInfo.fattrib & (AM_HID | AM_SYS)))
        {
            bool result = callback(FileInfo.fname, NULL, NULL, 0, data, FileInfo.fattrib & AM_DIR);

            if (!result)
            {
                break;
            }
        }

        Result = f_findnext (&Directory, &FileInfo);
    }

#else
    TIC_DIR *dir = NULL;
    struct tic_dirent* ent = NULL;

    const FsString* pathString = utf8ToString(path);

    if ((dir = tic_opendir(pathString)) != NULL)
    {
        FsString fullPath[TICNAME_MAX];
        struct tic_stat_struct s;
        
        while ((ent = tic_readdir(dir)) != NULL)
        {
            if(*ent->d_name != _S('.'))
            {
                tic_strncpy(fullPath, pathString, COUNT_OF(fullPath));
                tic_strncat(fullPath, ent->d_name, COUNT_OF(fullPath) - 1);

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
    if (isRoot(fs) && !onItem(PublicDir, NULL, NULL, 0, data, true))
    {
        onDone(data);
        return;
    }

#if defined(TIC_BUILD_WITH_LUA) && defined(BUILD_EDITORS)
    if(isPublic(fs))
    {
        char request[TICNAME_MAX];
        snprintf(request, sizeof request, "/api?fn=dir&path=%s", fs->work + sizeof(TIC_HOST));

        NetDirData netDirData = { onItem, onDone, data };
        tic_net_get(fs->net, request, onDirResponse, MOVE(netDirData));

        return;
    }
#endif

    const char* path = tic_fs_path(fs, "");

    enumFiles(fs, path, onItem, data);

    onDone(data);
}

bool tic_fs_deldir(tic_fs* fs, const char* name)
{
#if defined(BAREMETALPI)
    // TODO BAREMETALPI
    dbg("tic_fs_deldir %s", name);
    return 0;
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
#if defined(BAREMETALPI)
    dbg("tic_fs_delfile %s", name);
    // TODO BAREMETALPI
    return false;
#else
    const char* path = tic_fs_path(fs, name);

    const FsString* pathString = utf8ToString(path);
    bool result = tic_remove(pathString);
    freeString(pathString);

#if defined(__EMSCRIPTEN__)
    syncfs();
#endif  

    return result;
#endif
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

    while (ptr > start && *ptr != '/') ptr--;
    while (*ptr) *ptr++ = '\0';
}

void tic_fs_dir(tic_fs* fs, char* dir)
{
    strncpy(dir, fs->work, TICNAME_MAX);
}

void tic_fs_changedir(tic_fs* fs, const char* dir)
{
    if(strlen(fs->work))
        strncat(fs->work, "/", TICNAME_MAX);
                
    strcat(fs->work, dir);

#if defined(__TIC_WINDOWS__)
    for(char *ptr = fs->work, *end = ptr + strlen(ptr); ptr < end; ptr++)
        if(*ptr == '\\')
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

bool tic_fs_isdir(tic_fs* fs, const char* name)
{
    if (*name == '.') return false;
    if (isRoot(fs) && strcmp(name, PublicDir) == 0) return true;

#if defined(BAREMETALPI)
    dbg("fsIsDirSync %s\n", name);
    FILINFO s;
    const char* path = tic_fs_path(fs, name);
    FRESULT res = f_stat(path, &s);
    if (res != FR_OK) return false;

    return s.fattrib & AM_DIR;
#else
    const char* path = tic_fs_path(fs, name);
    struct tic_stat_struct s;
    const FsString* pathString = utf8ToString(path);
    bool ret = tic_stat(pathString, &s) == 0 && S_ISDIR(s.st_mode);
    freeString(pathString);

    return ret;
#endif
}

void tic_fs_isdir_async(tic_fs* fs, const char* name, fs_isdir_callback callback, void* data)
{
    if(isPublic(fs))
    {
        EnumPublicDirsData enumPublicDirsData = { strdup(name), false, callback, data};
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
    FRESULT res = f_open (&File, name, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK)
    {
        return false;
    }

    u32 written=0;
    
    res = f_write(&File, buffer, size, &written);
    
    f_close(&File);
    if (res != FR_OK)
    {
        return false;
    }
    if(written!=size)
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
    if(res!=FR_OK) return NULL;
    FIL file;
    res = f_open (&file, path, FA_READ | FA_OPEN_EXISTING);
    if(res!=FR_OK) return NULL;
    *size = fi.fsize; // size is in output!
    void* buffer = malloc(*size);
    UINT read = 0;
    res = f_read(&file, buffer, fi.fsize, &read);

    f_close(&file);
    if(read!=(*size)) return NULL;

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

        if((buffer = malloc(*size)) && fread(buffer, *size, 1, file)) {}

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

bool tic_fs_save(tic_fs* fs, const char* name, const void* data, s32 size, bool overwrite)
{
    if(!overwrite)
    {
        if(tic_fs_exists(fs, name))
            return false;
    }

    return fs_write(tic_fs_path(fs, name), data, size);
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

typedef struct
{
    tic_fs* fs;
    fs_load_callback done;
    void* data;
    char* cachePath;
} LoadFileByHashData;

static void fileByHashLoaded(const net_get_data* netData)
{
    LoadFileByHashData* loadFileByHashData = netData->calldata;

    if (netData->type == net_get_done)
    {
        tic_fs_saveroot(loadFileByHashData->fs, loadFileByHashData->cachePath, netData->done.data, netData->done.size, false);
        loadFileByHashData->done(netData->done.data, netData->done.size, loadFileByHashData->data);
    }

    switch (netData->type)
    {
    case net_get_done:
    case net_get_error:

        free(loadFileByHashData->cachePath);
        free(loadFileByHashData);
        break;
    default: break;
    }
}

void tic_fs_hashload(tic_fs* fs, const char* name, const char* hash, fs_load_callback callback, void* data)
{
#if defined(BAREMETALPI)
    // TODO BAREMETALPI
    return;
#else

    char cachePath[TICNAME_MAX];
    snprintf(cachePath, sizeof cachePath, TIC_CACHE "%s.tic", hash);

    {
        s32 size = 0;
        void* buffer = tic_fs_loadroot(fs, cachePath, &size);
        if (buffer)
        {
            callback(buffer, size, data);
            return;
        }
    }

#if defined(BUILD_EDITORS)
    char path[TICNAME_MAX];
    snprintf(path, sizeof path, "/cart/%s/%s", hash, name);

    LoadFileByHashData loadFileByHashData = { fs, callback, data, strdup(cachePath) };
    tic_net_get(fs->net, path, fileByHashLoaded, MOVE(loadFileByHashData));
#endif

#endif
}

void* tic_fs_load(tic_fs* fs, const char* name, s32* size)
{
#if defined(BAREMETALPI)
    dbg("tic_fs_load x %s\n", name);
    dbg("fs.dir %s\n", fs->dir);
    dbg("fs.work %s\n", fs->work);
    
    if(isPublic(fs))
    {
        dbg("Public ??\n");
        return NULL;
    }
    else
    {
        dbg("non public \n");
        const char* fp = tic_fs_path(fs, name);
        dbg("loading: %s\n", fp);


        FILINFO fi;
        FRESULT res = f_stat(fp, &fi);
        dbg("fstat done %d \n", res);

        if(res!=FR_OK) 
        {
            dbg("NO F_STAT %d\n", res);
            return NULL;
        }
        FIL file;
        res = f_open (&file, fp, FA_READ | FA_OPEN_EXISTING);
        if(res!=FR_OK)
        {
            dbg("NO F_OPEN %d\n", res);
            return NULL;
        }
        dbg("BUFFERING %d\n", res);

        void* buffer = malloc(fi.fsize);
        dbg("BUFFERED %d\n", fi.fsize);

        UINT read = 0;
        res = f_read(&file, buffer, fi.fsize, &read);
        dbg("F_READ %d %ld\n", res, read);

        f_close(&file);
        if(read!=fi.fsize) 
        {
            dbg("NO F_READ %d \n", res);
            return NULL;
        }
        dbg("RETURNING!!\n");
        *size = fi.fsize;
        return buffer;
    }
    return NULL;
#else

    const FsString* pathString = utf8ToString(tic_fs_path(fs, name));
    FILE* file = tic_fopen(pathString, _S("rb"));
    freeString(pathString);

    void* ptr = NULL;

    if(file)
    {
        fseek(file, 0, SEEK_END);
        *size = ftell(file);
        fseek(file, 0, SEEK_SET);

        u8* buffer = malloc(*size);

        if(buffer && fread(buffer, *size, 1, file)) ptr = buffer;

        fclose(file);
    }

    return ptr;     

#endif
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
    if (path && *path) {                      // make sure result has at least
      if (path[strlen(path) - 1] == '/')    // one character
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

void tic_fs_openfolder(tic_fs* fs)
{
    const char* path = tic_fs_path(fs, "");

    if(isPublic(fs))
        path = fs->dir;

    tic_sys_open_path(path);
}

#if defined(__TIC_WINDOWS__)
#define SEP "\\"
#else
#define SEP "/"
#endif

tic_fs* tic_fs_create(const char* path, tic_net* net)
{
    tic_fs* fs = (tic_fs*)calloc(1, sizeof(tic_fs));

    strncpy(fs->dir, path, TICNAME_MAX);

    if(path[strlen(path) - 1] != SEP[0])
        strcat(fs->dir, SEP);

    fs->net = net;

    return fs;
}
