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

#include "converter/reverse_converter.h"

#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace converter {
// Tries normalizing input text as a math expression, where full-width numbers
// and math symbols are converted to their half-width equivalents except for
// some special symbols, e.g., "×", "÷", and "・". Returns false if the input
// string contains non-math characters.
bool TryNormalizingKeyAsMathExpression(const absl::string_view s,
                                       std::string *key) {
  key->reserve(s.size());
  for (ConstChar32Iterator iter(s); !iter.Done(); iter.Next()) {
    // Half-width arabic numbers.
    if ('0' <= iter.Get() && iter.Get() <= '9') {
      key->append(1, static_cast<char>(iter.Get()));
      continue;
    }
    // Full-width arabic numbers ("０" -- "９")
    if (0xFF10 <= iter.Get() && iter.Get() <= 0xFF19) {
      const char c = iter.Get() - 0xFF10 + '0';
      key->append(1, c);
      continue;
    }
    switch (iter.Get()) {
      case 0x002B:
      case 0xFF0B:  // "+", "＋"
        key->append(1, '+');
        break;
      case 0x002D:
      case 0x30FC:  // "-", "ー"
        key->append(1, '-');
        break;
      case 0x002A:
      case 0xFF0A:
      case 0x00D7:  // "*", "＊", "×"
        key->append(1, '*');
        break;
      case 0x002F:
      case 0xFF0F:
      case 0x30FB:
      case 0x00F7:
        // "/",  "／", "・", "÷"
        key->append(1, '/');
        break;
      case 0x0028:
      case 0xFF08:  // "(", "（"
        key->append(1, '(');
        break;
      case 0x0029:
      case 0xFF09:  // ")", "）"
        key->append(1, ')');
        break;
      case 0x003D:
      case 0xFF1D:  // "=", "＝"
        key->append(1, '=');
        break;
      default:
        return false;
    }
  }
  return true;
}

ReverseConverter::ReverseConverter(
    const ImmutableConverterInterface &immutable_converter)
    : immutable_converter_(immutable_converter) {}

bool ReverseConverter::ReverseConvert(absl::string_view key,
                                      Segments *segments) const {
  // Check if |key| looks like a math expression.  In such case, there's no
  // chance to get the correct reading by the immutable converter.  Rather,
  // simply returns normalized value.
  {
    std::string value;
    if (TryNormalizingKeyAsMathExpression(key, &value)) {
      Candidate *cand = segments->mutable_segment(0)->push_back_candidate();
      strings::Assign(cand->key, key);
      cand->value = std::move(value);
      return true;
    }
  }

  const ConversionRequest default_request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::REVERSE_CONVERSION})
          .Build();
  if (!immutable_converter_.ConvertForRequest(default_request, segments)) {
    return false;
  }
  if (segments->segments_size() == 0) {
    LOG(WARNING) << "no segments from reverse conversion";
    return false;
  }
  for (const Segment &seg : *segments) {
    if (seg.candidates_size() == 0 || seg.candidate(0).value.empty()) {
      segments->Clear();
      LOG(WARNING) << "got an empty segment from reverse conversion";
      return false;
    }
  }
  return true;
}

}  // namespace converter
}  // namespace mozc
