// Copyright 2010-2018, Google Inc.
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

#include "converter/connector.h"

#include <algorithm>

#include "base/logging.h"
#include "base/port.h"
#include "base/stl_util.h"
#include "data_manager/data_manager_interface.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

using mozc::storage::louds::SimpleSuccinctBitVectorIndex;

namespace mozc {
namespace {

const uint32 kInvalidCacheKey = 0xFFFFFFFF;
const uint16 kConnectorMagicNumber = 0xCDAB;
const uint8 kInvalid1ByteCostValue = 255;

inline uint32 GetHashValue(uint16 rid, uint16 lid, uint32 hash_mask) {
  return (3 * static_cast<uint32>(rid) + lid) & hash_mask;
  // Note: The above value is equivalent to
  //   return (3 * rid + lid) % cache_size_
  // because cache_size_ is the power of 2.
  // Multiplying '3' makes the conversion speed faster.
  // The result hash value becomes reasonalbly random.
}

inline uint32 EncodeKey(uint16 rid, uint16 lid) {
  return (static_cast<uint32>(rid) << 16) | lid;
}

}  // namespace

class Connector::Row {
 public:
  Row() : chunk_bits_index_(sizeof(uint32)),
          compact_bits_index_(sizeof(uint32)) {}

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
      if (*value == kInvalid1ByteCostValue) {
        *value = kInvalidCost;
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

Connector *Connector::CreateFromDataManager(
    const DataManagerInterface &data_manager) {
#ifdef OS_ANDROID
  const int kCacheSize = 256;
#else
  const int kCacheSize = 1024;
#endif  // OS_ANDROID
  const char *connection_data = nullptr;
  size_t connection_data_size = 0;
  data_manager.GetConnectorData(&connection_data, &connection_data_size);
  return new Connector(connection_data, connection_data_size, kCacheSize);
}

Connector::Connector(const char *connection_data,
                     size_t connection_size,
                     int cache_size)
    : default_cost_(nullptr),
      cache_size_(cache_size),
      cache_hash_mask_(cache_size - 1),
      cache_key_(new uint32[cache_size]),
      cache_value_(new int[cache_size]) {
  const uint16 *ptr = reinterpret_cast<const uint16 *>(connection_data);
  CHECK_EQ(kConnectorMagicNumber, ptr[0]);
  resolution_ = ptr[1];
  const uint16 rsize = ptr[2];
  const uint16 lsize = ptr[3];
  CHECK_EQ(rsize, lsize) << "The connector matrix should be square.";
  default_cost_ = ptr + 4;

  // Calculate the row's beginning position. Note that it should be aligned to
  // 32-bits boundary.
  size_t offset = 8 + (rsize + (rsize & 1)) * 2;

  // The number of valid bits in a chunk. Each bit is bitwise-or of consecutive
  // 8-bits.
  const size_t num_chunk_bits = (lsize + 7) / 8;

  // Then calculate the actual size of chunk in bytes, which is aligned to
  // 32-bits boundary.
  const size_t chunk_bits_size = (num_chunk_bits + 31) / 32 * 4;

  const bool use_1byte_value = resolution_ != 1;

  rows_.reserve(rsize);
  for (size_t i = 0; i < rsize; ++i) {
    const uint16 *size_data =
        reinterpret_cast<const uint16 *>(connection_data + offset);
    Row *row = new Row;
    const uint16 compact_bits_size = size_data[0];
    CHECK_EQ(compact_bits_size % 4, 0) << compact_bits_size;
    const uint16 values_size = size_data[1];
    CHECK_EQ(values_size % 4, 0) << values_size;

    const uint8 *chunk_bits =
        reinterpret_cast<const uint8 *>(connection_data + offset + 4);
    const uint8 *compact_bits = chunk_bits + chunk_bits_size;
    const uint8 *values = compact_bits + compact_bits_size;
    row->Init(chunk_bits, chunk_bits_size, compact_bits, compact_bits_size,
              values, use_1byte_value);
    rows_.push_back(row);

    offset += 4 + chunk_bits_size + compact_bits_size + values_size;
  }

  // Check if the cache_size is the power of 2 and clear cache.
  DCHECK_EQ(0, cache_size & (cache_size - 1));
  ClearCache();
}

Connector::~Connector() {
  STLDeleteElements(&rows_);
}


int Connector::GetTransitionCost(uint16 rid, uint16 lid) const {
  const uint32 index = EncodeKey(rid, lid);
  const uint32 bucket = GetHashValue(rid, lid, cache_hash_mask_);
  if (cache_key_[bucket] == index) {
    return cache_value_[bucket];
  }
  const int value = LookupCost(rid, lid);
  cache_key_[bucket] = index;
  cache_value_[bucket] = value;
  return value;
}

int Connector::GetResolution() const {
  return resolution_;
}

void Connector::ClearCache() {
  std::fill(cache_key_.get(), cache_key_.get() + cache_size_, kInvalidCacheKey);
}

int Connector::LookupCost(uint16 rid, uint16 lid) const {
  uint16 value;
  if (!rows_[rid]->GetValue(lid, &value)) {
    return default_cost_[rid];
  }
  return value * resolution_;
}

}  // namespace mozc
