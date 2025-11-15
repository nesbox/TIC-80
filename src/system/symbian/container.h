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

#pragma once

#include "studio/system.h"
#include "tic80.h"

#ifdef USE_GLES2
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#else
#include <GLES/egl.h>
#include <GLES/gl.h>
#endif
#include <aknappui.h>
#include <coecntrl.h>
#include <mdaaudiosampleeditor.h>
#include <mdaaudiooutputstream.h>
#include <mda/common/audio.h>
#include <mmf/common/mmfutilities.h>
#include <w32std.h>

#ifdef SYMBIAN_FIXED_KEYMAP
#include <list>
#include <set>
#endif

struct CTicAppUi;
struct CWsEventReceiver;

struct CTicContainer : CCoeControl, MMdaAudioOutputStreamCallback {
  friend struct CTicAppUi;
  friend struct CWsEventReceiver;

  void ConstructL(const TRect &aRect, CAknAppUi *aAppUi);

  CTicContainer();

  ~CTicContainer() override;

  static TInt DrawCallBack(TAny *aInstance);

  bool HandleWsEvent(TWsEvent &aEvent);

public:
  void MaoscOpenComplete(TInt aError) override;

  void MaoscBufferCopied(TInt aError, const TDesC8& aBuffer) override;
  
  void MaoscPlayComplete(TInt aError) override;

private:
  void SizeChanged() override;

  void HandleResourceChange(TInt aType) override;

  TInt CountComponentControls() const override;

  CCoeControl *ComponentControl(TInt aIndex) const override;

  void Draw(const TRect &aRect) const override;

  void ProcessInputs();

  void RequestSound();

  static bool IsScanCodeNonModifier(TInt iScanCode);

private:
  enum TStatus {
    ENotReady,
    EOpen,
    ESetVolume
  };

  enum { KBufferMaxFrames = 10 };

  CWsEventReceiver *iWsEventReceiver;
#if USE_GLES2
  GLuint iTextures[2];
  GLuint iFramebuffer;
  GLuint iShaderProgram;
  GLint iPositionHandle, iTexCoordHandle, iTexSamplerHandle;
#else
  GLuint iTextures[1];
#endif
  EGLDisplay iEglDisplay;
  EGLSurface iEglSurface;
  EGLContext iEglContext;
  CPeriodic *iPeriodic;
  CAknAppUi *iAppUi;

  bool iButton1Down;
  bool iButton1DownNextFrame;
  TPoint iPosition;
#if SYMBIAN_FIXED_KEYMAP
  std::list<TInt> iPressedKeys;
  std::set<TInt> iDepressedModifiers;
  bool iModifierUsed;
#else
  TKeyEvent iPressedKey;
#endif

  tic80_input iInput;
  Studio *iStudio;

  TPtrC8 iSoundBufferPtr;
  TUint iTicFrameSize;

  TInt iVolume;
  TUint iVolumeStep;

  TMdaAudioDataSettings iStreamSettings;
  CMdaAudioOutputStream* iOutputStream;
  TStatus iOutputStatus;
};

struct CWsEventReceiver : CActive {
  void RunL() override;

  void DoCancel() override;

  static CWsEventReceiver *NewL(CTicContainer &aParent, RWsSession *aWsSession);

  ~CWsEventReceiver();

private:
  CWsEventReceiver();

  void ConstructL(CTicContainer &aParent, RWsSession *aWsSession);

private:
  RWsSession *iWsSession;

  CTicContainer *iParent;
};
