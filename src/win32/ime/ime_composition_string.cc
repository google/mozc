// Copyright 2010-2012, Google Inc.
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

#include "win32/ime/ime_composition_string.h"

#include <ime.h>
#include <strsafe.h>

#include "google/protobuf/stubs/common.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "win32/base/immdev.h"
#include "win32/base/string_util.h"
#include "win32/ime/ime_scoped_context.h"
#include "win32/ime/ime_message_queue.h"

namespace mozc {
namespace win32 {
namespace {
const DWORD kPreeditUpdateFlags =
    (GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
     GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART);
const DWORD kResultUpdateFlags =
    (GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE | GCS_RESULTSTR |
     GCS_RESULTCLAUSE);
const DWORD kPreeditAndResultUpdateFlags =
    (kPreeditUpdateFlags | kResultUpdateFlags);
}  // anonymous namespace

bool CompositionString::Initialize() {
  ::ZeroMemory(this, sizeof(*this));
  info.dwSize = sizeof(CompositionString);

  // Composition string.
  info.dwCompStrOffset = offsetof(CompositionString, composition_);
  info.dwCompAttrOffset = offsetof(CompositionString, composition_attribute_);
  info.dwCompClauseOffset = offsetof(CompositionString, composition_clause_);

  // Composition reading string.
  info.dwCompReadStrOffset = offsetof(CompositionString, composition_reading_);
  info.dwCompReadAttrOffset =
      offsetof(CompositionString, composition_reading_attribute_);
  info.dwCompReadClauseOffset =
      offsetof(CompositionString, composition_reading_clause_);

  // Result.
  info.dwResultStrOffset = offsetof(CompositionString, result_);
  info.dwResultClauseOffset = offsetof(CompositionString, result_clause_);

  // Result reading.
  info.dwResultReadStrOffset = offsetof(CompositionString, result_reading_);
  info.dwResultReadClauseOffset =
      offsetof(CompositionString, result_reading_clause_);
  return true;
}

bool CompositionString::Update(
    const mozc::commands::Output &output, vector<UIMessage> *messages) {
  CompositionString prev_composition;
  ::CopyMemory(&prev_composition, this, sizeof(CompositionString));

  Initialize();

  DWORD update_flags = 0;
  if (!UpdateInternal(output, &update_flags)) {
    return false;
  }

  const bool is_oneshot_composition =
      (prev_composition.info.dwCompStrLen == 0) &&
      (info.dwCompStrLen == 0) &&
      (info.dwResultReadStrLen > 0);

  const bool prev_has_composition = (prev_composition.info.dwCompStrLen > 0);
  const bool has_composition = (info.dwCompStrLen > 0);

  // Check start composition.
  if (!prev_has_composition && (is_oneshot_composition || has_composition)) {
    messages->push_back(UIMessage(WM_IME_STARTCOMPOSITION, 0, 0));
  }

  {
    if (update_flags != 0) {
      // TODO(yukawa): support wparam of WM_IME_COMPOSITION.
      messages->push_back(UIMessage(WM_IME_COMPOSITION, 0, update_flags));
    }
  }

  // Check end composition.
  if ((prev_has_composition && !has_composition) || is_oneshot_composition) {
    // In OOo, we need this message to restore caret status.
    // We should not send this null-WM_IME_COMPOSITION when
    // |info.dwResultStrLen != 0|.  Otherwise, the result string will be
    // commited twice in wordpad.exe.
    if (info.dwResultStrLen == 0) {
      messages->push_back(UIMessage(WM_IME_COMPOSITION, 0, 0));
    }

    messages->push_back(UIMessage(WM_IME_ENDCOMPOSITION, 0, 0));
  }

  return true;
}

bool CompositionString::HandleResult(const mozc::commands::Output &output) {
  if (!output.has_result()) {
    return true;
  }

  HRESULT result = S_OK;

  wstring result_string;
  mozc::Util::UTF8ToWide(output.result().value(), &result_string);
  result = ::StringCchCopyN(result_,
                            ARRAYSIZE(result_),
                            result_string.c_str(),
                            ARRAYSIZE(result_));
  if (FAILED(result)) {
    return false;
  }
  info.dwResultStrLen = result_string.size();

  // Since the Mozc server does not support clause information for the
  // result string, we always declare the result string to be one segment.
  // TODO(yukawa): Set clause after b/3135804 is implemented.
  COMPILE_ASSERT(ARRAYSIZE(result_reading_clause_) >= 2,
                 RESULT_CLAUSE_ARRAY_IS_TOO_SMALL);
  info.dwResultClauseLen = sizeof(result_clause_[0]) +
                           sizeof(result_clause_[1]);
  result_clause_[0] = 0;
  result_clause_[1] = info.dwResultStrLen;

  if (output.result().has_key()) {
    // Reading string should be stored as half-width katakana like
    // other major IMEs.  See b/1793283 for details.
    const wstring &reading_string =
        StringUtil::KeyToReading(output.result().key());
    result = ::StringCchCopyN(result_reading_,
                              ARRAYSIZE(result_reading_),
                              reading_string.c_str(),
                              ARRAYSIZE(result_reading_));
    if (FAILED(result)) {
      return false;
    }
    info.dwResultReadStrLen = reading_string.size();

    // Some applications such as Excel 2003 do not use the result string
    // unless clause information is also available. (b/2959222)
    // Since the Mozc server does not return clause information for the
    // result string, we always declare the result string to be one segment.
    // TODO(yukawa): Set clause after b/3135804 is implemented.
    COMPILE_ASSERT(ARRAYSIZE(result_reading_clause_) >= 2,
                   RESULT_READING_CLAUSE_ARRAY_IS_TOO_SMALL);
    info.dwResultReadClauseLen = sizeof(result_reading_clause_[0]) +
                                 sizeof(result_reading_clause_[1]);
    result_reading_clause_[0] = 0;
    result_reading_clause_[1] = info.dwResultReadStrLen;
  }

  return true;
}

bool CompositionString::HandlePreedit(const mozc::commands::Output &output) {
  if (!output.has_preedit()) {
    return true;
  }

  const mozc::commands::Preedit &preedit = output.preedit();

  vector<BYTE> reading_attributes;
  vector<DWORD> reading_clauses;
  reading_clauses.push_back(0);

  vector<BYTE> composition_attributes;
  vector<DWORD> composition_clauses;
  composition_clauses.push_back(0);

  wstring reading_string;
  wstring composition_string;

  // As filed in b/2962397, we should use ATTR_CONVERTED as default
  // attribute when the preedit state is 'Convert' ("変換") or 'Prediction'
  // ("サジェスト選択中").  Fortunately, these states can be identified
  // with |has_highlighted_position()| for the moment.  This strategy also
  // satisfies the requirement of b/2955151.
  const BYTE default_attribute =
      (preedit.has_highlighted_position() ? ATTR_CONVERTED : ATTR_INPUT);

  string preedit_utf8;
  for (size_t segment_index = 0;
       segment_index < preedit.segment_size(); ++segment_index) {
    const mozc::commands::Preedit::Segment &segment =
        preedit.segment(segment_index);
    if (segment.has_key()) {
      // Reading string should be stored as half-width katakana like
      // other major IMEs.  See b/1793283 for details.
      const wstring &segment_reading =
          StringUtil::KeyToReading(segment.key());
      reading_string.append(segment_reading);
      for (size_t i = 0; i < segment_reading.size(); ++i) {
        switch (segment.annotation()) {
          case mozc::commands::Preedit::Segment::HIGHLIGHT:
            reading_attributes.push_back(ATTR_TARGET_CONVERTED);
            break;
          case mozc::commands::Preedit::Segment::UNDERLINE:
          case mozc::commands::Preedit::Segment::NONE:
          default:
            reading_attributes.push_back(default_attribute);
            break;
        }
      }
    }
    reading_clauses.push_back(reading_string.size());
    DCHECK(segment.has_value());
    {
      wstring segment_composition;
      mozc::Util::UTF8ToWide(segment.value(), &segment_composition);
      composition_string.append(segment_composition);
      preedit_utf8.append(segment.value());

      for (size_t i = 0; i < segment_composition.size(); ++i) {
        switch (segment.annotation()) {
          case mozc::commands::Preedit::Segment::HIGHLIGHT:
            composition_attributes.push_back(ATTR_TARGET_CONVERTED);
            break;
          case mozc::commands::Preedit::Segment::UNDERLINE:
          case mozc::commands::Preedit::Segment::NONE:
          default:
            composition_attributes.push_back(default_attribute);
            break;
        }
      }
    }
    composition_clauses.push_back(composition_string.size());
  }

  if (preedit.has_cursor()) {
    // |info.dwCursorPos| is supposed to be wide character index but
    // |preedit.cursor()| is the number of Unicode characters. In case
    // surrogate pair appears, use Util::WideCharsLen to calculate the
    // cursor position as wide character index. See b/4163234 for details.
    info.dwCursorPos = Util::WideCharsLen(
        Util::SubString(preedit_utf8, 0, preedit.cursor()));
  }

  if (preedit.has_highlighted_position()) {
    // Calculate the wide char index of the highlight segment so that
    // prediction/candidate windows are aligned to the highlight segment.
    // Note that |preedit.cursor()| is the number of Unicode characters.
    // In case surrogate pair appears, use Util::WideCharsLen to calculate
    // the highlighted position as wide character index.
    // See b/4163234 for details.
    const size_t highlighted_position_as_wchar_index = Util::WideCharsLen(
        Util::SubString(preedit_utf8, 0, preedit.highlighted_position()));

    focused_character_index_ = highlighted_position_as_wchar_index;

    // TODO(yukawa): do not update cursor pos here if target application
    //   suppors IMECHARPOSITION protocol.
    info.dwCursorPos = highlighted_position_as_wchar_index;
  }

  // Currently we can assume the suggest window is always aligned to the
  // first character in the preedit.  Perhaps we might want to have a
  // dedicated field for this purpose in future.
  if (output.has_candidates() && output.candidates().has_category() &&
      output.candidates().category() == commands::SUGGESTION) {
    focused_character_index_ = 0;
  }

  // Always set 0 to |dwDeltaStart| so that Excel updates composition.
  // See b/2959161 for details.
  // TODO(yukawa): Optimize this values so that Excel can optimize redraw
  //   region in the composition string.
  // TODO(yukawa): Use Util::WideCharsLen to support surrogate-pair.
  info.dwDeltaStart = 0;

  DCHECK_EQ(composition_string.size(), composition_attributes.size());
  info.dwCompAttrLen = composition_attributes.size();
  for (size_t i = 0; i < composition_attributes.size(); ++i) {
    composition_attribute_[i] = composition_attributes[i];
  }

  DCHECK_EQ(reading_string.size(), reading_attributes.size());
  info.dwCompReadAttrLen = reading_attributes.size();
  for (size_t i = 0; i < reading_attributes.size(); ++i) {
    composition_reading_attribute_[i] = reading_attributes[i];
  }

  HRESULT result = S_OK;
  result = ::StringCchCopyN(composition_,
                            ARRAYSIZE(composition_),
                            composition_string.c_str(),
                            ARRAYSIZE(composition_));
  if (FAILED(result)) {
    return false;
  }
  info.dwCompStrLen = composition_string.size();

  result = ::StringCchCopyN(composition_reading_,
                            ARRAYSIZE(composition_reading_),
                            reading_string.c_str(),
                            ARRAYSIZE(composition_reading_));
  if (FAILED(result)) {
    return false;
  }
  info.dwCompReadStrLen = reading_string.size();

  if (ARRAYSIZE(composition_clause_) <= composition_clauses.size()) {
    return false;
  }
  info.dwCompClauseLen = composition_clauses.size() * sizeof(DWORD);
  for (size_t i = 0; i < composition_clauses.size(); ++i) {
    composition_clause_[i] = composition_clauses[i];
  }

  if (ARRAYSIZE(composition_reading_clause_) <= reading_clauses.size()) {
    return false;
  }
  info.dwCompReadClauseLen = reading_clauses.size() * sizeof(DWORD);
  for (size_t i = 0; i < reading_clauses.size(); ++i) {
    composition_reading_clause_[i] = reading_clauses[i];
  }

  return true;
}

bool CompositionString::UpdateInternal(const mozc::commands::Output &output,
                                       DWORD *update) {
  info.dwCursorPos = -1;

  if (output.has_result() && !HandleResult(output)) {
    return false;
  }

  if (output.has_preedit() && !HandlePreedit(output)) {
    return false;
  }

  // We always set update flags as predefined combination regardless of which
  // field is actually updated.  Otherwise, some applications such as wordpad
  // OOo Writer 3.0 will update neither composition window nor caret state
  // properly.
  if (output.has_preedit() && output.has_result()) {
    // This situation actually occurs when you type a printable character in
    // candidate selection mode.
    // This situation also occurs by partial commit.
    *update = kPreeditAndResultUpdateFlags;
  } else if (output.has_preedit()) {
    *update = kPreeditUpdateFlags;
  } else if (output.has_result()) {
    *update = kResultUpdateFlags;
  }

  return true;
}

DWORD CompositionString::focused_character_index() const {
  return focused_character_index_;
}
}  // namespace win32
}  // namespace mozc
