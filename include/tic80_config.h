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

#define TIC_BUILD_WITH_LUA 		1
#define TIC_BUILD_WITH_MOON 	1
#define TIC_BUILD_WITH_FENNEL 	1
#define TIC_BUILD_WITH_JS 		1
#define TIC_BUILD_WITH_WREN 	1

#if defined(__APPLE__)
// TODO: this disables macos config 
#	include "AvailabilityMacros.h"
#	include "TargetConditionals.h"
// #	ifndef TARGET_OS_IPHONE
#		undef __TIC_MACOSX__
#		define __TIC_MACOSX__ 1
#		if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
#			error SDL for Mac OS X only supports deploying on 10.6 and above.
#		endif /* MAC_OS_X_VERSION_MIN_REQUIRED < 1060 */
// #	endif /* TARGET_OS_IPHONE */
#endif /* defined(__APPLE__) */

#if defined(WIN32) || defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
#	if defined(_MSC_VER) && defined(__has_include)
#		define HAVE_WINAPIFAMILY_H __has_include(<winapifamily.h>)
#	elif defined(_MSC_VER) && (_MSC_VER >= 1700 && !_USING_V110_SDK71_)
#		define HAVE_WINAPIFAMILY_H 1
#	else
#		define HAVE_WINAPIFAMILY_H 0
#	endif
#	if HAVE_WINAPIFAMILY_H
#		include <winapifamily.h>
#		define WINAPI_FAMILY_WINRT (!WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP))
#	else
#		define WINAPI_FAMILY_WINRT 0
#	endif /* HAVE_WINAPIFAMILY_H */
#	if WINAPI_FAMILY_WINRT
#		undef __TIC_WINRT__
#		define __TIC_WINRT__ 1
#	else
#		undef __TIC_WINDOWS__
#		define __TIC_WINDOWS__ 1
#	endif
#endif 

#if (defined(linux) || defined(__linux) || defined(__linux__))
#	undef __TIC_LINUX__
#	define __TIC_LINUX__   1
#endif

#ifndef TIC80_API
#	if defined(TIC80_SHARED)
#		if defined(__TIC_WINDOWS__) || defined(__TIC_WINRT__)
#			define TIC80_API __declspec(dllexport)
#		elif defined(__TIC_LINUX__)
#			define TIC80_API __attribute__ ((visibility("default")))
#		endif
#	else
#		define TIC80_API
#	endif
#endif