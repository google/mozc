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

#ifndef MOZC_DATA_MANAGER_EMOJI_DATA_H_
#define MOZC_DATA_MANAGER_EMOJI_DATA_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

namespace mozc {

// Emoji Version Data, in Unicode.
// Emoji Version information is available at:
// https://unicode.org/Public/emoji/.
// LINT.IfChange
enum EmojiVersion : uint32_t {
  E0_6,
  E0_7,
  E1_0,
  E2_0,
  E3_0,
  E4_0,
  E5_0,
  E11_0,
  E12_0,
  E12_1,
  E13_0,
  E13_1,
  E14_0,
  E15_0,
  E15_1,
  E16_0,
  E17_0,
  EMOJI_MAX_VERSION = E17_0,
};
// LINT.ThenChange(//rewriter/gen_emoji_rewriter_data.py)

// Emoji data token is 28 bytes data of the following format:
//
// +-------------------------------------+
// | Key index (4 byte)                  |
// +-------------------------------------+
// | UTF8 emoji index (4 byte)           |
// +-------------------------------------+
// | Unicode Emoji version (4 byte)      |
// +-------------------------------------+
// | UTF8 description index (4 byte)     |
// +-------------------------------------+
// | Unused field (4 byte)               |
// +-------------------------------------+
// | Unused field (4 byte)               |
// +-------------------------------------+
// | Unused field (4 byte)               |
// +-------------------------------------+
//
// Here, index is the position in the string array at which the corresponding
// string value is stored.  Tokens are sorted in order of key so that it can
// be search by binary search.
//
// The following iterator class can be used to iterate over token array.
class EmojiDataIterator {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = uint32_t;
  using difference_type = std::ptrdiff_t;
  using pointer = uint32_t*;
  using reference = uint32_t&;

  static constexpr size_t kEmojiDataByteLength = 28;

  EmojiDataIterator() : ptr_(nullptr) {}
  explicit EmojiDataIterator(const char* ptr) : ptr_(ptr) {}

  uint32_t key_index() const {
    return *reinterpret_cast<const uint32_t*>(ptr_);
  }
  uint32_t emoji_index() const {
    return *reinterpret_cast<const uint32_t*>(ptr_ + 4);
  }
  uint32_t unicode_version_index() const {
    return *reinterpret_cast<const uint32_t*>(ptr_ + 8);
  }
  uint32_t description_utf8_index() const {
    return *reinterpret_cast<const uint32_t*>(ptr_ + 12);
  }

  // Returns key index as token array is searched by key.
  uint32_t operator*() const { return key_index(); }

  void swap(EmojiDataIterator& x) {
    using std::swap;
    swap(ptr_, x.ptr_);
  }
  friend void swap(EmojiDataIterator& x, EmojiDataIterator& y) {
    return x.swap(y);
  }

  EmojiDataIterator& operator++() {
    ptr_ += kEmojiDataByteLength;
    return *this;
  }

  EmojiDataIterator operator++(int) {
    const char* tmp = ptr_;
    ptr_ += kEmojiDataByteLength;
    return EmojiDataIterator(tmp);
  }

  EmojiDataIterator& operator--() {
    ptr_ -= kEmojiDataByteLength;
    return *this;
  }

  EmojiDataIterator operator--(int) {
    const char* tmp = ptr_;
    ptr_ -= kEmojiDataByteLength;
    return EmojiDataIterator(tmp);
  }

  EmojiDataIterator& operator+=(ptrdiff_t n) {
    ptr_ += n * kEmojiDataByteLength;
    return *this;
  }

  EmojiDataIterator& operator-=(ptrdiff_t n) {
    ptr_ -= n * kEmojiDataByteLength;
    return *this;
  }

  friend EmojiDataIterator operator+(EmojiDataIterator x, ptrdiff_t n) {
    return x += n;
  }

  friend EmojiDataIterator operator+(ptrdiff_t n, EmojiDataIterator x) {
    return x += n;
  }

  friend EmojiDataIterator operator-(EmojiDataIterator x, ptrdiff_t n) {
    return x -= n;
  }

  friend ptrdiff_t operator-(EmojiDataIterator x, EmojiDataIterator y) {
    return (x.ptr_ - y.ptr_) / kEmojiDataByteLength;
  }

  friend bool operator==(EmojiDataIterator x, EmojiDataIterator y) {
    return x.ptr_ == y.ptr_;
  }

  friend bool operator!=(EmojiDataIterator x, EmojiDataIterator y) {
    return x.ptr_ != y.ptr_;
  }

  friend bool operator<(EmojiDataIterator x, EmojiDataIterator y) {
    return x.ptr_ < y.ptr_;
  }

  friend bool operator<=(EmojiDataIterator x, EmojiDataIterator y) {
    return x.ptr_ <= y.ptr_;
  }

  friend bool operator>(EmojiDataIterator x, EmojiDataIterator y) {
    return x.ptr_ > y.ptr_;
  }

  friend bool operator>=(EmojiDataIterator x, EmojiDataIterator y) {
    return x.ptr_ >= y.ptr_;
  }

 private:
  const char* ptr_ = nullptr;
};
}  // namespace mozc
#endif  // MOZC_DATA_MANAGER_EMOJI_DATA_H_
