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

#include <Windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <Ctffunc.h>
#include <atlbase.h>
#include <atlcom.h>

#include "win32/tip/tip_ref_count.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using ATL::CComPtr;

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const wchar_t kGetPreferredTouchKeyboardLayoutDisplayName[] =
    L"Google Japanese Input: GetPreferredTouchKeyboardLayout Function";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
const wchar_t kGetPreferredTouchKeyboardLayoutDisplayName[] =
    L"Mozc: GetPreferredTouchKeyboardLayout Function";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

class GetPreferredTouchKeyboardLayoutImpl
    : public ITfFnGetPreferredTouchKeyboardLayout {
 public:
  GetPreferredTouchKeyboardLayoutImpl() {}
  GetPreferredTouchKeyboardLayoutImpl(
      const GetPreferredTouchKeyboardLayoutImpl &) = delete;
  GetPreferredTouchKeyboardLayoutImpl &operator=(
      const GetPreferredTouchKeyboardLayoutImpl &) = delete;

  // The IUnknown interface methods.
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id,
                                                   void **object) {
    if (!object) {
      return E_INVALIDARG;
    }

    // Find a matching interface from the ones implemented by this object.
    // This object implements IUnknown and ITfEditSession.
    if (::IsEqualIID(interface_id, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(this);
    } else if (IsEqualIID(interface_id, IID_ITfFunction)) {
      *object = static_cast<ITfFunction *>(this);
    } else if (IsEqualIID(interface_id,
                          IID_ITfFnGetPreferredTouchKeyboardLayout)) {
      *object = static_cast<ITfFnGetPreferredTouchKeyboardLayout *>(this);
    } else {
      *object = nullptr;
      return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef() { return ref_count_.AddRefImpl(); }

  virtual ULONG STDMETHODCALLTYPE Release() {
    const ULONG count = ref_count_.ReleaseImpl();
    if (count == 0) {
      delete this;
    }
    return count;
  }

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

  TipRefCount ref_count_;
};

}  // namespace

// static
IUnknown *TipPreferredTouchKeyboard::New() {
  return new GetPreferredTouchKeyboardLayoutImpl();
}

// static
const IID &TipPreferredTouchKeyboard::GetIID() {
  return IID_ITfFnGetPreferredTouchKeyboardLayout;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
