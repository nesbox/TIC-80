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
#include "ext/file_dialog.h"

#if defined(BAREMETALPI)
#include "../../circle-stdlib/libs/circle/addon/fatfs/ff.h"
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#if defined(__TIC_WINRT__) || defined(__TIC_WINDOWS__)
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#define PUBLIC_DIR TIC_HOST "/play"
#define PUBLIC_DIR_SLASH PUBLIC_DIR "/"


//define dbg(...) printf(__VA_ARGS__)
#define dbg(...)


static const char* PublicDir = PUBLIC_DIR;

struct FileSystem
{
    char dir[TICNAME_MAX];
    char work[TICNAME_MAX];
};

const char* fsGetRootFilePath(FileSystem* fs, const char* name)
{
    static char path[TICNAME_MAX] = {0};

    sprintf(path, "%s%s", fs->dir, name);

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

const char* fsGetFilePath(FileSystem* fs, const char* name)
{
    static char path[TICNAME_MAX] = {0};

    if(strlen(fs->work))
        sprintf(path, "%s/%s", fs->work, name);
    else 
        strcpy(path, name);

    return fsGetRootFilePath(fs, path);
}

static bool isRoot(FileSystem* fs)
{
    return fs->work[0] == '\0';
}

static bool isPublicRoot(FileSystem* fs)
{
    return strcmp(fs->work, PublicDir) == 0;
}

static bool isPublic(FileSystem* fs)
{
    return strcmp(fs->work, PublicDir) == 0 || memcmp(fs->work, PUBLIC_DIR_SLASH, sizeof PUBLIC_DIR_SLASH - 1) == 0;
}

bool fsIsInPublicDir(FileSystem* fs)
{
    return isPublic(fs);
}

#if defined(__TIC_WINDOWS__) || defined(__TIC_WINRT__)

typedef wchar_t fsString;

#define __S(x) L ## x 
#define _S(x) __S(x)

static const fsString* utf8ToString(const char* str)
{
    fsString* wstr = malloc(TICNAME_MAX * sizeof(fsString));

    MultiByteToWideChar(CP_UTF8, 0, str, TICNAME_MAX, wstr, TICNAME_MAX);

    return wstr;
}

static const char* stringToUtf8(const fsString* wstr)
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
#define tic_strcpy wcscpy
#define tic_strcat wcscat

#else

typedef char fsString;

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
#define tic_mkdir(name) mkdir(name, 0700)
#define tic_strcpy strcpy
#define tic_strcat strcat

#endif

typedef struct
{
    ListCallback callback;
    void* data;
} NetDirData;

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

static void onDirResponse(u8* buffer, s32 size, void* data)
{
    NetDirData* netDirData = (NetDirData*)data;

    if(buffer && size)
    {
        lua_State* lua = netLuaInit(buffer, size);
        free(buffer);

        if(lua)
        {
            {
                lua_getglobal(lua, "folders");

                if(lua_type(lua, -1) == LUA_TTABLE)
                {
                    s32 count = (s32)lua_rawlen(lua, -1);

                    for(s32 i = 1; i <= count; i++)
                    {
                        lua_geti(lua, -1, i);

                        {
                            lua_getfield(lua, -1, "name");
                            if(lua_isstring(lua, -1))
                                netDirData->callback(lua_tostring(lua, -1), NULL, 0, netDirData->data, true);

                            lua_pop(lua, 1);
                        }

                        lua_pop(lua, 1);
                    }
                }

                lua_pop(lua, 1);
            }

            {
                lua_getglobal(lua, "files");

                if(lua_type(lua, -1) == LUA_TTABLE)
                {
                    s32 count = (s32)lua_rawlen(lua, -1);

                    for(s32 i = 1; i <= count; i++)
                    {
                        lua_geti(lua, -1, i);

                        char hash[TICNAME_MAX] = {0};
                        char name[TICNAME_MAX] = {0};

                        {
                            lua_getfield(lua, -1, "hash");
                            if(lua_isstring(lua, -1))
                                strcpy(hash, lua_tostring(lua, -1));

                            lua_pop(lua, 1);
                        }

                        {
                            lua_getfield(lua, -1, "name");

                            if(lua_isstring(lua, -1))
                                strcpy(name, lua_tostring(lua, -1));

                            lua_pop(lua, 1);
                        }

                        {
                            lua_getfield(lua, -1, "id");

                            if(lua_isinteger(lua, -1))
                                netDirData->callback(name, hash, lua_tointeger(lua, -1), netDirData->data, false);

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
}

static void netDirRequest(const char* path, ListCallback callback, void* data)
{
    char request[TICNAME_MAX] = {'\0'};
    sprintf(request, "/api?fn=dir&path=%s", path);

    s32 size = 0;
    void* buffer = getSystem()->httpGetSync(request, &size);

    NetDirData netDirData = {callback, data};
    onDirResponse(buffer, size, &netDirData);
}

static void enumFiles(FileSystem* fs, const char* path, ListCallback callback, void* data, bool folder)
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

    static char path2[TICNAME_MAX] = {0};
    strcpy(path2, path);

    if (path2[strlen(path2) - 1] == '/')    // one character
                path2[strlen(path2) - 1] = 0;

    dbg("enumFiles Real %s", path2);


    DIR Directory;
    FILINFO FileInfo;
    FRESULT Result = f_findfirst (&Directory, &FileInfo, path2, "*");
    dbg("enumFilesRes %d", Result);

    for (unsigned i = 0; Result == FR_OK && FileInfo.fname[0]; i++)
    {
        bool check = FileInfo.fattrib & (folder ? AM_DIR : AM_ARC);

        if (check &&  !(FileInfo.fattrib & (AM_HID | AM_SYS)))
        {
            bool result = callback(FileInfo.fname, NULL, 0, data, folder);

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

    const fsString* pathString = utf8ToString(path);

    if ((dir = tic_opendir(pathString)) != NULL)
    {
        fsString fullPath[TICNAME_MAX];
        struct tic_stat_struct s;
        
        while ((ent = tic_readdir(dir)) != NULL)
        {
            if(*ent->d_name != _S('.'))
            {
                tic_strcpy(fullPath, pathString);
                tic_strcat(fullPath, ent->d_name);

                if(tic_stat(fullPath, &s) == 0 && folder ? S_ISDIR(s.st_mode) : S_ISREG(s.st_mode))
                {
                    const char* name = stringToUtf8(ent->d_name);
                    bool result = callback(name, NULL, 0, data, folder);
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

void fsEnumFiles(FileSystem* fs, ListCallback callback, void* data)
{
    if(isRoot(fs) && !callback(PublicDir, NULL, 0, data, true))return;

    if(isPublic(fs))
    {
        netDirRequest(fs->work + sizeof(TIC_HOST), callback, data);
        return;
    }

    const char* path = fsGetFilePath(fs, "");

    enumFiles(fs, path, callback, data, true);
    enumFiles(fs, path, callback, data, false);
}

bool fsDeleteDir(FileSystem* fs, const char* name)
{
#if defined(BAREMETALPI)
    // TODO BAREMETALPI
    dbg("fsDeleteDir %s", name);
    return 0;
#else
#if defined(__TIC_WINRT__) || defined(__TIC_WINDOWS__)
    const char* path = fsGetFilePath(fs, name);

    const fsString* pathString = utf8ToString(path);
    bool result = tic_rmdir(pathString);
    freeString(pathString);

#else
    bool result = rmdir(fsGetFilePath(fs, name));
#endif

#if defined(__EMSCRIPTEN__)
    EM_ASM(FS.syncfs(function(){}));
#endif  

    return result;
#endif
}

bool fsDeleteFile(FileSystem* fs, const char* name)
{
#if defined(BAREMETALPI)
    dbg("fsDeleteFile %s", name);
    // TODO BAREMETALPI
    return false;
#else
    const char* path = fsGetFilePath(fs, name);

    const fsString* pathString = utf8ToString(path);
    bool result = tic_remove(pathString);
    freeString(pathString);

#if defined(__EMSCRIPTEN__)
    EM_ASM(FS.syncfs(function(){}));
#endif  

    return result;
#endif
}

typedef struct
{
    FileSystem* fs;
    AddCallback callback;
    void* data;
} AddFileData;

static void onAddFile(const char* name, const u8* buffer, s32 size, void* data, u32 mode)
{
#if defined(BAREMETALPI)
    dbg("onAddFile %s", name);
    // TODO BAREMETALPI
#else
    AddFileData* addFileData = (AddFileData*)data;
    FileSystem* fs = addFileData->fs;

    if(name)
    {
        const char* destname = fsGetFilePath(fs, name);

        const fsString* destString = utf8ToString(destname);
        FILE* file = tic_fopen(destString, _S("rb"));
        freeString(destString);

        if(file)
        {
            fclose(file);

            addFileData->callback(name, FS_FILE_EXISTS, addFileData->data);
        }
        else
        {
            const char* path = fsGetFilePath(fs, name);

            const fsString* pathString = utf8ToString(path);
            FILE* dest = tic_fopen(pathString, _S("wb"));
            freeString(pathString);

            if (dest)
            {
                fwrite(buffer, 1, size, dest);
                fclose(dest);

#if !defined(__TIC_WINRT__) && !defined(__TIC_WINDOWS__) && !defined(_3DS)
                if(mode)
                    chmod(path, mode);
#endif

#if defined(__EMSCRIPTEN__)
                EM_ASM(FS.syncfs(function(){}));
#endif
                
                addFileData->callback(name, FS_FILE_ADDED, addFileData->data);
            }
        }
    }
    else
    {
        addFileData->callback(name, FS_FILE_NOT_ADDED, addFileData->data);
    }

    free(addFileData);
#endif
}

void fsAddFile(FileSystem* fs, AddCallback callback, void* data)
{
    AddFileData* addFileData = (AddFileData*)malloc(sizeof(AddFileData));

    *addFileData = (AddFileData) { fs, callback, data };

    getSystem()->fileDialogLoad(&onAddFile, addFileData);
}

typedef struct
{
    GetCallback callback;
    void* data;
    void* buffer;
} GetFileData;

static void onGetFile(bool result, void* data)
{
    GetFileData* command = (GetFileData*)data;

    command->callback(result ? FS_FILE_DOWNLOADED : FS_FILE_NOT_DOWNLOADED, command->data);

    free(command->buffer);
    free(command);
}

static u32 fsGetMode(FileSystem* fs, const char* name)
{
#if defined(BAREMETALPI)
    dbg("fsGetMode %s", name);
    // TODO BAREMETALPI
    return 0;
#else

#if defined(__TIC_WINRT__) || defined(__TIC_WINDOWS__)
    return 0;
#else
    const char* path = fsGetFilePath(fs, name);
    mode_t mode = 0;
    struct stat s;
    if(stat(path, &s) == 0)
        mode = s.st_mode;

    return mode;
#endif

#endif

}

void fsHomeDir(FileSystem* fs)
{
    memset(fs->work, 0, sizeof fs->work);
}

void fsDirBack(FileSystem* fs)
{
    if(isPublicRoot(fs))
    {
        fsHomeDir(fs);
        return;
    }

    char* start = fs->work;
    char* ptr = start + strlen(fs->work);

    while(ptr > start && *ptr != '/') ptr--;

    *ptr = '\0';
}

void fsGetDir(FileSystem* fs, char* dir)
{
    strcpy(dir, fs->work);
}

bool fsChangeDir(FileSystem* fs, const char* dir)
{
    if(fsIsDir(fs, dir))
    {
        if(strlen(fs->work))
            strcat(fs->work, "/");
                
        strcat(fs->work, dir);

        return true;
    }

    return false;
}

typedef struct
{
    const char* name;
    bool found;

} EnumPublicDirsData;

static bool onEnumPublicDirs(const char* name, const char* info, s32 id, void* data, bool dir)
{
    EnumPublicDirsData* enumPublicDirsData = (EnumPublicDirsData*)data;

    if(strcmp(name, enumPublicDirsData->name) == 0)
    {
        enumPublicDirsData->found = true;
        return false;
    }

    return true;
}

bool fsIsDir(FileSystem* fs, const char* name)
{
    if(*name == '.') return false;
#if defined(BAREMETALPI)
    dbg("fsIsDir %s\n", name);
    FILINFO s;

    FRESULT res = f_stat(name, &s);
    if(res != FR_OK) return false;
    
    return s.fattrib & AM_DIR;
#else

    if(isRoot(fs) && strcmp(name, PublicDir) == 0)
        return true;

    if(isPublicRoot(fs))
    {
        EnumPublicDirsData enumPublicDirsData =
        {
            .name = name,
            .found = false,
        };

        fsEnumFiles(fs, onEnumPublicDirs, &enumPublicDirsData);

        return enumPublicDirsData.found;
    }

    const char* path = fsGetFilePath(fs, name);
    struct tic_stat_struct s;
    const fsString* pathString = utf8ToString(path);
    bool ret = tic_stat(pathString, &s) == 0 && S_ISDIR(s.st_mode);
    freeString(pathString);

    return ret;
#endif

}

void fsGetFileData(GetCallback callback, const char* name, void* buffer, size_t size, u32 mode, void* data)
{
    GetFileData* command = (GetFileData*)malloc(sizeof(GetFileData));
    *command = (GetFileData) {callback, data, buffer};

    getSystem()->fileDialogSave(onGetFile, name, buffer, size, command, mode);
}

typedef struct
{
    OpenCallback callback;
    void* data;
} OpenFileData;

static void onOpenFileData(const char* name, const u8* buffer, s32 size, void* data, u32 mode)
{
    OpenFileData* command = (OpenFileData*)data;

    command->callback(name, buffer, size, command->data);

    free(command);
}

void fsOpenFileData(OpenCallback callback, void* data)
{
    OpenFileData* command = (OpenFileData*)malloc(sizeof(OpenFileData));

    *command = (OpenFileData){callback, data};

    getSystem()->fileDialogLoad(onOpenFileData, command);
}

void fsGetFile(FileSystem* fs, GetCallback callback, const char* name, void* data)
{
    s32 size = 0;
    void* buffer = fsLoadFile(fs, name, &size);

    if(buffer)
    {
        GetFileData* command = (GetFileData*)malloc(sizeof(GetFileData));
        *command = (GetFileData) {callback, data, buffer};

        s32 mode = fsGetMode(fs, name);
        getSystem()->fileDialogSave(onGetFile, name, buffer, size, command, mode);
    }
    else callback(FS_FILE_NOT_DOWNLOADED, data);
}

bool fsWriteFile(const char* name, const void* buffer, s32 size)
{
#if defined(BAREMETALPI)
    dbg("fsWriteFile %s\n", name);
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
    const fsString* pathString = utf8ToString(name);
    FILE* file = tic_fopen(pathString, _S("wb"));
    freeString(pathString);

    if(file)
    {
        fwrite(buffer, 1, size, file);
        fclose(file);

#if defined(__EMSCRIPTEN__)
        EM_ASM(FS.syncfs(function(){}));
#endif

        return true;
    }

    return false;
#endif
}

bool fsCopyFile(const char* src, const char* dst)
{
#if defined(BAREMETALPI)
    // TODO BAREMETALPI
    return false;
#else
    bool done = false;

    void* buffer = NULL;
    s32 size = 0;

    {
        const fsString* pathString = utf8ToString(src);
        FILE* file = tic_fopen(pathString, _S("rb"));
        freeString(pathString);

        if(file)
        {
            fseek(file, 0, SEEK_END);
            size = ftell(file);
            fseek(file, 0, SEEK_SET);

            if((buffer = malloc(size)) && fread(buffer, size, 1, file)) {}

            fclose(file);
        }       
    }

    if(buffer)
    {
        const fsString* pathString = utf8ToString(dst);
        FILE* file = tic_fopen(pathString, _S("wb"));
        freeString(pathString);

        if(file)
        {
            fwrite(buffer, 1, size, file);
            fclose(file);

            done = true;
        }

        free(buffer);
    }

    return done;
#endif

}

void* fsReadFile(const char* path, s32* size)
{
#if defined(BAREMETALPI)
    dbg("fsReadFile %s\n", path);
    FILINFO fi;
    FRESULT res = f_stat(path, &fi);
    if(res!=FR_OK) return NULL;
    FIL file;
    res = f_open (&file, path, FA_READ | FA_OPEN_EXISTING);
    if(res!=FR_OK) return NULL;

    void* buffer = malloc(*size);
    UINT read = 0;
    res = f_read(&file, buffer, fi.fsize, &read);

    f_close(&file);
    if(read!=(*size)) return NULL;

    return buffer;

#else
    const fsString* pathString = utf8ToString(path);
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

static void makeDir(const char* name)
{
#if defined(BAREMETALPI)
    // TODO BAREMETALPI
    dbg("makeDir %s\n", name);

    char* path = strdup(name);
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
#else
    const fsString* pathString = utf8ToString(name);
    tic_mkdir(pathString);
    freeString(pathString);

#if defined(__EMSCRIPTEN__)
    EM_ASM(FS.syncfs(function(){}));
#endif
#endif
}

static void fsFullname(const char *path, char *fullname)
{
#if defined(BAREMETALPI) || defined(_3DS)
    dbg("fsFullname %s", path);
    // TODO BAREMETALPI
#else
#if defined(__TIC_WINDOWS__) || defined(__TIC_WINRT__)
    static wchar_t wpath[TICNAME_MAX];

    const fsString* pathString = utf8ToString(path);
    GetFullPathNameW(pathString, sizeof(wpath), wpath, NULL);
    freeString(pathString);

    const char* res = stringToUtf8(wpath);

#else

    const char* res = realpath(path, NULL);

#endif

    strcpy(fullname, res);
    free((void*)res);
#endif
}

void fsFilename(const char *path, char* out)
{
    char full[TICNAME_MAX];
    fsFullname(path, full);

    char base[TICNAME_MAX];
    fsBasename(path, base);

    strcpy(out, full + strlen(base));
}

void fsBasename(const char *path, char* out)
{
#if defined(BAREMETALPI)
    // TODO BAREMETALPI
    dbg("fsBasename %s\n", path);
#else

    char* result = NULL;

#if defined(__TIC_WINDOWS__) || defined(__TIC_WINRT__)
#define SEP "\\"
#else
#define SEP "/"
#endif


    char full[TICNAME_MAX];
    fsFullname(path, full);

    struct tic_stat_struct s;

    const fsString* fullString = utf8ToString(full);
    s32 ret = tic_stat(fullString, &s);
    freeString(fullString);

    if(ret == 0)
    {
        result = full;

        if(S_ISREG(s.st_mode))
        {
            const char* ptr = result + strlen(result);

            while(ptr >= result)
            {
                if(*ptr == SEP[0])
                {
                    result[ptr-result] = '\0';
                    break;
                }

                ptr--;
            }
        }
    }

    if (result)
    {
        if (result[strlen(result) - 1] != SEP[0])
            strcat(result, SEP);

        strcpy(out, result);
    }
#endif
}

bool fsExists(const char* name)
{
#if defined(BAREMETALPI)
    dbg("fsExists %s\n", name);
    FILINFO s;

    FRESULT res = f_stat(name, &s);
    return res == FR_OK;
#else
    struct tic_stat_struct s;

    const fsString* pathString = utf8ToString(name);
    bool ret = tic_stat(pathString, &s) == 0;
    freeString(pathString);

    return ret;
#endif
}

bool fsExistsFile(FileSystem* fs, const char* name)
{
    return fsExists(fsGetFilePath(fs, name));
}

u64 fsMDate(FileSystem* fs, const char* name)
{
#if defined(BAREMETALPI)
    dbg("fsMDate %s\n", name);
    // TODO BAREMETALPI
    return 0;
#else
    struct tic_stat_struct s;

    const fsString* pathString = utf8ToString(fsGetFilePath(fs, name));
    s32 ret = tic_stat(pathString, &s);
    freeString(pathString);

    if(ret == 0 && S_ISREG(s.st_mode))
    {
        return s.st_mtime;
    }

    return 0;
#endif
}

bool fsSaveFile(FileSystem* fs, const char* name, const void* data, size_t size, bool overwrite)
{
    if(!overwrite)
    {
        if(fsExistsFile(fs, name))
            return false;
    }

    return fsWriteFile(fsGetFilePath(fs, name), data, size);
}

bool fsSaveRootFile(FileSystem* fs, const char* name, const void* data, size_t size, bool overwrite)
{
    char path[TICNAME_MAX];
    strcpy(path, fs->work);
    fsHomeDir(fs);

    bool ret = fsSaveFile(fs, name, data, size, overwrite);

    strcpy(fs->work, path);

    return ret;
}

typedef struct
{
    const char* name;
    char hash[TICNAME_MAX];

} LoadPublicCartData;

static bool onLoadPublicCart(const char* name, const char* info, s32 id, void* data, bool dir)
{
    LoadPublicCartData* loadPublicCartData = (LoadPublicCartData*)data;

    if(strcmp(name, loadPublicCartData->name) == 0 && info && strlen(info))
    {
        strcpy(loadPublicCartData->hash, info);
        return false;
    }

    return true;
}

void* fsLoadFileByHash(FileSystem* fs, const char* hash, s32* size)
{
#if defined(BAREMETALPI)
    // TODO BAREMETALPI
    return NULL;
#else
    char cachePath[TICNAME_MAX] = {0};
    sprintf(cachePath, TIC_CACHE "%s.tic", hash);

    {
        void* data = fsLoadRootFile(fs, cachePath, size);
        if(data) return data;
    }

    char path[TICNAME_MAX] = {0};
    sprintf(path, "/cart/%s/cart.tic", hash);
    void* data = getSystem()->httpGetSync(path, size);

    if(data)
        fsSaveRootFile(fs, cachePath, data, *size, false);

    return data;
#endif
}

void* fsLoadFile(FileSystem* fs, const char* name, s32* size)
{
#if defined(BAREMETALPI)
    dbg("fsLoadFile x %s\n", name);
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
        const char* fp = fsGetFilePath(fs, name);
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
    if(isPublic(fs))
    {
        LoadPublicCartData loadPublicCartData = 
        {
            .name = name,
            .hash = {0},
        };

        fsEnumFiles(fs, onLoadPublicCart, &loadPublicCartData);

        if(strlen(loadPublicCartData.hash))
            return fsLoadFileByHash(fs, loadPublicCartData.hash, size);
    }
    else
    {
        const fsString* pathString = utf8ToString(fsGetFilePath(fs, name));
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
    }

    return NULL;
#endif
}

void* fsLoadRootFile(FileSystem* fs, const char* name, s32* size)
{
    char path[TICNAME_MAX];
    strcpy(path, fs->work);
    fsHomeDir(fs);

    void* ret = fsLoadFile(fs, name, size);

    strcpy(fs->work, path);

    return ret;
}

void fsMakeDir(FileSystem* fs, const char* name)
{
    makeDir(fsGetFilePath(fs, name));
}

void fsOpenWorkingFolder(FileSystem* fs)
{
    const char* path = fsGetFilePath(fs, "");

    if(isPublic(fs))
        path = fs->dir;

    getSystem()->openSystemPath(path);
}

FileSystem* createFileSystem(const char* path)
{
    FileSystem* fs = (FileSystem*)malloc(sizeof(FileSystem));
    memset(fs, 0, sizeof(FileSystem));

    strcpy(fs->dir, path);

    return fs;
}
