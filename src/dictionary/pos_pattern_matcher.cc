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

#include "dictionary/pos_pattern_matcher.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dictionary/pos_id_map.h"

namespace mozc::dictionary {
namespace {

struct Pattern {
  absl::string_view value;
  bool exact_match = false;

  bool Matches(absl::string_view pos_string) const {
    if (exact_match) {
      return pos_string == value;
    }
    return absl::StartsWith(pos_string, value);
  }
};

Pattern ParsePattern(absl::string_view pattern_str) {
  Pattern pattern;
  pattern_str = absl::StripAsciiWhitespace(pattern_str);
  if (absl::EndsWith(pattern_str, "$")) {
    pattern.exact_match = true;
    pattern_str.remove_suffix(1);
  }
  pattern.value = pattern_str;
  return pattern;
}

}  // namespace

PosPatternMatcher::PosPatternMatcher(absl::string_view pattern,
                                     const PosIdMap& pos_id_map) {
  BuildTable({pattern}, pos_id_map);
}

PosPatternMatcher::PosPatternMatcher(
    absl::Span<const absl::string_view> patterns, const PosIdMap& pos_id_map) {
  BuildTable(patterns, pos_id_map);
}

PosPatternMatcher::PosPatternMatcher(
    std::initializer_list<absl::string_view> patterns,
    const PosIdMap& pos_id_map) {
  BuildTable(patterns, pos_id_map);
}

void PosPatternMatcher::BuildTable(absl::Span<const absl::string_view> patterns,
                                   const PosIdMap& pos_id_map) {
  if (pos_id_map.GetPosIdCount() == 0 || patterns.empty()) {
    return;
  }

  std::vector<Pattern> compiled_patterns;
  compiled_patterns.reserve(patterns.size());
  for (absl::string_view pattern_str : patterns) {
    Pattern p = ParsePattern(pattern_str);
    if (!p.value.empty()) {
      compiled_patterns.push_back(std::move(p));
    }
  }

  if (compiled_patterns.empty()) {
    return;
  }

  // Cap pos_id_count to the maximum number of unique uint16_t values.
  constexpr size_t kMaxPosIdCount =
      static_cast<size_t>(std::numeric_limits<uint16_t>::max()) + 1;
  const size_t pos_id_count =
      std::min(pos_id_map.GetPosIdCount(), kMaxPosIdCount);
  table_.assign(pos_id_count, false);

  for (size_t pos_id = 0; pos_id < pos_id_count; ++pos_id) {
    const absl::string_view pos_string =
        pos_id_map.GetPosString(static_cast<uint16_t>(pos_id));
    if (pos_string.empty()) {
      continue;
    }
    for (const Pattern& pattern : compiled_patterns) {
      if (pattern.Matches(pos_string)) {
        table_[pos_id] = true;
        if (!first_id_.has_value()) {
          first_id_ = static_cast<uint16_t>(pos_id);
        }
        break;
      }
    }
  }
}

}  // namespace mozc::dictionary
