// Copyright 2010-2013, Google Inc.
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

#include <string>

#include "base/port.h"

struct ITfContext;
struct ITfEditSession;

namespace mozc {

namespace commands {
class Output;
}  // namespace commands

namespace win32 {
namespace tsf {

class TipTextService;

struct TipSurroundingTextInfo {
  TipSurroundingTextInfo();

  wstring preceding_text;
  wstring selected_text;
  wstring following_text;
  bool has_preceding_text;
  bool has_selected_text;
  bool has_following_text;
  bool is_transitory;   // context is a transitory context
  bool in_composition;  // context has a composition owned by Mozc.
};

class TipSurroundingText {
 public:
  // Returns true when succeeds to retrieve surrounding text information
  // from the context specified by |context|.
  // Caveats: This method internally depends on synchronous edit session.
  //     You should call this method when and only when a synchronous edit
  //     session is guaranteed to be safe. A keyevent hander is one of
  //     examples. See the following document for details.
  //     http://blogs.msdn.com/b/tsfaware/archive/2007/05/17/rules-of-text-services.aspx
  static bool Get(TipTextService *text_service,
                  ITfContext *context,
                  TipSurroundingTextInfo *info);

  // A variant of TipSurroundingText::Get. One difference is that this method
  // moves the anchor position of the selection at the end of the range.
  // Another difference is that this method uses IMM32 message when fails to
  // retrieve/update the selection.
  // TODO(yukawa): Consider to unify this method with TipSurroundingText::Get.
  static bool PrepareForReconversion(TipTextService *text_service,
                                     ITfContext *context,
                                     TipSurroundingTextInfo *info);

  // Returns true when succeeds to delete preceeding text from the beginning of
  // the selected range.
  // Caveats: |num_characters_to_be_deleted_in_ucs4| is not the number of
  //     elements in UTF16. Beware of surrogate pairs.
  // Caveats: This method internally depends on synchronous edit session.
  //     You should call this method when and only when a synchronous edit
  //     session is guaranteed to be safe. A keyevent hander is one of
  //     examples. See the following document for details.
  //     http://blogs.msdn.com/b/tsfaware/archive/2007/05/17/rules-of-text-services.aspx
  static bool DeletePrecedingText(TipTextService *text_service,
                                  ITfContext *context,
                                  size_t num_characters_to_be_deleted_in_ucs4);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TipSurroundingText);
};

class TipSurroundingTextUtil {
 public:
  // Returns true if |text| has more than |characters_in_ucs4| characters.
  // When succeeds, the last |*characters_in_utf16| characters in |text|
  // can be measured as |characters_in_ucs4| in the unit of UCS4.
  static bool MeasureCharactersBackward(const wstring &text,
                                        size_t characters_in_ucs4,
                                        size_t *characters_in_utf16);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_SURROUNDING_TEXT_H_
