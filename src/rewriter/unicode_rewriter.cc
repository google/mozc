// Copyright 2010-2011, Google Inc.
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

#include "base/util.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"

namespace mozc {

UnicodeRewriter::UnicodeRewriter() {}
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

  const string hexcode = input.substr(2, input.npos);

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
  const string hexcode = input.substr(2, input.npos);
  return Util::SafeHexStrToUInt32(hexcode, ucs4);
}

// Checks if given ucs4 value is acceptable or not.
bool IsAcceptableUnicode(uint32 ucs4) {
  if (Util::GetScriptType(ucs4) != Util::UNKNOWN_SCRIPT) {
    return true;
  }

  // Expands acceptable characters.
  // 0x20 to 0x7E is visible characters.
  if (ucs4 >= 0x20 && ucs4 <= 0x7E) {
    return true;
  }

  return false;
}

}  // namespace

bool UnicodeRewriter::Rewrite(Segments *segments) const {
  string key;
  DCHECK(segments);
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

  string output;
  Util::UCS4ToUTF8(ucs4, &output);

  if (output.empty()) {
    return false;
  }
  mozc::ConverterInterface *converter
      = mozc::ConverterFactory::GetConverter();
  if (converter == NULL) {
    return false;
  }

  if (segments->conversion_segments_size() > 1) {
    if (segments->resized()) {
      // The given segments are resized by user so don't modify anymore.
      return false;
    }
    const uint32 resize_len = Util::CharsLen(key) -
        Util::CharsLen(segments->conversion_segment(0).key());
    if (!converter->ResizeSegment(segments, 0, resize_len)) {
      return false;
    }
  }

  Segment* seg = segments->mutable_conversion_segment(0);
  DCHECK(seg);
  Segment::Candidate *candidate = seg->insert_candidate(0);
  DCHECK(candidate);

  candidate->Init();
  seg->set_key(key);
  candidate->key = key;
  candidate->value = output;
  candidate->content_value = output;
  // "Unicode 変換(" + key + ")"
  candidate->description = "Unicode \xE5\xA4\x89\xE6\x8F\x9B ("+key+")";
  candidate->attributes |= Segment::Candidate::NO_LEARNING;
  return true;
}

}  // namespace mozc
