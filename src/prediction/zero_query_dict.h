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

#ifndef MOZC_PREDICTION_ZERO_QUERY_DICT_H_
#define MOZC_PREDICTION_ZERO_QUERY_DICT_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

#include "absl/strings/string_view.h"
#include "base/bits.h"
#include "base/container/serialized_string_array.h"

namespace mozc {

enum ZeroQueryType : uint16_t {
  ZERO_QUERY_NONE = 0,  // "☁" (symbol, non-unicode 6.0 emoji), and rule based.
  ZERO_QUERY_NUMBER_SUFFIX,  // "階" from "2"
  ZERO_QUERY_EMOTICON,       // "(>ω<)" from "うれしい"
  ZERO_QUERY_EMOJI,          // <umbrella emoji> from "かさ"
  // Following types are defined for usage stats.
  // The candidates of these types will not be stored at |ZeroQueryList|.
  // - "ヒルズ" from "六本木"
  // These candidates will be generated from dictionary entries
  // such as "六本木ヒルズ".
  ZERO_QUERY_BIGRAM,
  // - "に" from "六本木".
  // These candidates will be generated from suffix dictionary.
  ZERO_QUERY_SUFFIX,
  // These candidates will be generated from supplemental model.
  ZERO_QUERY_SUPPLEMENTAL_MODEL,
};

// Zero query dictionary is a multimap from string to a list of zero query
// entries, where each entry can be looked up by equal_range() method.  The data
// is serialized to two binary data: token array and string array.  Token array
// encodes an array of zero query entries, where each entry is encoded in 16
// bytes as follows:
//
// ZeroQueryEntry {
//   uint32 key_index:          4 bytes
//   uint32 value_index:        4 bytes
//   ZeroQueryType type:        2 bytes
//   uint16 unused_field:       2 bytes
//   uint32 unused_field:       4 bytes
// }
//
// The token array is sorted in ascending order of key_index for binary search.
// String values of key and value are encoded separately in the string array,
// which can be extracted by using |key_index| and |value_index|.  The string
// array is also sorted in ascending order of strings.  For the serialization
// format of string array, see base/serialized_string_array.h".
class ZeroQueryDict {
 public:
  static constexpr size_t kTokenByteSize = 16;

  class iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = uint32_t;
    using difference_type = std::ptrdiff_t;
    using pointer = uint32_t*;
    using reference = uint32_t&;

    iterator(const char* ptr, const SerializedStringArray* array)
        : ptr_(ptr), string_array_(array) {}
    iterator(const iterator& x) = default;
    iterator& operator=(const iterator& x) = default;

    uint32_t operator*() const { return key_index(); }

    uint32_t operator[](ptrdiff_t n) const {
      return LoadUnaligned<uint32_t>(ptr_ + n * kTokenByteSize);
    }

    const iterator* operator->() const { return this; }

    uint32_t key_index() const { return LoadUnaligned<uint32_t>(ptr_); }

    uint32_t value_index() const { return LoadUnaligned<uint32_t>(ptr_ + 4); }

    ZeroQueryType type() const {
      const uint16_t val = LoadUnaligned<uint16_t>(ptr_ + 8);
      return static_cast<ZeroQueryType>(val);
    }

    absl::string_view key() const { return (*string_array_)[key_index()]; }
    absl::string_view value() const { return (*string_array_)[value_index()]; }

    iterator& operator++() {
      ptr_ += kTokenByteSize;
      return *this;
    }

    iterator operator++(int) {
      const iterator tmp(ptr_, string_array_);
      ptr_ += kTokenByteSize;
      return tmp;
    }

    iterator& operator+=(ptrdiff_t n) {
      ptr_ += n * kTokenByteSize;
      return *this;
    }

    friend iterator operator+(iterator iter, ptrdiff_t n) {
      iter += n;
      return iter;
    }

    friend iterator operator+(ptrdiff_t n, iterator iter) {
      iter += n;
      return iter;
    }

    iterator& operator--() {
      ptr_ -= kTokenByteSize;
      return *this;
    }

    iterator operator--(int) {
      const iterator tmp(ptr_, string_array_);
      ptr_ -= kTokenByteSize;
      return tmp;
    }

    iterator& operator-=(ptrdiff_t n) {
      ptr_ -= n * kTokenByteSize;
      return *this;
    }

    friend iterator operator-(iterator iter, ptrdiff_t n) {
      iter -= n;
      return iter;
    }

    friend ptrdiff_t operator-(iterator x, iterator y) {
      return (x.ptr_ - y.ptr_) / kTokenByteSize;
    }

    friend bool operator==(iterator x, iterator y) { return x.ptr_ == y.ptr_; }

    friend bool operator!=(iterator x, iterator y) { return x.ptr_ != y.ptr_; }

    friend bool operator<(iterator x, iterator y) { return x.ptr_ < y.ptr_; }

    friend bool operator<=(iterator x, iterator y) { return x.ptr_ <= y.ptr_; }

    friend bool operator>(iterator x, iterator y) { return x.ptr_ > y.ptr_; }

    friend bool operator>=(iterator x, iterator y) { return x.ptr_ >= y.ptr_; }

   private:
    const char* ptr_;
    const SerializedStringArray* string_array_;
  };

  void Init(absl::string_view token_array_data,
            absl::string_view string_array_data) {
    token_array_ = token_array_data;
    string_array_.Set(string_array_data);
  }

  iterator begin() const {
    return iterator(token_array_.data(), &string_array_);
  }

  iterator end() const {
    return iterator(token_array_.data() + token_array_.size(), &string_array_);
  }

  std::pair<iterator, iterator> equal_range(absl::string_view key) const {
    const auto iter =
        std::lower_bound(string_array_.begin(), string_array_.end(), key);
    if (iter == string_array_.end() || *iter != key) {
      return std::pair<iterator, iterator>(end(), end());
    }
    return std::equal_range(begin(), end(), iter.index());
  }

 private:
  absl::string_view token_array_;
  SerializedStringArray string_array_;
};

}  // namespace mozc

#endif  // MOZC_PREDICTION_ZERO_QUERY_DICT_H_
