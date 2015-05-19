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

#include "rewriter/unicode_rewriter.h"

#include <cctype>
#include <string>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/conversion_request.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"

namespace mozc {

UnicodeRewriter::UnicodeRewriter(const ConverterInterface *parent_converter)
    : parent_converter_(parent_converter) {
  DCHECK(parent_converter_);
}

UnicodeRewriter::~UnicodeRewriter() {}

namespace {

// Checks given string is ucs4 expression or not.
bool IsValidUCS4Expression(const string &input) {
  if (input.size() < 3 || input.size() > 8) {
    return false;
  }

  if (!Util::StartsWith(input, "U+")) {
    return false;
  }

  const string hexcode(input, 2, input.npos);

  for (size_t i = 0; i < hexcode.size(); ++i) {
    if (!::isxdigit(hexcode.at(i))) {
      return false;
    }
  }
  return true;
}

// Converts given string to 32bit unsigned integer.
bool UCS4ExpressionToInteger(const string &input, uint32 *ucs4) {
  DCHECK(ucs4);
  const string hexcode(input, 2, input.npos);
  return NumberUtil::SafeHexStrToUInt32(hexcode, ucs4);
}

// Checks if given ucs4 value is acceptable or not.
bool IsAcceptableUnicode(const uint32 ucs4) {
  // Unicode character should be less than 0x10FFFF.
  if (ucs4 > 0x10FFFF) {
    return false;
  }

  // Control characters are not acceptable.  0x7F is DEL.
  if (ucs4 < 0x20 || (0x7F <= ucs4 && ucs4 <= 0x9F)) {
    return false;
  }

  // Bidirectional text control are not acceptable.
  // See: http://en.wikipedia.org/wiki/Unicode_control_characters
  if (ucs4 == 0x200E || ucs4 == 0x200F || (0x202A <= ucs4 && ucs4 <= 0x202E)) {
    return false;
  }

  return true;
}

void AddCandidate(
    const string &key, const string &value, int index, Segment *segment) {
  DCHECK(segment);

  if (index > segment->candidates_size()) {
    index = segment->candidates_size();
  }

  Segment::Candidate *candidate = segment->insert_candidate(index);
  DCHECK(candidate);

  candidate->Init();
  segment->set_key(key);
  candidate->key = key;
  candidate->value = value;
  candidate->content_value = value;
  // "Unicode 変換(" + key + ")"
  candidate->description = "Unicode \xE5\xA4\x89\xE6\x8F\x9B (" + key + ")";
  candidate->attributes |= (Segment::Candidate::NO_LEARNING |
                            Segment::Candidate::NO_VARIANTS_EXPANSION);
}
}  // namespace

// If the key is a single unicode character, the correspoinding
// Unicode "U+xxxx" format is added. (ex. "A" -> "U+0041").  This is
// triggered on reverse conversion only.
bool UnicodeRewriter::RewriteToUnicodeCharFormat(
    const ConversionRequest &request,
    Segments *segments) const {
  if (!request.has_composer()) {
    return false;
  }

  if (request.composer().source_text().empty() ||
      segments->conversion_segments_size() != 1) {
    return false;
  }

  const string &source_text = request.composer().source_text();
  const size_t source_text_size = Util::CharsLen(source_text);
  if (source_text_size != 1) {
    return false;
  }

  const string &source_char = request.composer().source_text();
  size_t mblen = 0;
  const char32 ucs4 = Util::UTF8ToUCS4(source_char.data(),
                                       source_char.data() + source_char.size(),
                                       &mblen);
  const string value = Util::StringPrintf("U+%04X", ucs4);

  const string &key = segments->conversion_segment(0).key();
  Segment *segment = segments->mutable_conversion_segment(0);
  AddCandidate(key, value, 5, segment);
  return true;
}

// If the key is in the "U+xxxx" format, the corresponding Unicode
// character is added. (ex. "U+0041" -> "A").
bool UnicodeRewriter::RewriteFromUnicodeCharFormat(
    const ConversionRequest &request,
    Segments *segments) const {
  string key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    key += segments->conversion_segment(i).key();
  }

  if (!IsValidUCS4Expression(key)) {
    return false;
  }

  uint32 ucs4 = 0;
  if (!UCS4ExpressionToInteger(key, &ucs4)) {
    return false;
  }

  if (!IsAcceptableUnicode(ucs4)) {
    return false;
  }

  string value;
  Util::UCS4ToUTF8(ucs4, &value);
  if (value.empty()) {
    return false;
  }

  if (segments->conversion_segments_size() > 1) {
    if (segments->resized()) {
      // The given segments are resized by user so don't modify anymore.
      return false;
    }

    const uint32 resize_len = Util::CharsLen(key) -
        Util::CharsLen(segments->conversion_segment(0).key());
    if (!parent_converter_->ResizeSegment(segments, request, 0, resize_len)) {
      return false;
    }
  }
  DCHECK_EQ(1, segments->conversion_segments_size());

  Segment *segment = segments->mutable_conversion_segment(0);
  AddCandidate(key, value, 0, segment);
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
