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

#ifndef MOZC_WIN32_TIP_TIP_SURROUNDING_TEXT_H_
#define MOZC_WIN32_TIP_TIP_SURROUNDING_TEXT_H_

#include <msctf.h>

#include <cstddef>
#include <string>
#include <string_view>

#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

struct TipSurroundingTextInfo {
  std::wstring preceding_text;
  std::wstring selected_text;
  std::wstring following_text;
  bool has_preceding_text = false;
  bool has_selected_text = false;
  bool has_following_text = false;
  bool in_composition = false;  // context has a composition owned by Mozc.
};

class TipSurroundingText {
 public:
  TipSurroundingText() = delete;
  TipSurroundingText(const TipSurroundingText &) = delete;
  TipSurroundingText &operator=(const TipSurroundingText &) = delete;

  // Returns true when succeeds to retrieve surrounding text information
  // from the context specified by |context|.
  // Caveats: This method internally depends on synchronous edit session.
  //     You should call this method when and only when a synchronous edit
  //     session is guaranteed to be safe. A keyevent hander is one of
  //     examples. See the following document for details.
  //     http://blogs.msdn.com/b/tsfaware/archive/2007/05/17/rules-of-text-services.aspx
  static bool Get(TipTextService *text_service, ITfContext *context,
                  TipSurroundingTextInfo *info);

  // A variant of TipSurroundingText::Get. One difference is that this method
  // moves the anchor position of the selection at the end of the range.
  // Another difference is that this method uses IMM32 message when fails to
  // retrieve/update the selection.
  // In order to emulate the IMM32 reconversion, we need to use async edit
  // session if |need_async_reconversion| is set to be true. See
  // IMN_PRIVATE/kNotifyReconvertFromIME in IMM32-Mozc about IMM32 reconversion.
  // TODO(yukawa): Consider to unify this method with TipSurroundingText::Get.
  static bool PrepareForReconversionFromIme(TipTextService *text_service,
                                            ITfContext *context,
                                            TipSurroundingTextInfo *info,
                                            bool *need_async_reconversion);

  // Returns true when succeeds to delete preceding text from the beginning of
  // the selected range.
  // Caveats: |num_characters_to_be_deleted_in_codepoint| is not the number of
  //     elements in UTF16. Beware of surrogate pairs.
  // Caveats: This method internally depends on synchronous edit session.
  //     You should call this method when and only when a synchronous edit
  //     session is guaranteed to be safe. A keyevent hander is one of
  //     examples. See the following document for details.
  //     http://blogs.msdn.com/b/tsfaware/archive/2007/05/17/rules-of-text-services.aspx
  static bool DeletePrecedingText(
      TipTextService *text_service, ITfContext *context,
      size_t num_characters_to_be_deleted_in_codepoint);
};

class TipSurroundingTextUtil {
 public:
  // Returns true if |text| has more than |characters_in_codepoint| characters.
  // When succeeds, the last |*characters_in_utf16| characters in |text|
  // can be measured as |characters_in_codepoint| in the unit of UCS4.
  static bool MeasureCharactersBackward(std::wstring_view text,
                                        size_t characters_in_codepoint,
                                        size_t *characters_in_utf16);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_SURROUNDING_TEXT_H_
