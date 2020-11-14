// Copyright 2010-2020, Google Inc.
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
#include <cstdint>
#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/port.h"
#include "base/status.h"
#include "base/statusor.h"
#include "base/util.h"
#include "data_manager/data_manager_interface.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"

namespace mozc {
namespace {

using ::mozc::storage::louds::SimpleSuccinctBitVectorIndex;

constexpr uint32 kInvalidCacheKey = 0xFFFFFFFF;
constexpr uint16 kConnectorMagicNumber = 0xCDAB;
constexpr uint8 kInvalid1ByteCostValue = 255;

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

mozc::Status IsMemoryAligned32(const void *ptr) {
  const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
  const auto alignment = addr % 4;
  if (alignment != 0) {
    return mozc::FailedPreconditionError(
        absl::StrCat("Aligned at ", alignment, " byte"));
  }
  return mozc::Status();
}

// Data stored in the first 8 bytes of the connection data.
struct Metadata {
  static constexpr size_t kByteSize = 8;

  uint16 magic = std::numeric_limits<uint16>::max();
  uint16 resolution = std::numeric_limits<uint16>::max();
  uint16 rsize = std::numeric_limits<uint16>::max();
  uint16 lsize = std::numeric_limits<uint16>::max();

  // The number of valid bits in a chunk. Each bit is bitwise-or of
  // consecutive 8-bits.
  size_t NumChunkBits() const { return (lsize + 7) / 8; }

  // Then calculate the actual size of chunk in bytes, which is aligned to
  // 32-bits boundary.
  size_t ChunkBitsSize() const { return (NumChunkBits() + 31) / 32 * 4; }

  // True if value is quantized to 1 byte.
  bool Use1ByteValue() const { return resolution != 1; }

  // Number of elements in the default cost array.
  size_t DefaultCostArraySize() const { return rsize + (rsize & 1); }

  std::string DebugString() const {
    return absl::StrCat("Metadata{magic: ", magic, ", resolution: ", resolution,
                        ", rsize: ", rsize, ", lsize: ", lsize, "}");
  }
};

mozc::StatusOr<Metadata> ParseMetadata(const char *connection_data,
                                       size_t connection_size) {
  if (connection_size < Metadata::kByteSize) {
    const auto &data =
        Util::Escape(absl::string_view(connection_data, connection_size));
    return mozc::FailedPreconditionError(absl::StrCat(
        "connector.cc: At least ", Metadata::kByteSize,
        " bytes expected.  Bytes: '", data, "' (", connection_size, " bytes)"));
  }
  const uint16 *data = reinterpret_cast<const uint16 *>(connection_data);
  Metadata metadata;
  metadata.magic = data[0];
  metadata.resolution = data[1];
  metadata.rsize = data[2];
  metadata.lsize = data[3];

  if (metadata.magic != kConnectorMagicNumber) {
    return mozc::FailedPreconditionError(absl::StrCat(
        "connector.cc: Unexpected magic number. Expected: ",
        kConnectorMagicNumber, " Actual: ", metadata.DebugString()));
  }
  if (metadata.lsize != metadata.rsize) {
    return mozc::FailedPreconditionError(absl::StrCat(
        "connector.cc: Matrix is not square: ", metadata.DebugString()));
  }
  return metadata;
}

}  // namespace

class Connector::Row {
 public:
  Row()
      : chunk_bits_index_(sizeof(uint32)),
        compact_bits_index_(sizeof(uint32)) {}

  Row(const Row &) = delete;
  Row &operator=(const Row &) = delete;

  ~Row() = default;

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
  const uint8 *values_ = nullptr;
  bool use_1byte_value_ = false;
};

mozc::StatusOr<std::unique_ptr<Connector>> Connector::CreateFromDataManager(
    const DataManagerInterface &data_manager) {
#ifdef OS_ANDROID
  const int kCacheSize = 256;
#else
  const int kCacheSize = 1024;
#endif  // OS_ANDROID
  const char *connection_data = nullptr;
  size_t connection_data_size = 0;
  data_manager.GetConnectorData(&connection_data, &connection_data_size);
  return Create(connection_data, connection_data_size, kCacheSize);
}

mozc::StatusOr<std::unique_ptr<Connector>> Connector::Create(
    const char *connection_data, size_t connection_size, int cache_size) {
  auto connector = absl::make_unique<Connector>();
  auto status = connector->Init(connection_data, connection_size, cache_size);
  if (!status.ok()) {
    return status;
  }
  return connector;
}

Connector::Connector() = default;
Connector::~Connector() = default;

mozc::Status Connector::Init(const char *connection_data,
                             size_t connection_size, int cache_size) {
  // Check if the cache_size is the power of 2.
  if ((cache_size & (cache_size - 1)) != 0) {
    return mozc::InvalidArgumentError(absl::StrCat(
        "connector.cc: Cache size must be 2^n: size=", cache_size));
  }
  cache_size_ = cache_size;
  cache_hash_mask_ = cache_size - 1;
  cache_key_ = absl::make_unique<uint32[]>(cache_size);
  cache_value_ = absl::make_unique<int[]>(cache_size);

  mozc::StatusOr<Metadata> metadata =
      ParseMetadata(connection_data, connection_size);
  if (!metadata.ok()) {
    return std::move(metadata).status();
  }
  resolution_ = metadata->resolution;

  // Set the read location to the metadata end.
  auto *ptr = connection_data + Metadata::kByteSize;
  const auto *data_end = connection_data + connection_size;

  const auto &gen_debug_info = [connection_data, connection_size,
                                &metadata](const char *ptr) -> std::string {
    return absl::StrCat(metadata->DebugString(),
                        ", Reader{location: ", ptr - connection_data,
                        ", datasize: ", connection_size, "}");
  };

  // A helper macro to check if the array is aligned at 32-bit boundary.  If
  // false, return the error.
#define VALIDATE_ALIGNMENT(array)                                   \
  do {                                                              \
    const auto status = IsMemoryAligned32((array));                 \
    if (status.ok()) {                                              \
      break;                                                        \
    }                                                               \
    return mozc::FailedPreconditionError(absl::StrCat(              \
        "connector.cc:", __LINE__, ": ", gen_debug_info(ptr),       \
        ": " #array " is not 32-bit aligned: ", status.message())); \
  } while (false)

  // A helper macro to check if the data range [ptr, ptr + num_bytes) is not
  // out-of-range.
#define VALIDATE_SIZE(ptr, num_bytes, ...)                               \
  do {                                                                   \
    const auto *p = reinterpret_cast<const char *>(ptr);                 \
    const std::ptrdiff_t remaining = data_end - p;                       \
    if (remaining >= num_bytes) {                                        \
      break;                                                             \
    }                                                                    \
    return mozc::OutOfRangeError(absl::StrCat(                           \
        "connector.cc", __LINE__, ": ", gen_debug_info(p),               \
        ": Tried to read past-the-end from " #ptr ".  Required bytes: ", \
        num_bytes, ", remaining: ", remaining, ": ", __VA_ARGS__));      \
  } while (false)

  // Read default cost array and move the read pointer.
  default_cost_ = reinterpret_cast<const uint16 *>(ptr);
  // Each element of default cost array is 2 bytes.
  const size_t default_cost_size = metadata->DefaultCostArraySize() * 2;
  VALIDATE_SIZE(default_cost_, default_cost_size, "Default cost");
  VALIDATE_ALIGNMENT(default_cost_);
  ptr += default_cost_size;

  const size_t chunk_bits_size = metadata->ChunkBitsSize();
  const uint16 rsize = metadata->rsize;
  rows_.reserve(rsize);
  for (size_t i = 0; i < rsize; ++i) {
    // Each row is formatted as follows:
    // +-------------------+-------------+------------+------------+---------+
    // |      uint16       |   uint16    |   uint8[]  |  uint8[]   | uint8[] |
    // | compact_bits_size | values_size | chunk_bits |compact_bits| values  |
    // +-------------------+-------------+------------+------------+---------+
    // ^
    // |ptr| points to here now.  Every uint8[] block needs to be aligned at
    // 32-bit boundary.
    VALIDATE_SIZE(ptr, 2, "Compact bits size of row ", i, "/", rsize);
    const uint16 compact_bits_size = *reinterpret_cast<const uint16 *>(ptr);
    ptr += 2;

    VALIDATE_SIZE(ptr, 2, "Values size of row ", i, "/", rsize);
    const uint16 values_size = *reinterpret_cast<const uint16 *>(ptr);
    ptr += 2;

    VALIDATE_SIZE(ptr, chunk_bits_size, "Chunk bits of row ", i, "/", rsize);
    const uint8 *chunk_bits = reinterpret_cast<const uint8 *>(ptr);
    VALIDATE_ALIGNMENT(chunk_bits);
    ptr += chunk_bits_size;

    VALIDATE_SIZE(ptr, compact_bits_size, "Compact bits of row ", i, "/",
                  rsize);
    const uint8 *compact_bits = reinterpret_cast<const uint8 *>(ptr);
    VALIDATE_ALIGNMENT(compact_bits);
    ptr += compact_bits_size;

    VALIDATE_SIZE(ptr, values_size, "Values of row ", i, "/", rsize);
    const uint8 *values = reinterpret_cast<const uint8 *>(ptr);
    VALIDATE_ALIGNMENT(values);
    ptr += values_size;

    auto row = absl::make_unique<Row>();
    row->Init(chunk_bits, chunk_bits_size, compact_bits, compact_bits_size,
              values, metadata->Use1ByteValue());
    rows_.push_back(std::move(row));
  }
  VALIDATE_SIZE(ptr, 0, "Data end");
  ClearCache();
  return mozc::Status();

#undef VALIDATE_ALIGNMENT
#undef VALIDATE_SIZE
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

int Connector::GetResolution() const { return resolution_; }

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
