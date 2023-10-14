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

#ifndef MOZC_WIN32_TIP_TIP_RANGE_UTIL_H_
#define MOZC_WIN32_TIP_TIP_RANGE_UTIL_H_

#include <inputscope.h>
#include <msctf.h>
#include <windows.h>

#include <string>
#include <vector>

namespace mozc {
namespace win32 {
namespace tsf {

// A utility class to handle ITfRange object.
class TipRangeUtil {
 public:
  TipRangeUtil() = delete;
  TipRangeUtil(const TipRangeUtil &) = delete;
  TipRangeUtil &operator=(const TipRangeUtil &) = delete;

  // Sets the specified |range| into |context|.
  // Returns the general result code.
  static HRESULT SetSelection(ITfContext *context, TfEditCookie edit_cookie,
                              ITfRange *range, TfActiveSelEnd active_sel_end);

  // Retrieves the default selection from |context| into |range|.
  // Returns the general result code.
  static HRESULT GetDefaultSelection(ITfContext *context,
                                     TfEditCookie edit_cookie, ITfRange **range,
                                     TfActiveSelEnd *active_sel_end);

  // Retrieves the text from |range| into |text|.
  // Returns the general result code.
  static HRESULT GetText(ITfRange *range, TfEditCookie edit_cookie,
                         std::wstring *text);

  // Retrieves the input scopes from |range| into |input_scopes|.
  // Returns the general result code.
  static HRESULT GetInputScopes(ITfRange *range, TfEditCookie read_cookie,
                                std::vector<InputScope> *input_scopes);

  // Retrieves whether the specified |range| is for vertical writing or not.
  // Returns the general result code.
  static HRESULT IsVerticalWriting(ITfRange *range, TfEditCookie read_cookie,
                                   bool *vertical_writing);

  // Checks whether or not |range_test| becomes a subset of |range_cover|.
  static bool IsRangeCovered(TfEditCookie edit_cookie, ITfRange *range_test,
                             ITfRange *range_cover);

  // Returns the result of ITfContextView::GetTextExt with a workaround for a
  // TSF bug which prevents an IME from receiving TF_E_NOLAYOUT error code
  // correctly from ITfContextView::GetTextExt. The workaround may or may not
  // work depending on the attached application implements
  // ITextStoreACP::GetACPFromPoint.
  static HRESULT GetTextExt(ITfContextView *context_view,
                            TfEditCookie read_cookie, ITfRange *range,
                            RECT *rect, bool *clipped);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_RANGE_UTIL_H_
