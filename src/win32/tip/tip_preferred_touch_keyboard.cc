// Copyright 2010-2021, Google Inc.
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

#include "win32/tip/tip_preferred_touch_keyboard.h"

#include <ctffunc.h>
#include <guiddef.h>
#include <wil/com.h>
#include <windows.h>

#include "base/win32/com.h"
#include "base/win32/com_implements.h"
#include "win32/tip/tip_dll_module.h"

namespace mozc {
namespace win32 {

template <>
bool IsIIDOf<ITfFnGetPreferredTouchKeyboardLayout>(REFIID riid) {
  return IsIIDOf<ITfFnGetPreferredTouchKeyboardLayout, ITfFunction>(riid);
}

namespace tsf {
namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr wchar_t kGetPreferredTouchKeyboardLayoutDisplayName[] =
    L"Google Japanese Input: GetPreferredTouchKeyboardLayout Function";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
constexpr wchar_t kGetPreferredTouchKeyboardLayoutDisplayName[] =
    L"Mozc: GetPreferredTouchKeyboardLayout Function";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

class GetPreferredTouchKeyboardLayoutImpl final
    : public TipComImplements<ITfFnGetPreferredTouchKeyboardLayout> {
 public:
 private:
  // The ITfFunction interface method.
  virtual HRESULT STDMETHODCALLTYPE GetDisplayName(BSTR *name) {
    if (name == nullptr) {
      return E_INVALIDARG;
    }
    *name = ::SysAllocString(kGetPreferredTouchKeyboardLayoutDisplayName);
    return S_OK;
  }

  // ITfFnGetPreferredTouchKeyboardLayout
  virtual HRESULT STDMETHODCALLTYPE GetLayout(TKBLayoutType *layout_type,
                                              WORD *preferred_layout_id) {
    if (layout_type != nullptr) {
      *layout_type = TKBLT_OPTIMIZED;
    }
    if (preferred_layout_id != nullptr) {
      *preferred_layout_id = TKBL_OPT_JAPANESE_ABC;
    }
    return S_OK;
  }
};

}  // namespace

// static
wil::com_ptr_nothrow<ITfFnGetPreferredTouchKeyboardLayout>
TipPreferredTouchKeyboard::New() {
  return MakeComPtr<GetPreferredTouchKeyboardLayoutImpl>();
}

// static
const IID &TipPreferredTouchKeyboard::GetIID() {
  return IID_ITfFnGetPreferredTouchKeyboardLayout;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
