// The Android side of the sokol layer: JNI calls the platform needs and
// sokol_app does not make, all through the activity sokol_app hands out.

#include "android.h"

#if defined(__ANDROID__)

#include "defines.h"
#include "studio/system.h"

#include <android/native_activity.h>
#include <sokol_app.h>

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The sokol loop runs on a thread of its own, where the activity's env does
// not belong; every call attaches it and releases its references in a frame.
static JNIEnv* android_env(void)
{
    const ANativeActivity* activity = sapp_android_get_native_activity();

    if(!activity || !activity->vm)
        return NULL;

    JNIEnv* env = NULL;

    if((*activity->vm)->GetEnv(activity->vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK)
        if((*activity->vm)->AttachCurrentThread(activity->vm, &env, NULL) != JNI_OK)
            return NULL;

    return env;
}

static JNIEnv* android_begin(void)
{
    JNIEnv* env = android_env();

    if(env && (*env)->PushLocalFrame(env, 32) != JNI_OK)
        return NULL;

    return env;
}

static void android_end(JNIEnv* env)
{
    if((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);

    (*env)->PopLocalFrame(env, NULL);
}

static jobject android_activity(void)
{
    const ANativeActivity* activity = sapp_android_get_native_activity();
    return activity ? activity->clazz : NULL;
}

// A JNI lookup that fails throws; the next call on the thread aborts, so the
// exception is cleared here and the caller sees a null result instead.
static bool android_failed(JNIEnv* env)
{
    if((*env)->ExceptionCheck(env))
    {
        (*env)->ExceptionClear(env);
        return true;
    }

    return false;
}

static int android_sdk(JNIEnv* env)
{
    jclass version = (*env)->FindClass(env, "android/os/Build$VERSION");

    if(android_failed(env) || !version)
        return 0;

    jfieldID field = (*env)->GetStaticFieldID(env, version, "SDK_INT", "I");

    if(android_failed(env) || !field)
        return 0;

    return (*env)->GetStaticIntField(env, version, field);
}

static jobject android_decor(JNIEnv* env)
{
    jobject activity = android_activity();

    if(!activity)
        return NULL;

    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID getWindow = (*env)->GetMethodID(env, activityClass, "getWindow", "()Landroid/view/Window;");

    if(!getWindow)
        return NULL;

    jobject window = (*env)->CallObjectMethod(env, activity, getWindow);

    if(!window)
        return NULL;

    jclass windowClass = (*env)->GetObjectClass(env, window);
    jmethodID getDecor = (*env)->GetMethodID(env, windowClass, "getDecorView", "()Landroid/view/View;");

    return getDecor ? (*env)->CallObjectMethod(env, window, getDecor) : NULL;
}

static float android_method_int(JNIEnv* env, jobject values, jclass class, const char* name)
{
    jmethodID method = (*env)->GetMethodID(env, class, name, "()I");

    if(android_failed(env) || !method)
        return 0.0f;

    return (float)(*env)->CallIntMethod(env, values, method);
}

// Android 11's Insets carries the values in public fields; the getters arrive later.
static float android_field_int(JNIEnv* env, jobject values, jclass class, const char* name)
{
    jfieldID field = (*env)->GetFieldID(env, class, name, "I");

    if(android_failed(env) || !field)
        return 0.0f;

    return (float)(*env)->GetIntField(env, values, field);
}

const char* android_app_folder(void)
{
    static char folder[TICNAME_MAX];
    static bool known = false;

    if(known)
        return folder;

    JNIEnv* env = android_begin();

    if(!env)
        return NULL;

    jobject activity = android_activity();

    if(!activity)
    {
        android_end(env);
        return NULL;
    }

    jclass context = (*env)->GetObjectClass(env, activity);
    jobject dir = NULL;

    if(context)
    {
        // The gradle app's folder: SDL_AndroidGetExternalStoragePath() is this.
        jmethodID external = (*env)->GetMethodID(env, context, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");

        if(external)
            dir = (*env)->CallObjectMethod(env, activity, external, NULL);

        if(!dir)
        {
            jmethodID internal = (*env)->GetMethodID(env, context, "getFilesDir", "()Ljava/io/File;");

            if(internal)
                dir = (*env)->CallObjectMethod(env, activity, internal);
        }
    }

    if(dir)
    {
        jclass file = (*env)->GetObjectClass(env, dir);
        jmethodID path = (*env)->GetMethodID(env, file, "getAbsolutePath", "()Ljava/lang/String;");

        if(path)
        {
            jstring string = (*env)->CallObjectMethod(env, dir, path);
            const char* utf = string ? (*env)->GetStringUTFChars(env, string, NULL) : NULL;

            if(utf)
            {
                snprintf(folder, sizeof folder, "%s/" TIC_NAME "/", utf);
                known = true;
                (*env)->ReleaseStringUTFChars(env, string, utf);
            }
        }
    }

    android_end(env);

    return known ? folder : NULL;
}

void* android_java_vm(void)
{
    const ANativeActivity* activity = sapp_android_get_native_activity();
    return activity ? activity->vm : NULL;
}

void android_open_url(const char* url)
{
    JNIEnv* env = android_begin();

    if(!env)
        return;

    jobject activity = android_activity();
    jclass uriClass = (*env)->FindClass(env, "android/net/Uri");
    jclass intentClass = (*env)->FindClass(env, "android/content/Intent");

    if(activity && uriClass && intentClass)
    {
        jmethodID parse = (*env)->GetStaticMethodID(env, uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
        jmethodID ctor = (*env)->GetMethodID(env, intentClass, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");

        if(parse && ctor)
        {
            jstring address = (*env)->NewStringUTF(env, url);
            jobject uri = (*env)->CallStaticObjectMethod(env, uriClass, parse, address);
            jstring action = (*env)->NewStringUTF(env, "android.intent.action.VIEW");
            jobject intent = (*env)->NewObject(env, intentClass, ctor, action, uri);

            if(intent)
            {
                jclass activityClass = (*env)->GetObjectClass(env, activity);
                jmethodID start = (*env)->GetMethodID(env, activityClass, "startActivity", "(Landroid/content/Intent;)V");

                if(start)
                    (*env)->CallVoidMethod(env, activity, start, intent);
            }
        }
    }

    android_end(env);
}

char* android_clipboard_get(void)
{
    char* copy = NULL;
    JNIEnv* env = android_begin();

    if(!env)
        return NULL;

    jobject activity = android_activity();

    if(!activity)
    {
        android_end(env);
        return NULL;
    }

    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID service = (*env)->GetMethodID(env, activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring name = (*env)->NewStringUTF(env, "clipboard");
    jobject manager = service ? (*env)->CallObjectMethod(env, activity, service, name) : NULL;

    if(manager)
    {
        jclass managerClass = (*env)->GetObjectClass(env, manager);
        jmethodID getClip = (*env)->GetMethodID(env, managerClass, "getPrimaryClip", "()Landroid/content/ClipData;");
        jobject clip = getClip ? (*env)->CallObjectMethod(env, manager, getClip) : NULL;

        if(clip)
        {
            jclass clipClass = (*env)->GetObjectClass(env, clip);
            jmethodID count = (*env)->GetMethodID(env, clipClass, "getItemCount", "()I");

            if(count && (*env)->CallIntMethod(env, clip, count) > 0)
            {
                jmethodID itemAt = (*env)->GetMethodID(env, clipClass, "getItemAt", "(I)Landroid/content/ClipData$Item;");
                jobject item = itemAt ? (*env)->CallObjectMethod(env, clip, itemAt, 0) : NULL;

                if(item)
                {
                    jclass itemClass = (*env)->GetObjectClass(env, item);
                    jmethodID getText = (*env)->GetMethodID(env, itemClass, "getText", "()Ljava/lang/CharSequence;");
                    jobject text = getText ? (*env)->CallObjectMethod(env, item, getText) : NULL;

                    if(text)
                    {
                        jclass textClass = (*env)->GetObjectClass(env, text);
                        jmethodID toString = (*env)->GetMethodID(env, textClass, "toString", "()Ljava/lang/String;");
                        jstring string = toString ? (*env)->CallObjectMethod(env, text, toString) : NULL;

                        if(string)
                        {
                            const char* utf = (*env)->GetStringUTFChars(env, string, NULL);

                            if(utf)
                            {
                                copy = strdup(utf);
                                (*env)->ReleaseStringUTFChars(env, string, utf);
                            }
                        }
                    }
                }
            }
        }
    }

    android_end(env);

    return copy;
}

bool android_clipboard_has(void)
{
    char* text = android_clipboard_get();
    const bool has = text && *text;

    android_clipboard_free(text);

    return has;
}

void android_clipboard_free(char* text)
{
    free(text);
}

void android_clipboard_set(const char* text)
{
    JNIEnv* env = android_begin();

    if(!env)
        return;

    jobject activity = android_activity();

    if(!activity)
    {
        android_end(env);
        return;
    }

    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID service = (*env)->GetMethodID(env, activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring name = (*env)->NewStringUTF(env, "clipboard");
    jobject manager = service ? (*env)->CallObjectMethod(env, activity, service, name) : NULL;
    jclass clipClass = (*env)->FindClass(env, "android/content/ClipData");
    jmethodID plain = clipClass ? (*env)->GetStaticMethodID(env, clipClass, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;") : NULL;

    if(manager && plain)
    {
        jstring label = (*env)->NewStringUTF(env, TIC_NAME);
        jstring value = (*env)->NewStringUTF(env, text);
        jobject clip = (*env)->CallStaticObjectMethod(env, clipClass, plain, label, value);

        if(clip)
        {
            jclass managerClass = (*env)->GetObjectClass(env, manager);
            jmethodID set = (*env)->GetMethodID(env, managerClass, "setPrimaryClip", "(Landroid/content/ClipData;)V");

            if(set)
                (*env)->CallVoidMethod(env, manager, set, clip);
        }
    }

    android_end(env);
}

void android_fullscreen(void)
{
    JNIEnv* env = android_begin();

    if(!env)
        return;

    jobject decor = android_decor(env);

    if(decor)
    {
        // FULLSCREEN | HIDE_NAVIGATION | IMMERSIVE_STICKY | the three layout
        // flags; deprecated in API 30, and still the only call that can be made
        // before the view is attached.
        jclass viewClass = (*env)->GetObjectClass(env, decor);
        jmethodID set = (*env)->GetMethodID(env, viewClass, "setSystemUiVisibility", "(I)V");

        if(set)
            (*env)->CallVoidMethod(env, decor, set, 4 | 2 | 4096 | 256 | 512 | 1024);
    }

    android_end(env);
}

// sokol_app handles sapp_quit() on every platform but Android, whose loop
// never looks at quit_ordered. The task is removed as well, or the app stays
// in the recents list and starts again from there.
void android_finish(void)
{
    JNIEnv* env = android_begin();

    if(!env)
        return;

    jobject activity = android_activity();

    if(activity)
    {
        jclass class = (*env)->GetObjectClass(env, activity);
        jmethodID finish = (*env)->GetMethodID(env, class, "finishAndRemoveTask", "()V");

        if(android_failed(env))
            finish = NULL;

        if(finish)
            (*env)->CallVoidMethod(env, activity, finish);
    }

    android_end(env);
}

void android_insets(float* top, float* right, float* bottom, float* left)
{
    *top = *right = *bottom = *left = 0.0f;

    JNIEnv* env = android_begin();

    if(!env)
        return;

    jobject decor = android_decor(env);

    if(decor)
    {
        jclass viewClass = (*env)->GetObjectClass(env, decor);
        jmethodID root = (*env)->GetMethodID(env, viewClass, "getRootWindowInsets", "()Landroid/view/WindowInsets;");

        if(android_failed(env))
            root = NULL;

        jobject insets = root ? (*env)->CallObjectMethod(env, decor, root) : NULL;

        if(insets)
        {
            jclass insetsClass = (*env)->GetObjectClass(env, insets);

            if(android_sdk(env) >= 30)
            {
                jclass typeClass = (*env)->FindClass(env, "android/view/WindowInsets$Type");

                if(android_failed(env))
                    typeClass = NULL;

                jmethodID bars = typeClass ? (*env)->GetStaticMethodID(env, typeClass, "systemBars", "()I") : NULL;

                if(android_failed(env))
                    bars = NULL;

                jmethodID cutout = typeClass ? (*env)->GetStaticMethodID(env, typeClass, "displayCutout", "()I") : NULL;

                if(android_failed(env))
                    cutout = NULL;

                jmethodID get = (*env)->GetMethodID(env, insetsClass, "getInsets", "(I)Landroid/graphics/Insets;");

                if(android_failed(env))
                    get = NULL;

                if(bars && cutout && get)
                {
                    const jint mask = (*env)->CallStaticIntMethod(env, typeClass, bars) | (*env)->CallStaticIntMethod(env, typeClass, cutout);
                    jobject values = (*env)->CallObjectMethod(env, insets, get, mask);

                    if(values)
                    {
                        jclass valuesClass = (*env)->GetObjectClass(env, values);

                        *top = android_field_int(env, values, valuesClass, "top");
                        *right = android_field_int(env, values, valuesClass, "right");
                        *bottom = android_field_int(env, values, valuesClass, "bottom");
                        *left = android_field_int(env, values, valuesClass, "left");
                    }
                }
            }
            else
            {
                *top = android_method_int(env, insets, insetsClass, "getSystemWindowInsetTop");
                *right = android_method_int(env, insets, insetsClass, "getSystemWindowInsetRight");
                *bottom = android_method_int(env, insets, insetsClass, "getSystemWindowInsetBottom");
                *left = android_method_int(env, insets, insetsClass, "getSystemWindowInsetLeft");
            }
        }
    }

    android_end(env);
}

#endif
