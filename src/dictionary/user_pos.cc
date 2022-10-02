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
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {

UserPos::UserPos(absl::string_view token_array_data,
                 absl::string_view string_array_data)
    : token_array_data_(token_array_data) {
  DCHECK_EQ(token_array_data.size() % 8, 0);
  DCHECK(SerializedStringArray::VerifyData(string_array_data));
  string_array_.Set(string_array_data);
}

UserPos::~UserPos() = default;

void UserPos::GetPosList(std::vector<std::string> *pos_list) const {
  pos_list->clear();
  std::set<uint16_t> seen;
  for (auto iter = begin(); iter != end(); ++iter) {
    if (!seen.insert(iter.pos_index()).second) {
      continue;
    }
    const absl::string_view pos = string_array_[iter.pos_index()];
    pos_list->emplace_back(pos.data(), pos.size());
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

bool UserPos::GetPosIds(absl::string_view pos, uint16_t *id) const {
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
                        std::vector<Token> *tokens) const {
  if (key.empty() || value.empty() || pos.empty() || tokens == nullptr) {
    return false;
  }

  tokens->clear();
  const auto str_iter =
      std::lower_bound(string_array_.begin(), string_array_.end(), pos);
  if (str_iter == string_array_.end() || *str_iter != pos) {
    return false;
  }
  std::pair<iterator, iterator> range =
      std::equal_range(begin(), end(), str_iter.index());
  if (range.first == range.second) {
    return false;
  }
  const size_t size = range.second - range.first;
  CHECK_GE(size, 1);
  tokens->resize(size);

  // TODO(taku)  Change the cost by seeing cost_type
  const bool is_non_ja_locale =
      !locale.empty() && !absl::StartsWith(locale, "ja");

  constexpr char kIsolatedWordPos[] = "短縮よみ";
  constexpr char kSuggestionOnlyPos[] = "サジェストのみ";

  uint16_t attributes = 0;
  if (pos == kIsolatedWordPos) {
    attributes = UserPos::Token::ISOLATED_WORD;
  } else if (pos == kSuggestionOnlyPos) {
    attributes = UserPos::Token::SUGGESTION_ONLY;
  }
  if (is_non_ja_locale) {
    attributes |= UserPos::Token::NON_JA_LOCALE;
  }

  if (size == 1) {  // no conjugation
    const auto &token_iter = range.first;
    (*tokens)[0].key = std::string(key);
    (*tokens)[0].value = std::string(value);
    (*tokens)[0].id = token_iter.conjugation_id();
    (*tokens)[0].attributes = attributes;
  } else {
    const auto &base_form_token_iter = range.first;
    // expand all other forms
    std::string key_stem = std::string(key);
    std::string value_stem = std::string(value);
    // assume that conjugation_form[0] contains the suffix of "base form".
    const absl::string_view base_key_suffix =
        string_array_[base_form_token_iter.key_suffix_index()];
    const absl::string_view base_value_suffix =
        string_array_[base_form_token_iter.value_suffix_index()];

    if (base_key_suffix.size() < key.size() &&
        base_value_suffix.size() < value.size() &&
        absl::EndsWith(key, base_key_suffix) &&
        absl::EndsWith(value, base_value_suffix)) {
      key_stem.assign(key.data(), key.size() - base_key_suffix.size());
      value_stem.assign(value.data(), value.size() - base_value_suffix.size());
    }
    for (size_t i = 0; i < size; ++i, ++range.first) {
      const auto &token_iter = range.first;
      const absl::string_view key_suffix =
          string_array_[token_iter.key_suffix_index()];
      const absl::string_view value_suffix =
          string_array_[token_iter.value_suffix_index()];
      (*tokens)[i].key.clear();
      (*tokens)[i].value.clear();
      absl::StrAppend(&(*tokens)[i].key, key_stem, key_suffix);
      absl::StrAppend(&(*tokens)[i].value, value_stem, value_suffix);
      (*tokens)[i].id = token_iter.conjugation_id();
      (*tokens)[i].attributes = attributes;
    }
    DCHECK(range.first == range.second);
  }

  return true;
}

std::unique_ptr<UserPos> UserPos::CreateFromDataManager(
    const DataManagerInterface &manager) {
  absl::string_view token_array_data, string_array_data;
  manager.GetUserPosData(&token_array_data, &string_array_data);
  return std::make_unique<UserPos>(token_array_data, string_array_data);
}

}  // namespace dictionary
}  // namespace mozc
