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

#ifndef MOZC_PREDICTION_ZERO_QUERY_DICT_H_
#define MOZC_PREDICTION_ZERO_QUERY_DICT_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/bits.h"
#include "base/container/serialized_string_array.h"

namespace mozc {

enum ZeroQueryType : uint16_t {
  ZERO_QUERY_NONE = 0,  // "☁" (symbol, non-unicode 6.0 emoji), and rule based.
  ZERO_QUERY_NUMBER_SUFFIX,  // "階" from "2"
  ZERO_QUERY_EMOTICON,       // "(>ω<)" from "うれしい"
  ZERO_QUERY_EMOJI,          // <umbrella emoji> from "かさ"
  // Following types are defined for usage stats.
  // The candidates of these types will not be stored at |ZeroQueryList|.
  // - "ヒルズ" from "六本木"
  // These candidates will be generated from dictionary entries
  // such as "六本木ヒルズ".
  ZERO_QUERY_BIGRAM,
  // - "に" from "六本木".
  // These candidates will be generated from suffix dictionary.
  ZERO_QUERY_SUFFIX,
  // These candidates will be generated from supplemental model.
  ZERO_QUERY_SUPPLEMENTAL_MODEL,
};

// Zero query dictionary is a multimap from string to a list of zero query
// entries, where each entry can be looked up by equal_range() method.  The data
// is serialized to two binary data: token array and string array.  Token array
// encodes an array of zero query entries, where each entry is encoded in 16
// bytes as follows:
//
// ZeroQueryEntry {
//   uint32 key_index:          4 bytes
//   uint32 value_index:        4 bytes
//   ZeroQueryType type:        2 bytes
//   uint16 unused_field:       2 bytes
//   uint32 unused_field:       4 bytes
// }
//
// The token array is sorted in ascending order of key_index for binary search.
// String values of key and value are encoded separately in the string array,
// which can be extracted by using |key_index| and |value_index|.  The string
// array is also sorted in ascending order of strings.  For the serialization
// format of string array, see base/serialized_string_array.h".
struct ZeroQueryEntry {
  uint32_t key_index = 0;
  uint32_t value_index = 0;
  ZeroQueryType type = ZERO_QUERY_NONE;
  uint8_t unused[6];
} ABSL_ATTRIBUTE_PACKED;

static_assert(sizeof(ZeroQueryEntry) == 16);

ASSERT_ALIGNED(ZeroQueryEntry, key_index);
ASSERT_ALIGNED(ZeroQueryEntry, value_index);
ASSERT_ALIGNED(ZeroQueryEntry, type);

class ZeroQueryDict {
 public:
  void Init(absl::string_view token_array_data,
            absl::string_view string_array_data) {
    token_array_ = token_array_data;
    string_array_.Set(string_array_data);
  }

  absl::string_view key(const ZeroQueryEntry& entry) const {
    return string_array_[entry.key_index];
  }

  absl::string_view value(const ZeroQueryEntry& entry) const {
    return string_array_[entry.value_index];
  }

  absl::Span<const ZeroQueryEntry> GetZeroQueryEntreis() const {
    return MakeAlignedConstSpan<ZeroQueryEntry>(token_array_);
  }

  absl::Span<const ZeroQueryEntry> equal_range(absl::string_view key) const {
    absl::Span<const ZeroQueryEntry> tokens = GetZeroQueryEntreis();
    const auto [it_begin, it_end] = std::equal_range(
        tokens.begin(), tokens.end(), key,
        // The `lhs/rhs` can be either ZeroQueryEntry or absl::string_view.
        [&](const auto& lhs, const auto& rhs) {
          auto get_key_string = [&](const auto& arg) {
            // The `arg` can be either ZeroQueryEntry or absl::string_view,
            // depending on whether lower_bound or upper_bound is used.
            // The compiler selects the correct type at compile time using the
            // following block
            if constexpr (std::is_same_v<std::decay_t<decltype(arg)>,
                                         ZeroQueryEntry>) {
              return string_array_[arg.key_index];  // key in tokens.
            } else {
              return arg;  // key in query.
            }
          };
          return get_key_string(lhs) < get_key_string(rhs);
        });
    return absl::MakeConstSpan(it_begin, it_end);
  }

 private:
  absl::string_view token_array_;
  SerializedStringArray string_array_;
};
}  // namespace mozc

#endif  // MOZC_PREDICTION_ZERO_QUERY_DICT_H_
