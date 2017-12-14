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
#include "ext/file_dialog.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(__WINRT__) || defined(__WINDOWS__)
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

	Net* net;
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

#if defined(__WINDOWS__)
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

#if defined(__WINDOWS__) || defined(__WINRT__)

#define UTF8ToString(S) (wchar_t *)SDL_iconv_string("UTF-16LE", "UTF-8", (char *)(S), SDL_strlen(S)+1)
#define StringToUTF8(S) SDL_iconv_string("UTF-8", "UTF-16LE", (char *)(S), (SDL_wcslen(S)+1)*sizeof(wchar_t))

FILE* _wfopen(const wchar_t *, const wchar_t *);
int _wremove(const wchar_t *);

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
#define tic_system _wsystem

#else

#define UTF8ToString(S) (S)
#define StringToUTF8(S) (S)

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
#define tic_system system

#endif

void fsEnumFiles(FileSystem* fs, ListCallback callback, void* data)
{
#if !defined(__EMSCRIPTEN__)

	if(isRoot(fs) && !callback(PublicDir, NULL, 0, data, true))return;

	if(isPublic(fs))
	{
		netDirRequest(fs->net, fs->work + sizeof(TIC_HOST), callback, data);
		return;
	}

#endif

	TIC_DIR *dir = NULL;
	struct tic_dirent* ent = NULL;

	const char* path = getFilePath(fs, "");

	if ((dir = tic_opendir(UTF8ToString(path))) != NULL)
	{
		while ((ent = tic_readdir(dir)) != NULL)
		{
			if (ent->d_type == DT_DIR && *ent->d_name != '.')
			{
				if (!callback(StringToUTF8(ent->d_name), NULL, 0, data, true))break;
			}
		}

		tic_closedir(dir);
	}

	if ((dir = tic_opendir(UTF8ToString(path))) != NULL)
	{
		while ((ent = tic_readdir(dir)) != NULL)
		{
			if (ent->d_type == DT_REG)
			{
				if (!callback(StringToUTF8(ent->d_name), NULL, 0, data, false))break;
			}
		}

		tic_closedir(dir);
	}
}

bool fsDeleteDir(FileSystem* fs, const char* name)
{
#if defined(__WINRT__) || defined(__WINDOWS__)
	const char* path = getFilePath(fs, name);
	bool result = tic_rmdir(UTF8ToString(path));
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
	bool result = tic_remove(UTF8ToString(path));

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

		FILE* file = tic_fopen(UTF8ToString(destname), UTF8ToString("rb"));
		if(file)
		{
			fclose(file);

			addFileData->callback(name, FS_FILE_EXISTS, addFileData->data);
		}
		else
		{
			const char* path = getFilePath(fs, name);
			FILE* dest = tic_fopen(UTF8ToString(path), UTF8ToString("wb"));

			if (dest)
			{
				fwrite(buffer, 1, size, dest);
				fclose(dest);

#if !defined(__WINRT__) && !defined(__WINDOWS__)
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

	SDL_free(addFileData);
}

void fsAddFile(FileSystem* fs, AddCallback callback, void* data)
{
	AddFileData* addFileData = (AddFileData*)SDL_malloc(sizeof(AddFileData));

	*addFileData = (AddFileData) { fs, callback, data };

	file_dialog_load(&onAddFile, addFileData);
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

	SDL_free(command->buffer);
	SDL_free(command);
}

static u32 fsGetMode(FileSystem* fs, const char* name)
{

#if defined(__WINRT__) || defined(__WINDOWS__)
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

const char* fsGetDir(FileSystem* fs)
{
	return fs->work;
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
	return tic_stat(UTF8ToString(path), &s) == 0 && S_ISDIR(s.st_mode);
}

void fsGetFileData(GetCallback callback, const char* name, void* buffer, size_t size, u32 mode, void* data)
{
	GetFileData* command = (GetFileData*)SDL_malloc(sizeof(GetFileData));
	*command = (GetFileData) {callback, data, buffer};

	file_dialog_save(onGetFile, name, buffer, size, command, mode);
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

	SDL_free(command);
}

void fsOpenFileData(OpenCallback callback, void* data)
{
	OpenFileData* command = (OpenFileData*)SDL_malloc(sizeof(OpenFileData));

	*command = (OpenFileData){callback, data};

	file_dialog_load(onOpenFileData, command);
}

void fsGetFile(FileSystem* fs, GetCallback callback, const char* name, void* data)
{
	s32 size = 0;
	void* buffer = fsLoadFile(fs, name, &size);

	if(buffer)
	{
		GetFileData* command = (GetFileData*)SDL_malloc(sizeof(GetFileData));
		*command = (GetFileData) {callback, data, buffer};

		s32 mode = fsGetMode(fs, name);
		file_dialog_save(onGetFile, name, buffer, size, command, mode);
	}
	else callback(FS_FILE_NOT_DOWNLOADED, data);
}

bool fsWriteFile(const char* name, const void* buffer, s32 size)
{
	FILE* file = tic_fopen(UTF8ToString(name), UTF8ToString("wb"));

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
		FILE* file = tic_fopen(UTF8ToString(src), UTF8ToString("rb"));
		if(file)
		{
			fseek(file, 0, SEEK_END);
			size = ftell(file);
			fseek(file, 0, SEEK_SET);

			if((buffer = SDL_malloc(size)) && fread(buffer, size, 1, file)) {}

			fclose(file);
		}		
	}

	if(buffer)
	{
		FILE* file = tic_fopen(UTF8ToString(dst), UTF8ToString("wb"));

		if(file)
		{
			fwrite(buffer, 1, size, file);
			fclose(file);

			done = true;
		}

		SDL_free(buffer);
	}

	return done;
}

void* fsReadFile(const char* path, s32* size)
{
	FILE* file = tic_fopen(UTF8ToString(path), UTF8ToString("rb"));
	void* buffer = NULL;

	if(file)
	{

		fseek(file, 0, SEEK_END);
		*size = ftell(file);
		fseek(file, 0, SEEK_SET);

		if((buffer = SDL_malloc(*size)) && fread(buffer, *size, 1, file)) {}

		fclose(file);
	}

	return buffer;
}

static void makeDir(const char* name)
{
	tic_mkdir(UTF8ToString(name));

#if defined(__EMSCRIPTEN__)
	EM_ASM(FS.syncfs(function(){}));
#endif
}

const char* fsFilename(const char *path)
{
	const char* full = fsFullname(path);
	const char* base = fsBasename(path);

	return base ? full + strlen(fsBasename(path)) : NULL;
}

const char* fsFullname(const char *path)
{
	char* result = NULL;

#if defined(__WINDOWS__) || defined(__WINRT__)
	static wchar_t wpath[FILENAME_MAX];
	GetFullPathNameW(UTF8ToString(path), sizeof(wpath), wpath, NULL);

	result = StringToUTF8(wpath);
#else

	result = realpath(path, NULL);

#endif

	return result;
}

const char* fsBasename(const char *path)
{
	char* result = NULL;

#if defined(__WINDOWS__) || defined(__WINRT__)
#define SEP "\\"
#else
#define SEP "/"
#endif

	{
		char* full = (char*)fsFullname(path);

		struct tic_stat_struct s;
		if(tic_stat(UTF8ToString(full), &s) == 0)
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

	return result;
}

bool fsExists(const char* name)
{
	struct tic_stat_struct s;
	return tic_stat(UTF8ToString(name), &s) == 0;
}

bool fsExistsFile(FileSystem* fs, const char* name)
{
	return fsExists(getFilePath(fs, name));
}

u64 fsMDate(FileSystem* fs, const char* name)
{
	struct tic_stat_struct s;

	if(tic_stat(UTF8ToString(getFilePath(fs, name)), &s) == 0 && S_ISREG(s.st_mode))
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
		{
			char cachePath[FILENAME_MAX] = {0};
			sprintf(cachePath, TIC_CACHE "%s.tic", loadPublicCartData.hash);

			{
				void* data = fsLoadRootFile(fs, cachePath, size);
				if(data) return data;
			}

			char path[FILENAME_MAX] = {0};
			sprintf(path, "/cart/%s/cart.tic", loadPublicCartData.hash);
			void* data = netGetRequest(fs->net, path, size);

			if(data)
				fsSaveRootFile(fs, cachePath, data, *size, false);

			return data;
		}
	}
	else
	{
		FILE* file = tic_fopen(UTF8ToString(getFilePath(fs, name)), UTF8ToString("rb"));
		void* ptr = NULL;

		if(file)
		{
			fseek(file, 0, SEEK_END);
			*size = ftell(file);
			fseek(file, 0, SEEK_SET);

			u8* buffer = SDL_malloc(*size);

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

#if defined(__WINDOWS__) || defined(__LINUX__) || defined(__MACOSX__)

s32 fsOpenSystemPath(FileSystem* fs, const char* path)
{
	char command[FILENAME_MAX];

#if defined(__WINDOWS__)

	sprintf(command, "explorer \"%s\"", path);

#elif defined(__LINUX__)

	sprintf(command, "xdg-open \"%s\"", path);

#elif defined(__MACOSX__)

	sprintf(command, "open \"%s\"", path);

#endif

	return tic_system(UTF8ToString(command));
}

#else

s32 fsOpenSystemPath(FileSystem* fs, const char* path)
{
	return 0;
}

#endif

void fsOpenWorkingFolder(FileSystem* fs)
{
	const char* path = getFilePath(fs, "");

	if(isPublic(fs))
		path = fs->dir;

	fsOpenSystemPath(fs, path);
}

void createFileSystem(const char* path, void(*callback)(FileSystem*))
{
	FileSystem* fs = (FileSystem*)SDL_malloc(sizeof(FileSystem));
	memset(fs, 0, sizeof(FileSystem));

	fs->net = createNet();

	if(path)
	{
		strcpy(fs->dir, path);
		callback(fs);
	}
	else
	{

#if defined(__EMSCRIPTEN__)

		strcpy(fs->dir, "/" TIC_PACKAGE "/" TIC_NAME "/");

#elif defined(__ANDROID__)

		strcpy(fs->dir, SDL_AndroidGetExternalStoragePath());
		const char AppFolder[] = "/" TIC_NAME "/";
		strcat(fs->dir, AppFolder);
		mkdir(fs->dir, 0700);

#else

		char* path = SDL_GetPrefPath(TIC_PACKAGE, TIC_NAME);
		strcpy(fs->dir, path);
		SDL_free(path);

#endif
		
#if defined(__EMSCRIPTEN__)
		EM_ASM_
		(
			{
				var dir = "";
				Module.Pointer_stringify($0).split("/").forEach(function(val)
				{
					if(val.length)
					{
						dir += "/" + val;
						FS.mkdir(dir);
					}
				});
				
				FS.mount(IDBFS, {}, dir);
				FS.syncfs(true, function(error)
				{
					if(error) console.log(error);
					else Runtime.dynCall('vi', $1, [$2]);
				});			
			}, fs->dir, callback, fs
		);
#else
		callback(fs);
#endif
	}
}