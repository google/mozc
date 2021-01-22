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

#include <cmath>
#include <cstring>

#include "base/logging.h"
#include "base/port.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace storage {
namespace {

// Rotate the value in 'original' by 'num_bits'
inline uint64 RotateLeft64(uint64 original, int num_bits) {
  // TODO(team): we may want to use rotl64 depending on the platform.
  DCHECK(0 < num_bits && num_bits < 64) << num_bits;
  return (original << (64 - num_bits)) | (original >> num_bits);
}

inline uint32 BitsToWords(uint32 bits) {
  uint32 words = (bits + 31) >> 5;
  if (bits > 0 && words == 0) {
    words = 1 << (32 - 5);  // possible overflow
  }
  return words;
}

}  // namespace

class ExistenceFilter::BlockBitmap {
 public:
  BlockBitmap(uint32 length, bool is_mutable);
  ~BlockBitmap();
  void Clear();
  bool Get(uint32 index) const;
  void Set(uint32 index);

  // REQUIRES: "iter" is zero, or was set by a preceding call
  // to GetMutableFragment().
  //
  // This allows caller to peek into and write to the underlying bitmap
  // as a series of non-empty fragments in whole number of 4-byte words.
  // If the entire bitmap has been exhausted, return false.  Otherwise,
  // return true and point caller to the next non-empty fragment.
  //
  // Usage:
  //    char** ptr;
  //    size_t bytes;
  //    for (uint32 iter = 0; bm.GetMutableFragment(&iter, &ptr, &bytes); ) {
  //      Process(*ptr, bytes);
  //    }
  bool GetMutableFragment(uint32 *itr, char ***ptr, size_t *size);

 private:
  static constexpr int kBlockShift = 21;  // 2^21 bits == 256KB block
  static constexpr int kBlockBits = 1 << kBlockShift;
  static constexpr int kBlockMask = kBlockBits - 1;
  static constexpr int kBlockBytes = kBlockBits >> 3;
  static constexpr int kBlockWords = kBlockBits >> 5;

  // Array of blocks. Each block has kBlockBits region except for last block.
  uint32 **block_;
  uint32 num_blocks_;
  uint32 bytes_in_last_;
  const bool is_mutable_;

  DISALLOW_COPY_AND_ASSIGN(BlockBitmap);
};

ExistenceFilter::BlockBitmap::BlockBitmap(uint32 length, bool is_mutable)
    : is_mutable_(is_mutable) {
  CHECK_GT(length, 0);
  const uint32 bits_in_last_block = (length & kBlockMask);

  // Allocate the block array
  num_blocks_ = (length >> kBlockShift);
  if (bits_in_last_block) {
    // Need an extra block for the leftover bits
    num_blocks_++;
  }
  CHECK_GT(num_blocks_, 0);

  block_ = new uint32 *[num_blocks_];
  CHECK(block_);

  // Allocate full blocks
  for (size_t i = 0; i < num_blocks_ - 1; ++i) {
    block_[i] = is_mutable_ ? new uint32[kBlockWords] : nullptr;
  }

  // Allocate the last block
  if (bits_in_last_block) {
    bytes_in_last_ = sizeof(uint32) * ((bits_in_last_block + 31) / 32);
  } else {
    bytes_in_last_ = kBlockBytes;
  }
  CHECK_EQ(bytes_in_last_ % sizeof(uint32), 0);

  block_[num_blocks_ - 1] =
      is_mutable_ ? new uint32[bytes_in_last_ / sizeof(uint32)] : nullptr;
}

ExistenceFilter::BlockBitmap::~BlockBitmap() {
  if (is_mutable_) {
    for (int i = 0; i < num_blocks_; ++i) {
      delete[] block_[i];
    }
  }
  delete[] block_;
}

void ExistenceFilter::BlockBitmap::Clear() {
  if (!is_mutable_) {
    return;
  }
  for (int i = 0; i < num_blocks_ - 1; ++i) {
    memset(block_[i], 0, kBlockBytes);
  }
  memset(block_[num_blocks_ - 1], 0, bytes_in_last_);
}

ExistenceFilter::ExistenceFilter(uint32 m, uint32 n, int k)
    : vec_size_(m ? m : 1), expected_nelts_(n), num_hashes_(k) {
  CHECK_LT(num_hashes_, 8);
  rep_ = absl::make_unique<BlockBitmap>(m ? m : 1, true);
  rep_->Clear();
}

// this is private constructor
ExistenceFilter::ExistenceFilter(uint32 m, uint32 n, int k, bool is_mutable)
    : vec_size_(m ? m : 1), expected_nelts_(n), num_hashes_(k) {
  CHECK_LT(num_hashes_, 8);
  rep_ = absl::make_unique<BlockBitmap>(m ? m : 1, is_mutable);
  rep_->Clear();
}

// static
ExistenceFilter *ExistenceFilter::CreateImmutableExietenceFilter(uint32 m,
                                                                 uint32 n,
                                                                 int k) {
  return new ExistenceFilter(m, n, k, false);
}

ExistenceFilter *ExistenceFilter::CreateOptimal(size_t size_in_bytes,
                                                uint32 estimated_insertions) {
  CHECK_LT(size_in_bytes, (1 << 29)) << "Requested size is too big";
  CHECK_GT(estimated_insertions, 0);
  const uint32 m = size_in_bytes * 8;
  const uint32 n = estimated_insertions;

  int optimal_k =
      static_cast<int>((static_cast<float>(m) / n * log(2.0)) + 0.5);
  if (optimal_k < 1) {
    optimal_k = 1;
  }
  if (optimal_k > 7) {
    optimal_k = 7;
  }

  VLOG(1) << "optimal_k: " << optimal_k;

  ExistenceFilter *filter = new ExistenceFilter(m, n, optimal_k);
  CHECK(filter);
  return filter;
}

ExistenceFilter::~ExistenceFilter() {}

void ExistenceFilter::Clear() { rep_->Clear(); }

inline bool ExistenceFilter::BlockBitmap::Get(uint32 index) const {
  const uint32 bindex = index >> kBlockShift;
  const uint32 windex = (index & kBlockMask) >> 5;
  const uint32 bitpos = index & 31;
  return (block_[bindex][windex] >> bitpos) & 1;
}

inline void ExistenceFilter::BlockBitmap::Set(uint32 index) {
  const uint32 bindex = index >> kBlockShift;
  const uint32 windex = (index & kBlockMask) >> 5;
  const uint32 bitpos = index & 31;
  block_[bindex][windex] |= (static_cast<uint32>(1) << bitpos);
}

bool ExistenceFilter::BlockBitmap::GetMutableFragment(uint32 *iter, char ***ptr,
                                                      size_t *size) {
  const uint32 b = *iter;
  if (b >= num_blocks_) {
    // |iter| reached to the end of the block.
    return false;
  }

  (*iter)++;
  *ptr = reinterpret_cast<char **>(&block_[b]);
  *size = (b == num_blocks_ - 1) ? bytes_in_last_ : kBlockBytes;
  return true;
}

bool ExistenceFilter::Exists(uint64 hash) const {
  for (size_t i = 0; i < num_hashes_; ++i) {
    hash = RotateLeft64(hash, 8);
    uint32 index = hash % vec_size_;
    if (!rep_->Get(index)) {
      return false;
    }
  }
  return true;
}

void ExistenceFilter::Insert(uint64 hash) {
  for (size_t i = 0; i < num_hashes_; ++i) {
    hash = RotateLeft64(hash, 8);
    uint32 index = hash % vec_size_;
    rep_->Set(index);
  }
}

size_t ExistenceFilter::Size() const {
  return (BitsToWords(vec_size_) * sizeof(uint32));
}

size_t ExistenceFilter::MinFilterSizeInBytesForErrorRate(float error_rate,
                                                         size_t num_elements) {
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

// allocate 'buf' and write filter to the buf.
// 'size' will hold the size of buf
void ExistenceFilter::Write(char **buf, size_t *size) {
  const int require_bytes = sizeof(Header) + Size();

  *buf = new char[require_bytes];
  CHECK(*buf);
  *size = require_bytes;

  char *buf_ptr = *buf;

  // write header
  memcpy(buf_ptr, &vec_size_, sizeof(vec_size_));
  buf_ptr += sizeof(vec_size_);
  memcpy(buf_ptr, &expected_nelts_, sizeof(expected_nelts_));
  buf_ptr += sizeof(expected_nelts_);
  memcpy(buf_ptr, &num_hashes_, sizeof(num_hashes_));
  buf_ptr += sizeof(num_hashes_);
  LOG(INFO) << "Write header : vec_size" << vec_size_ << " expected_nelts "
            << expected_nelts_ << " num_hashes " << num_hashes_;

  // write bitmap
  char **fragment_ptr = nullptr;
  size_t bytes = 0;
  size_t write = 0;
  for (uint32 iter = 0;
       rep_->GetMutableFragment(&iter, &fragment_ptr, &bytes);) {
    memcpy(buf_ptr, *fragment_ptr, bytes);
    buf_ptr += bytes;
    write += bytes;
  }
  if (write != Size()) {
    LOG(ERROR) << "Write " << write << " bytes instead of " << Size();
  }
}

bool ExistenceFilter::ReadHeader(const char *buf, Header *header) {
  memcpy(&(header->m), buf, sizeof(header->m));
  buf += sizeof(header->m);
  memcpy(&(header->n), buf, sizeof(header->n));
  buf += sizeof(header->n);
  memcpy(&(header->k), buf, sizeof(header->k));
  buf += sizeof(header->k);
  if (header->k >= 8 || header->k <= 0) {
    LOG(ERROR) << "Bad number of hashes (header->k)";
    return false;
  }
  return true;
}

ExistenceFilter *ExistenceFilter::Read(const char *buf, size_t size) {
  Header header;
  const uint32 header_bytes =
      sizeof(header.m) + sizeof(header.n) + sizeof(header.k);
  if (size < header_bytes) {
    LOG(ERROR) << "Not enough bufsize: could not read header";
    return nullptr;
  }

  if (!ReadHeader(buf, &header)) {
    LOG(ERROR) << "Invalid format: could not read header";
    return nullptr;
  }
  buf += header_bytes;

  const uint32 filter_size = BitsToWords(header.m);
  const uint32 filter_bytes = filter_size * sizeof(uint32);
  VLOG(1) << "Reading bloom filter with size: " << filter_bytes << " bytes, "
          << "estimated insertions: " << header.n << " (k: " << header.k << ")";

  if (size < header_bytes + filter_bytes) {
    LOG(ERROR) << "Not enough bufsize: could not read filter";
    return nullptr;
  }

  ExistenceFilter *filter = ExistenceFilter::CreateImmutableExietenceFilter(
      header.m, header.n, header.k);
  char **ptr = nullptr;
  size_t n = 0;
  size_t read = 0;
  for (uint32 iter = 0; filter->rep_->GetMutableFragment(&iter, &ptr, &n);) {
    *ptr = const_cast<char *>(buf);  // TODO(taku): don't want to remove const
    buf += n;
    read += n;
  }
  if (read != filter_bytes) {
    LOG(ERROR) << "Read " << read << " bytes instead of " << filter_bytes;
    delete filter;
    return nullptr;
  }
  return filter;
}

}  // namespace storage
}  // namespace mozc
