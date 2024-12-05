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

#include <cstdint>

#include "absl/base/attributes.h"
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
  PosMatcher() = default;
  explicit PosMatcher(absl::Span<const uint16_t> data) : data_(data) {}

  PosMatcher(const PosMatcher &) = default;
  PosMatcher &operator=(const PosMatcher &) = default;

  void Set(absl::Span<const uint16_t> data) { data_ = data; }

 private:
  // Used in pos_matcher_impl.inc.
  constexpr bool IsRuleInTable(int index, uint16_t id) const;

  absl::Span<const uint16_t> data_;

#include "dictionary/pos_matcher_impl.inc"  // IWYU pragma: export
};

constexpr bool PosMatcher::IsRuleInTable(const int index,
                                         const uint16_t id) const {
  // kLidTableSize is defined in pos_matcher_impl.inc.
  const uint16_t offset = data_[kLidTableSize + index];
  for (const uint16_t *ptr = data_.data() + offset; *ptr != 0xFFFFu; ptr += 2) {
    if (id >= *ptr && id <= *(ptr + 1)) {
      return true;
    }
  }
  return false;
}

}  // namespace mozc::dictionary

#endif  // MOZC_DICTIONARY_POS_MATCHER_H_
