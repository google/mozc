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

#ifndef MOZC_CONVERTER_CONNECTOR_H_
#define MOZC_CONVERTER_CONNECTOR_H_

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/bits.h"

namespace mozc {

class Connector final {
 public:
  static constexpr int16_t kInvalidCost = 30000;

  static absl::StatusOr<Connector> Create(absl::string_view connection_data);

  int GetTransitionCost(uint16_t rid, uint16_t lid) const;
  int GetResolution() const { return resolution_; }

 private:
  class Row;

  absl::Status Init(absl::string_view connection_data);

  int LookupCost(uint16_t rid, uint16_t lid) const;

  // Storage for the rank indices of all rows, allocated once with the exact
  // size. Each row points into it.
  std::unique_ptr<uint16_t[]> rank_index_storage_;
  std::vector<Row> rows_;
  const uint16_t* default_cost_ = nullptr;
  int resolution_ = 0;
  // True if the values of the rows are quantized to 1 byte.
  bool use_1byte_value_ = false;
  // Cache for transition cost.
  using cache_t = std::vector<std::atomic<uint64_t>>;
  mutable std::unique_ptr<cache_t> cache_;
};

class Connector::Row final {
 public:
  Row() = default;

  // Stores the spans of the row data. The rank indices are built later by
  // BuildIndex, once the storage for all rows is allocated.
  void Init(absl::Span<const uint8_t> chunk_bits,
            absl::Span<const uint8_t> compact_bits,
            absl::Span<const uint8_t> values);
  // Returns the number of uint16_t entries that BuildIndex needs.
  size_t IndexSize() const {
    return chunk_bits_index_.IndexSize() + compact_bits_index_.IndexSize();
  }
  // Builds the rank indices at the head of `index`, which must have at least
  // IndexSize() entries, and returns the rest of `index`.
  absl::Span<uint16_t> BuildIndex(absl::Span<uint16_t> index);
  // Returns the value in the row if found. `use_1byte_value` tells the format
  // of the values, which is common to all rows of the connection data.
  std::optional<uint16_t> GetValue(uint16_t index, bool use_1byte_value) const;

 private:
  // Bit vector that supports only "is bit n set, and if so how many 1-bits
  // precede it", which is all a row needs. Stores the cumulative number of
  // 1-bits before each 32-bit word.
  class RankBitVector final {
   public:
    // The unit in which the data is read. The index has one entry per word.
    using Word = uint32_t;
    static constexpr uint32_t kWordBits = std::numeric_limits<Word>::digits;

    void set_data(absl::Span<const uint8_t> data) { data_ = data; }
    // Returns the number of index entries for the data.
    size_t IndexSize() const { return data_.size() / sizeof(Word); }
    // Builds the index for the data at the head of `index`, which must have
    // at least IndexSize() entries, and returns the rest of `index`.
    absl::Span<uint16_t> BuildIndex(absl::Span<uint16_t> index);

    // Returns the number of 1-bits in [0, n) if bit n is set.
    std::optional<int> Rank1IfSet(uint32_t n) const {
      const size_t word_index = n / kWordBits;
      const Word word = LoadUnaligned<Word>(&data_[sizeof(Word) * word_index]);
      const Word bit = Word{1} << (n % kWordBits);
      if ((word & bit) == 0) {
        return std::nullopt;
      }
      return rank_index_[word_index] + std::popcount(word & (bit - 1));
    }

   private:
    absl::Span<const uint8_t> data_;
    // The number of 1-bits before each word of `data_`. uint16_t is enough for
    // a row of well-formed connection data: each 1-bit stands for one of its
    // lsize columns, and lsize is a uint16_t.
    const uint16_t* rank_index_ = nullptr;
  };

  RankBitVector chunk_bits_index_;
  RankBitVector compact_bits_index_;
  absl::Span<const uint8_t> values_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONNECTOR_H_
