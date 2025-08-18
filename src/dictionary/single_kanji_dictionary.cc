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

#include "dictionary/single_kanji_dictionary.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "data_manager/data_manager.h"
#include "data_manager/serialized_dictionary.h"

namespace mozc {
namespace dictionary {
namespace {

// A random access iterator over uint32_t array that increments a pointer by N:
// iter     -> array[0]
// iter + 1 -> array[N]
// iter + 2 -> array[2 * N]
// ...
template <size_t N>
class Uint32ArrayIterator {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = uint32_t;
  using difference_type = std::ptrdiff_t;
  using pointer = uint32_t*;
  using reference = uint32_t&;

  explicit Uint32ArrayIterator(const uint32_t* ptr) : ptr_(ptr) {}

  uint32_t operator*() const { return *ptr_; }

  uint32_t operator[](size_t i) const {
    DCHECK_LT(i, N);
    return ptr_[i];
  }

  void swap(Uint32ArrayIterator& x) {
    using std::swap;
    swap(ptr_, x.ptr_);
  }

  friend void swap(Uint32ArrayIterator& x, Uint32ArrayIterator& y) {
    x.swap(y);
  }

  Uint32ArrayIterator& operator++() {
    ptr_ += N;
    return *this;
  }

  Uint32ArrayIterator operator++(int) {
    const uint32_t* tmp = ptr_;
    ptr_ += N;
    return Uint32ArrayIterator(tmp);
  }

  Uint32ArrayIterator& operator--() {
    ptr_ -= N;
    return *this;
  }

  Uint32ArrayIterator operator--(int) {
    const uint32_t* tmp = ptr_;
    ptr_ -= N;
    return Uint32ArrayIterator(tmp);
  }

  Uint32ArrayIterator& operator+=(ptrdiff_t n) {
    ptr_ += n * N;
    return *this;
  }

  Uint32ArrayIterator& operator-=(ptrdiff_t n) {
    ptr_ -= n * N;
    return *this;
  }

  friend Uint32ArrayIterator operator+(Uint32ArrayIterator x, ptrdiff_t n) {
    return Uint32ArrayIterator(x.ptr_ + n * N);
  }

  friend Uint32ArrayIterator operator+(ptrdiff_t n, Uint32ArrayIterator x) {
    return Uint32ArrayIterator(x.ptr_ + n * N);
  }

  friend Uint32ArrayIterator operator-(Uint32ArrayIterator x, ptrdiff_t n) {
    return Uint32ArrayIterator(x.ptr_ - n * N);
  }

  friend ptrdiff_t operator-(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return (x.ptr_ - y.ptr_) / N;
  }

  friend bool operator==(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ == y.ptr_;
  }

  friend bool operator!=(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ != y.ptr_;
  }

  friend bool operator<(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ < y.ptr_;
  }

  friend bool operator<=(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ <= y.ptr_;
  }

  friend bool operator>(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ > y.ptr_;
  }

  friend bool operator>=(Uint32ArrayIterator x, Uint32ArrayIterator y) {
    return x.ptr_ >= y.ptr_;
  }

 private:
  const uint32_t* ptr_;
};

}  // namespace

SingleKanjiDictionary::SingleKanjiDictionary(const DataManager& data_manager) {
  absl::string_view string_array_data;
  absl::string_view variant_type_array_data;
  absl::string_view variant_string_array_data;
  absl::string_view noun_prefix_token_array_data;
  absl::string_view noun_prefix_string_array_data;
  data_manager.GetSingleKanjiRewriterData(
      &single_kanji_token_array_, &string_array_data, &variant_type_array_data,
      &variant_token_array_, &variant_string_array_data,
      &noun_prefix_token_array_data, &noun_prefix_string_array_data);

  // Single Kanji token array is an array of uint32_t.  Its size must be
  // multiple of 2; see the comment above LookupKanjiEntries.
  DCHECK_EQ(0, single_kanji_token_array_.size() % (2 * sizeof(uint32_t)));
  DCHECK(SerializedStringArray::VerifyData(string_array_data));
  single_kanji_string_array_.Set(string_array_data);

  DCHECK(SerializedStringArray::VerifyData(variant_type_array_data));
  variant_type_array_.Set(variant_type_array_data);

  // Variant token array is an array of uint32_t.  Its size must be multiple
  // of 3; see the comment above GenerateDescription.
  DCHECK_EQ(0, variant_token_array_.size() % (3 * sizeof(uint32_t)));
  DCHECK(SerializedStringArray::VerifyData(variant_string_array_data));
  variant_string_array_.Set(variant_string_array_data);

  DCHECK(SerializedDictionary::VerifyData(noun_prefix_token_array_data,
                                          noun_prefix_string_array_data));
  noun_prefix_dictionary_ = std::make_unique<SerializedDictionary>(
      noun_prefix_token_array_data, noun_prefix_string_array_data);
}

// The underlying token array, |single_kanji_token_array_|, has the following
// format:
//
// +------------------+
// | index of key 0   |
// +------------------+
// | index of value 0 |
// +------------------+
// | index of key 1   |
// +------------------+
// | index of value 1 |
// +------------------+
// | ...              |
//
// Here, each element is of uint32_t type.  Each of actual string values are
// stored in |single_kanji_string_array_| at its index.
std::vector<std::string> SingleKanjiDictionary::LookupKanjiEntries(
    absl::string_view key, bool use_svs) const {
  std::vector<std::string> kanji_list;
  const uint32_t* token_array =
      reinterpret_cast<const uint32_t*>(single_kanji_token_array_.data());
  const size_t token_array_size =
      single_kanji_token_array_.size() / sizeof(uint32_t);

  const Uint32ArrayIterator<2> end(token_array + token_array_size);
  const auto iter = std::lower_bound(
      Uint32ArrayIterator<2>(token_array), end, key,
      [this](uint32_t index, const absl::string_view target_key) {
        return this->single_kanji_string_array_[index] < target_key;
      });
  if (iter == end || single_kanji_string_array_[iter[0]] != key) {
    return kanji_list;
  }
  const absl::string_view values = single_kanji_string_array_[iter[1]];
  if (use_svs) {
    std::string svs_values;
    if (TextNormalizer::NormalizeTextToSvs(values, &svs_values)) {
      Util::SplitStringToUtf8Graphemes(svs_values, &kanji_list);
      return kanji_list;
    }
  }

  Util::SplitStringToUtf8Graphemes(values, &kanji_list);

  return kanji_list;
}

// The underlying token array, |variant_token_array_|, has the following
// format:
//
// +-------------------------+
// | index of target 0       |
// +-------------------------+
// | index of original 0     |
// +-------------------------+
// | index of variant type 0 |
// +-------------------------+
// | index of target 1       |
// +-------------------------+
// | index of original 1     |
// +-------------------------+
// | index of variant type 1 |
// +-------------------------+
// | ...                     |
//
// Here, each element is of uint32_t type.  Actual strings of target and
// original are stored in |variant_string_array_|, while strings of variant type
// are stored in |variant_type_array_|.
bool SingleKanjiDictionary::GenerateDescription(absl::string_view kanji_surface,
                                                std::string* desc) const {
  DCHECK(desc);
  const uint32_t* token_array =
      reinterpret_cast<const uint32_t*>(variant_token_array_.data());
  const size_t token_array_size =
      variant_token_array_.size() / sizeof(uint32_t);

  const Uint32ArrayIterator<3> end(token_array + token_array_size);
  const auto iter = std::lower_bound(
      Uint32ArrayIterator<3>(token_array), end, kanji_surface,
      [this](uint32_t index, const absl::string_view target_key) {
        return this->variant_string_array_[index] < target_key;
      });
  if (iter == end || variant_string_array_[iter[0]] != kanji_surface) {
    return false;
  }
  const absl::string_view original = variant_string_array_[iter[1]];
  const uint32_t type_id = iter[2];
  DCHECK_LT(type_id, variant_type_array_.size());
  // Format like "XXXのYYY"
  *desc = absl::StrCat(original, "の", variant_type_array_[type_id]);
  return true;
}

}  // namespace dictionary
}  // namespace mozc
