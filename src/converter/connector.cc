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

#include "converter/connector.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "data_manager/data_manager.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace {

constexpr uint32_t kInvalidCacheKey = 0xFFFFFFFF;
constexpr uint16_t kConnectorMagicNumber = 0xCDAB;
constexpr uint8_t kInvalid1ByteCostValue = 255;

inline uint32_t GetHashValue(uint16_t rid, uint16_t lid, uint32_t hash_mask) {
  return (3 * static_cast<uint32_t>(rid) + lid) & hash_mask;
  // Note: The above value is equivalent to
  //   return (3 * rid + lid) % cache_size_
  // because cache_size_ is the power of 2.
  // Multiplying '3' makes the conversion speed faster.
  // The result hash value becomes reasonably random.
}

inline uint32_t EncodeKey(uint16_t rid, uint16_t lid) {
  return (static_cast<uint32_t>(rid) << 16) | lid;
}

absl::Status IsMemoryAligned32(const void *ptr) {
  const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
  const auto alignment = addr % 4;
  if (alignment != 0) {
    return absl::FailedPreconditionError(
        absl::StrCat("Aligned at ", alignment, " byte"));
  }
  return absl::Status();
}

// Data stored in the first 8 bytes of the connection data.
struct Metadata {
  static constexpr size_t kByteSize = 8;

  uint16_t magic = std::numeric_limits<uint16_t>::max();
  uint16_t resolution = std::numeric_limits<uint16_t>::max();
  uint16_t rsize = std::numeric_limits<uint16_t>::max();
  uint16_t lsize = std::numeric_limits<uint16_t>::max();

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

absl::StatusOr<Metadata> ParseMetadata(const char *connection_data,
                                       size_t connection_size) {
  if (connection_size < Metadata::kByteSize) {
    const std::string data =
        absl::CHexEscape(absl::string_view(connection_data, connection_size));
    return absl::FailedPreconditionError(absl::StrCat(
        "connector.cc: At least ", Metadata::kByteSize,
        " bytes expected.  Bytes: '", data, "' (", connection_size, " bytes)"));
  }
  const uint16_t *data = reinterpret_cast<const uint16_t *>(connection_data);
  Metadata metadata;
  metadata.magic = data[0];
  metadata.resolution = data[1];
  metadata.rsize = data[2];
  metadata.lsize = data[3];

  if (metadata.magic != kConnectorMagicNumber) {
    return absl::FailedPreconditionError(absl::StrCat(
        "connector.cc: Unexpected magic number. Expected: ",
        kConnectorMagicNumber, " Actual: ", metadata.DebugString()));
  }
  if (metadata.lsize != metadata.rsize) {
    return absl::FailedPreconditionError(absl::StrCat(
        "connector.cc: Matrix is not square: ", metadata.DebugString()));
  }
  return metadata;
}

}  // namespace

void Connector::Row::Init(const uint8_t *chunk_bits, size_t chunk_bits_size,
                          const uint8_t *compact_bits, size_t compact_bits_size,
                          const uint8_t *values, bool use_1byte_value) {
  chunk_bits_index_.Init(chunk_bits, chunk_bits_size);
  compact_bits_index_.Init(compact_bits, compact_bits_size);
  values_ = values;
  use_1byte_value_ = use_1byte_value;
}

std::optional<uint16_t> Connector::Row::GetValue(uint16_t index) const {
  int chunk_bit_position = index / 8;
  if (!chunk_bits_index_.Get(chunk_bit_position)) {
    return std::nullopt;
  }
  int compact_bit_position =
      chunk_bits_index_.Rank1(chunk_bit_position) * 8 + index % 8;
  if (!compact_bits_index_.Get(compact_bit_position)) {
    return std::nullopt;
  }
  int value_position = compact_bits_index_.Rank1(compact_bit_position);
  uint16_t value;
  if (use_1byte_value_) {
    value = values_[value_position];
    if (value == kInvalid1ByteCostValue) {
      value = kInvalidCost;
    }
  } else {
    value = std::launder(
        reinterpret_cast<const uint16_t *>(values_))[value_position];
  }
  return value;
}

absl::StatusOr<Connector> Connector::CreateFromDataManager(
    const DataManager &data_manager) {
#ifdef __ANDROID__
  constexpr int kCacheSize = 256;
#else   // __ANDROID__
  constexpr int kCacheSize = 1024;
#endif  // __ANDROID__
  return Create(data_manager.GetConnectorData(), kCacheSize);
}

absl::StatusOr<Connector> Connector::Create(absl::string_view connection_data,
                                            int cache_size) {
  Connector connector;
  absl::Status status = connector.Init(connection_data, cache_size);
  if (!status.ok()) {
    return status;
  }
  return connector;
}

absl::Status Connector::Init(absl::string_view connection_data,
                             int cache_size) {
  // Check if the cache_size is the power of 2.
  if ((cache_size & (cache_size - 1)) != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "connector.cc: Cache size must be 2^n: size=", cache_size));
  }
  cache_hash_mask_ = cache_size - 1;
  cache_ = std::make_unique<cache_t>(cache_size);

  absl::StatusOr<Metadata> metadata =
      ParseMetadata(connection_data.data(), connection_data.size());
  if (!metadata.ok()) {
    return std::move(metadata).status();
  }
  resolution_ = metadata->resolution;

  // Set the read location to the metadata end.
  const char *ptr = connection_data.data() + Metadata::kByteSize;
  const char *data_end = connection_data.data() + connection_data.size();

  const auto &gen_debug_info = [connection_data,
                                &metadata](const char *ptr) -> std::string {
    return absl::StrCat(metadata->DebugString(),
                        ", Reader{location: ", ptr - connection_data.data(),
                        ", datasize: ", connection_data.size(), "}");
  };

  // A helper macro to check if the array is aligned at 32-bit boundary.  If
  // false, return the error.
#define VALIDATE_ALIGNMENT(array)                                   \
  do {                                                              \
    const auto status = IsMemoryAligned32((array));                 \
    if (status.ok()) {                                              \
      break;                                                        \
    }                                                               \
    return absl::FailedPreconditionError(absl::StrCat(              \
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
    return absl::OutOfRangeError(absl::StrCat(                           \
        "connector.cc", __LINE__, ": ", gen_debug_info(p),               \
        ": Tried to read past-the-end from " #ptr ".  Required bytes: ", \
        num_bytes, ", remaining: ", remaining, ": ", __VA_ARGS__));      \
  } while (false)

  // Read default cost array and move the read pointer.
  default_cost_ = reinterpret_cast<const uint16_t *>(ptr);
  // Each element of default cost array is 2 bytes.
  const size_t default_cost_size = metadata->DefaultCostArraySize() * 2;
  VALIDATE_SIZE(default_cost_, default_cost_size, "Default cost");
  VALIDATE_ALIGNMENT(default_cost_);
  ptr += default_cost_size;

  const size_t chunk_bits_size = metadata->ChunkBitsSize();
  const uint16_t rsize = metadata->rsize;
  rows_.resize(rsize);
  for (size_t i = 0; i < rsize; ++i) {
    // Each row is formatted as follows:
    // +-------------------+-------------+------------+------------+-----------+
    // |     uint16_t      |  uint16_t   | uint8_t[]  | uint8_t[]  | uint8_t[] |
    // | compact_bits_size | values_size | chunk_bits |compact_bits| values    |
    // +-------------------+-------------+------------+------------+-----------+
    // ^
    // |ptr| points to here now.  Every uint8_t[] block needs to be aligned at
    // 32-bit boundary.
    VALIDATE_SIZE(ptr, 2, "Compact bits size of row ", i, "/", rsize);
    const uint16_t compact_bits_size = *reinterpret_cast<const uint16_t *>(ptr);
    ptr += 2;

    VALIDATE_SIZE(ptr, 2, "Values size of row ", i, "/", rsize);
    const uint16_t values_size = *reinterpret_cast<const uint16_t *>(ptr);
    ptr += 2;

    VALIDATE_SIZE(ptr, chunk_bits_size, "Chunk bits of row ", i, "/", rsize);
    const uint8_t *chunk_bits = reinterpret_cast<const uint8_t *>(ptr);
    VALIDATE_ALIGNMENT(chunk_bits);
    ptr += chunk_bits_size;

    VALIDATE_SIZE(ptr, compact_bits_size, "Compact bits of row ", i, "/",
                  rsize);
    const uint8_t *compact_bits = reinterpret_cast<const uint8_t *>(ptr);
    VALIDATE_ALIGNMENT(compact_bits);
    ptr += compact_bits_size;

    VALIDATE_SIZE(ptr, values_size, "Values of row ", i, "/", rsize);
    const uint8_t *values = reinterpret_cast<const uint8_t *>(ptr);
    VALIDATE_ALIGNMENT(values);
    ptr += values_size;

    rows_[i].Init(chunk_bits, chunk_bits_size, compact_bits, compact_bits_size,
                  values, metadata->Use1ByteValue());
  }
  VALIDATE_SIZE(ptr, 0, "Data end");
  ClearCache();
  return absl::Status();

#undef VALIDATE_ALIGNMENT
#undef VALIDATE_SIZE
}

int Connector::GetTransitionCost(uint16_t rid, uint16_t lid) const {
  const uint32_t index = EncodeKey(rid, lid);
  const uint32_t bucket = GetHashValue(rid, lid, cache_hash_mask_);
  // don't care the memory order. atomic access is only required.
  // Upper 32bits stores the value-cost, lower 32 bits the index.
  const uint64_t cv = (*cache_)[bucket].load(std::memory_order_relaxed);
  if (static_cast<uint32_t>(cv) == index) {
    return static_cast<int>(cv >> 32);
  }
  const int value = LookupCost(rid, lid);
  (*cache_)[bucket].store(static_cast<uint64_t>(value) << 32 | index,
                          std::memory_order_relaxed);
  return value;
}

void Connector::ClearCache() {
  for (std::atomic<uint64_t> &x : *cache_) {
    x.store(kInvalidCacheKey);
  }
}

int Connector::LookupCost(uint16_t rid, uint16_t lid) const {
  std::optional<uint16_t> value = rows_[rid].GetValue(lid);
  if (!value.has_value()) {
    return default_cost_[rid];
  }
  return *value * resolution_;
}

}  // namespace mozc
