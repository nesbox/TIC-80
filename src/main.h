#pragma once

#include "tic.h"
#include "ext/file_dialog.h"

typedef struct
{
	void	(*setClipboardText)(const char* text);
	bool	(*hasClipboardText)();
	char* 	(*getClipboardText)();
	u64 	(*getPerformanceCounter)();
	u64 	(*getPerformanceFrequency)();

	void* (*getUrlRequest)(const char* url, s32* size);

	void (*file_dialog_load)(file_dialog_load_callback callback, void* data);
	void (*file_dialog_save)(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode);

	void (*goFullscreen)();
	void (*showMessageBox)(const char* title, const char* message);
	void (*setWindowTitle)(const char* title);

	void (*openSystemPath)(const char* path);

} System;

System* getSystem();
