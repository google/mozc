// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_WIN32_IME_IME_COMPOSITION_STRING_H_
#define MOZC_WIN32_IME_IME_COMPOSITION_STRING_H_

#include <windows.h>
#include <ime.h>

#include <vector>

#include "base/port.h"
#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST()
#include "win32/ime/ime_types.h"

namespace mozc {

namespace commands {
class KeyEvent;
class Output;
}  // namespace commands

namespace win32 {
// TODO(yukawa): back port this implementation to shared IME component.
struct CompositionString {
 public:
  bool Initialize();
  bool Update(const mozc::commands::Output &output,
              vector<UIMessage> *messages);

  // Returns |focused_character_index_|, which represents the index of wide
  // character where suggest/predict/candidate window is aligned.
  DWORD focused_character_index() const;

 private:
  bool UpdateInternal(const mozc::commands::Output &output, DWORD *update);
  bool HandleResult(const mozc::commands::Output &output);
  bool HandlePreedit(const mozc::commands::Output &output);

  static const int kMaxCompositionLength = 500;
  static const int kMaxCompositionClauseLength = kMaxCompositionLength + 1;
  static const int kMaxResultLength = kMaxCompositionLength;
  static const int kMaxResultClauseLength = kMaxResultLength + 1;

  COMPOSITIONSTRING info;

  // Represents the index of wide character where
  // suggest/predict/candidate window is aligned.
  DWORD focused_character_index_;

  // Composition.
  wchar_t composition_[kMaxCompositionLength];
  DWORD composition_clause_[kMaxCompositionClauseLength];
  BYTE composition_attribute_[kMaxCompositionLength];

  // Composition reading string.
  wchar_t composition_reading_[kMaxCompositionLength];
  DWORD composition_reading_clause_[kMaxCompositionClauseLength];
  BYTE composition_reading_attribute_[kMaxCompositionLength];

  // Result.
  wchar_t result_[kMaxResultLength];
  DWORD result_clause_[kMaxResultClauseLength];

  // Result reading string.
  wchar_t result_reading_[kMaxResultLength];
  DWORD result_reading_clause_[kMaxResultClauseLength];

  FRIEND_TEST(ImeCompositionStringTest, StartCompositionTest);
  FRIEND_TEST(ImeCompositionStringTest,
              EndCompositionWhenCompositionBecomesEmpty);
  FRIEND_TEST(ImeCompositionStringTest,
              EndCompositionWhenCompositionIsCommited);
  FRIEND_TEST(ImeCompositionStringTest,
              EndCompositionWhenCompositionIsCommitedWithPreedit);
  FRIEND_TEST(ImeCompositionStringTest, SpaceKeyWhenIMEIsTurnedOn_Issue3200585);
  FRIEND_TEST(ImeCompositionStringTest, Suggest);
  FRIEND_TEST(ImeCompositionStringTest, Predict);
  FRIEND_TEST(ImeCompositionStringTest, Convert);
  FRIEND_TEST(ImeCompositionStringTest, SurrogatePairSupport);
};
}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_IME_IME_COMPOSITION_STRING_H_
