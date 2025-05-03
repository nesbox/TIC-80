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

#pragma once

#include "defines.h"

#if defined(__TIC_WINDOWS__)

#include <windows.h>

#define mutex_t         CRITICAL_SECTION
#define thread_t        HANDLE
#define thread_proc_t   LPTHREAD_START_ROUTINE
#define thread_ret_t    DWORD

#define mutex_init(m)                       InitializeCriticalSection(m)
#define mutex_destroy(m)                    DeleteCriticalSection(m)
#define mutex_lock(m)                       EnterCriticalSection(m)
#define mutex_unlock(m)                     LeaveCriticalSection(m)
#define thread_create(t, proc, userdata)    *t = CreateThread(NULL, 0, proc, userdata, 0, 0)
#define thread_cancel(t)                    CloseHandle(t)

#else

#include <pthread.h>

#define mutex_t         pthread_mutex_t
#define thread_t        pthread_t
#define thread_ret_t    void*
typedef void* (*thread_proc_t)(void*);

#define mutex_init(m) {             \
    pthread_mutexattr_t attr;       \
    pthread_mutexattr_init(&attr);  \
    pthread_mutex_init(m, &attr);}

#define mutex_destroy(m)                    pthread_mutex_destroy(m)
#define mutex_lock(m)                       pthread_mutex_lock(m)
#define mutex_unlock(m)                     pthread_mutex_unlock(m)
#define thread_create(t, proc, userdata)    pthread_create(t, NULL, proc, userdata)

#if defined(__TIC_ANDROID__)
#   define thread_cancel(t)
#else
#   define thread_cancel(t) pthread_cancel(t)
#endif

#endif
