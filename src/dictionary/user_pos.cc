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

#include "dictionary/user_pos.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "base/strings/assign.h"
#include "data_manager/data_manager.h"

namespace mozc {
namespace dictionary {

UserPos::UserPos(absl::string_view token_array_data,
                 absl::string_view string_array_data)
    : token_array_data_(token_array_data) {
  DCHECK_EQ(token_array_data.size() % 8, 0);
  DCHECK(SerializedStringArray::VerifyData(string_array_data));
  string_array_.Set(string_array_data);
  InitPosList();
}

void UserPos::InitPosList() {
  absl::flat_hash_set<uint16_t> seen;
  for (auto iter = begin(); iter != end(); ++iter) {
    if (!seen.insert(iter.pos_index()).second) {
      continue;
    }
    const absl::string_view pos = string_array_[iter.pos_index()];
    if (pos == "名詞") {
      // "名詞" is the default POS.
      pos_list_default_index_ = pos_list_.size();
    }
    pos_list_.push_back({pos.data(), pos.size()});
  }
}

bool UserPos::IsValidPos(absl::string_view pos) const {
  const auto iter =
      std::lower_bound(string_array_.begin(), string_array_.end(), pos);
  if (iter == string_array_.end()) {
    return false;
  }
  return std::binary_search(begin(), end(), iter.index());
}

bool UserPos::GetPosIds(absl::string_view pos, uint16_t* id) const {
  const auto str_iter =
      std::lower_bound(string_array_.begin(), string_array_.end(), pos);
  if (str_iter == string_array_.end() || *str_iter != pos) {
    return false;
  }
  const auto token_iter = std::lower_bound(begin(), end(), str_iter.index());
  if (token_iter == end() || token_iter.pos_index() != str_iter.index()) {
    return false;
  }
  *id = token_iter.conjugation_id();
  return true;
}

bool UserPos::GetTokens(absl::string_view key, absl::string_view value,
                        absl::string_view pos, absl::string_view locale,
                        std::vector<Token>* tokens) const {
  if (key.empty() || value.empty() || pos.empty() || tokens == nullptr) {
    return false;
  }

  tokens->clear();
  const auto str_iter =
      std::lower_bound(string_array_.begin(), string_array_.end(), pos);
  if (str_iter == string_array_.end() || *str_iter != pos) {
    return false;
  }
  const auto [first, last] = std::equal_range(begin(), end(), str_iter.index());
  if (first == last) {
    return false;
  }
  const ptrdiff_t size = last - first;
  CHECK_GE(size, 1);
  tokens->resize(size);

  // TODO(taku)  Change the cost by seeing cost_type
  const bool is_non_ja_locale = !locale.empty() && !locale.starts_with("ja");

  constexpr absl::string_view kIsolatedWordPos = "短縮よみ";
  constexpr absl::string_view kSuggestionOnlyPos = "サジェストのみ";
  constexpr absl::string_view kNoPos = "品詞なし";

  uint16_t attributes = 0;
  if (pos == kIsolatedWordPos) {
    attributes = UserPos::Token::ISOLATED_WORD;
  } else if (pos == kSuggestionOnlyPos) {
    attributes = UserPos::Token::SUGGESTION_ONLY;
  } else if (pos == kNoPos) {
    attributes = UserPos::Token::SHORTCUT;
  }
  if (is_non_ja_locale) {
    attributes |= UserPos::Token::NON_JA_LOCALE;
  }

  if (size == 1) {  // no conjugation
    strings::Assign(tokens->front().key, key);
    strings::Assign(tokens->front().value, value);
    tokens->front().id = first.conjugation_id();
    tokens->front().attributes = attributes;
  } else {
    // expand all other forms
    absl::string_view key_stem = key;
    absl::string_view value_stem = value;
    // assume that conjugation_form[0] contains the suffix of "base form".
    const absl::string_view base_key_suffix =
        string_array_[first.key_suffix_index()];
    const absl::string_view base_value_suffix =
        string_array_[first.value_suffix_index()];

    if (base_key_suffix.size() < key.size() &&
        base_value_suffix.size() < value.size() &&
        key.ends_with(base_key_suffix) && value.ends_with(base_value_suffix)) {
      key_stem.remove_suffix(base_key_suffix.size());
      value_stem.remove_suffix(base_value_suffix.size());
    }
    auto dest = tokens->begin();
    for (auto src = first; src != last; ++src, ++dest) {
      const absl::string_view key_suffix =
          string_array_[src.key_suffix_index()];
      const absl::string_view value_suffix =
          string_array_[src.value_suffix_index()];
      dest->key = absl::StrCat(key_stem, key_suffix);
      dest->value = absl::StrCat(value_stem, value_suffix);
      dest->id = src.conjugation_id();
      dest->attributes = attributes;
    }
  }

  return true;
}

std::unique_ptr<UserPos> UserPos::CreateFromDataManager(
    const DataManager& manager) {
  absl::string_view token_array_data, string_array_data;
  manager.GetUserPosData(&token_array_data, &string_array_data);
  return std::make_unique<UserPos>(token_array_data, string_array_data);
}

}  // namespace dictionary
}  // namespace mozc
