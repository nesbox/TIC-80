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

#include "file_dialog.h"

#include <SDL.h>

#if defined(__WINDOWS__)

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>

wchar_t* wcsrchr(const wchar_t *, wchar_t);
wchar_t* wcscpy(wchar_t *, const wchar_t *);

void file_dialog_load(file_dialog_load_callback callback, void* data)
{
	OPENFILENAMEW ofn;
	SDL_zero(ofn);

	wchar_t filename[MAX_PATH];
	memset(filename, 0, sizeof(filename));

	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = filename;
	ofn.nMaxFile = sizeof(filename);
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if(GetOpenFileNameW(&ofn))
	{
		FILE* source = _wfopen(filename, L"rb");
		if (source)
		{
			fseek(source, 0, SEEK_END);
			s32 size = ftell(source);
			fseek(source, 0, SEEK_SET);

			u8* buffer = malloc(size);

			if(buffer)
				fread(buffer, size, 1, source);

			fclose(source);

			if(buffer)
			{
				const wchar_t* basename = wcsrchr(filename, L'\\');
				const wchar_t* name = basename ? basename + 1 : filename;

				char resName[MAX_PATH];
				wcstombs(resName, name, MAX_PATH);
				callback(resName, buffer, size, data, 0);

				free(buffer);
				return;
			}
		}
	}

	callback(NULL, NULL, 0, data, 0);
}

void file_dialog_save(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode)
{
	OPENFILENAMEW ofn;
	SDL_zero(ofn);

	wchar_t filename[MAX_PATH];
	mbstowcs(filename, name, MAX_PATH);

	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = filename;
	ofn.nMaxFile = sizeof(filename);
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if(GetSaveFileNameW(&ofn))
	{
		FILE* file = _wfopen(filename, L"wb");

		if(file)
		{
			fwrite(buffer, 1, size, file);
			fclose(file);

#if !defined(__WINDOWS__)
			chmod(filename, mode);
#endif
			callback(true, data);
			return;
		}
	}

	callback(false, data);
}

#elif defined(__EMSCRIPTEN__)

#include <emscripten.h>

void file_dialog_load(file_dialog_load_callback callback, void* data)
{
	EM_ASM_
	({
		Module.showAddPopup(function(filename, rom)
		{
			if(filename == null || rom == null)
			{
				Runtime.dynCall('viiiii', $0, [0, 0, 0, $1, 0]);
			}
			else
			{
				var filePtr = Module._malloc(filename.length + 1);
				stringToUTF8(filename, filePtr, filename.length + 1);

				var dataPtr = Module._malloc(rom.length);
				writeArrayToMemory(rom, dataPtr);

				Runtime.dynCall('viiiii', $0, [filePtr, dataPtr, rom.length, $1, 0]);

				Module._free(filePtr);
				Module._free(dataPtr);
			}
		});
	}, callback, data);
}

void file_dialog_save(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode)
{
	EM_ASM_
	({
		var name = Pointer_stringify($0);
		var blob = new Blob([HEAPU8.subarray($1, $1 + $2)], {type: "application/octet-stream"});

		Module.saveAs(blob, name);
	}, name, buffer, size);

	callback(true, data);
}

#elif defined(__LINUX__)

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <sys/stat.h>

static void WaitForCleanup(void)
{
	while (gtk_events_pending())
		gtk_main_iteration();
}

void file_dialog_load(file_dialog_load_callback callback, void* data)
{
	bool done = false;

	if(gtk_init_check(NULL,NULL))
	{
		GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File", NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
			"_Cancel", GTK_RESPONSE_CANCEL,
			"_Open", GTK_RESPONSE_ACCEPT,
			NULL);

		if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT )
		{
			char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

			FILE* source = fopen(filename, "rb");
			if (source)
			{
				fseek(source, 0, SEEK_END);
				s32 size = ftell(source);
				fseek(source, 0, SEEK_SET);

				u8* buffer = malloc(size);

				if(buffer && fread(buffer, size, 1, source));

				fclose(source);

				if(buffer)
				{
					const char* basename = strrchr(filename, '/');

					mode_t mode = 0;
					{
						struct stat s;
						if(stat(filename, &s) == 0)
							mode = s.st_mode;
					}

					callback(basename ? basename + 1 : filename, buffer, size, data, mode);
					free(buffer);

					done = true;
				}
			}
		}

		WaitForCleanup();
		gtk_widget_destroy(dialog);
		WaitForCleanup();
	}

	if(!done)
		callback(NULL, NULL, 0, data, 0);
}

void file_dialog_save(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode)
{
	bool done = false;

	if(gtk_init_check(NULL,NULL))
	{
		GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File", NULL, GTK_FILE_CHOOSER_ACTION_SAVE,
			"_Cancel", GTK_RESPONSE_CANCEL,
			"_Save", GTK_RESPONSE_ACCEPT,
			NULL);

		gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);

		if(gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_ACCEPT)
		{
			char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

			FILE* file = fopen(filename, "wb");

			if(file)
			{
				fwrite(buffer, 1, size, file);
				fclose(file);

				chmod(filename, mode);

				callback(true, data);
				done = true;
			}
		}

		WaitForCleanup();
		gtk_widget_destroy(dialog);
		WaitForCleanup();
	}

	if(!done)
		callback(false, data);
}

#elif defined(__MACOSX__)

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

bool file_dialog_load_path(char* buffer);
bool file_dialog_save_path(const char* name, char* buffer);

void file_dialog_load(file_dialog_load_callback callback, void* data) 
{
	bool done = false;

	char filename[FILENAME_MAX];
	if(file_dialog_load_path(filename))
	{
		FILE* source = fopen(filename, "rb");
		if (source)
		{
			fseek(source, 0, SEEK_END);
			s32 size = ftell(source);
			fseek(source, 0, SEEK_SET);

			u8* buffer = malloc(size);

			if(buffer && fread(buffer, size, 1, source)){}

			fclose(source);

			if(buffer)
			{
				const char* basename = strrchr(filename, '/');

				mode_t mode = 0;
				{
					struct stat s;
					if(stat(filename, &s) == 0)
						mode = s.st_mode;
				}

				callback(basename ? basename + 1 : filename, buffer, size, data, mode);
				free(buffer);

				done = true;
			}
		}
	}

	if(!done)
		callback(NULL, NULL, 0, data, 0);
}

void file_dialog_save(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode) 
{
	bool done = false;

	char filename[FILENAME_MAX];
	if(file_dialog_save_path(name, filename))
	{
		FILE* file = fopen(filename, "wb");

		if(file)
		{
			fwrite(buffer, 1, size, file);
			fclose(file);

			chmod(filename, mode);

			callback(true, data);
			done = true;
		}
	}

	if(!done)
		callback(false, data);
}

#elif defined(__ANDROID__)

#include <jni.h>
#include <sys/stat.h>

void file_dialog_load(file_dialog_load_callback callback, void* data)
{
	JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
	jobject activity = (jobject)SDL_AndroidGetActivity();
	jclass clazz = (*env)->GetObjectClass(env, activity);

	jmethodID method_id = (*env)->GetMethodID(env, clazz, "loadFile", "()Ljava/lang/String;");
	jstring jPath = (jstring) (*env)->CallObjectMethod(env, activity, method_id);

	const char *filename = (*env)->GetStringUTFChars(env, jPath, NULL);

	(*env)->DeleteLocalRef(env, activity);
	(*env)->DeleteLocalRef(env, clazz);

	bool done = false;

	if(filename && strlen(filename))
	{
		FILE* source = fopen(filename, "rb");
		if (source)
		{
			fseek(source, 0, SEEK_END);
			s32 size = ftell(source);
			fseek(source, 0, SEEK_SET);

			u8* buffer = malloc(size);

			if(buffer && fread(buffer, size, 1, source)){}

			fclose(source);

			if(buffer)
			{
				const char* basename = strrchr(filename, '/');

				mode_t mode = 0;
				{
					struct stat s;
					if(stat(filename, &s) == 0)
						mode = s.st_mode;
				}

				callback(basename ? basename + 1 : filename, buffer, size, data, mode);
				free(buffer);

				done = true;
			}
		}
	}

	if(!done)
		callback(NULL, NULL, 0, data, 0);

	(*env)->ReleaseStringUTFChars(env, jPath, filename);
}

void file_dialog_save(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode) 
{
	JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
	jobject activity = (jobject)SDL_AndroidGetActivity();
	jclass clazz = (*env)->GetObjectClass(env, activity);

	jstring jName = (*env)->NewStringUTF(env, name); 
	jmethodID method_id = (*env)->GetMethodID(env, clazz, "saveFile", "(Ljava/lang/String;)Ljava/lang/String;");
	jstring jPath = (jstring) (*env)->CallObjectMethod(env, activity, method_id, jName);

	const char *filename = (*env)->GetStringUTFChars(env, jPath, NULL);

	(*env)->DeleteLocalRef(env, jName);
	(*env)->DeleteLocalRef(env, activity);
	(*env)->DeleteLocalRef(env, clazz);

	bool done = false;

	if(filename && strlen(filename))
	{
		FILE* file = fopen(filename, "wb");

		if(file)
		{
			fwrite(buffer, 1, size, file);
			fclose(file);

			chmod(filename, mode);

			callback(true, data);
			done = true;
		}
	}

	if(!done)
		callback(false, data);

	(*env)->ReleaseStringUTFChars(env, jPath, filename);
}

#else

void file_dialog_load(file_dialog_load_callback callback, void* data) {}
void file_dialog_save(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode) {}

#endif
