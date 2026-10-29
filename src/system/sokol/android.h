#pragma once

// The Android hooks the sokol layer needs and sokol_app does not make: the app
// folder, net's JavaVM, the system clipboard, URL opening and the window's insets.

#if defined(__ANDROID__)

#include <stdbool.h>

const char* android_app_folder(void);
void* android_java_vm(void);
void android_fullscreen(void);
void android_insets(float* top, float* right, float* bottom, float* left);
void android_open_url(const char* url);
bool android_clipboard_has(void);
char* android_clipboard_get(void);
void android_clipboard_free(char* text);
void android_clipboard_set(const char* text);

#endif
