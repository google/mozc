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

#include "rewriter/unicode_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

// Checks given string is codepoint expression or not.
bool IsValidCodepointExpression(const absl::string_view input) {
  if (input.size() < 3 || input.size() > 8) {
    return false;
  }

  if (!absl::StartsWith(input, "U+")) {
    return false;
  }
  return absl::c_all_of(input.substr(2),
                        [](char c) { return absl::ascii_isxdigit(c); });
}

// Converts given string to 32bit unsigned integer.
bool UCS4ExpressionToInteger(const absl::string_view input,
                             uint32_t *codepoint) {
  DCHECK(codepoint);
  return absl::SimpleHexAtoi(input.substr(2), codepoint);
}

void AddCandidate(absl::string_view key, absl::string_view value, int index,
                  Segment *segment) {
  DCHECK(segment);

  if (index > segment->candidates_size()) {
    index = segment->candidates_size();
  }

  Segment::Candidate *candidate = segment->insert_candidate(index);
  DCHECK(candidate);

  segment->set_key(key);
  candidate->key = key;
  candidate->value = value;
  candidate->content_value = value;
  candidate->description = absl::StrCat("Unicode 変換 (", candidate->key, ")");
  // NO_MODIFICATION is required here, in order to escape
  // EnvironmentalFilterRewriter. Otherwise, some candidates from
  // UnicodeRewriter will be removed because they are unrenderable.
  candidate->attributes |= (Segment::Candidate::NO_LEARNING |
                            Segment::Candidate::NO_VARIANTS_EXPANSION |
                            Segment::Candidate::NO_MODIFICATION);
}
}  // namespace

// If the key is a single unicode character, the corresponding
// Unicode "U+xxxx" format is added. (ex. "A" -> "U+0041").  This is
// triggered on reverse conversion only.
bool UnicodeRewriter::RewriteToUnicodeCharFormat(
    const ConversionRequest &request, Segments *segments) const {
  if (request.composer().source_text().empty() ||
      segments->conversion_segments_size() != 1) {
    return false;
  }

  absl::string_view source_text = request.composer().source_text();
  const size_t source_text_size = Util::CharsLen(source_text);
  if (source_text_size != 1) {
    return false;
  }

  absl::string_view source_char = request.composer().source_text();
  const char32_t codepoint = Util::Utf8ToCodepoint(source_char);
  std::string value = absl::StrFormat("U+%04X", codepoint);

  absl::string_view key = segments->conversion_segment(0).key();
  Segment *segment = segments->mutable_conversion_segment(0);
  AddCandidate(key, value, 5, segment);
  return true;
}

namespace {
std::optional<std::string> GetValue(absl::string_view key) {
  if (!IsValidCodepointExpression(key)) {
    return std::nullopt;
  }

  uint32_t codepoint = 0;
  if (!UCS4ExpressionToInteger(key, &codepoint)) {
    return std::nullopt;
  }

  if (!Util::IsAcceptableCharacterAsCandidate(codepoint)) {
    return std::nullopt;
  }

  std::string value = Util::CodepointToUtf8(codepoint);
  if (value.empty()) {
    return std::nullopt;
  }

  return value;
}
}  // namespace

std::optional<RewriterInterface::ResizeSegmentsRequest>
UnicodeRewriter::CheckResizeSegmentsRequest(const ConversionRequest &request,
                                            const Segments &segments) const {
  if (segments.resized() || segments.conversion_segments_size() <= 1) {
    // The given segments are already resized.
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

// If the key is in the "U+xxxx" format, the corresponding Unicode
// character is added. (ex. "U+0041" -> "A").
bool UnicodeRewriter::RewriteFromUnicodeCharFormat(
    const ConversionRequest &request, Segments *segments) const {
  if (segments->conversion_segments_size() != 1) {
    return false;
  }

  absl::string_view key = request.key();
  std::optional<std::string> value = GetValue(key);
  if (!value.has_value()) {
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);
  AddCandidate(key, value.value(), 0, segment);
  return true;
}

bool UnicodeRewriter::Rewrite(const ConversionRequest &request,
                              Segments *segments) const {
  DCHECK(segments);
  // "A" -> "U+0041" (Reverse conversion only).
  if (RewriteToUnicodeCharFormat(request, segments)) {
    return true;
  }

  // "U+0041" -> "A"
  if (RewriteFromUnicodeCharFormat(request, segments)) {
    return true;
  }

  return false;
}

}  // namespace mozc
