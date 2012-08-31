// Copyright 2010-2012, Google Inc.
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

#include "converter/sparse_connector.h"

#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {

using mozc::storage::louds::SimpleSuccinctBitVectorIndex;

class SparseConnector::Row {
 public:
  Row()
      : chunk_bits_index_(sizeof(uint32)),
        compact_bits_index_(sizeof(uint32)) {
  }

  void Init(const uint8 *chunk_bits, size_t chunk_bits_size,
            const uint8 *compact_bits, size_t compact_bits_size,
            const uint8 *values, bool use_1byte_value) {
    chunk_bits_index_.Init(chunk_bits, chunk_bits_size);
    compact_bits_index_.Init(compact_bits, compact_bits_size);
    values_ = values;
    use_1byte_value_ = use_1byte_value;
  }

  // Returns true if the value is found in the row and then store the found
  // value into |value|. Otherwise returns false.
  bool GetValue(uint16 index, uint16 *value) const {
    int chunk_bit_position = index / 8;
    if (!chunk_bits_index_.Get(chunk_bit_position)) {
      return false;
    }
    int compact_bit_position =
        chunk_bits_index_.Rank1(chunk_bit_position) * 8 + index % 8;
    if (!compact_bits_index_.Get(compact_bit_position)) {
      return false;
    }
    int value_position = compact_bits_index_.Rank1(compact_bit_position);
    if (use_1byte_value_) {
      *value = values_[value_position];
      if (*value == SparseConnector::kInvalid1ByteCostValue) {
        *value = ConnectorInterface::kInvalidCost;
      }
    } else {
      *value = reinterpret_cast<const uint16 *>(values_)[value_position];
    }

    return true;
  }

 private:
  SimpleSuccinctBitVectorIndex chunk_bits_index_;
  SimpleSuccinctBitVectorIndex compact_bits_index_;
  const uint8 *values_;
  bool use_1byte_value_;

  DISALLOW_COPY_AND_ASSIGN(Row);
};

SparseConnector::SparseConnector(const char *ptr, size_t size)
    : default_cost_(NULL) {
  // Parse header info.
  // Please refer to gen_connection_data.py for the basic idea how to
  // compress the connection data, and its binary format.
  CHECK_EQ(*reinterpret_cast<const uint16 *>(ptr), kSparseConnectorMagic);
  resolution_ = *reinterpret_cast<const uint16 *>(ptr + 2);
  const uint16 rsize = *reinterpret_cast<const uint16 *>(ptr + 4);
  const uint16 lsize = *reinterpret_cast<const uint16 *>(ptr + 6);
  CHECK_EQ(rsize, lsize)
      << "The sparse connector data should be squre matrix";
  default_cost_ = reinterpret_cast<const uint16 *>(ptr + 8);

  // Calculate the row's beginning position. Note that it should be aligned to
  // 32-bits boundary.
  size_t offset = 8 + (rsize + (rsize & 1)) * 2;

  // The number of valid bits in a chunk. Each bit is bitwise-or of consecutive
  // 8-bits.
  size_t num_chunk_bits = (lsize + 7) / 8;

  // Then calculate the actual size of chunk in bytes, which is aligned to
  // 32-bits boundary.
  size_t chunk_bits_size = (num_chunk_bits + 31) / 32 * 4;

  bool use_1byte_value = resolution_ != 1;

  rows_.reserve(rsize);
  for (size_t i = 0; i < rsize; ++i) {
    Row *row = new Row;
    uint16 compact_bits_size = *reinterpret_cast<const uint16 *>(ptr + offset);
    CHECK_EQ(compact_bits_size % 4, 0) << compact_bits_size;
    uint16 values_size = *reinterpret_cast<const uint16 *>(ptr + offset + 2);
    CHECK_EQ(values_size % 4, 0) << values_size;

    const uint8 *chunk_bits = reinterpret_cast<const uint8 *>(
        ptr + offset + 4);
    const uint8 *compact_bits = reinterpret_cast<const uint8 *>(
        ptr + offset + 4 + chunk_bits_size);
    const uint8 *values = reinterpret_cast<const uint8 *>(
        ptr + offset + 4 + chunk_bits_size + compact_bits_size);
    row->Init(chunk_bits, chunk_bits_size,
              compact_bits, compact_bits_size,
              values, use_1byte_value);
    rows_.push_back(row);

    offset += 4 + chunk_bits_size + compact_bits_size + values_size;
  }

  // Make sure that the data is fully read.
  CHECK_EQ(offset, size);
}

SparseConnector::~SparseConnector() {
  STLDeleteElements(&rows_);
}

int SparseConnector::GetTransitionCost(uint16 rid, uint16 lid) const {
  uint16 value;
  if (!rows_[rid]->GetValue(lid, &value)) {
    return default_cost_[rid];
  }
  return value * resolution_;
}

int SparseConnector::GetResolution() const {
  return resolution_;
}

}  // namespace mozc
