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

#include "rewriter/a11y_description_rewriter.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/serialized_dictionary.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

bool InRange(char32_t codepoint, char32_t low, char32_t high) {
  return codepoint >= low && codepoint <= high;
}

}  // namespace

A11yDescriptionRewriter::CharacterType
A11yDescriptionRewriter::GetCharacterType(char32_t codepoint) const {
  if (InRange(codepoint, U'ぁ', U'ん') && codepoint != U'ゑ' &&
      codepoint != U'ゐ') {
    // 'ゐ', 'ゑ' are excluded.
    if (small_letter_set_.contains(codepoint)) {
      return HIRAGANA_SMALL_LETTER;
    }
    return HIRAGANA;
  }
  if (InRange(codepoint, U'ァ', U'ワ') || codepoint == U'ヲ' ||
      codepoint == U'ン') {
    // 'ヰ', 'ヱ', 'ヴ', 'ヵ', 'ヶ' are excluded.
    // codepoint order is "ワ,ヰ,ヱ,ヲ,ン,ヴ,ヵ,ヶ".
    if (small_letter_set_.contains(codepoint)) {
      return KATAKANA_SMALL_LETTER;
    }
    return KATAKANA;
  }
  if (InRange(codepoint, U'ｧ', U'ﾟ') || InRange(codepoint, U'ｦ', U'ｯ')) {
    // 　'ｱ' - 'ﾝ', '゛', 'ﾟ', 'ｦ' - 'ｯ'
    if (small_letter_set_.contains(codepoint)) {
      return HALF_WIDTH_KATAKANA_SMALL_LETTER;
    }
    return HALF_WIDTH_KATAKANA;
  }
  if (codepoint == U'ー') {
    return PROLONGED_SOUND_MARK;
  }
  if (InRange(codepoint, 'a', 'z')) {
    return HALF_ALPHABET_LOWER;
  }
  if (InRange(codepoint, 'A', 'Z')) {
    return HALF_ALPHABET_UPPER;
  }
  if (InRange(codepoint, U'ａ', U'ｚ')) {
    return FULL_ALPHABET_LOWER;
  }
  if (InRange(codepoint, U'Ａ', U'Ｚ')) {
    return FULL_ALPHABET_UPPER;
  }
  return OTHERS;
}

std::string A11yDescriptionRewriter::GetKanaCharacterLabel(
    char32_t codepoint, CharacterType current_type,
    CharacterType previous_type) const {
  std::string buf;
  if ((previous_type == current_type) &&
      (current_type != HIRAGANA_SMALL_LETTER &&
       current_type != KATAKANA_SMALL_LETTER &&
       current_type != HALF_WIDTH_KATAKANA_SMALL_LETTER)) {
    // The expected result of "あい" is "あい。 ヒラガナ あい",
    // thus the output of "い" should be "い" only rather than "ヒラガナ い".
    Util::CodepointToUtf8Append(codepoint, &buf);
    return buf;
  }
  absl::StrAppend(&buf, "。");
  switch (current_type) {
    case HIRAGANA:
      absl::StrAppend(&buf, "ヒラガナ ");
      break;
    case HIRAGANA_SMALL_LETTER:
      absl::StrAppend(&buf, "ヒラガナコモジ ");
      codepoint++;  // transform the character from small to normal
      break;
    case KATAKANA:
      absl::StrAppend(&buf, "カタカナ ");
      break;
    case KATAKANA_SMALL_LETTER:
      absl::StrAppend(&buf, "カタカナコモジ ");
      codepoint++;  // transform the character from small to normal
      break;
    case HALF_WIDTH_KATAKANA:
      absl::StrAppend(&buf, "ハンカクカタカナ ");
      break;
    case HALF_WIDTH_KATAKANA_SMALL_LETTER:
      absl::StrAppend(&buf, "ハンカクカタカナコモジ ");
      codepoint = half_width_small_katakana_to_large_katakana_.at(codepoint);
      break;
    case PROLONGED_SOUND_MARK:
      absl::StrAppend(&buf, "チョウオン ");
      break;
    case HALF_ALPHABET_LOWER:
      absl::StrAppend(&buf, "コモジ ");
      break;
    case HALF_ALPHABET_UPPER:
      absl::StrAppend(&buf, "オオモジ ");
      break;
    case FULL_ALPHABET_LOWER:
      absl::StrAppend(&buf, "ゼンカクコモジ ");
      break;
    case FULL_ALPHABET_UPPER:
      absl::StrAppend(&buf, "ゼンカクオオモジ ");
      break;
    default:
      break;
  }
  Util::CodepointToUtf8Append(codepoint, &buf);
  return buf;
}

A11yDescriptionRewriter::A11yDescriptionRewriter(
    const DataManager &data_manager)
    : small_letter_set_(
          {// Small hiragana
           U'ぁ', U'ぃ', U'ぅ', U'ぇ', U'ぉ', U'ゃ', U'ゅ', U'ょ', U'っ', U'ゎ',
           // Small katakana
           U'ァ', U'ィ', U'ゥ', U'ェ', U'ォ', U'ャ', U'ュ', U'ョ', U'ッ', U'ヮ',
           // Half-width small katakana
           U'ｧ', U'ｨ', U'ｩ', U'ｪ', U'ｫ', U'ｬ', U'ｭ', U'ｮ', U'ｯ'}),
      half_width_small_katakana_to_large_katakana_({
          {U'ｧ', U'ｱ'},
          {U'ｨ', U'ｲ'},
          {U'ｩ', U'ｳ'},
          {U'ｪ', U'ｴ'},
          {U'ｫ', U'ｵ'},
          {U'ｬ', U'ﾔ'},
          {U'ｭ', U'ﾕ'},
          {U'ｮ', U'ﾖ'},
          {U'ｯ', U'ﾂ'},
      }) {
  absl::string_view token_array_data, string_array_data;
  data_manager.GetA11yDescriptionRewriterData(&token_array_data,
                                              &string_array_data);
  description_map_ = (token_array_data.empty() || string_array_data.empty())
                         ? nullptr
                         : std::make_unique<SerializedDictionary>(
                               token_array_data, string_array_data);
}

void A11yDescriptionRewriter::AddA11yDescription(
    converter::Candidate *candidate) const {
  absl::string_view value = candidate->value;
  std::string buf(value);
  CharacterType previous_type = INITIAL_STATE;
  CharacterType current_type = INITIAL_STATE;
  std::vector<std::string> graphemes;
  Util::SplitStringToUtf8Graphemes(value, &graphemes);
  for (absl::string_view grapheme : graphemes) {
    const std::u32string codepoints = Util::Utf8ToUtf32(grapheme);
    for (const char32_t codepoint : codepoints) {
      previous_type = current_type;
      current_type = GetCharacterType(codepoint);
      if (current_type == OTHERS) {
        const std::string key = Util::CodepointToUtf8(codepoint);
        const SerializedDictionary::IterRange range =
            description_map_->equal_range(key);
        if (range.first != range.second) {
          // Add a punctuation for better Talkback result.
          absl::StrAppend(&buf, "。", range.first.value());
        }
        continue;
      }
      if ((current_type == PROLONGED_SOUND_MARK ||
           current_type == HIRAGANA_SMALL_LETTER ||
           current_type == KATAKANA_SMALL_LETTER) &&
          (previous_type == HIRAGANA || previous_type == KATAKANA)) {
        current_type = previous_type;
      }
      absl::StrAppend(
          &buf, GetKanaCharacterLabel(codepoint, current_type, previous_type));
    }
  }
  candidate->a11y_description = std::move(buf);
}

int A11yDescriptionRewriter::capability(
    const ConversionRequest &request) const {
  if (description_map_ && request.request().enable_a11y_description()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::NOT_AVAILABLE;
}

bool A11yDescriptionRewriter::Rewrite(const ConversionRequest &request,
                                      Segments *segments) const {
  bool modified = false;
  for (Segment &segment : segments->conversion_segments()) {
    for (size_t j = 0; j < segment.candidates_size(); ++j) {
      converter::Candidate *candidate = segment.mutable_candidate(j);
      AddA11yDescription(candidate);
      modified = true;
    }
  }
  return modified;
}

}  // namespace mozc
