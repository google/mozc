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

#include <map>
#include <string>
#include <vector>

#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {
// Here std::map is used instead of char* or other collections. Because these
// mapping can be extended for other letters like '+' or 'a', implementation
// based on array will not work in the future. In order to avoid that,
// std::map is chosen.
const std::map<char, absl::string_view> *kSuperscriptTable =
    new std::map<char, absl::string_view>({
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

const std::map<char, absl::string_view> *kSubscriptTable =
    new std::map<char, absl::string_view>({
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
bool ConvertExpressions(const std::string &input, std::string *value) {
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
        if (isdigit(c)) {
          value->append(kSuperscriptTable->at(c).data(),
                        kSuperscriptTable->at(c).size());
          state = SUPERSCRIPT_DIGIT;
        } else if (kSuperscriptTable->find(c) != kSuperscriptTable->end()) {
          value->append(kSuperscriptTable->at(c).data(),
                        kSuperscriptTable->at(c).size());
          state = DEFAULT;
        } else {
          value->push_back('^');
          value->push_back(c);
          state = DEFAULT;
        }
        break;
      case SUBSCRIPT_ALL:
        if (isdigit(c)) {
          value->append(kSubscriptTable->at(c).data(),
                        kSubscriptTable->at(c).size());
          state = SUBSCRIPT_DIGIT;
        } else if (kSubscriptTable->find(c) != kSubscriptTable->end()) {
          value->append(kSubscriptTable->at(c).data(),
                        kSubscriptTable->at(c).size());
          state = DEFAULT;
        } else {
          value->push_back('_');
          value->push_back(c);
          state = DEFAULT;
        }
        break;
      case SUPERSCRIPT_DIGIT:
        if (isdigit(c)) {
          value->append(kSuperscriptTable->at(c).data(),
                        kSuperscriptTable->at(c).size());
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
        if (isdigit(c)) {
          value->append(kSubscriptTable->at(c).data(),
                        kSubscriptTable->at(c).size());
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

bool EnsureSingleSegment(const ConversionRequest &request, Segments *segments,
                         const ConverterInterface *parent_converter,
                         const std::string &key) {
  if (segments->conversion_segments_size() == 1) {
    return true;
  }

  if (segments->resized()) {
    // The given segments are resized by user so don't modify anymore.
    return false;
  }

  const uint32_t resize_len =
      Util::CharsLen(key) -
      Util::CharsLen(segments->conversion_segment(0).key());
  if (!parent_converter->ResizeSegment(segments, request, 0, resize_len)) {
    return false;
  }
  DCHECK_EQ(1, segments->conversion_segments_size());
  return true;
}

void AddCandidate(const std::string &key, const std::string &description,
                  const std::string &value, int index, Segment *segment) {
  DCHECK(segment);

  if (index < 0 || index > segment->candidates_size()) {
    index = segment->candidates_size();
  }

  Segment::Candidate *candidate = segment->insert_candidate(index);
  DCHECK(candidate);

  candidate->Init();
  segment->set_key(key);
  candidate->key = key;
  candidate->value = value;
  candidate->content_value = value;
  candidate->description = description;
  candidate->attributes |= (Segment::Candidate::NO_LEARNING |
                            Segment::Candidate::NO_VARIANTS_EXPANSION);
}

}  // namespace

SmallLetterRewriter::SmallLetterRewriter(
    const ConverterInterface *parent_converter)
    : parent_converter_(parent_converter) {
  DCHECK(parent_converter_);
}

SmallLetterRewriter::~SmallLetterRewriter() = default;

int SmallLetterRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool SmallLetterRewriter::Rewrite(const ConversionRequest &request,
                                  Segments *segments) const {
  std::string key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    key += segments->conversion_segment(i).key();
  }

  std::string value;
  if (!ConvertExpressions(key, &value)) {
    return false;
  }

  if (value.empty()) {
    return false;
  }

  if (!EnsureSingleSegment(request, segments, parent_converter_, key)) {
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);

  // Candidates from this function should not be on high position. -1 will
  // overwritten with the last index of candidates.
  AddCandidate(key, "上下付き文字", value, -1, segment);
  return true;
}

}  // namespace mozc
