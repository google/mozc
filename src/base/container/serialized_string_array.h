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

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/bits.h"

namespace mozc {
namespace serialized_string_array_internal {

class const_iterator {
 public:
  using value_type = absl::string_view;
  using difference_type = int32_t;
  using pointer = const value_type*;
  using reference = const value_type&;
  using iterator_category = std::random_access_iterator_tag;

  constexpr const_iterator() : array_(nullptr), index_(0) {}
  constexpr const_iterator(const char* array, difference_type index)
      : array_(array), index_(index) {}

  constexpr difference_type index() const { return index_; }
  value_type operator*() const { return (*this)[0]; }
  value_type operator[](difference_type n) const;

  const_iterator& operator++() {
    ++index_;
    return *this;
  }
  const_iterator operator++(int) {
    const_iterator tmp = *this;
    ++index_;
    return tmp;
  }
  const_iterator& operator--() {
    --index_;
    return *this;
  }
  const_iterator operator--(int) {
    const_iterator tmp = *this;
    --index_;
    return tmp;
  }
  const_iterator& operator+=(difference_type n) {
    index_ += n;
    return *this;
  }
  const_iterator& operator-=(difference_type n) {
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

  // The following comparison operators make sense only for iterators obtained
  // from the same array.
  friend constexpr bool operator==(const_iterator x, const_iterator y) {
    DCHECK_EQ(x.array_, y.array_);
    return x.index_ == y.index_;
  }
  friend constexpr std::strong_ordering operator<=>(const_iterator x,
                                                    const_iterator y) {
    DCHECK_EQ(x.array_, y.array_);
    return x.index_ <=> y.index_;
  }

 private:
  const char* array_;
  difference_type index_;
};

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
  using pointer = value_type*;
  using const_pointer = const pointer;
  using reference = value_type&;
  using const_reference = const value_type&;
  using size_type = uint32_t;
  using difference_type = int32_t;

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
    if (data_.size() < sizeof(uint32_t)) [[unlikely]] {
      return 0;
    }
    return LoadUnaligned<uint32_t>(data_.data());
  }

  value_type operator[](size_t i) const { return DataAt(data_.data(), i); }

  bool empty() const { return size() == 0; }
  absl::string_view data() const { return data_; }
  void clear() { data_ = absl::string_view(); }

  const_iterator begin() const { return const_iterator(data_.data(), 0); }
  const_iterator end() const { return const_iterator(data_.data(), size()); }

  void swap(SerializedStringArray& other) noexcept { data_.swap(other.data_); }

  // Checks if the data is a valid array image.
  static bool VerifyData(absl::string_view data);

  // Creates a byte image of |strs| in |buffer| and returns the memory block in
  // |buffer| pointing to the image.  Note that uint32_t array is used for
  // buffer to align data at 4 byte boundary.
  static absl::string_view SerializeToBuffer(
      absl::Span<const absl::string_view> strs,
      std::unique_ptr<uint32_t[]>* buffer);

  template <typename Container>
    requires std::convertible_to<const Container&,
                                 absl::Span<const std::string>>
  static absl::string_view SerializeToBuffer(
      const Container& strs, std::unique_ptr<uint32_t[]>* buffer) {
    return SerializeStringsToBuffer(strs, buffer);
  }

  static void SerializeToFile(absl::Span<const absl::string_view> strs,
                              const std::string& filepath);

 private:
  friend class serialized_string_array_internal::const_iterator;

  // Returns the byte offset of the [offset, length] metadata entry for `index`
  // (or the total header byte size when `index` equals the element count).
  // Header layout in uint32_t words: 1 word for the element count, followed by
  // 2 words (offset, length) per entry. Computed in uint64_t to prevent 32-bit
  // integer overflow even on 32-bit targets.
  static constexpr uint64_t EntryByteOffset(uint64_t index) {
    constexpr uint64_t kElementCountWords = 1;
    constexpr uint64_t kWordsPerEntry = 2;
    return sizeof(uint32_t) * (kElementCountWords + kWordsPerEntry * index);
  }

  static uint64_t OffsetAt(const char* data, uint32_t index) {
    return LoadUnaligned<uint32_t>(data +
                                   static_cast<size_t>(EntryByteOffset(index)));
  }

  static uint64_t LengthAt(const char* data, uint32_t index) {
    return LoadUnaligned<uint32_t>(
        data + static_cast<size_t>(EntryByteOffset(index)) + sizeof(uint32_t));
  }

  static absl::string_view DataAt(const char* data, size_t index) {
    if (data == nullptr || index >= LoadUnaligned<uint32_t>(data))
        [[unlikely]] {
      return absl::string_view();
    }
    const uint32_t i = static_cast<uint32_t>(index);
    return absl::string_view(data + static_cast<size_t>(OffsetAt(data, i)),
                             static_cast<size_t>(LengthAt(data, i)));
  }

  template <typename T>
  static absl::string_view SerializeToBufferImpl(
      absl::Span<const T> strs, std::unique_ptr<uint32_t[]>* buffer);

  static absl::string_view SerializeStringsToBuffer(
      absl::Span<const std::string> strs, std::unique_ptr<uint32_t[]>* buffer);

  // Invariant: data_.data() == nullptr or data_.size() >= sizeof(uint32_t).
  absl::string_view data_;
};

namespace serialized_string_array_internal {

inline const_iterator::value_type const_iterator::operator[](
    difference_type n) const {
  const int64_t target = static_cast<int64_t>(index_) + n;
  if (target < 0) [[unlikely]] {
    return absl::string_view();
  }
  return SerializedStringArray::DataAt(array_, static_cast<size_t>(target));
}

}  // namespace serialized_string_array_internal

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_SERIALIZED_STRING_ARRAY_H_
