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

#if !defined(TIC_BUILD_WITH_LUA)      && \
    !defined(TIC_BUILD_WITH_MOON)     && \
    !defined(TIC_BUILD_WITH_FENNEL)   && \
    !defined(TIC_BUILD_WITH_JS)       && \
    !defined(TIC_BUILD_WITH_WREN)     && \
    !defined(TIC_BUILD_WITH_SCHEME)   && \
    !defined(TIC_BUILD_WITH_SQUIRREL) && \
    !defined(TIC_BUILD_WITH_PYTHON)   && \
    !defined(TIC_BUILD_WITH_WASM)

#   define TIC_BUILD_WITH_LUA      1
#   define TIC_BUILD_WITH_MOON     1
#   define TIC_BUILD_WITH_FENNEL   1
#   define TIC_BUILD_WITH_JS       1
#   define TIC_BUILD_WITH_WREN     1
#   define TIC_BUILD_WITH_SCHEME   1
#   define TIC_BUILD_WITH_SQUIRREL 1
#   define TIC_BUILD_WITH_PYTHON   1
#   define TIC_BUILD_WITH_WASM     1

#endif

#if defined(TIC_BUILD_WITH_FENNEL) || defined(TIC_BUILD_WITH_MOON)
#   define TIC_BUILD_WITH_LUA 1
#endif

#if defined(__APPLE__)
// TODO: this disables macos config 
#   include "AvailabilityMacros.h"
#   include "TargetConditionals.h"
// #    ifndef TARGET_OS_IPHONE
#       undef __TIC_MACOSX__
#       define __TIC_MACOSX__ 1
// #    endif /* TARGET_OS_IPHONE */
#endif /* defined(__APPLE__) */

#if defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
#   undef __TIC_WINDOWS__
#   define __TIC_WINDOWS__ 1
#endif 

#if defined(ANDROID) || defined(__ANDROID__)
#   undef __TIC_ANDROID__
#   define __TIC_ANDROID__ 1
#elif (defined(linux) || defined(__linux) || defined(__linux__))
#   undef __TIC_LINUX__
#   define __TIC_LINUX__ 1
#endif

#ifndef TIC80_API
#   if defined(TIC80_SHARED)
#       if defined(__TIC_WINDOWS__)
#           define TIC80_API __declspec(dllexport)
#       elif defined(__TIC_LINUX__)
#           define TIC80_API __attribute__ ((visibility("default")))
#       endif
#   else
#       define TIC80_API
#   endif
#endif
