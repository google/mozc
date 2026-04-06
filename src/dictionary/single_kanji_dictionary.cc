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

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "data_manager/data_manager.h"
#include "data_manager/serialized_dictionary.h"

namespace mozc {
namespace dictionary {
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

  struct Token {
    uint32_t key_index = 0;
    uint32_t value_index = 0;
  } ABSL_ATTRIBUTE_PACKED;

  static_assert(sizeof(Token) == 8);

  absl::Span<const Token> tokens = absl::MakeConstSpan(
      reinterpret_cast<const Token*>(single_kanji_token_array_.data()),
      single_kanji_token_array_.size() / sizeof(Token));

  const auto iter = std::lower_bound(
      tokens.begin(), tokens.end(), key,
      [&](const Token& token, absl::string_view target_key) {
        return single_kanji_string_array_[token.key_index] < target_key;
      });

  if (iter == tokens.end() ||
      single_kanji_string_array_[iter->key_index] != key) {
    return kanji_list;
  }

  const absl::string_view values = single_kanji_string_array_[iter->value_index];
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

  struct Token {
    uint32_t target_index = 0;
    uint32_t original_index = 0;
    uint32_t variant_type = 0;
  } ABSL_ATTRIBUTE_PACKED;

  static_assert(sizeof(Token) == 12);

  absl::Span<const Token> tokens = absl::MakeConstSpan(
      reinterpret_cast<const Token*>(variant_token_array_.data()),
      variant_token_array_.size() / sizeof(Token));

  const auto iter = std::lower_bound(
      tokens.begin(), tokens.end(), kanji_surface,
      [&](const Token& token, absl::string_view target_key) {
        return variant_string_array_[token.target_index] < target_key;
      });

  if (iter == tokens.end() ||
      variant_string_array_[iter->target_index] != kanji_surface) {
    return false;
  }

  const absl::string_view original = variant_string_array_[iter->original_index];
  const uint32_t type_id = iter->variant_type;
  DCHECK_LT(type_id, variant_type_array_.size());
  // Format like "XXXのYYY"
  *desc = absl::StrCat(original, "の", variant_type_array_[type_id]);
  return true;
}

}  // namespace dictionary
}  // namespace mozc
