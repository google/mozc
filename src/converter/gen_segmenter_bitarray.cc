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

// Generates:
// |kCompressedLSize|, |kCompressedRSize|,
// |kCompressedLIDTable|, |kCompressedRIDTable|,
// |kSegmenterBitArrayData_size|, |kSegmenterBitArrayData_data|

#include "converter/gen_segmenter_bitarray.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "base/container/bitarray.h"
#include "base/file_stream.h"
#include "protocol/segmenter_data.pb.h"

namespace mozc {

namespace {
class StateTable {
 public:
  explicit StateTable(const size_t size) : compressed_size_(0) {
    idarray_.resize(size);
  }

  StateTable(const StateTable&) = delete;
  StateTable& operator=(const StateTable&) = delete;

  // |str| is an 1-dimensional row (or column) represented in byte array.
  void Add(uint16_t id, std::string str) {
    CHECK_LT(id, idarray_.size());
    idarray_[id] = std::move(str);
  }

  void Build() {
    compressed_table_.resize(idarray_.size());
    uint16_t id = 0;
    absl::flat_hash_map<std::string, uint16_t> dup;
    for (size_t i = 0; i < idarray_.size(); ++i) {
      const auto it = dup.find(idarray_[i]);
      if (it != dup.end()) {
        compressed_table_[i] = it->second;
      } else {
        compressed_table_[i] = id;
        dup.emplace(idarray_[i], id);
        ++id;
      }
    }

    compressed_size_ = dup.size();

    // verify
    for (size_t i = 0; i < idarray_.size(); ++i) {
      CHECK_LT(compressed_table_[i], compressed_size_);
      CHECK_EQ(dup[idarray_[i]], compressed_table_[i]);
    }

    CHECK_LT(compressed_size_, idarray_.size());
  }

  uint16_t id(uint16_t id) const {
    CHECK_LT(id, idarray_.size());
    return compressed_table_[id];
  }

  size_t compressed_size() const { return compressed_size_; }

  void Output(std::ostream* os) {
    const char* data = reinterpret_cast<const char*>(compressed_table_.data());
    const size_t bytelen = compressed_table_.size() * sizeof(uint16_t);
    os->write(data, bytelen);
  }

 private:
  std::vector<std::string> idarray_;
  std::vector<uint16_t> compressed_table_;
  size_t compressed_size_;
};
}  // namespace

void SegmenterBitarrayGenerator::GenerateBitarray(
    int lsize, int rsize, IsBoundaryFunc is_boundary,
    const std::string& output_size_info, const std::string& output_ltable,
    const std::string& output_rtable, const std::string& output_bitarray) {
  // Load the original matrix into an array
  std::vector<uint8_t> array((lsize + 1) * (rsize + 1));

  for (size_t rid = 0; rid <= lsize; ++rid) {
    for (size_t lid = 0; lid <= rsize; ++lid) {
      const uint32_t index = rid + lsize * lid;
      CHECK_LT(index, array.size());
      if (rid == lsize || lid == rsize) {
        array[index] = 1;
        continue;
      }
      if (is_boundary(rid, lid)) {
        array[index] = 1;
      } else {
        array[index] = 0;
      }
    }
  }

  // Reduce left states (remove duplicate rows)
  StateTable ltable(lsize + 1);
  for (size_t rid = 0; rid <= lsize; ++rid) {
    std::string buf;
    for (size_t lid = 0; lid <= rsize; ++lid) {
      const uint32_t index = rid + lsize * lid;
      buf += array[index];
    }
    ltable.Add(rid, std::move(buf));
  }

  // Reduce right states (remove duplicate columns)
  StateTable rtable(rsize + 1);
  for (size_t lid = 0; lid <= rsize; ++lid) {
    std::string buf;
    for (size_t rid = 0; rid <= lsize; ++rid) {
      const uint32_t index = rid + lsize * lid;
      buf += array[index];
    }
    rtable.Add(lid, std::move(buf));
  }

  // make lookup table
  rtable.Build();
  ltable.Build();

  const size_t kCompressedLSize = ltable.compressed_size();
  const size_t kCompressedRSize = rtable.compressed_size();

  CHECK_GT(kCompressedLSize, 0);
  CHECK_GT(kCompressedRSize, 0);

  // make bitarray
  BitArray barray(kCompressedLSize * kCompressedRSize);
  for (size_t rid = 0; rid <= lsize; ++rid) {
    for (size_t lid = 0; lid <= rsize; ++lid) {
      const int index = rid + lsize * lid;
      const uint32_t cindex =
          ltable.id(rid) + kCompressedLSize * rtable.id(lid);
      if (array[index] > 0) {
        barray.set(cindex);
      } else {
        barray.clear(cindex);
      }
    }
  }

  // verify the table
  for (size_t rid = 0; rid <= lsize; ++rid) {
    for (size_t lid = 0; lid <= rsize; ++lid) {
      const int index = rid + lsize * lid;
      const uint32_t cindex =
          ltable.id(rid) + kCompressedLSize * rtable.id(lid);
      CHECK_EQ(barray.get(cindex), (array[index] != 0));
    }
  }

  CHECK(barray.array());
  CHECK_GT(barray.size(), 0);

  static_assert(std::endian::native == std::endian::little,
                "Architecture must be little endian");
  {
    converter::SegmenterDataSizeInfo pb;
    pb.set_compressed_lsize(kCompressedLSize);
    pb.set_compressed_rsize(kCompressedRSize);
    OutputFileStream ofs(output_size_info,
                         std::ios_base::out | std::ios_base::binary);
    CHECK(ofs);
    CHECK(pb.SerializeToOstream(&ofs));
    ofs.close();
  }
  {
    OutputFileStream ofs(output_ltable,
                         std::ios_base::out | std::ios_base::binary);
    CHECK(ofs);
    ltable.Output(&ofs);
    ofs.close();
  }
  {
    OutputFileStream ofs(output_rtable,
                         std::ios_base::out | std::ios_base::binary);
    CHECK(ofs);
    rtable.Output(&ofs);
    ofs.close();
  }
  {
    OutputFileStream ofs(output_bitarray,
                         std::ios_base::out | std::ios_base::binary);
    CHECK(ofs);
    ofs.write(barray.array(), barray.array_size());
    ofs.close();
  }
}

}  // namespace mozc
