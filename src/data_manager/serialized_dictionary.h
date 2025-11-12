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

#ifndef MOZC_DATA_MANAGER_SERIALIZED_DICTIONARY_H_
#define MOZC_DATA_MANAGER_SERIALIZED_DICTIONARY_H_

#include <cstddef>
#include <cstdint>
#include <istream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/bits.h"
#include "base/container/serialized_string_array.h"

namespace mozc {

// This class serializes Mozc's dictionary data and load it at runtime without
// cost of deserialization.  Mozc's dictionary is analogous to
// std::multimap<Key, Value>, where Key is a string (e.g., reading for symbols,
// etc.), and Value is its associated data:
//
// Value {
//   string value;  // Surface form
//   string description;
//   string additional_description;
//   uint16_t lid;
//   uint16_t rid;
//   int16_t cost;
// }
//
// * Prerequisite
// Little endian is assumed.
//
// * Creating serialized data
// The binary data consists of two data, token array and string array.  Use
// SerializedDictionary::Compile() to create the images.
//
// * Map access
// At runtime, we can access map contents just by loading two binary images,
// token array and string array, e.g., from files, onto memory blocks.  But
// these two memory blocks must be aligned at 4 byte boundary.  Accessors are
// designed to have similar interfaces to std::multimap<string, Value>, so
// values can be looked up by equal_range(), etc.
//
// * Binary format
//
// ** String array
// All the strings, such as keys and values, are serialized into one array using
// SerializedStringArray.  In the map structure (see below), every string is
// stored as an index to this array.
//
// ** Token array
// A key value pair of map entry is encoded as data block of fixed byte length:
//
// Token layout (24 bytes)
// +---------------------------------------+
// | Key index  (4 bytes)                  |
// + - - - - - - - - - - - - - - - - - - - +
// | Value index (4 bytes)                 |
// + - - - - - - - - - - - - - - - - - - - +
// | Description index  (4 bytes)          |
// + - - - - - - - - - - - - - - - - - - - +
// | Additional description index (4 bytes)|
// + - - - - - - - - - - - - - - - - - - - +
// | LID (2 bytes)                         |
// + - - - - - - - - - - - - - - - - - - - +
// | RID (2 bytes)                         |
// + - - - - - - - - - - - - - - - - - - - +
// | Cost (2 bytes)                        |
// + - - - - - - - - - - - - - - - - - - - +
// | Padding = 0x0000 (2 bytes)            |
// +---------------------------------------+
//
// The map structure is serialized as a sorted array of tokens where tokens are
// sorted first by key and then by cost, both in ascending order.  Thus, the
// array has 24 * #tokens bytes.  Note that each token is properly aligned at 4
// byte boundary by the insertion of padding.  String values of a token (key,
// value, description, additional_description) can be retrieved from the string
// array by index.
class SerializedDictionary {
 public:
  struct CompilerToken {
    std::string value;
    std::string description;
    std::string additional_description;
    uint16_t lid;
    uint16_t rid;
    int16_t cost;
  };

  using TokenList = std::vector<std::unique_ptr<CompilerToken>>;

  static constexpr size_t kTokenByteLength = 24;

  class iterator {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = absl::string_view;
    using difference_type = std::ptrdiff_t;
    using pointer = absl::string_view*;
    using reference = absl::string_view&;

    iterator() : token_ptr_(nullptr), string_array_(nullptr) {}
    iterator(const char* token_ptr, const SerializedStringArray* string_array)
        : token_ptr_(token_ptr), string_array_(string_array) {}
    iterator(const iterator& x) = default;

    uint32_t key_index() const { return LoadUnaligned<uint32_t>(token_ptr_); }
    absl::string_view key() const { return (*string_array_)[key_index()]; }

    uint32_t value_index() const {
      return LoadUnaligned<uint32_t>(token_ptr_ + 4);
    }
    absl::string_view value() const { return (*string_array_)[value_index()]; }

    uint32_t description_index() const {
      return LoadUnaligned<uint32_t>(token_ptr_ + 8);
    }

    absl::string_view description() const {
      return (*string_array_)[description_index()];
    }

    uint32_t additional_description_index() const {
      return LoadUnaligned<uint32_t>(token_ptr_ + 12);
    }

    absl::string_view additional_description() const {
      return (*string_array_)[additional_description_index()];
    }

    uint16_t lid() const { return LoadUnaligned<uint16_t>(token_ptr_ + 16); }

    uint16_t rid() const { return LoadUnaligned<uint16_t>(token_ptr_ + 18); }

    int16_t cost() const { return LoadUnaligned<int16_t>(token_ptr_ + 20); }

    absl::string_view operator*() { return key(); }
    absl::string_view operator*() const { return key(); }

    void swap(iterator& x) {
      using std::swap;
      swap(token_ptr_, x.token_ptr_);
      swap(string_array_, x.string_array_);
    }

    friend void swap(iterator& x, iterator& y) { x.swap(y); }

    iterator& operator++() {
      token_ptr_ += kTokenByteLength;
      return *this;
    }

    iterator operator++(int) {
      const char* tmp = token_ptr_;
      token_ptr_ += kTokenByteLength;
      return iterator(tmp, string_array_);
    }

    iterator& operator--() {
      token_ptr_ -= kTokenByteLength;
      return *this;
    }

    iterator operator--(int) {
      const char* tmp = token_ptr_;
      token_ptr_ -= kTokenByteLength;
      return iterator(tmp, string_array_);
    }

    iterator& operator+=(difference_type n) {
      token_ptr_ += n * kTokenByteLength;
      return *this;
    }

    iterator& operator-=(difference_type n) {
      token_ptr_ -= n * kTokenByteLength;
      return *this;
    }

    friend iterator operator+(iterator x, difference_type n) {
      return iterator(x.token_ptr_ + n * kTokenByteLength, x.string_array_);
    }

    friend iterator operator+(difference_type n, iterator x) {
      return iterator(x.token_ptr_ + n * kTokenByteLength, x.string_array_);
    }

    friend iterator operator-(iterator x, difference_type n) {
      return iterator(x.token_ptr_ - n * kTokenByteLength, x.string_array_);
    }

    friend difference_type operator-(iterator x, iterator y) {
      return (x.token_ptr_ - y.token_ptr_) / kTokenByteLength;
    }

    // The following comparison operators make sense only for iterators obtained
    // from the same dictionary.
    friend bool operator==(iterator x, iterator y) {
      DCHECK_EQ(x.string_array_, y.string_array_);
      return x.token_ptr_ == y.token_ptr_;
    }

    friend bool operator!=(iterator x, iterator y) {
      DCHECK_EQ(x.string_array_, y.string_array_);
      return x.token_ptr_ != y.token_ptr_;
    }

    friend bool operator<(iterator x, iterator y) {
      DCHECK_EQ(x.string_array_, y.string_array_);
      return x.token_ptr_ < y.token_ptr_;
    }

    friend bool operator<=(iterator x, iterator y) {
      DCHECK_EQ(x.string_array_, y.string_array_);
      return x.token_ptr_ <= y.token_ptr_;
    }

    friend bool operator>(iterator x, iterator y) {
      DCHECK_EQ(x.string_array_, y.string_array_);
      return x.token_ptr_ > y.token_ptr_;
    }

    friend bool operator>=(iterator x, iterator y) {
      DCHECK_EQ(x.string_array_, y.string_array_);
      return x.token_ptr_ >= y.token_ptr_;
    }

   private:
    const char* token_ptr_;
    const SerializedStringArray* string_array_;
  };

  using const_iterator = iterator;

  using IterRange = std::pair<const_iterator, const_iterator>;

  // Creates serialized data into buffers.  The first and second string views of
  // returned value points to memory block for token array and string array,
  // respectively.  The input stream should supply TSV file of Mozc's dctionary
  // format; see, e.g., data/symbol/symbol.tsv.
  static std::pair<absl::string_view, absl::string_view> Compile(
      std::istream* input, std::unique_ptr<uint32_t[]>* output_token_array_buf,
      std::unique_ptr<uint32_t[]>* output_string_array_buf);
  static std::pair<absl::string_view, absl::string_view> Compile(
      const std::map<std::string, TokenList>& dic,
      std::unique_ptr<uint32_t[]>* output_token_array_buf,
      std::unique_ptr<uint32_t[]>* output_string_array_buf);

  // Creates serialized data and writes them to files.
  static void CompileToFiles(const std::string& input,
                             const std::string& output_token_array,
                             const std::string& output_string_array);
  static void CompileToFiles(const std::map<std::string, TokenList>& dic,
                             const std::string& output_token_array,
                             const std::string& output_string_array);

  // Validates the serialized data.
  static bool VerifyData(absl::string_view token_array_data,
                         absl::string_view string_array_data);

  // Both |token_array| and |string_array_data| must be aligned at 4-byte
  // boundary.
  SerializedDictionary(absl::string_view token_array,
                       absl::string_view string_array_data);
  ~SerializedDictionary() = default;

  std::size_t size() const { return token_array_.size() / kTokenByteLength; }

  iterator begin() { return iterator(token_array_.data(), &string_array_); }
  const_iterator begin() const {
    return iterator(token_array_.data(), &string_array_);
  }

  iterator end() {
    return iterator(token_array_.data() + token_array_.size(), &string_array_);
  }
  iterator end() const {
    return iterator(token_array_.data() + token_array_.size(), &string_array_);
  }

  // Returns the range of iterators whose keys match the given key.  The range
  // is sorted in ascending order of cost.
  IterRange equal_range(absl::string_view key) const;

 private:
  absl::string_view token_array_;
  SerializedStringArray string_array_;
};

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_SERIALIZED_DICTIONARY_H_
