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

static jobject android_window(JNIEnv* env)
{
    jobject activity = android_activity();

    if(!activity)
        return NULL;

    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID getWindow = (*env)->GetMethodID(env, activityClass, "getWindow", "()Landroid/view/Window;");

    if(android_failed(env) || !getWindow)
        return NULL;

    return (*env)->CallObjectMethod(env, activity, getWindow);
}

static jobject android_decor(JNIEnv* env)
{
    jobject window = android_window(env);

    if(!window)
        return NULL;

    jclass windowClass = (*env)->GetObjectClass(env, window);
    jmethodID getDecor = (*env)->GetMethodID(env, windowClass, "getDecorView", "()Landroid/view/View;");

    return android_failed(env) || !getDecor ? NULL : (*env)->CallObjectMethod(env, window, getDecor);
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

// The `add` command: the picker activity copies the file itself, so nothing
// comes back here. False when the picker could not be started.
bool android_add_file(void)
{
    bool started = false;
    JNIEnv* env = android_begin();

    if(!env)
        return false;

    jobject activity = android_activity();

    if(activity)
    {
        jclass activityClass = (*env)->GetObjectClass(env, activity);
        jmethodID packageOf = (*env)->GetMethodID(env, activityClass, "getPackageName", "()Ljava/lang/String;");
        jmethodID start = (*env)->GetMethodID(env, activityClass, "startActivity", "(Landroid/content/Intent;)V");
        jclass intentClass = (*env)->FindClass(env, "android/content/Intent");
        jmethodID ctor = intentClass ? (*env)->GetMethodID(env, intentClass, "<init>", "()V") : NULL;
        jmethodID setClassName = intentClass ? (*env)->GetMethodID(env, intentClass, "setClassName", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;") : NULL;

        if(android_failed(env))
            packageOf = start = ctor = setClassName = NULL;

        if(packageOf && start && ctor && setClassName)
        {
            jstring package = (*env)->CallObjectMethod(env, activity, packageOf);
            jstring name = (*env)->NewStringUTF(env, "com.nesbox.tic.TICAddActivity");
            jobject intent = (*env)->NewObject(env, intentClass, ctor);

            (*env)->CallObjectMethod(env, intent, setClassName, package, name);
            (*env)->CallVoidMethod(env, activity, start, intent);
            started = !(*env)->ExceptionCheck(env);
        }
    }

    android_end(env);

    return started;
}

// A class of the app cannot be found by name from the studio's thread; the
// activity's loader is the one that knows it.
static jclass android_app_class(JNIEnv* env, jobject activity, const char* name)
{
    jclass activityClass = (*env)->GetObjectClass(env, activity);
    jmethodID loaderOf = (*env)->GetMethodID(env, activityClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = loaderOf ? (*env)->CallObjectMethod(env, activity, loaderOf) : NULL;
    jclass loaderClass = loader ? (*env)->GetObjectClass(env, loader) : NULL;
    jmethodID load = loaderClass ? (*env)->GetMethodID(env, loaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;") : NULL;
    jobject loaded = NULL;

    if(load)
    {
        jstring className = (*env)->NewStringUTF(env, name);

        if(className)
            loaded = (*env)->CallObjectMethod(env, loader, load, className);
    }

    if(android_failed(env))
        loaded = NULL;

    return (jclass)loaded;
}

// The `get` command: the file goes to Download/TIC-80 through MediaStore,
// where every file manager can see it.
bool android_get_file(const char* name, const void* data, int size)
{
    bool saved = false;
    JNIEnv* env = android_begin();

    if(!env)
        return false;

    jobject activity = android_activity();

    if(activity && data && size > 0)
    {
        jclass filesClass = android_app_class(env, activity, "com.nesbox.tic.TICFiles");
        jmethodID save = filesClass ? (*env)->GetStaticMethodID(env, filesClass, "saveToDownloads", "(Landroid/content/Context;Ljava/lang/String;[B)Z") : NULL;

        if(android_failed(env))
            save = NULL;

        if(save)
        {
            jstring fileName = (*env)->NewStringUTF(env, name);
            jbyteArray bytes = (*env)->NewByteArray(env, size);

            if(fileName && bytes)
            {
                (*env)->SetByteArrayRegion(env, bytes, 0, size, (const jbyte*)data);
                saved = (*env)->CallStaticBooleanMethod(env, filesClass, save, activity, fileName, bytes) == JNI_TRUE;
            }
        }
    }

    android_end(env);

    return saved;
}

// The cart the launch came with, copied by the activity before the native
// side starts; sokol_main makes it the studio's argument.
char* android_launch_cart(void)
{
    char* result = NULL;
    JNIEnv* env = android_begin();

    if(!env)
        return NULL;

    jobject activity = android_activity();
    jclass activityClass = activity ? android_app_class(env, activity, "com.nesbox.tic.TICActivity") : NULL;

    if(activityClass)
    {
        jmethodID take = (*env)->GetStaticMethodID(env, activityClass, "takeLaunchCart", "()Ljava/lang/String;");

        if(!android_failed(env) && take)
        {
            jstring cart = (*env)->CallStaticObjectMethod(env, activityClass, take);

            if(cart)
            {
                const char* utf = (*env)->GetStringUTFChars(env, cart, NULL);

                if(utf)
                {
                    result = strdup(utf);
                    (*env)->ReleaseStringUTFChars(env, cart, utf);
                }
            }
        }
    }

    android_end(env);

    return result;
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

// The manifest's fullscreen theme makes the window fullscreen; the SDL build
// starts windowed, and the window's own flags are what take that back. Called
// from sokol_main, which runs on the activity's main thread.
void android_windowed(void)
{
    JNIEnv* env = android_begin();

    if(!env)
        return;

    jobject window = android_window(env);

    if(window)
    {
        jclass windowClass = (*env)->GetObjectClass(env, window);
        jmethodID decorOf = (*env)->GetMethodID(env, windowClass, "getDecorView", "()Landroid/view/View;");
        jmethodID addFlags = (*env)->GetMethodID(env, windowClass, "addFlags", "(I)V");
        jmethodID clearFlags = (*env)->GetMethodID(env, windowClass, "clearFlags", "(I)V");

        if(android_failed(env))
            decorOf = addFlags = clearFlags = NULL;

        jobject decor = decorOf ? (*env)->CallObjectMethod(env, window, decorOf) : NULL;

        if(decor)
        {
            jclass viewClass = (*env)->GetObjectClass(env, decor);
            jmethodID set = (*env)->GetMethodID(env, viewClass, "setSystemUiVisibility", "(I)V");

            if(android_failed(env))
                set = NULL;

            // VISIBLE | LAYOUT_STABLE: the bars belong to the window again.
            if(set)
                (*env)->CallVoidMethod(env, decor, set, 256);
        }

        if(addFlags)
            (*env)->CallVoidMethod(env, window, addFlags, 2048); // FLAG_FORCE_NOT_FULLSCREEN

        if(clearFlags)
            (*env)->CallVoidMethod(env, window, clearFlags, 1024); // FLAG_FULLSCREEN
    }

    android_end(env);
}

// sokol_app has no cursor on Android: the icon belongs to the window's view,
// and TYPE_NULL draws nothing. The last icon is remembered so a moving mouse
// does not make a JNI call per event.
void android_pointer_show(bool show)
{
    static bool known = false;
    static bool shown = true;

    if(known && shown == show)
        return;

    JNIEnv* env = android_begin();

    if(!env)
        return;

    jobject decor = android_decor(env);

    if(decor)
    {
        jclass viewClass = (*env)->GetObjectClass(env, decor);
        jmethodID set = (*env)->GetMethodID(env, viewClass, "setPointerIcon", "(Landroid/view/PointerIcon;)V");

        if(android_failed(env))
            set = NULL;

        if(set)
        {
            jobject icon = NULL;

            if(!show)
            {
                jclass iconClass = (*env)->FindClass(env, "android/view/PointerIcon");
                jmethodID system = iconClass ? (*env)->GetStaticMethodID(env, iconClass, "getSystemIcon", "(Landroid/content/Context;I)Landroid/view/PointerIcon;") : NULL;
                jobject activity = android_activity();

                if(android_failed(env))
                    system = NULL;

                // The second argument is PointerIcon.TYPE_NULL.
                if(system && activity)
                    icon = (*env)->CallStaticObjectMethod(env, iconClass, system, activity, 0);
            }

            (*env)->CallVoidMethod(env, decor, set, icon);
            known = true;
            shown = show;
        }
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
