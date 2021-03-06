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

#include <cstdint>
#include <memory>
#include <vector>

#include "base/port.h"
#include "base/status.h"
#include "base/statusor.h"

namespace mozc {

class DataManagerInterface;

class Connector final {
 public:
  static constexpr int16_t kInvalidCost = 30000;

  static mozc::StatusOr<std::unique_ptr<Connector>> CreateFromDataManager(
      const DataManagerInterface &data_manager);

  static mozc::StatusOr<std::unique_ptr<Connector>> Create(
      const char *connection_data, size_t connection_size, int cache_size);

  Connector();
  ~Connector();

  Connector(const Connector &) = delete;
  Connector &operator=(const Connector &) = delete;

  int GetTransitionCost(uint16_t rid, uint16_t lid) const;
  int GetResolution() const;

  void ClearCache();

 private:
  class Row;

  mozc::Status Init(const char *connection_data, size_t connection_size,
                    int cache_size);

  int LookupCost(uint16_t rid, uint16_t lid) const;

  std::vector<std::unique_ptr<Row>> rows_;
  const uint16_t *default_cost_ = nullptr;
  int resolution_ = 0;
  int cache_size_ = 0;
  uint32_t cache_hash_mask_ = 0;
  mutable std::unique_ptr<uint32_t[]> cache_key_;
  mutable std::unique_ptr<int[]> cache_value_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONNECTOR_H_
