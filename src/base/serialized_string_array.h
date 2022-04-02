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

#ifndef MOZC_BASE_SERIALIZED_STRING_ARRAY_H_
#define MOZC_BASE_SERIALIZED_STRING_ARRAY_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "absl/strings/string_view.h"

namespace mozc {

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
// The former block of size 4 + 8 * N bytes is an array of uint32 (in little
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
  class iterator {
   public:
    using value_type = absl::string_view;
    using difference_type = ptrdiff_t;
    using pointer = const absl::string_view *;
    using reference = const absl::string_view &;
    using iterator_category = std::random_access_iterator_tag;

    iterator() : array_(nullptr), index_(0) {}
    iterator(const SerializedStringArray *array, size_t index)
        : array_(array), index_(index) {}
    iterator(const iterator &x) = default;

    size_t index() const { return index_; }
    absl::string_view operator*() { return (*array_)[index_]; }
    absl::string_view operator*() const { return (*array_)[index_]; }
    absl::string_view operator[](difference_type n) const {
      return (*array_)[index_ + n];
    }

    void swap(iterator &x) {
      using std::swap;
      swap(array_, x.array_);
      swap(index_, x.index_);
    }

    friend void swap(iterator &x, iterator &y) { x.swap(y); }

    iterator &operator++() {
      ++index_;
      return *this;
    }

    iterator operator++(int) {
      const size_t tmp = index_;
      ++index_;
      return iterator(array_, tmp);
    }

    iterator &operator--() {
      --index_;
      return *this;
    }

    iterator operator--(int) {
      const size_t tmp = index_;
      --index_;
      return iterator(array_, tmp);
    }

    iterator &operator+=(difference_type n) {
      index_ += n;
      return *this;
    }

    iterator &operator-=(difference_type n) {
      index_ -= n;
      return *this;
    }

    friend iterator operator+(iterator x, difference_type n) {
      return iterator(x.array_, x.index_ + n);
    }

    friend iterator operator+(difference_type n, iterator x) {
      return iterator(x.array_, x.index_ + n);
    }

    friend iterator operator-(iterator x, difference_type n) {
      return iterator(x.array_, x.index_ - n);
    }

    friend difference_type operator-(iterator x, iterator y) {
      return x.index_ - y.index_;
    }

    // The following comparison operators make sense only for iterators obtained
    // from the same array.
    friend bool operator==(iterator x, iterator y) {
      DCHECK_EQ(x.array_, y.array_);
      return x.index_ == y.index_;
    }

    friend bool operator!=(iterator x, iterator y) {
      DCHECK_EQ(x.array_, y.array_);
      return x.index_ != y.index_;
    }

    friend bool operator<(iterator x, iterator y) {
      DCHECK_EQ(x.array_, y.array_);
      return x.index_ < y.index_;
    }

    friend bool operator<=(iterator x, iterator y) {
      DCHECK_EQ(x.array_, y.array_);
      return x.index_ <= y.index_;
    }

    friend bool operator>(iterator x, iterator y) {
      DCHECK_EQ(x.array_, y.array_);
      return x.index_ > y.index_;
    }

    friend bool operator>=(iterator x, iterator y) {
      DCHECK_EQ(x.array_, y.array_);
      return x.index_ >= y.index_;
    }

   private:
    const SerializedStringArray *array_;
    size_t index_;
  };

  using const_iterator = iterator;

  SerializedStringArray();  // Default is an empty array.
  ~SerializedStringArray();

  // Initializes the array from given memory block.  The block must be aligned
  // at 4 byte boundary.  Returns false when the data is invalid.
  bool Init(absl::string_view data_aligned_at_4byte_boundary);

  // Initializes the array from given memory block without verifying data.
  void Set(absl::string_view data_aligned_at_4byte_boundary);

  uint32_t size() const {
    // The first 4 bytes of data stores the number of elements in this array in
    // little endian order.
    return *reinterpret_cast<const uint32_t *>(data_.data());
  }

  absl::string_view operator[](size_t i) const {
    const uint32_t *ptr = reinterpret_cast<const uint32_t *>(data_.data()) + 1;
    const uint32_t offset = ptr[2 * i];
    const uint32_t len = ptr[2 * i + 1];
    return data_.substr(offset, len);
  }

  bool empty() const { return size() == 0; }
  absl::string_view data() const { return data_; }
  void clear();

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, size()); }
  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }

  // Checks if the data is a valid array image.
  static bool VerifyData(absl::string_view data);

  // Creates a byte image of |strs| in |buffer| and returns the memory block in
  // |buffer| pointing to the image.  Note that uint32 array is used for buffer
  // to align data at 4 byte boundary.
  static absl::string_view SerializeToBuffer(
      const std::vector<absl::string_view> &strs,
      std::unique_ptr<uint32_t[]> *buffer);

  static void SerializeToFile(const std::vector<absl::string_view> &strs,
                              const std::string &filepath);

 private:
  absl::string_view data_;

  DISALLOW_COPY_AND_ASSIGN(SerializedStringArray);
};

}  // namespace mozc

#endif  // MOZC_BASE_SERIALIZED_STRING_ARRAY_H_
