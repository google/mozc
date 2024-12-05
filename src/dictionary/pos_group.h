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

#ifndef MOZC_DICTIONARY_POS_GROUP_H_
#define MOZC_DICTIONARY_POS_GROUP_H_

#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/types/span.h"

namespace mozc {
namespace dictionary {

// Manages pos grouping rule.
// This class holds a pointer to the array managed by DataManager, so pass it by
// value like string_view.
class PosGroup {
 public:
  explicit PosGroup(absl::Span<const uint8_t> lid_group)
      : lid_group_(lid_group) {}

  PosGroup(const PosGroup &) = default;
  PosGroup &operator=(const PosGroup &) = default;

  // Returns grouped pos id based on an array pre-generated from
  // data/rules/user_segment_history_pos_group.def.
  inline uint8_t GetPosGroup(const uint16_t lid) const {
    return lid_group_[lid];
  }

 private:
  const absl::Span<const uint8_t> lid_group_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_POS_GROUP_H_
