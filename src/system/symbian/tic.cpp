// MIT License

// Copyright (c) 2023 Julia Nelz

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "defines.h"
#include "tic.h"
#include "tic80.h"
#include <coedef.h>
#include <cstdint>
#include <ctype.h>
#include <escapeutils.h>
#include <txtetext.h>
#include <e32base.h>
#include <e32hal.h>
#include <e32keys.h>
#include <e32std.h>
#include <fbs.h>
#include <limits.h>
#include <pthread.h>
#include <pthreadtypes.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/limits.h>
#include <sys/stat.h>
#include <time.h>
#include <baclipb.h>
#include <coecntrl.h>
#include <e32cmn.h>
#include <eikapp.h>
#include <eikstart.h>
#include <apgcli.h>
#include <hal.h>
#include <mda/common/audio.h>
#include <mmf/common/mmfutilities.h>
#include <unistd.h>

namespace {

const TUid KClipboardUidTypePlainText = { 268450333 };

char *GetClipboardL() {
  CClipboard* cb = NULL;

  TRAPD(ret, cb = CClipboard::NewForReadingL(CCoeEnv::Static()->FsSession()));
  CleanupStack::PushL(cb);
  if (ret != KErrNone) {
    User::Leave(ret);
  }

  TStreamId stid = (cb->StreamDictionary()).At(KClipboardUidTypePlainText);
  if (stid == KNullStreamId) {
    User::Leave(0);
  }

  CPlainText *item = CPlainText::NewL();
  CleanupStack::PushL(item);

  RStoreReadStream stream;
  stream.OpenLC(cb->Store(), stid);

  TInt dataLength = item->PasteFromStoreL(cb->Store(), cb->StreamDictionary(), 0);
  if (dataLength == 0) {
      User::Leave(KErrNotFound);
  }

  HBufC* hBuf = HBufC::NewL(dataLength);
  auto buf = hBuf->Des();
  CleanupStack::PushL(hBuf);
  item->Extract(buf, 0, dataLength);

  auto utf = EscapeUtils::ConvertFromUnicodeToUtf8L(buf);
  CleanupStack::PushL(utf);
  auto text = new char[dataLength + 1]();
  memcpy(text, reinterpret_cast<const char *>(utf->Ptr()), dataLength);

  CleanupStack::PopAndDestroy(5);

  return text;
}

void SetClipboardL(const char *text) {
  CClipboard* cb = CClipboard::NewForWritingLC(CCoeEnv::Static()->FsSession());

  CPlainText *item = CPlainText::NewL();
  CleanupStack::PushL(item);

  TPtrC8 ptext{reinterpret_cast<const TUint8 *>(text)};
  auto uni = EscapeUtils::ConvertToUnicodeFromUtf8L(ptext);
  CleanupStack::PushL(uni);

  item->InsertL(0, uni->Des());

  CleanupStack::PopAndDestroy();

  RStoreWriteStream stream;
  TStreamId stid = stream.CreateLC(cb->Store());
  item->CopyToStoreL(cb->Store(), cb->StreamDictionary(),
                     0, ptext.Length());
  (cb->StreamDictionary()).AssignL(KClipboardUidTypePlainText, stid);
  
  cb->CommitL();
  CleanupStack::PopAndDestroy(3);
}

#if 0
void OpenFileExternalL(const char *path) {
  TPtrC8 utf8Ptr{reinterpret_cast<const TUint8*>(path)};
  
  auto uniPath = EscapeUtils::ConvertToUnicodeFromUtf8L(utf8Ptr);
  CleanupStack::PushL(uniPath);
  
  RApaLsSession apaLsSession;
  User::LeaveIfError(apaLsSession.Connect());
  
  TThreadId dummyThreadId;
  auto err = apaLsSession.StartDocument(uniPath->Des(), dummyThreadId);
  
  CleanupStack::PopAndDestroy(); 
  
  apaLsSession.Close();

  if (err != KErrNone) {
    User::Leave(err);
  }
}
#endif
}

extern "C" {

void tic_sys_clipboard_set(const char *text) {
  TRAPD(err, SetClipboardL(text));
}

void tic_sys_clipboard_free(char *text) {
  delete[] text;
}

bool tic_sys_clipboard_has() {
  char *item;
  TRAPD(err, item = GetClipboardL());
  if (err == KErrNone) {
    tic_sys_clipboard_free(item);
    return true;
  } else {
    return false;
  }
}

char *tic_sys_clipboard_get() {
  char *item;
  TRAPD(err, item = GetClipboardL());
  return item;
}

u64 tic_sys_counter_get() {
  auto tickCount = User::NTickCount();
  return tickCount;
}

u64 tic_sys_freq_get() {
  //TTimeIntervalMicroSeconds32 value{0};
  //UserHal::TickPeriod(value);
  //return 1'000'000ULL / static_cast<unsigned long long>(value.Int());
  return 1000;
}

void tic_sys_fullscreen_set(bool value) {}

bool tic_sys_fullscreen_get() { return true; }

void tic_sys_message(const char *title, const char *message) {}

void tic_sys_title(const char *title) {}

void tic_sys_open_path(const char *path) {}
void tic_sys_open_url(const char *url) {}

void tic_sys_preseed() {
  srand(time(nullptr));
  rand();
}

void tic_sys_update_config() {}

void tic_sys_default_mapping(tic_mapping *mapping) {
  *mapping = (tic_mapping){
      tic_key_up, tic_key_down, tic_key_left, tic_key_right,

      tic_key_z, // a
      tic_key_x, // b
      tic_key_a, // x
      tic_key_s, // y
  };
}

bool tic_sys_keyboard_text(char *text) { return false; }
}
