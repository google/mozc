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

#ifndef MOZC_BASE_CONTAINER_SERIALIZED_STRING_ARRAY_H_
#define MOZC_BASE_CONTAINER_SERIALIZED_STRING_ARRAY_H_

#include <cstdint>
#include <iterator>
#include <memory>
#include <new>
#include <string>

#include "base/logging.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {
namespace serialized_string_array_internal {

inline const uint32_t *GetOffsetArray(const char *data) {
  return std::launder(reinterpret_cast<const uint32_t *>(data)) + 1;
}

inline uint32_t OffsetAt(const char *data, uint32_t index) {
  return GetOffsetArray(data)[index * 2];
}

inline uint32_t LengthAt(const char *data, uint32_t index) {
  return GetOffsetArray(data)[index * 2 + 1];
}

inline absl::string_view DataAt(const char *data, uint32_t index) {
  return absl::string_view(data + OffsetAt(data, index), LengthAt(data, index));
}

class const_iterator {
 public:
  using value_type = absl::string_view;
  using difference_type = uint32_t;
  using pointer = const value_type *;
  using reference = const value_type &;
  using iterator_category = std::random_access_iterator_tag;

  constexpr const_iterator() : array_(nullptr), index_(0) {}
  constexpr const_iterator(const char *array, difference_type index)
      : array_(array), index_(index) {}

  constexpr difference_type index() const { return index_; }
  value_type operator*() const { return DataAt(array_, index_); }
  value_type operator[](difference_type n) const {
    return DataAt(array_, index_ + n);
  }

  const_iterator &operator++() {
    ++index_;
    return *this;
  }
  const_iterator operator++(int) {
    const_iterator tmp = *this;
    ++index_;
    return tmp;
  }
  const_iterator &operator--() {
    --index_;
    return *this;
  }
  const_iterator operator--(int) {
    const_iterator tmp = *this;
    --index_;
    return tmp;
  }
  const_iterator &operator+=(difference_type n) {
    index_ += n;
    return *this;
  }
  const_iterator &operator-=(difference_type n) {
    index_ -= n;
    return *this;
  }
  friend const_iterator operator+(const_iterator x, difference_type n) {
    x.index_ += n;
    return x;
  }
  friend const_iterator operator+(difference_type n, const_iterator x) {
    x += n;
    return x;
  }
  friend const_iterator operator-(const_iterator x, difference_type n) {
    x.index_ -= n;
    return x;
  }
  friend difference_type operator-(const_iterator x, const_iterator y) {
    return x.index_ - y.index_;
  }

  constexpr int compare(const const_iterator &other) const {
    DCHECK_EQ(array_, other.array_);
    return index_ - other.index_;
  }

 private:
  const char *array_;
  difference_type index_;
};

// The following comparison operators make sense only for iterators obtained
// from the same array.
constexpr bool operator==(const_iterator x, const_iterator y) {
  return x.compare(y) == 0;
}
constexpr bool operator!=(const_iterator x, const_iterator y) {
  return x.compare(y) != 0;
}
constexpr bool operator<(const_iterator x, const_iterator y) {
  return x.compare(y) < 0;
}
constexpr bool operator<=(const_iterator x, const_iterator y) {
  return x.compare(y) <= 0;
}
constexpr bool operator>(const_iterator x, const_iterator y) {
  return x.compare(y) > 0;
}
constexpr bool operator>=(const_iterator x, const_iterator y) {
  return x.compare(y) >= 0;
}

}  // namespace serialized_string_array_internal

// Immutable array of strings serialized in binary image.  This class is used to
// serialize arrays of strings to byte sequence, and access the serialized
// array, in such a way that no deserialization is required at runtime, which is
// different from protobuf's repeated string field.
//
// * Prerequisite
// Little endian is assumed.
//
// * Serialized data creation
// To create a binary image, use SerializedStringArray::SerializeToBuffer() or
// build_tools/serialized_string_array_builder.py.
//
// * Array access
// At runtime, we can access array contents just by loading a binary image,
// e.g., from a file, onto memory where the first address must be aligned at
// 4-byte boundary.  For array access, a similar interface to
// vector<absl::string_view> is available; e.g., operator[], size(), and
// iterator.
//
// * Binary format
// The former block of size 4 + 8 * N bytes is an array of uint32_t (in little
// endian order) storing the array size and offset and length of each string;
// see the diagram below.  These data can be used to extract strings from the
// latter block.
//
// +=====================================================================+
// | Number of elements N in array  (4 byte)                             |
// +---------------------------------------------------------------------+
// | Byte offset of string[0]  (4 byte)                                  |
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// | Byte length of string[0]  (4 byte, excluding terminating '\0')      |
// +---------------------------------------------------------------------+
// | Byte offset of string[1]  (4 byte)                                  |
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// | Byte length of string[1]  (4 byte, excluding terminating '\0')      |
// +---------------------------------------------------------------------+
// |                      .                                              |
// |                      .                                              |
// |                      .                                              |
// +---------------------------------------------------------------------+
// | Byte offset of string[N - 1]  (4 byte)                              |
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// | Byte length of string[N - 1]  (4 byte, excluding terminating '\0')  |
// +=====================================================================+
// | string[0]  (Variable length)                                        |
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// | '\0'       (1 byte)                                                 |
// +---------------------------------------------------------------------+
// | string[1]  (Variable length)                                        |
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// | '\0'       (1 byte)                                                 |
// +---------------------------------------------------------------------+
// |                      .                                              |
// |                      .                                              |
// |                      .                                              |
// +---------------------------------------------------------------------+
// | string[N - 1]  (Variable length)                                    |
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// | '\0'           (1 byte)                                             |
// +=====================================================================+
class SerializedStringArray {
 public:
  using value_type = absl::string_view;
  using pointer = value_type *;
  using const_pointer = const pointer;
  using reference = value_type &;
  using const_reference = const value_type &;
  using size_type = uint32_t;
  using difference_type = uint32_t;

  using iterator = serialized_string_array_internal::const_iterator;
  using const_iterator = iterator;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Initializes the array from given memory block.  The block must be aligned
  // at 4 byte boundary.  Returns false when the data is invalid.
  bool Init(absl::string_view data_aligned_at_4byte_boundary);

  // Initializes the array from given memory block without verifying data.
  void Set(absl::string_view data_aligned_at_4byte_boundary);

  size_type size() const {
    // The first 4 bytes of data stores the number of elements in this array in
    // little endian order.
    if (data_.empty()) return 0;
    return *std::launder(reinterpret_cast<const uint32_t *>(data_.data()));
  }

  value_type operator[](difference_type i) const {
    return serialized_string_array_internal::DataAt(data_.data(), i);
  }

  bool empty() const { return size() == 0; }
  absl::string_view data() const { return data_; }
  void clear() { data_ = absl::string_view(); }

  const_iterator begin() const { return const_iterator(data_.data(), 0); }
  const_iterator end() const { return const_iterator(data_.data(), size()); }

  // Checks if the data is a valid array image.
  static bool VerifyData(absl::string_view data);

  // Creates a byte image of |strs| in |buffer| and returns the memory block in
  // |buffer| pointing to the image.  Note that uint32_t array is used for
  // buffer to align data at 4 byte boundary.
  static absl::string_view SerializeToBuffer(
      absl::Span<const absl::string_view> strs,
      std::unique_ptr<uint32_t[]> *buffer);

  static void SerializeToFile(absl::Span<const absl::string_view> strs,
                              const std::string &filepath);

 private:
  absl::string_view data_;
};

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_SERIALIZED_STRING_ARRAY_H_
