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

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

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

static const char* PublicDir = PUBLIC_DIR;

struct FileSystem
{
	char dir[FILENAME_MAX];
	char work[FILENAME_MAX];
};

static const char* getFilePath(FileSystem* fs, const char* name)
{
	static char path[FILENAME_MAX] = {0};

	strcpy(path, fs->dir);

	if(strlen(fs->work))
	{
		strcat(path, fs->work);
		strcat(path, "/");
	}

	strcat(path, name);

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

#if !defined(__EMSCRIPTEN__)

static bool isRoot(FileSystem* fs)
{
	return strlen(fs->work) == 0;
}

#endif

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
	fsString* wstr = malloc(FILENAME_MAX * sizeof(fsString));

	MultiByteToWideChar(CP_UTF8, 0, str, FILENAME_MAX, wstr, FILENAME_MAX);

	return wstr;
}

static const char* stringToUtf8(const fsString* wstr)
{
	char* str = malloc(FILENAME_MAX * sizeof(char));

	WideCharToMultiByte(CP_UTF8, 0, wstr, FILENAME_MAX, str, FILENAME_MAX, 0, 0);

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

#if !defined(__EMSCRIPTEN__)

typedef struct
{
	ListCallback callback;
	void* data;
} NetDirData;

static lua_State* netLuaInit(u8* buffer, s32 size)
{
    if (buffer && size)
    {
        lua_State* lua = luaL_newstate();

        if(lua)
        {
            if(luaL_loadstring(lua, (char*)buffer) == LUA_OK && lua_pcall(lua, 0, LUA_MULTRET, 0) == LUA_OK)
                return lua;

            else lua_close(lua);
        }
    }

    return NULL;
}

static void onDirResponse(u8* buffer, s32 size, void* data)
{
	NetDirData* netDirData = (NetDirData*)data;

	lua_State* lua = netLuaInit(buffer, size);

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

					char hash[FILENAME_MAX] = {0};
					char name[FILENAME_MAX] = {0};

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

static void netDirRequest(const char* path, ListCallback callback, void* data)
{
	char request[FILENAME_MAX] = {'\0'};
	sprintf(request, "/api?fn=dir&path=%s", path);

	s32 size = 0;
	void* buffer = getSystem()->getUrlRequest(request, &size);

	NetDirData netDirData = {callback, data};
	onDirResponse(buffer, size, &netDirData);
}

#endif

static void enumFiles(FileSystem* fs, const char* path, ListCallback callback, void* data, bool folder)
{
	TIC_DIR *dir = NULL;
	struct tic_dirent* ent = NULL;

	const fsString* pathString = utf8ToString(path);

	if ((dir = tic_opendir(pathString)) != NULL)
	{
		fsString fullPath[FILENAME_MAX];
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
}

void fsEnumFiles(FileSystem* fs, ListCallback callback, void* data)
{
#if !defined(__EMSCRIPTEN__)

	if(isRoot(fs) && !callback(PublicDir, NULL, 0, data, true))return;

	if(isPublic(fs))
	{
		netDirRequest(fs->work + sizeof(TIC_HOST), callback, data);
		return;
	}

#endif

	const char* path = getFilePath(fs, "");

	enumFiles(fs, path, callback, data, true);
	enumFiles(fs, path, callback, data, false);
}

bool fsDeleteDir(FileSystem* fs, const char* name)
{
#if defined(__TIC_WINRT__) || defined(__TIC_WINDOWS__)
	const char* path = getFilePath(fs, name);

	const fsString* pathString = utf8ToString(path);
	bool result = tic_rmdir(pathString);
	freeString(pathString);

#else
	bool result = rmdir(getFilePath(fs, name));
#endif

#if defined(__EMSCRIPTEN__)
	EM_ASM(FS.syncfs(function(){}));
#endif	

	return result;
}

bool fsDeleteFile(FileSystem* fs, const char* name)
{
	const char* path = getFilePath(fs, name);

	const fsString* pathString = utf8ToString(path);
	bool result = tic_remove(pathString);
	freeString(pathString);

#if defined(__EMSCRIPTEN__)
	EM_ASM(FS.syncfs(function(){}));
#endif	

	return result;
}

typedef struct
{
	FileSystem* fs;
	AddCallback callback;
	void* data;
} AddFileData;

static void onAddFile(const char* name, const u8* buffer, s32 size, void* data, u32 mode)
{
	AddFileData* addFileData = (AddFileData*)data;
	FileSystem* fs = addFileData->fs;

	if(name)
	{
		const char* destname = getFilePath(fs, name);

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
			const char* path = getFilePath(fs, name);

			const fsString* pathString = utf8ToString(path);
			FILE* dest = tic_fopen(pathString, _S("wb"));
			freeString(pathString);

			if (dest)
			{
				fwrite(buffer, 1, size, dest);
				fclose(dest);

#if !defined(__TIC_WINRT__) && !defined(__TIC_WINDOWS__)
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

#if defined(__TIC_WINRT__) || defined(__TIC_WINDOWS__)
	return 0;
#else
	const char* path = getFilePath(fs, name);
	mode_t mode = 0;
	struct stat s;
	if(stat(path, &s) == 0)
		mode = s.st_mode;

	return mode;
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

#if !defined(__EMSCRIPTEN__)
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
#endif

bool fsIsDir(FileSystem* fs, const char* name)
{
	if(*name == '.') return false;

#if !defined(__EMSCRIPTEN__)
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
#endif

	const char* path = getFilePath(fs, name);
	struct tic_stat_struct s;
	const fsString* pathString = utf8ToString(path);
	bool ret = tic_stat(pathString, &s) == 0 && S_ISDIR(s.st_mode);
	freeString(pathString);

	return ret;
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
}

bool fsCopyFile(const char* src, const char* dst)
{
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
}

void* fsReadFile(const char* path, s32* size)
{
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
}

static void makeDir(const char* name)
{
	const fsString* pathString = utf8ToString(name);
	tic_mkdir(pathString);
	freeString(pathString);

#if defined(__EMSCRIPTEN__)
	EM_ASM(FS.syncfs(function(){}));
#endif
}

static void fsFullname(const char *path, char *fullname)
{
#if defined(__TIC_WINDOWS__) || defined(__TIC_WINRT__)
	static wchar_t wpath[FILENAME_MAX];

	const fsString* pathString = utf8ToString(path);
	GetFullPathNameW(pathString, sizeof(wpath), wpath, NULL);
	freeString(pathString);

	const char* res = stringToUtf8(wpath);

#else

	const char* res = realpath(path, NULL);

#endif

	strcpy(fullname, res);
	free((void*)res);
}

void fsFilename(const char *path, char* out)
{
	char full[FILENAME_MAX];
	fsFullname(path, full);

	char base[FILENAME_MAX];
	fsBasename(path, base);

	strcpy(out, full + strlen(base));
}

void fsBasename(const char *path, char* out)
{
	char* result = NULL;

#if defined(__TIC_WINDOWS__) || defined(__TIC_WINRT__)
#define SEP "\\"
#else
#define SEP "/"
#endif

	{
		char full[FILENAME_MAX];
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
	}

	if(result && result[strlen(result)-1] != SEP[0])
		strcat(result, SEP);

	strcpy(out, result);
}

bool fsExists(const char* name)
{
	struct tic_stat_struct s;

	const fsString* pathString = utf8ToString(name);
	bool ret = tic_stat(pathString, &s) == 0;
	freeString(pathString);

	return ret;
}

bool fsExistsFile(FileSystem* fs, const char* name)
{
	return fsExists(getFilePath(fs, name));
}

u64 fsMDate(FileSystem* fs, const char* name)
{
	struct tic_stat_struct s;

	const fsString* pathString = utf8ToString(getFilePath(fs, name));
	s32 ret = tic_stat(pathString, &s);
	freeString(pathString);

	if(ret == 0 && S_ISREG(s.st_mode))
	{
		return s.st_mtime;
	}

	return 0;
}

bool fsSaveFile(FileSystem* fs, const char* name, const void* data, size_t size, bool overwrite)
{
	if(!overwrite)
	{
		if(fsExistsFile(fs, name))
			return false;
	}

	return fsWriteFile(getFilePath(fs, name), data, size);
}

bool fsSaveRootFile(FileSystem* fs, const char* name, const void* data, size_t size, bool overwrite)
{
	char path[FILENAME_MAX];
	strcpy(path, fs->work);
	fsHomeDir(fs);

	bool ret = fsSaveFile(fs, name, data, size, overwrite);

	strcpy(fs->work, path);

	return ret;
}

typedef struct
{
	const char* name;
	char hash[FILENAME_MAX];

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
	char cachePath[FILENAME_MAX] = {0};
	sprintf(cachePath, TIC_CACHE "%s.tic", hash);

	{
		void* data = fsLoadRootFile(fs, cachePath, size);
		if(data) return data;
	}

	char path[FILENAME_MAX] = {0};
	sprintf(path, "/cart/%s/cart.tic", hash);
	void* data = getSystem()->getUrlRequest(path, size);

	if(data)
		fsSaveRootFile(fs, cachePath, data, *size, false);

	return data;
}

void* fsLoadFile(FileSystem* fs, const char* name, s32* size)
{
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
		const fsString* pathString = utf8ToString(getFilePath(fs, name));
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
}

void* fsLoadRootFile(FileSystem* fs, const char* name, s32* size)
{
	char path[FILENAME_MAX];
	strcpy(path, fs->work);
	fsHomeDir(fs);

	void* ret = fsLoadFile(fs, name, size);

	strcpy(fs->work, path);

	return ret;
}

void fsMakeDir(FileSystem* fs, const char* name)
{
	makeDir(getFilePath(fs, name));
}

void fsOpenWorkingFolder(FileSystem* fs)
{
	const char* path = getFilePath(fs, "");

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