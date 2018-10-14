#pragma once

#include "ticapi.h"
#include "ext/file_dialog.h"

typedef struct
{
	void	(*setClipboardText)(const char* text);
	bool	(*hasClipboardText)();
	char* 	(*getClipboardText)();
	void 	(*freeClipboardText)(const char* text);

	u64 	(*getPerformanceCounter)();
	u64 	(*getPerformanceFrequency)();

	void* (*getUrlRequest)(const char* url, s32* size);

	void (*fileDialogLoad)(file_dialog_load_callback callback, void* data);
	void (*fileDialogSave)(file_dialog_save_callback callback, const char* name, const u8* buffer, size_t size, void* data, u32 mode);

	void (*goFullscreen)();
	void (*showMessageBox)(const char* title, const char* message);
	void (*setWindowTitle)(const char* title);

	void (*openSystemPath)(const char* path);
	
	void (*preseed)();
	void (*poll)();

	void (*updateConfig)();

} System;

typedef struct
{
	struct
	{
		struct
		{
			s32 arrow;
			s32 hand;
			s32 ibeam;

			bool pixelPerfect;
		} cursor;

		struct
		{
			tic_code_theme syntax;

			u8 bg;
			u8 select;
			u8 cursor;
			bool shadow;
			bool altFont;

		} code;

		struct
		{
			struct
			{
				u8 alpha;
			} touch;

		} gamepad;

	} theme;

	s32 gifScale;
	s32 gifLength;
	
	bool checkNewVersion;
	bool noSound;
	bool showSync;
	bool crtMonitor;

	const char* crtShader;
	const tic_cartridge* cart;

	s32 uiScale;

} StudioConfig;

typedef struct
{
	tic_mem* tic;
	bool quit;

	void (*tick)();
	void (*exit)();
	void (*close)();
	void (*updateProject)();
	const StudioConfig* (*config)();

	// TODO: remove this method, system has to know nothing about current mode
	bool (*isGamepadMode)();
} Studio;

TIC80_API Studio* studioInit(s32 argc, char **argv, s32 samplerate, const char* appFolder, System* system);
