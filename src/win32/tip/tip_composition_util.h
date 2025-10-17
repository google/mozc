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

#ifndef MOZC_WIN32_TIP_TIP_COMPOSITION_UTIL_H_
#define MOZC_WIN32_TIP_TIP_COMPOSITION_UTIL_H_

#include <msctf.h>
#include <wil/com.h>
#include <windef.h>

namespace mozc {
namespace win32 {
namespace tsf {

class TipCompositionUtil {
 public:
  TipCompositionUtil() = delete;
  TipCompositionUtil(const TipCompositionUtil &) = delete;
  TipCompositionUtil &operator=(const TipCompositionUtil &) = delete;

  // Returns composition object if there is a composition which belongs
  // to Mozc in |context|. Otherwise returns nullptr.
  static wil::com_ptr_nothrow<ITfComposition> GetComposition(
      ITfContext *context, TfEditCookie edit_cookie);

  // Returns composition view object if there is a composition which belongs
  // to Mozc in |context|. Otherwise returns nullptr.
  static wil::com_ptr_nothrow<ITfCompositionView> GetCompositionView(
      ITfContext *context, TfEditCookie edit_cookie);

  // Returns composition view object if there is a composition which belongs
  // to Mozc in |range|. Otherwise returns nullptr.
  static wil::com_ptr_nothrow<ITfCompositionView> GetCompositionViewFromRange(
      ITfRange *range, TfEditCookie edit_cookie);

  // Removes display attributes from |composition|. Returns the result.
  static HRESULT ClearDisplayAttributes(ITfContext *context,
                                        ITfComposition *composition,
                                        TfEditCookie write_cookie);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_COMPOSITION_UTIL_H_
