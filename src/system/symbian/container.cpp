// MIT License
//
// Copyright (c) 2023 Julia Nelz
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "appui.h"
#include "container.h"

#include "defines.h"
#include "studio/system.h"

#include <AknDef.h>
#include <eikdoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <coemain.h>
#include <w32std.h>
#include <e32event.h>
#ifdef SYMBIAN_FIXED_KEYMAP
#include <algorithm>
#endif
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#if !defined(EKeyLeftUpArrow) && \
    !defined(EKeyRightUpArrow) && \
    !defined(EKeyRightDownArrow) && \
    !defined(EKeyLeftDownArrow)
#define EKeyLeftUpArrow      EKeyDevice10  // Diagonal arrow event
#define EKeyRightUpArrow     EKeyDevice11  // Diagonal arrow event
#define EKeyRightDownArrow   EKeyDevice12  // Diagonal arrow event
#define EKeyLeftDownArrow    EKeyDevice13  // Diagonal arrow event
#endif

#pragma mark - CTicContainer constants

#ifdef SYMBIAN_FIXED_KEYMAP
extern "C" const char KReMapping[256];
extern "C" const char KReMappingF[256];
extern "C" const char KReMappingS[256];
#endif

// clang-format off
#ifdef USE_GLES2
static const GLfloat vertices[] = {
    -1.0f, -1.0f,
    1.0f, -1.0f,
    -1.0f, 1.0f,
    1.0f, 1.0f
};
#else
static const GLfloat vertices[] = {
    -1.0f, -1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
    -1.0f, 1.0f, 0.0f,
    1.0f, 1.0f, 0.0f
};
#endif

static const GLfloat texCoords[] = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 0.0f
};
#ifdef USE_GLES2
static const GLfloat texCoords2[] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f
};
#endif

#ifdef USE_GLES2
const char *vertexShaderCode =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texCoord;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  v_texCoord = a_texCoord;\n"
    "  gl_PointSize = 1.0;\n"
    "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";

const char *fragmentShaderCode =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(u_texture, v_texCoord);\n"
    "}\n";
#endif
// clang-format on

#pragma mark - CTicContainer impl

CTicContainer::CTicContainer() : iSoundBufferPtr{KNullDesC8} {}

void CTicContainer::ConstructL(const TRect &aRect, CAknAppUi *aAppUi) {
  iAppUi = aAppUi;

  CreateWindowL();
  SetExtentToWholeScreen();
  ActivateL();

  mkdir("D:/Private/71c57d10", 0777);
  freopen("D:/Private/71c57d10/stdout.txt", "w+", stdout);
  freopen("D:/Private/71c57d10/stderr.txt", "w+", stderr);
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  mkdir("/data", 0777);
  mkdir("/data/Others", 0777);
  mkdir("/data/Others/" TIC_PACKAGE, 0777);
  char *argv[]{strdup("/sys/bin/tic80.exe"), nullptr};
  iStudio = studio_create(1, argv, 44100, TIC80_PIXEL_COLOR_RGBA8888,
                          "/data/Others/" TIC_PACKAGE, INT_MAX, tic_layout_unknown);
  free(argv[0]);

  memset(&iInput, 0, sizeof(iInput));

  auto mem = studio_mem(iStudio);

  iTicFrameSize = mem->product.samples.count;

  iSoundBufferPtr.Set(reinterpret_cast<TUint8 *>(mem->product.samples.buffer),
                      iTicFrameSize * TIC80_SAMPLESIZE);

  //iStreamSettings.Query();
  iStreamSettings.iChannels = TMdaAudioDataSettings::EChannelsStereo;
  iStreamSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate44100Hz;
  iStreamSettings.iCaps = TMdaAudioDataSettings::ERealTime | TMdaAudioDataSettings::ESampleRateFixed;
	//iStreamSettings.iFlags = TMdaAudioDataSettings::ENoNetworkRouting;

  iOutputStream = CMdaAudioOutputStream::NewL(*this);
  iOutputStream->Open(&iStreamSettings);

  iEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (iEglDisplay == NULL) {
    _LIT(KGetDisplayFailed, "eglGetDisplay failed");
    User::Panic(KGetDisplayFailed, 0);
  }

  if (eglInitialize(iEglDisplay, NULL, NULL) == EGL_FALSE) {
    _LIT(KInitializeFailed, "eglInitialize failed");
    User::Panic(KInitializeFailed, 0);
  }

  EGLConfig config;
  EGLint numOfConfigs = 0;

  TDisplayMode displayMode = Window().DisplayMode();
  TInt bufferSize = TDisplayModeUtils::NumDisplayModeBitsPerPixel(displayMode);

  // clang-format off
#ifdef USE_GLES2
  const EGLint attribList[] = {EGL_BUFFER_SIZE, bufferSize,
                               EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                               EGL_NONE};
#else
  const EGLint attribList[] = {EGL_NONE};
#endif
  // clang-format on

  if (eglChooseConfig(iEglDisplay, attribList, &config, 1, &numOfConfigs) ==
      EGL_FALSE) {
    _LIT(KChooseConfigFailed, "eglChooseConfig failed");
    User::Panic(KChooseConfigFailed, 0);
  }

  if (numOfConfigs == 0) {
    _LIT(KNoConfig, "Can't find the requested config.");
    User::Panic(KNoConfig, 0);
  }

  iEglSurface = eglCreateWindowSurface(iEglDisplay, config, &Window(), NULL);

  if (iEglSurface == NULL) {
    _LIT(KCreateWindowSurfaceFailed, "eglCreateWindowSurface failed");
    User::Panic(KCreateWindowSurfaceFailed, 0);
  }

  // clang-format off
#ifdef USE_GLES2
  const EGLint contextAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2,
                                 EGL_NONE};
#else
  const EGLint contextAttrs[] = {EGL_NONE};
#endif
  // clang-format on

  iEglContext =
      eglCreateContext(iEglDisplay, config, EGL_NO_CONTEXT, contextAttrs);
  if (iEglContext == NULL) {
    _LIT(KCreateContextFailed, "eglCreateContext failed");
    User::Panic(KCreateContextFailed, 0);
  }

  if (eglMakeCurrent(iEglDisplay, iEglSurface, iEglSurface, iEglContext) ==
      EGL_FALSE) {
    _LIT(KMakeCurrentFailed, "eglMakeCurrent failed");
    User::Panic(KMakeCurrentFailed, 0);
  }

#ifdef USE_GLES2
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderCode, NULL);
  glCompileShader(vertexShader);

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderCode, NULL);
  glCompileShader(fragmentShader);

  iShaderProgram = glCreateProgram();
  glAttachShader(iShaderProgram, vertexShader);
  glAttachShader(iShaderProgram, fragmentShader);
  glLinkProgram(iShaderProgram);

  GLint bufsz;
  GLsizei ilen;
  glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &bufsz);
  char vertexInfo[bufsz];
  glGetShaderInfoLog(vertexShader, bufsz, &ilen, vertexInfo);
  fprintf(stderr, "Vertex info (s=%d,l=%d):\n%s\n", bufsz, ilen, vertexInfo);

  glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &bufsz);
  char fragmentInfo[bufsz];
  glGetShaderInfoLog(fragmentShader, bufsz, &ilen, fragmentInfo);
  fprintf(stderr, "Fragment info (s=%d,l=%d):\n%s\n", bufsz, ilen,
          fragmentInfo);

  glGetProgramiv(iShaderProgram, GL_INFO_LOG_LENGTH, &bufsz);
  char programInfo[bufsz];
  glGetProgramInfoLog(iShaderProgram, bufsz, &ilen, programInfo);
  fprintf(stderr, "Program info (s=%d,l=%d):\n%s\n", bufsz, ilen, programInfo);

  iPositionHandle = glGetAttribLocation(iShaderProgram, "a_position");
  iTexCoordHandle = glGetAttribLocation(iShaderProgram, "a_texCoord");
  iTexSamplerHandle = glGetUniformLocation(iShaderProgram, "u_texture");
#endif

#ifdef USE_GLES2
  glGenTextures(2, iTextures);
  glGenFramebuffers(1, &iFramebuffer);
#else
  glGenTextures(1, iTextures);
#endif

#if 0
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback([](GLenum source,
                            GLenum type,
                            GLuint id,
                            GLenum severity,
                            GLsizei length,
                            const GLchar* message,
                            const void* userParam) {

    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
             (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
  }, 0);
#endif

  HandleResourceChange(KEikDynamicLayoutVariantSwitch);

  iPeriodic = CPeriodic::NewL(CActive::EPriorityIdle);
  iPeriodic->Start(100, 100, TCallBack(CTicContainer::DrawCallBack, this));

	Window().PointerFilter(EPointerFilterDrag, 0);
  iWsEventReceiver = CWsEventReceiver::NewL(*this, &CCoeEnv::Static()->WsSession());
}

void CTicContainer::MaoscOpenComplete(TInt aError) {
  if (aError == KErrNone) {
    iVolume = iOutputStream->MaxVolume() / 2;
    iVolumeStep = iOutputStream->MaxVolume() / 8;
    if (iVolumeStep == 0)
      iVolumeStep = 1;

    iOutputStream->SetVolume(iVolume);
    iOutputStream->SetPriority(EPriorityNormal, EMdaPriorityPreferenceTime);
    //iOutputStream->SetDataTypeL(KMMFFourCCCodePCM16);

    iOutputStatus = EOpen;

    RequestSound();
  } else {
    iOutputStatus = ENotReady;
  }    
}

void CTicContainer::MaoscBufferCopied(TInt aError, const TDesC8& aBuffer) {
  if (aError == KErrAbort) {
    iOutputStatus = ENotReady;
    LPRINTF("Buffer copy aborted!");
  } else if (iOutputStatus != ENotReady) {
    RequestSound();
  }
}

void CTicContainer::MaoscPlayComplete(TInt aError) {
  iOutputStatus = ENotReady;
  LPRINTF("Sound output closed");
}

void CTicContainer::ProcessInputs() {
  iInput.mouse.left = iButton1Down;
  iButton1Down = iButton1DownNextFrame;
  iInput.mouse.x = (float)iPosition.iX * ((float)TIC80_FULLWIDTH / (float)Size().iWidth);
  iInput.mouse.y = (float)iPosition.iY * ((float)TIC80_FULLHEIGHT / (float)Size().iHeight);

  memset(&iInput.keyboard.keys, 0, sizeof(iInput.keyboard.keys));

  int i{0};
#ifdef SYMBIAN_FIXED_KEYMAP
  const char *mapping = KReMapping;
  if (std::count(iPressedKeys.begin(), iPressedKeys.end(), EStdKeyLeftFunc)) {
    mapping = KReMappingS;
  } else if (std::count(iPressedKeys.begin(), iPressedKeys.end(), EStdKeyRightFunc)) {
    mapping = KReMappingF;
  }
  for (const auto &key : iPressedKeys) {
    if (key > 0 && key < 256 && mapping[key]) {
      iInput.keyboard.keys[i] = mapping[key];
    } else if (key >= 'A' && key <= 'Z') {
      iInput.keyboard.keys[i] = tic_key_a + key - 'A';
    } else if (key >= 'a' && key <= 'z') {
      iInput.keyboard.keys[i] = tic_key_a + key - 'a';
    } else {
      switch (key) {
      case EStdKeyLeftArrow:
        iInput.keyboard.keys[i] = tic_key_left;
        break;
      case EStdKeyRightArrow:
        iInput.keyboard.keys[i] = tic_key_right;
        break;
      case EStdKeyUpArrow:
        iInput.keyboard.keys[i] = tic_key_up;
        break;
      case EStdKeyDownArrow:
        iInput.keyboard.keys[i] = tic_key_down;
        break;
      case EStdKeyEnter:
        iInput.keyboard.keys[i] = tic_key_return;
        break;
      case EStdKeyBackspace:
        iInput.keyboard.keys[i] = tic_key_backspace;
        break;
      case EStdKeySpace:
        iInput.keyboard.keys[i] = tic_key_space;
        break;
      case EStdKeyLeftShift:
      case EStdKeyRightShift:
        iInput.keyboard.keys[i] = tic_key_shift;
        break;
      case EStdKeyLeftCtrl:
      case EStdKeyRightCtrl:
        iInput.keyboard.keys[i] = tic_key_ctrl;
        break;
      default:
        continue;
      }
    }
    
    if (++i >= TIC80_KEY_BUFFER)
      break;
  }
#else
  const auto &key = iPressedKey.iCode;
  const auto &mods = iPressedKey.iModifiers;
  bool leftFuncMod = iPressedKey.iModifiers & EModifierLeftFunc;
  if (key >= '0' && key <= '9') {
    if (leftFuncMod) {
      if (key >= '1') {
        iInput.keyboard.keys[i] = tic_key_f1 + key - '1';
      } else {
        iInput.keyboard.keys[i] = tic_key_f10;
      }
    } else {
      iInput.keyboard.keys[i] = tic_key_0 + key - '0';
    }
  } else if (key >= 'A' && key <= 'Z') {
    iInput.keyboard.keys[i] = tic_key_a + key - 'A';
  } else if (key >= 'a' && key <= 'z') {
    iInput.keyboard.keys[i] = tic_key_a + key - 'a';
  } else {
    switch (key) {
    case '!':
      iInput.keyboard.keys[i] = tic_key_1;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '@':
      iInput.keyboard.keys[i] = tic_key_2;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '#':
      iInput.keyboard.keys[i] = tic_key_3;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '$':
      iInput.keyboard.keys[i] = tic_key_4;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '%':
      iInput.keyboard.keys[i] = tic_key_5;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '^':
      iInput.keyboard.keys[i] = tic_key_6;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '&':
      iInput.keyboard.keys[i] = tic_key_7;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '*':
      iInput.keyboard.keys[i] = tic_key_8;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '(':
      iInput.keyboard.keys[i] = tic_key_9;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case ')':
      iInput.keyboard.keys[i] = tic_key_0;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '_':
      iInput.keyboard.keys[i] = tic_key_minus;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '+':
      iInput.keyboard.keys[i] = tic_key_equals;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '-':
      iInput.keyboard.keys[i] = tic_key_minus;
      break;
    case '=':
      iInput.keyboard.keys[i] = tic_key_equals;
      break;
    case ',':
      iInput.keyboard.keys[i] = tic_key_comma;
      break;
    case '.':
      iInput.keyboard.keys[i] = tic_key_period;
      break;
    case '<':
      iInput.keyboard.keys[i] = tic_key_comma;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '>':
      iInput.keyboard.keys[i] = tic_key_period;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case ';':
      iInput.keyboard.keys[i] = tic_key_semicolon;
      break;
    case ':':
      iInput.keyboard.keys[i] = tic_key_semicolon;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '\'':
      iInput.keyboard.keys[i] = tic_key_apostrophe;
      break;
    case '"':
      iInput.keyboard.keys[i] = tic_key_apostrophe;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '/':
      iInput.keyboard.keys[i] = tic_key_slash;
      break;
    case '?':
      iInput.keyboard.keys[i] = tic_key_slash;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '\\':
      iInput.keyboard.keys[i] = tic_key_backslash;
      break;
    case '|':
      iInput.keyboard.keys[i] = tic_key_backslash;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case '[':
      iInput.keyboard.keys[i] = tic_key_leftbracket;
      break;
    case '{':
      iInput.keyboard.keys[i] = tic_key_leftbracket;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case ']':
      iInput.keyboard.keys[i] = tic_key_rightbracket;
      break;
    case '}':
      iInput.keyboard.keys[i] = tic_key_rightbracket;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_shift;
      break;
    case EKeyLeftArrow:
      iInput.keyboard.keys[i] = tic_key_left;
      break;
    case EKeyRightArrow:
      iInput.keyboard.keys[i] = tic_key_right;
      break;
    case EKeyUpArrow:
      iInput.keyboard.keys[i] = tic_key_up;
      break;
    case EKeyDownArrow:
      iInput.keyboard.keys[i] = tic_key_down;
      break;
    case EKeyLeftUpArrow:
      iInput.keyboard.keys[i] = tic_key_left;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_up;
      break;
    case EKeyRightUpArrow:
      iInput.keyboard.keys[i] = tic_key_right;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_up;
      break;
    case EKeyRightDownArrow:
      iInput.keyboard.keys[i] = tic_key_right;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_down;
      break;
    case EKeyLeftDownArrow:
      iInput.keyboard.keys[i] = tic_key_left;
      iInput.keyboard.keys[++i % TIC80_KEY_BUFFER] = tic_key_down;
      break;
    case EKeyEnter:
      if (leftFuncMod) {
        iInput.keyboard.keys[i] = tic_key_tab;
      } else {
        iInput.keyboard.keys[i] = tic_key_return;
      }
      break;
    case EKeyBackspace:
      if (leftFuncMod) {
        iInput.keyboard.keys[i] = tic_key_escape;
      } else {
        iInput.keyboard.keys[i] = tic_key_backspace;
      }
      break;
    case EKeySpace:
      iInput.keyboard.keys[i] = tic_key_space;
      break;
    }
  }
#endif
}

TInt CTicContainer::DrawCallBack(TAny *aInstance) {
  CTicContainer *instance = static_cast<CTicContainer *>(aInstance);

  instance->ProcessInputs();

  studio_tick(instance->iStudio, instance->iInput);
  auto mem = studio_mem(instance->iStudio);

  if (studio_alive(instance->iStudio))
    instance->iAppUi->Exit();

#ifdef USE_GLES2
  glUseProgram(instance->iShaderProgram);

  glVertexAttribPointer(instance->iPositionHandle, 2, GL_FLOAT, GL_FALSE, 0,
                        vertices);
  glEnableVertexAttribArray(instance->iPositionHandle);

  glVertexAttribPointer(instance->iTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                        texCoords);
  glEnableVertexAttribArray(instance->iTexCoordHandle);

  glUniform1i(instance->iTexSamplerHandle, 0);

  // Draw TIC to the FBO

  glBindFramebuffer(GL_FRAMEBUFFER, instance->iFramebuffer);

  glBindTexture(GL_TEXTURE_2D, instance->iTextures[1]);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TIC80_FULLWIDTH * 2, TIC80_FULLHEIGHT * 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, instance->iTextures[1], 0);

  GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
    LPRINTF("FBO Error: 0x%04X", framebufferStatus);
    instance->iAppUi->Exit();
  }

  glViewport(0, 0, TIC80_FULLWIDTH * 2, TIC80_FULLHEIGHT * 2);

  glClearColor(0.f, 1.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, instance->iTextures[0]);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TIC80_FULLWIDTH, TIC80_FULLHEIGHT,
               0, GL_RGBA, GL_UNSIGNED_BYTE, mem->product.screen);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Draw the FBO to the screen

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glVertexAttribPointer(instance->iTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0,
                        texCoords2);

  glViewport(0, 0, instance->Size().iWidth, instance->Size().iHeight);

  glClearColor(1.f, 0.f, 1.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, instance->iTextures[1]);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisableVertexAttribArray(instance->iPositionHandle);
  glDisableVertexAttribArray(instance->iTexCoordHandle);
#else
  // GLES 1.0 (no linear 2x)

  glViewport(0, 0, instance->Size().iWidth, instance->Size().iHeight);

  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glVertexPointer(3, GL_FLOAT, 0, vertices);
  glTexCoordPointer(2, GL_FLOAT, 0, texCoords);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, instance->iTextures[0]);

  glEnable(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TIC80_FULLWIDTH, TIC80_FULLHEIGHT,
               0, GL_RGBA, GL_UNSIGNED_BYTE, mem->product.screen);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisable(GL_TEXTURE_2D);

  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif

  eglSwapBuffers(instance->iEglDisplay, instance->iEglSurface);

  User::ResetInactivityTime();

  User::After(100);

  return 0;
}

bool CTicContainer::IsScanCodeNonModifier(TInt aScanCode) {
#ifdef SYMBIAN_FIXED_KEYMAP
  return !(aScanCode == EStdKeyLeftFunc ||
           aScanCode == EStdKeyRightFunc ||
           aScanCode == EStdKeyLeftCtrl ||
           aScanCode == EStdKeyRightCtrl ||
           aScanCode == EStdKeyLeftShift ||
           aScanCode == EStdKeyRightShift);
#else
  return !(aScanCode == EKeyLeftFunc ||
           aScanCode == EKeyRightFunc ||
           aScanCode == EKeyLeftCtrl ||
           aScanCode == EKeyRightCtrl ||
           aScanCode == EKeyLeftShift ||
           aScanCode == EKeyRightShift);
#endif
}

bool CTicContainer::HandleWsEvent(TWsEvent &aEvent) {
  TInt scanCode;
  switch (aEvent.Type()) {
  case EEventKeyDown:
    scanCode = aEvent.Key()->iScanCode;
    switch (aEvent.Key()->iCode) {
    case EStdKeyIncVolume:
      // TODO: Volume control broken?
      iVolume += iVolumeStep;
      iOutputStatus = ESetVolume;
      return true;
    case EStdKeyDecVolume:
      iVolume -= iVolumeStep;
      iOutputStatus = ESetVolume;
      return true;
    default:
#ifdef SYMBIAN_FIXED_KEYMAP
      if (!std::count(iPressedKeys.begin(), iPressedKeys.end(), scanCode)) {
        iPressedKeys.push_back(scanCode);
        if (CTicContainer::IsScanCodeNonModifier(scanCode)) {
          iModifierUsed = true;
        }
      }
      return true;
#else
      return false;
#endif
    }
    break;
#ifdef SYMBIAN_FIXED_KEYMAP
  case EEventKeyUp:
    scanCode = aEvent.Key()->iScanCode;
    if (CTicContainer::IsScanCodeNonModifier(scanCode)) {
      iPressedKeys.remove(scanCode);
      if (!std::count_if(iPressedKeys.begin(), iPressedKeys.end(), CTicContainer::IsScanCodeNonModifier)) {
        for (auto modifier : iDepressedModifiers) {
          iPressedKeys.remove(modifier);
        }
        iDepressedModifiers.clear();
      }
    } else if (iModifierUsed) {
      if (!std::count_if(iPressedKeys.begin(), iPressedKeys.end(), CTicContainer::IsScanCodeNonModifier)) {
        iPressedKeys.remove(scanCode);
        iDepressedModifiers.erase(scanCode);
      } else {
        iPressedKeys.clear();
      }
    } else {
      iDepressedModifiers.insert(scanCode);
    }
    if (!std::count_if(iPressedKeys.begin(), iPressedKeys.end(), [](auto scanCode) {
        return !CTicContainer::IsScanCodeNonModifier(scanCode);
      })) {
      iModifierUsed = false;
    }
    if (iPressedKeys.empty()) {
      iDepressedModifiers.clear();
    }
    return true;
#endif
  case EEventPointer:
    switch (aEvent.Pointer()->iType) {
    case TPointerEvent::EButton1Down: 
      iPosition = aEvent.Pointer()->iPosition;
      iButton1DownNextFrame = true;
      break;
    case TPointerEvent::EButton1Up:
      iPosition = aEvent.Pointer()->iPosition;
      iButton1DownNextFrame = false;
      break;
    case TPointerEvent::EDrag:
    case TPointerEvent::EMove:
      iPosition = aEvent.Pointer()->iPosition;
      break;
    }
    return true;
  default:
    break;
  }
  return false;
}

void CTicContainer::Draw(const TRect &aRect) const {}

void CTicContainer::SizeChanged() {
  eglSwapBuffers(iEglDisplay, iEglSurface);
}

void CTicContainer::HandleResourceChange(TInt aType) {
  switch (aType) {
  case KEikDynamicLayoutVariantSwitch:
    SetExtentToWholeScreen();
    eglSwapBuffers(iEglDisplay, iEglSurface);
    break;
  }
}

void CTicContainer::RequestSound() {
  studio_sound(iStudio);
  auto mem = studio_mem(iStudio);

  if (iOutputStatus == ESetVolume) {
    iOutputStatus = EOpen;
    if (iVolume < 0)
      iVolume = 0;
    if (iVolume > iOutputStream->MaxVolume())
      iVolume = iOutputStream->MaxVolume();
    LPRINTF("Set volume to %d", iVolume);
    iOutputStream->SetVolume(iVolume);
  }

  iOutputStream->WriteL(iSoundBufferPtr);
}

TInt CTicContainer::CountComponentControls() const { return 0; }

CCoeControl *CTicContainer::ComponentControl(TInt aIndex) const {
  return nullptr;
}

CTicContainer::~CTicContainer() {
  delete iPeriodic;

  if (iOutputStatus != ENotReady)
    iOutputStream->Stop();
  delete iOutputStream;

  studio_delete(iStudio);

  eglMakeCurrent(iEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroySurface(iEglDisplay, iEglSurface);
  eglDestroyContext(iEglDisplay, iEglContext);
  eglTerminate(iEglDisplay);
#ifdef USE_GLES2
  eglReleaseThread();
#endif
}

#pragma mark - CWsEventReceiver impl

CWsEventReceiver::CWsEventReceiver()
    : CActive(CActive::EPriorityHigh), iParent(NULL) {}

CWsEventReceiver::~CWsEventReceiver() { Cancel(); }

CWsEventReceiver *CWsEventReceiver::NewL(CTicContainer &aParent,
                                         RWsSession *aWsSession) {
  CWsEventReceiver *self = new (ELeave) CWsEventReceiver;

  CleanupStack::PushL(self);

  self->ConstructL(aParent, aWsSession);

  CleanupStack::Pop(self);

  return self;
}

void CWsEventReceiver::ConstructL(CTicContainer &aParent, RWsSession *aWsSession) {
  iParent = &aParent;
  iWsSession = aWsSession;
  iWsSession->EventReady(&iStatus);

  CActiveScheduler::Add(this);

  SetActive();
}

void CWsEventReceiver::RunL() {
  TWsEvent wsEvent;
  iWsSession->GetEvent(wsEvent);

  if (!iParent->HandleWsEvent(wsEvent)) {
    static_cast<CTicAppUi *>(iParent->iAppUi)->HandleEventL(wsEvent);
  }

  iWsSession->EventReady(&iStatus);

  SetActive();
}

void CWsEventReceiver::DoCancel() {
  iWsSession->EventReadyCancel();
}
