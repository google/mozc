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

#ifndef MOZC_DICTIONARY_POS_MATCHER_H_
#define MOZC_DICTIONARY_POS_MATCHER_H_

#include <cstddef>
#include <cstdint>

#include "absl/types/span.h"

namespace mozc::dictionary {

// The PosMatcher class has two methods for each POS matching rule:
//
// - GetXXXId(): returns the POS ID for XXX.
// - IsXXX(uint16_t id): checks if the given POS ID is XXX or not.
//
// where XXX is replaced by rule names; see data/rules/pos_matcher_rule.def.
// These methods are generated as pos_matcher_impl.inc by
// running gen_pos_matcher_code.py.
//
// PosMatcher is a pointer to a table managed by DataManager, so pass it by
// value like a string_view.
//
// Binary format
//
// Suppose there are N matching rules.  Then, the first 2*N bytes is the array
// of uint16_t that contains the results for GetXXXId() methods.  The latter
// part contains the ranges of POS IDs for each IsXXX(uint16_t id) methods
// (IsXXX should return true if id is in one of the ranges).  See the following
// figure:
//
// +===========================================+=============================
// | POS ID for rule 0 (2 bytes)               |   For GetXXXID() methods
// +-------------------------------------------+
// | POS ID for rule 1 (2 bytes)               |
// +-------------------------------------------+
// | ....                                      |
// +-------------------------------------------+
// | POS ID for rule N - 1 (2 bytes)           |
// +===========================================+=============================
// | POS range for rule 0: start 0 (2 bytes)   |   For IsXXX() for rule 0
// + - - - - - - - - - - - - - - - - - - - - - +
// | POS range for rule 0: end 0 (2 bytes)     |
// +-------------------------------------------+
// | POS range for rule 0: start 1 (2 bytes)   |
// + - - - - - - - - - - - - - - - - - - - - - +
// | POS range for rule 0: end 1 (2 bytes)     |
// |-------------------------------------------+
// | ....                                      |
// |-------------------------------------------+
// | POS range for rule 0: start M (2 bytes)   |
// + - - - - - - - - - - - - - - - - - - - - - +
// | POS range for rule 0: end M (2 bytes)     |
// |-------------------------------------------+
// | Sentinel element 0xFFFF (2 bytes)         |
// +===========================================+=============================
// | POS range for rule 1: start 0 (2 bytes)   |   For IsXXX() for rule 1
// + - - - - - - - - - - - - - - - - - - - - - +
// | POS range for rule 1: end 0 (2 bytes)     |
// +-------------------------------------------+
// | ....                                      |
// +-------------------------------------------+
// | Sentinel element 0xFFFF (2 bytes)         |
// +===========================================+
// | ....                                      |
// |                                           |
class PosMatcher {
 public:
  PosMatcher() = delete;
  // Callers should validate the binary data with VerifyData() first. Even on
  // unverified data, Get*Id() safely falls back to 0 and Is*() safely falls
  // back to false when bounds are exceeded.
  explicit PosMatcher(absl::Span<const uint16_t> data) : data_(data) {}

  PosMatcher(const PosMatcher&) = default;
  PosMatcher& operator=(const PosMatcher&) = default;

  static constexpr size_t num_rules();
  // Verifies that |data| matches the binary layout emitted by
  // gen_pos_matcher_code.py: a 2 * num_rules() header followed by strictly
  // contiguous, non-overlapping 0xFFFF-terminated range lists in rule order
  // with no trailing data.
  static constexpr bool VerifyData(absl::Span<const uint16_t> data);

 private:
  static constexpr uint16_t kRangeTableSentinel = 0xFFFFu;

  // Used in pos_matcher_impl.inc.
  constexpr bool IsRuleInTable(size_t index, uint16_t id) const;

  absl::Span<const uint16_t> data_;

#include "dictionary/pos_matcher_impl.inc"  // IWYU pragma: export
};

constexpr size_t PosMatcher::num_rules() { return kLidTableSize; }

constexpr bool PosMatcher::VerifyData(absl::Span<const uint16_t> data) {
  constexpr size_t kMinHeaderSize = 2 * kLidTableSize;
  if (data.size() < kMinHeaderSize) {
    return false;
  }
  size_t expected_pos = kMinHeaderSize;
  for (size_t i = 0; i < kLidTableSize; ++i) {
    if (data[kLidTableSize + i] != expected_pos) {
      return false;
    }
    size_t pos = expected_pos;
    if (pos >= data.size()) {
      return false;
    }
    while (data[pos] != kRangeTableSentinel) {
      pos += 2;
      if (pos >= data.size()) {
        return false;
      }
    }
    expected_pos = pos + 1;
  }
  return expected_pos == data.size();
}

constexpr bool PosMatcher::IsRuleInTable(const size_t index,
                                         const uint16_t id) const {
  const size_t offset_index = kLidTableSize + index;
  if (offset_index >= data_.size()) {
    return false;
  }
  for (size_t pos = data_[offset_index];
       pos < data_.size() && data_[pos] != kRangeTableSentinel; pos += 2) {
    if (pos + 1 >= data_.size()) {
      return false;
    }
    if (id >= data_[pos] && id <= data_[pos + 1]) {
      return true;
    }
  }
  return false;
}

}  // namespace mozc::dictionary

#endif  // MOZC_DICTIONARY_POS_MATCHER_H_
