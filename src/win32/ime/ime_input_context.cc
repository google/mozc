// Copyright 2010-2020, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "win32/ime/ime_input_context.h"

#include <atlbase.h>
#include <atlwin.h>

#include "base/logging.h"
#include "win32/ime/ime_core.h"

namespace mozc {
namespace win32 {
bool InputContext::Initialize() {
  DVLOG(3) << __FUNCTION__;

  // Initialize conversion mode.
  if (!(fdwInit & INIT_CONVERSION)) {
    DLOG(WARNING) << __FUNCTION__ << L"Conversion mode not initialized.";
    fdwConversion = IME_CMODE_NATIVE;
    fdwInit |= INIT_CONVERSION;
  }

  if ((fdwInit & INIT_SENTENCE) != INIT_SENTENCE) {
    DLOG(WARNING) << __FUNCTION__ << L"Sentence mode not initialized.";
    // Use IME_SMODE_PHRASEPREDICT as a default.
    // See b/2913510, b/2954777, and b/2955175 for details.
    fdwSentence = IME_SMODE_PHRASEPREDICT;
    fdwInit |= INIT_SENTENCE;
  }
  // Normalize sentence mode just in case.
  fdwSentence = ImeCore::GetSupportableSentenceMode(fdwSentence);

  // Do not take over the composition font because INPUTCONTEXT::lfFont might
  // be broken when the default IME is ATOK 2009, which directly updates
  // INPUTCONTEXT::lfFont.A even when the target Window is a native Unicode
  // window.  ATOK should have used ImmSetCompositionFont API.
  // See b/3042347 for details.
  if ((fdwInit & INIT_LOGFONT) == INIT_LOGFONT) {
    ::ZeroMemory(&lfFont, sizeof(lfFont));
    fdwInit &= ~INIT_LOGFONT;
  }

  // We can't assume the open status is true when the input method is opened in
  // a given context. For example, when you open the IME in the password control
  // of Opera, the open status provided by the applications is false. If we
  // change it to true here, it will be changed to false after the focus is
  // switching away and never changed back to true.
  //  fOpen = TRUE;
  return true;
}
}  // namespace win32
}  // namespace mozc
