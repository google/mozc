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

#include "storage/existence_filter.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "base/bits.h"
#include "base/vlog.h"

namespace mozc {
namespace storage {
namespace existence_filter_internal {
namespace {

struct BlockDimensions {
  explicit BlockDimensions(uint32_t bits) {
    count = bits >> kBlockShift;
    const uint32_t bits_in_last_block = bits & kBlockMask;
    if (bits_in_last_block) {
      ++count;
      last_size = (bits_in_last_block + 31) / 32;
    } else {
      last_size = kBlockWords;
    }
  }

  uint32_t count;
  uint32_t last_size;
};

}  // namespace

BlockBitmap::BlockBitmap(const uint32_t size, absl::Span<const uint32_t> buf) {
  const BlockDimensions dimensions(size);
  blocks_.reserve(dimensions.count);
  for (uint32_t i = 0; i < dimensions.count - 1; ++i) {
    blocks_.push_back(buf.subspan(0, kBlockWords));
    buf.remove_prefix(kBlockWords);
  }
  blocks_.push_back(buf.subspan(0, dimensions.last_size));
}

BlockBitmapBuilder::BlockBitmapBuilder(uint32_t size) {
  CHECK_GT(size, 0);
  const BlockDimensions dimensions(size);

  // Allocate the block array
  CHECK_GT(dimensions.count, 0);
  blocks_.reserve(dimensions.count);
  for (uint32_t i = 0; i < dimensions.count - 1; ++i) {
    blocks_.emplace_back(kBlockWords, 0);
  }
  blocks_.emplace_back(dimensions.last_size, 0);
}

std::string::iterator BlockBitmapBuilder::SerializeTo(
    std::string::iterator it) {
  for (const std::vector<uint32_t>& block : blocks_) {
    for (const uint32_t i : block) {
      it = StoreUnaligned<uint32_t>(i, it);
    }
  }
  return it;
}

BlockBitmap BlockBitmapBuilder::Build() const {
  std::vector<absl::Span<const uint32_t>> blocks;
  blocks.reserve(blocks_.size());
  for (const std::vector<uint32_t>& block : blocks_) {
    blocks.push_back(block);
  }
  return BlockBitmap(std::move(blocks));
}

}  // namespace existence_filter_internal

namespace {

constexpr uint32_t kHeaderSize = 3;

absl::StatusOr<ExistenceFilterParams> ReadHeader(
    absl::Span<const uint32_t> buf) {
  if (buf.size() < kHeaderSize) {
    return absl::InvalidArgumentError(
        "Not enough bufsize: could not read header");
  }

  auto it = buf.begin();
  ExistenceFilterParams params;
  params.size = *it++;
  params.expected_nelts = *it++;

  // Assumes little-endian.
  // num_hashes was originally stored as 32bit integer, so the old
  // binary stores the value in lower bits.
  const uint32_t v = *it++;
  params.num_hashes = v & 0xFFFF;
  params.fp_type = v >> 16;

  if (params.num_hashes >= 8 || params.num_hashes <= 0) {
    return absl::InvalidArgumentError("Bad number of hashes (header.k)");
  }

  if (params.fp_type >= ExistenceFilterParams::FP_TYPE_SIZE) {
    return absl::InvalidArgumentError("unsupported fp type");
  }

  return params;
}

constexpr uint32_t BitsToWords(uint32_t bits) {
  uint32_t words = (bits + 31) >> 5;
  if (bits > 0 && words == 0) {
    words = 1 << (32 - 5);  // possible overflow
  }
  return words;
}

}  // namespace

std::ostream& operator<<(std::ostream& os,
                         const ExistenceFilterParams& params) {
  os << absl::StreamFormat("%v", params);
  return os;
}

bool ExistenceFilter::Exists(uint64_t hash) const {
  for (int i = 0; i < params_.num_hashes; ++i) {
    hash = std::rotl(hash, 8);
    const uint32_t index = hash % params_.size;
    if (!rep_.Get(index)) {
      return false;
    }
  }
  return true;
}

absl::StatusOr<ExistenceFilter> ExistenceFilter::Read(
    absl::Span<const uint32_t> buf) {
  ExistenceFilterParams params;
  if (absl::StatusOr<ExistenceFilterParams> result = ReadHeader(buf);
      result.ok()) {
    params = *std::move(result);
  } else {
    return absl::InvalidArgumentError("Invalid format: could not read header");
  }
  buf.remove_prefix(kHeaderSize);

  MOZC_VLOG(1) << "Reading bloom filter with params: " << params;

  if (buf.size() < BitsToWords(params.size)) {
    return absl::InvalidArgumentError("Not enough bufsize: could not read");
  }

  return ExistenceFilter(std::move(params), buf);
}

ExistenceFilterBuilder ExistenceFilterBuilder::CreateOptimal(
    size_t size_in_bytes, uint32_t estimated_insertions, uint16_t fp_type) {
  CHECK_LT(size_in_bytes, (1 << 29)) << "Requested size is too big";
  CHECK_GT(estimated_insertions, 0);
  CHECK_LT(fp_type, ExistenceFilterParams::FP_TYPE_SIZE);
  const uint32_t m = std::max<uint32_t>(1, size_in_bytes * 8);
  const uint32_t n = estimated_insertions;

  uint16_t optimal_k =
      static_cast<uint16_t>(std::round(static_cast<float>(m) / n * log(2.0)));
  optimal_k = std::clamp<uint16_t>(optimal_k, 1, 7);

  MOZC_VLOG(1) << "optimal_k: " << optimal_k;

  return ExistenceFilterBuilder({m, n, optimal_k, fp_type});
}

void ExistenceFilterBuilder::Insert(uint64_t hash) {
  for (int i = 0; i < params_.num_hashes; ++i) {
    hash = std::rotl(hash, 8);
    const uint32_t index = hash % params_.size;
    rep_.Set(index);
  }
}

size_t ExistenceFilterBuilder::MinFilterSizeInBytesForErrorRate(
    float error_rate, size_t num_elements) {
  // (-num_hashes * num_elements) / log(1 - error_rate^(1/num_hashes))

  double min_bits = 0;
  for (size_t num_hashes = 1; num_hashes < 8; ++num_hashes) {
    double num_bits =
        (-1.0 * num_hashes * num_elements) /
        log(1.0 - pow(static_cast<double>(error_rate), (1.0 / num_hashes)));
    if (min_bits == 0 || num_bits < min_bits) min_bits = num_bits;
  }
  return static_cast<size_t>(ceil(min_bits / 8));
}

std::string ExistenceFilterBuilder::SerializeAsString() {
  const size_t required_bytes =
      (kHeaderSize + BitsToWords(params_.size)) * sizeof(uint32_t);
  std::string buf;
  buf.resize(required_bytes);

  auto it = buf.begin();
  // write header
  it = StoreUnaligned<uint32_t>(params_.size, it);
  it = StoreUnaligned<uint32_t>(params_.expected_nelts, it);
  // Original num_hashes was 32 bit integer. Pushes the num_hases first so
  // it can evaluated properly even when loading them as single 32 bit integer.
  it = StoreUnaligned<uint16_t>(params_.num_hashes, it);
  it = StoreUnaligned<uint16_t>(params_.fp_type, it);
  // This method is called on data generation and we can call LOG(INFO) here.
  LOG(INFO) << "Header written: " << params_;

  // write bitmap
  it = rep_.SerializeTo(it);

  if (it != buf.end()) {
    LOG(ERROR) << "Wrote " << std::distance(buf.begin(), it)
               << " bytes instead of " << buf.size();
  }
  return buf;
}

ExistenceFilter ExistenceFilterBuilder::Build() const {
  return ExistenceFilter(params_, rep_.Build());
}

}  // namespace storage
}  // namespace mozc
