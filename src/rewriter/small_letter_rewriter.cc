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

#include "rewriter/small_letter_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {
// Here std::map is used instead of char* or other collections. Because these
// mapping can be extended for other letters like '+' or 'a', implementation
// based on array will not work in the future. In order to avoid that,
// std::map is chosen.
const auto* kSuperscriptTable =
    new absl::flat_hash_map<char, absl::string_view>({
        {'0', "⁰"},
        {'1', "¹"},
        {'2', "²"},
        {'3', "³"},
        {'4', "⁴"},
        {'5', "⁵"},
        {'6', "⁶"},
        {'7', "⁷"},
        {'8', "⁸"},
        {'9', "⁹"},
        {'+', "⁺"},
        {'-', "⁻"},
        {'=', "⁼"},
        {'(', "⁽"},
        {')', "⁾"},
    });

const auto* kSubscriptTable = new absl::flat_hash_map<char, absl::string_view>({
    {'0', "₀"},
    {'1', "₁"},
    {'2', "₂"},
    {'3', "₃"},
    {'4', "₄"},
    {'5', "₅"},
    {'6', "₆"},
    {'7', "₇"},
    {'8', "₈"},
    {'9', "₉"},
    {'+', "₊"},
    {'-', "₋"},
    {'=', "₌"},
    {'(', "₍"},
    {')', "₎"},
});

enum ParserState : char {
  DEFAULT,
  SUPERSCRIPT_ALL,
  SUBSCRIPT_ALL,
  SUPERSCRIPT_DIGIT,
  SUBSCRIPT_DIGIT
};

// Converts given input into sequence containing subscripts and superscripts and
// store it in value. The return value becomes true, if value contains a
// transformed input. Otherwise it returns false and value should not be used.
// These are examples of conversion.
//  * x^2 -> x²
//  * CH_3 -> CH₃
//  * C_6H_12O_6 -> C₆H₁₂O₆
//  * O^2^- -> O²⁻
//  * x^^2_3 -> x^^2₃
// This function allows conversion of digits sequence. For example, _123 will be
// converted into ₁₂₃. Other symbols requires prefix as `^+` or `_(` for each
// occurrence. `^()` does not mean ⁽⁾ but means ⁽).
bool ConvertExpressions(const absl::string_view input, std::string* value) {
  // Check preconditions
  if (input.empty()) {
    return false;
  }

  ParserState state = DEFAULT;

  // The interest of this loop is only ASCII characters, and they never appear
  // in two-or-more-byte characters. Therefore, here simply char is used instead
  // of char32_t with Util::Utf8ToCodepoints.
  for (const char c : input) {
    switch (state) {
      case DEFAULT:
        if (c == '^') {
          state = SUPERSCRIPT_ALL;
        } else if (c == '_') {
          state = SUBSCRIPT_ALL;
        } else {
          value->push_back(c);
        }
        break;
      case SUPERSCRIPT_ALL:
        if (absl::ascii_isdigit(c)) {
          absl::StrAppend(value, kSuperscriptTable->at(c));
          state = SUPERSCRIPT_DIGIT;
        } else if (const auto it = kSuperscriptTable->find(c);
                   it != kSuperscriptTable->end()) {
          absl::StrAppend(value, it->second);
          state = DEFAULT;
        } else {
          value->push_back('^');
          value->push_back(c);
          state = DEFAULT;
        }
        break;
      case SUBSCRIPT_ALL:
        if (absl::ascii_isdigit(c)) {
          absl::StrAppend(value, kSubscriptTable->at(c));
          state = SUBSCRIPT_DIGIT;
        } else if (const auto it = kSubscriptTable->find(c);
                   it != kSubscriptTable->end()) {
          absl::StrAppend(value, it->second);
          state = DEFAULT;
        } else {
          value->push_back('_');
          value->push_back(c);
          state = DEFAULT;
        }
        break;
      case SUPERSCRIPT_DIGIT:
        if (absl::ascii_isdigit(c)) {
          absl::StrAppend(value, kSuperscriptTable->at(c));
        } else if (c == '^') {
          state = SUPERSCRIPT_ALL;
        } else if (c == '_') {
          state = SUBSCRIPT_ALL;
        } else {
          value->push_back(c);
          state = DEFAULT;
        }
        break;
      case SUBSCRIPT_DIGIT:
        if (absl::ascii_isdigit(c)) {
          absl::StrAppend(value, kSubscriptTable->at(c));
        } else if (c == '^') {
          state = SUPERSCRIPT_ALL;
        } else if (c == '_') {
          state = SUBSCRIPT_ALL;
        } else {
          value->push_back(c);
          state = DEFAULT;
        }
        break;
    }
  }
  if (state == SUPERSCRIPT_ALL) {
    value->push_back('^');
  } else if (state == SUBSCRIPT_ALL) {
    value->push_back('_');
  }

  // If not has conversion, it should not be added as candidate.
  return input != *value;
}

void AddCandidate(std::string key, std::string description, std::string value,
                  int index, Segment* segment) {
  DCHECK(segment);

  if (index < 0 || index > segment->candidates_size()) {
    index = segment->candidates_size();
  }

  converter::Candidate* candidate = segment->insert_candidate(index);
  DCHECK(candidate);

  segment->set_key(key);
  candidate->key = std::move(key);
  candidate->value = value;
  candidate->content_value = std::move(value);
  candidate->description = std::move(description);
  candidate->attributes |= (converter::Attribute::NO_LEARNING |
                            converter::Attribute::NO_VARIANTS_EXPANSION);
}

std::optional<std::string> GetValue(absl::string_view key) {
  std::string value;
  if (!ConvertExpressions(key, &value)) {
    return std::nullopt;
  }

  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}
}  // namespace

int SmallLetterRewriter::capability(const ConversionRequest& request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

std::optional<RewriterInterface::ResizeSegmentsRequest>
SmallLetterRewriter::CheckResizeSegmentsRequest(
    const ConversionRequest& request, const Segments& segments) const {
  if (segments.resized() || segments.conversion_segments_size() <= 1) {
    return std::nullopt;
  }

  absl::string_view key = request.key();
  const size_t key_len = Util::CharsLen(key);
  if (key_len > std::numeric_limits<uint8_t>::max()) {
    return std::nullopt;
  }
  const uint8_t segment_size = static_cast<uint8_t>(key_len);

  std::optional<std::string> value = GetValue(key);
  if (!value.has_value()) {
    return std::nullopt;
  }

  ResizeSegmentsRequest resize_request = {
      .segment_index = 0,
      .segment_sizes = {segment_size, 0, 0, 0, 0, 0, 0, 0},
  };
  return resize_request;
}

bool SmallLetterRewriter::Rewrite(const ConversionRequest& request,
                                  Segments* segments) const {
  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  absl::string_view key = request.key();
  std::optional<std::string> value = GetValue(key);
  if (!value.has_value()) {
    return false;
  }

  Segment* segment = segments->mutable_conversion_segment(0);

  // Candidates from this function should not be on high position. -1 will
  // overwritten with the last index of candidates.
  AddCandidate(std::string(key), "上下付き文字", std::move(value.value()), -1,
               segment);
  return true;
}

}  // namespace mozc
