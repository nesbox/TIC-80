#ifndef SOKOL_SYS_INCLUDED
/*
    sokol_sys.h -- cross-platform system folder

    Project URL: https://github.com/floooh/sokol

    Do this:
        #define SOKOL_IMPL
    before you include this file in *one* C or C++ file to create the
    implementation.
*/

#define SOKOL_SYS_INCLUDED (1)

#ifndef SOKOL_API_DECL
#if defined(_WIN32) && defined(SOKOL_DLL) && defined(SOKOL_IMPL)
#define SOKOL_API_DECL __declspec(dllexport)
#elif defined(_WIN32) && defined(SOKOL_DLL)
#define SOKOL_API_DECL __declspec(dllimport)
#else
#define SOKOL_API_DECL extern
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

SOKOL_API_DECL const char* ssys_app_folder(const char *org, const char *app);
SOKOL_API_DECL void ssys_open_folder(const char *folder);
SOKOL_API_DECL void ssys_open_url(const char *url);

#ifdef __cplusplus
} /* extern "C" */

#endif
#endif // SOKOL_SYS_INCLUDED

#ifdef SOKOL_IMPL
#define SOKOL_SYS_IMPL_INCLUDED (1)

#ifndef SOKOL_API_IMPL
    #define SOKOL_API_IMPL
#endif
#ifndef SOKOL_DEBUG
    #ifndef NDEBUG
        #define SOKOL_DEBUG (1)
    #endif
#endif

#ifndef _SOKOL_PRIVATE
    #if defined(__GNUC__) || defined(__clang__)
        #define _SOKOL_PRIVATE __attribute__((unused)) static
    #else
        #define _SOKOL_PRIVATE static
    #endif
#endif

typedef char _sbuf[256];
typedef wchar_t _wsbuf[sizeof(_sbuf)];

#if defined(_SAPP_APPLE)

#include <sys/stat.h>

SOKOL_API_DECL const char* ssys_app_folder(const char *org, const char *app)
{
    @autoreleasepool {
        static _sbuf result;
        NSArray *array;

        if (!app) 
        {
            return NULL;
        }

        if (!org) 
        {
            org = "";
        }

        array = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);

        if ([array count] > 0) 
        { // we only want the first item in the list.
            NSString *str = [array objectAtIndex:0];
            const char *base = [str fileSystemRepresentation];
            if (base) 
            {

                char *ptr;
                if (*org) 
                {
                    snprintf(result, sizeof result, "%s/%s/%s/", base, org, app);
                }
                else
                {
                    snprintf(result, sizeof result, "%s/%s/", base, app);
                }

                for (ptr = result + 1; *ptr; ptr++) 
                {
                    if (*ptr == '/') 
                    {
                        *ptr = '\0';
                        mkdir(result, 0700);
                        *ptr = '/';
                    }
                }
                mkdir(result, 0700);
            }
        }

        return result;
    }
}

SOKOL_API_DECL void ssys_open_folder(const char *folder)
{
    ssys_open_url(folder);
}

SOKOL_API_DECL void ssys_open_url(const char *url)
{
    _sbuf command;
    snprintf(command, _countof(command), "open \"%s\"", url);
    system(command);
}

#elif defined(_SAPP_WIN32)

#include <shlobj.h>
#include <initguid.h>

DEFINE_GUID(_FOLDERID_RoamingAppData, 0x3EB685DB, 0x65F9, 0x4CF6, 0xA0, 0x3A, 0xE3, 0xEF, 0x65, 0x72, 0x9F, 0x3D);

SOKOL_API_DECL const char* ssys_app_folder(const char *org, const char *app)
{
    static _sbuf result;
    PWSTR path;
    HRESULT hr = SHGetKnownFolderPath(&_FOLDERID_RoamingAppData, KF_FLAG_CREATE, NULL, &path);

    if (!SUCCEEDED(hr))
    {
        return NULL;
    }

    wcstombs(result, path, _countof(result));
    CoTaskMemFree(path);

    if(org)
    {
        strcat(result, "\\");
        strcat(result, org);

        _wsbuf path;
        mbstowcs(path, result, _countof(path));
        CreateDirectoryW(path, NULL);
    }

    if(app)
    {
        strcat(result, "\\");
        strcat(result, app);

        _wsbuf path;
        mbstowcs(path, result, _countof(path));
        CreateDirectoryW(path, NULL);
    }

    strcat(result, "\\");

    return result;
}

SOKOL_API_DECL void ssys_open_folder(const char *folder)
{
    ssys_open_url(folder);
}

SOKOL_API_DECL void ssys_open_url(const char *url)
{
    _wsbuf wurl;
    mbstowcs(wurl, url, _countof(wurl));
    ShellExecuteW(NULL, L"open", wurl, NULL, NULL, SW_SHOWNORMAL);
}

#elif defined(_SAPP_LINUX)

#include <sys/stat.h>

SOKOL_API_DECL const char* ssys_app_folder(const char *org, const char *app)
{
    static _sbuf result;

    const char *envr = getenv("XDG_DATA_HOME");
    const char *append;

    char *ptr = NULL;

    if (!app) 
    {
        return NULL;
    }

    if (!org) 
    {
        org = "";
    }

    if (!envr) 
    {
        envr = getenv("HOME");
        if (!envr) 
        {
            return NULL;
        }

        append = "/.local/share/";
    }
    else 
    {
        append = "/";
    }

    if (*org) 
    {
        snprintf(result, sizeof result, "%s%s%s/%s/", envr, append, org, app);
    } 
    else 
    {
        snprintf(result, sizeof result, "%s%s%s/", envr, append, app);
    }

    for (ptr = result + 1; *ptr; ptr++) 
    {
        if (*ptr == '/') 
        {
            *ptr = '\0';
            if (mkdir(result, 0700) != 0 && errno != EEXIST) 
            {
                goto error;
            }
            *ptr = '/';
        }
    }
    if (mkdir(result, 0700) != 0 && errno != EEXIST) 
    {
    error:
        return NULL;
    }

    return result;
}

SOKOL_API_DECL void ssys_open_folder(const char *folder)
{
    ssys_open_url(folder);
}

SOKOL_API_DECL void ssys_open_url(const char *url)
{
    _sbuf command;
    snprintf(command, _countof(command), "xdg-open \"%s\"", url);
    system(command);
}

#elif defined(_SAPP_EMSCRIPTEN)

#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

SOKOL_API_DECL const char* ssys_app_folder(const char *org, const char *app)
{
    static _sbuf result;

    snprintf(result, sizeof result, "%s/%s/", org, app);
    
    for (char *ptr = result + 1; *ptr; ptr++)
    {
        if (*ptr == '/') 
        {
            *ptr = '\0';
            if (mkdir(result, 0700) != 0 && errno != EEXIST) 
            {
                goto error;
            }
            *ptr = '/';
        }
    }
    if (mkdir(result, 0700) != 0 && errno != EEXIST) 
    {
    error:
        return NULL;
    }

    return result;
}

SOKOL_API_DECL void ssys_open_folder(const char *folder){}

SOKOL_API_DECL void ssys_open_url(const char *url)
{
    EM_ASM_(
    {
        window.open(UTF8ToString($0))
    }, url);
}

#elif defined(_SAPP_ANDROID)

SOKOL_API_DECL const char* ssys_app_folder(const char *org, const char *app)
{
    static _sbuf result;

    const ANativeActivity* activity = sapp_android_get_native_activity();
    JNIEnv *env = activity->env;

    (*activity->vm)->AttachCurrentThread(activity->vm, &env, NULL);

    // this is the Activity (subclass of Context)
    jobject context = activity->clazz;

    // context.getFilesDir()
    jclass contextClass = (*env)->GetObjectClass(env, context);
    jmethodID getFilesDirID = (*env)->GetMethodID(env, contextClass, "getFilesDir", "()Ljava/io/File;");
    jobject fileObj = (*env)->CallObjectMethod(env, context, getFilesDirID);

    // file.getCanonicalPath()
    jclass fileClass = (*env)->GetObjectClass(env, fileObj);
    jmethodID getCanonicalPathID = (*env)->GetMethodID(env, fileClass, "getCanonicalPath", "()Ljava/lang/String;");
    jstring canonicalPath = (jstring)(*env)->CallObjectMethod(env, fileObj, getCanonicalPathID);

    const char* path = (*env)->GetStringUTFChars(env, canonicalPath, NULL);
    snprintf(result, sizeof result, "%s/", path);
    (*env)->ReleaseStringUTFChars(env, canonicalPath, path);

    (*env)->DeleteLocalRef(env, contextClass);
    (*env)->DeleteLocalRef(env, fileObj);
    (*env)->DeleteLocalRef(env, fileClass);
    (*env)->DeleteLocalRef(env, canonicalPath);

    return result;
}

SOKOL_API_DECL void ssys_open_folder(const char *folder){}
SOKOL_API_DECL void ssys_open_url(const char *url) {}

#endif

#endif /* SOKOL_IMPL */
