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

#include <cstddef>
#include <cstdint>
#include <memory>

#include "data_manager/data_manager_interface.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace mozc {

class Connector final {
 public:
  static constexpr int16_t kInvalidCost = 30000;

  static absl::StatusOr<std::unique_ptr<Connector>> CreateFromDataManager(
      const DataManagerInterface &data_manager);

  static absl::StatusOr<std::unique_ptr<Connector>> Create(
      const char *connection_data, size_t connection_size, int cache_size);

  Connector() = default;

  Connector(const Connector &) = delete;
  Connector &operator=(const Connector &) = delete;

  int GetTransitionCost(uint16_t rid, uint16_t lid) const;
  int GetResolution() const;

  void ClearCache();

 private:
  class Row;

  absl::Status Init(const char *connection_data, size_t connection_size,
                    int cache_size);

  int LookupCost(uint16_t rid, uint16_t lid) const;

  std::unique_ptr<Row[]> rows_;
  const uint16_t *default_cost_ = nullptr;
  int resolution_ = 0;
  int cache_size_ = 0;
  uint32_t cache_hash_mask_ = 0;
  mutable std::unique_ptr<uint32_t[]> cache_key_;
  mutable std::unique_ptr<int[]> cache_value_;
};

class Connector::Row final {
 public:
  Row()
      : chunk_bits_index_(sizeof(uint32_t)),
        compact_bits_index_(sizeof(uint32_t)) {}

  Row(const Row &) = delete;
  Row &operator=(const Row &) = delete;

  void Init(const uint8_t *chunk_bits, size_t chunk_bits_size,
            const uint8_t *compact_bits, size_t compact_bits_size,
            const uint8_t *values, bool use_1byte_value);
  // Returns true if the value is found in the row and then store the found
  // value into |value|. Otherwise returns false.
  bool GetValue(uint16_t index, uint16_t *value) const;

 private:
  storage::louds::SimpleSuccinctBitVectorIndex chunk_bits_index_;
  storage::louds::SimpleSuccinctBitVectorIndex compact_bits_index_;
  const uint8_t *values_ = nullptr;
  bool use_1byte_value_ = false;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONNECTOR_H_
