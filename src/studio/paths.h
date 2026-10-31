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

#include "version.h"

// The build's local paths: the versioned studio folder, its cache and the
// config file. Shared by the services, with no studio in it.

#ifdef BAREMETALPI
#define TIC_LOCAL "../.tic80/"
#elif defined(__SWITCH__)
#define TIC_LOCAL "./data/"
#else
#define TIC_LOCAL ".local/"
#endif
#define TIC_LOCAL_VERSION TIC_LOCAL TIC_VERSION_HASH "/"
#define TIC_CACHE         TIC_LOCAL "cache/"

#define CONFIG_TIC      "config.tic"
#define CONFIG_TIC_PATH TIC_LOCAL_VERSION CONFIG_TIC
