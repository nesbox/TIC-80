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
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
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

#include <e32const.h>

CTicAppUi::~CTicAppUi() {
  if (iAppContainer) {
    RemoveFromStack(iAppContainer);
    delete iAppContainer;
  }
}

void CTicAppUi::HandleEventL(TWsEvent &aWsEvent) {
  HandleWsEventL(aWsEvent, iAppContainer);
}

void CTicAppUi::ConstructL() {
  BaseConstructL();

  iAppContainer = new (ELeave) CTicContainer;
  iAppContainer->SetMopParent(this);
  iAppContainer->ConstructL(ClientRect(), this);

  AddToStackL(iAppContainer);

  SetOrientationL(EAppUiOrientationLandscape);
}

void CTicAppUi::HandleCommandL(TInt aCommand) {
  switch (aCommand) {
  case EAknSoftkeyBack:
  case EEikCmdExit:
    Exit();
    break;
  }
}

TKeyResponse CTicAppUi::HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType) {
#ifndef SYMBIAN_FIXED_KEYMAP
  switch (aType) {
  case EEventKeyDown:
    if (CTicContainer::IsScanCodeNonModifier) {
      iAppContainer->iPressedKey = aKeyEvent;
    }
    break;
  case EEventKey:
    if (CTicContainer::IsScanCodeNonModifier) {
      iAppContainer->iPressedKey = aKeyEvent;
    }
    break;
  case EEventKeyUp:
    iAppContainer->iPressedKey = {};
    break;
  }
#endif

  return EKeyWasConsumed;
}
