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

#ifndef MOZC_DICTIONARY_POS_PATTERN_MATCHER_H_
#define MOZC_DICTIONARY_POS_PATTERN_MATCHER_H_

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dictionary/pos_id_map.h"

namespace mozc::dictionary {

// Matches a POS ID against one or more POS pattern rules.
// Pattern evaluation and lookup table building are performed at construction
// time using PosIdMap. Matches are evaluated in O(1) time at runtime.
//
// Note: `pos_id_map` is only used during construction to build the internal
// lookup table and does not need to outlive this object.
//
// Pattern syntax for each pattern:
// - Prefix matching by default:
//   Patterns match POS strings starting with the pattern (absl::StartsWith).
//   For example, "名詞,一般," matches any POS string starting with
//   "名詞,一般,". Similarly, "動詞,自立,*,*,五段" matches
//   "動詞,自立,*,*,五段・カ行促音便,..." naturally because "五段" prefixes
//   "五段・...".
//
// - Empty pattern:
//   An empty pattern "" is skipped and matches nothing. It does NOT act as a
//   wildcard matching everything.
//
// - Delimiter boundary attention (trailing comma):
//   Because matching is strictly prefix-based, note that a pattern like
//   "名詞,数" will also match "名詞,数接続" since "名詞,数" is a prefix of
//   both. To match strictly at the POS field boundary, include a trailing
//   comma: e.g. "名詞,数," will match "名詞,数,*,..." but NOT
//   "名詞,数接続,...".
//
// - Exact match with trailing '$':
//   If a pattern ends with '$', it requires an exact string match with the
//   entire POS string (the trailing '$' is stripped before comparison).
//   This is essential for matching specific lexicalized POS entries at the
//   final (7th) field where there is no trailing comma:
//   e.g. "助詞,並立助詞,*,*,*,*,と$" matches "助詞,並立助詞,*,*,*,*,と" exactly
//   and avoids matching "助詞,並立助詞,*,*,*,*,とか".
// - Multiple patterns can be passed as a list (initializer_list or Span) to
//   match if ANY of the patterns match (logical OR).
class PosPatternMatcher {
 public:
  // Constructs an unavailable matcher (IsAvailable() == false). Match() always
  // returns false safely.
  PosPatternMatcher() = default;

  // Constructs a matcher for a single pattern.
  // Example: "名詞,一般,"
  explicit PosPatternMatcher(absl::string_view pattern,
                             const PosIdMap& pos_id_map);

  // Constructs a matcher for multiple patterns (logical OR).
  // Matches if any of the patterns match.
  // Example: PosPatternMatcher({"名詞,固有名詞,人名,", "助詞,"}, pos_id_map);
  explicit PosPatternMatcher(absl::Span<const absl::string_view> patterns,
                             const PosIdMap& pos_id_map);
  // Necessary to avoid ambiguity with the string_view overload when using
  // `{...}`
  explicit PosPatternMatcher(std::initializer_list<absl::string_view> patterns,
                             const PosIdMap& pos_id_map);

  PosPatternMatcher(const PosPatternMatcher&) = delete;
  PosPatternMatcher& operator=(const PosPatternMatcher&) = delete;
  PosPatternMatcher(PosPatternMatcher&&) noexcept = default;
  PosPatternMatcher& operator=(PosPatternMatcher&&) noexcept = default;

  ~PosPatternMatcher() = default;

  // Returns true if the matcher is available (i.e. built with non-empty
  // patterns and a valid non-empty PosIdMap). If false, Match() always returns
  // false safely.
  bool IsAvailable() const { return !table_.empty(); }

  // Returns true if the given POS ID matches any of the patterns.
  // Runs in O(1) time.
  bool Match(uint16_t pos_id) const {
    if (pos_id >= table_.size()) {
      return false;
    }
    return table_[pos_id];
  }

  // Returns the lowest matching POS ID numerically (numerically first in
  // PosIdMap) that matches any of the patterns.
  // Returns std::nullopt if the matcher is not available or no POS matches.
  // Runs in O(1) time.
  std::optional<uint16_t> GetId() const {
    if (!IsAvailable()) {
      return std::nullopt;
    }
    return first_id_;
  }

 private:
  // Lookup table. Index is POS ID, value is match result.
  // std::vector<bool> is intentionally used here instead of
  // std::vector<uint8_t> for bit-packing efficiency (~330 bytes for ~2,600 POS
  // IDs), which fits easily in L1 cache and minimizes memory footprint when
  // multiple matchers are held by components. Since Match() returns bool by
  // value, the proxy object typical of std::vector<bool> does not leak or
  // affect performance.
  std::vector<bool> table_;
  std::optional<uint16_t> first_id_ = std::nullopt;

  void BuildTable(absl::Span<const absl::string_view> patterns,
                  const PosIdMap& pos_id_map);
};

}  // namespace mozc::dictionary

#endif  // MOZC_DICTIONARY_POS_PATTERN_MATCHER_H_
