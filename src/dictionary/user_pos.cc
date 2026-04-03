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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/strings/assign.h"
#include "data_manager/data_manager.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace dictionary {
namespace {

// The list contains pairs of [string_pos, cost]. If the cost is zero, a default
// value is used. This cost is approximately equivalent to the minimum cost
// found for words with the same part of speech in the system dictionary. The
// index of each element should be matched with the actual value of enum. See
// also user_dictionary_storage.p roto for the definition of the enum.
constexpr std::pair<absl::string_view, uint16_t> kPosTypeStringTable[] = {
    {"品詞なし", 2500},  // 0
    {"名詞", 2500},      // 1
    // Set smaller cost for "短縮よみ" in order to make the rank of the word
    // higher than others.
    {"短縮よみ", 200},         // 2
    {"サジェストのみ", 2500},  // 3
    {"固有名詞", 1500},        // 4
    {"人名", 1500},            // 5
    {"姓", 2500},              // 6
    {"名", 2500},              // 7
    {"組織", 2000},            // 8
    {"地名", 2000},            // 9
    {"名詞サ変", 2500},        // 10
    {"名詞形動", 1500},        // 11
    {"数", 1000},              // 12
    {"アルファベット", 2500},  // 13
    {"記号", 500},             // 14
    {"顔文字", 500},           // 15
    {"副詞", 1500},            // 16
    {"連体詞", 1000},          // 17
    {"接続詞", 1000},          // 18
    {"感動詞", 1000},          // 19
    {"接頭語", 1500},          // 20
    {"助数詞", 1000},          // 21
    {"接尾一般", 1500},        // 22
    {"接尾人名", 1000},        // 23
    {"接尾地名", 1000},        // 24
    // Uses the default cost for the words with conjugation.
    // TODO(taku): Optimize the costs for these words.
    {"動詞ワ行五段", 0},  // 25
    {"動詞カ行五段", 0},  // 26
    {"動詞サ行五段", 0},  // 27
    {"動詞タ行五段", 0},  // 27
    {"動詞ナ行五段", 0},  // 29
    {"動詞マ行五段", 0},  // 30
    {"動詞ラ行五段", 0},  // 31
    {"動詞ガ行五段", 0},  // 32
    {"動詞バ行五段", 0},  // 33
    {"動詞ハ行四段", 0},  // 34
    {"動詞一段", 0},      // 35
    {"動詞カ変", 0},      // 36
    {"動詞サ変", 0},      // 37
    {"動詞ザ変", 0},      // 38
    {"動詞ラ変", 0},      // 39
    {"形容詞", 0},        // 40
    {"終助詞", 100},      // 41
    {"句読点", 500},      // 42
    {"独立語", 500},      // 43
    {"抑制単語", 1000},   // 44
};

static_assert(std::size(kPosTypeStringTable) ==
              user_dictionary::UserDictionary::PosType_MAX + 1);
}  // namespace

UserPos::UserPos(absl::string_view token_array_data,
                 absl::string_view string_array_data)
    : token_array_data_(token_array_data) {
  DCHECK_EQ(token_array_data.size() % 8, 0);
  DCHECK(SerializedStringArray::VerifyData(string_array_data));
  string_array_.Set(string_array_data);
  InitPosList();
}

void UserPos::InitPosList() {
  // make pos_list_.
  absl::flat_hash_set<uint16_t> seen;
  for (int i = 0; i < TokenArrayDataSize(); ++i) {
    const TokenArrayData token = GetTokenArrayData(i);
    if (!seen.insert(token.pos_index).second) {
      continue;
    }
    const absl::string_view pos = string_array_[token.pos_index];
    if (pos == "名詞") {
      // "名詞" is the default POS.
      pos_list_default_index_ = pos_list_.size();
    }
    pos_list_.push_back(std::string(pos));
  }
  pos_list_.shrink_to_fit();

  // make token_array_index_
  absl::flat_hash_map<absl::string_view, std::vector<int>> pos_to_index;
  for (int i = 0; i < TokenArrayDataSize(); ++i) {
    absl::string_view pos = string_array_[GetTokenArrayData(i).pos_index];
    pos_to_index[pos].push_back(i);
  }

  token_array_index_.resize(std::size(kPosTypeStringTable));
  for (int i = 0; i < std::size(kPosTypeStringTable); ++i) {
    absl::string_view pos = kPosTypeStringTable[i].first;
    if (auto it = pos_to_index.find(pos); it != pos_to_index.end()) {
      token_array_index_[i] = std::move(it->second);
      token_array_index_[i].shrink_to_fit();
    } else {
      LOG(ERROR) << pos << " is not defined in the UserPosData";
    }
  }
}

std::optional<uint16_t> UserPos::GetPosIds(absl::string_view pos) const {
  const user_dictionary::UserDictionary::PosType pos_type =
      UserPos::ToPosType(pos);
  if (!user_dictionary::UserDictionary::PosType_IsValid(pos_type)) {
    return std::nullopt;
  }

  absl::Span<const int> token_array_index = token_array_index_[pos_type];
  if (token_array_index.empty()) {
    return std::nullopt;
  }

  return GetTokenArrayData(token_array_index.front()).conjugation_id;
}

std::vector<UserPos::Token> UserPos::GetTokens(
    absl::string_view key, absl::string_view value,
    user_dictionary::UserDictionary::PosType pos_type,
    absl::string_view locale) const {
  if (key.empty() || value.empty() ||
      !user_dictionary::UserDictionary::PosType_IsValid(pos_type)) {
    return {};
  }

  absl::Span<const int> token_array_index = token_array_index_[pos_type];
  if (token_array_index.empty()) {
    return {};
  }

  std::vector<Token> tokens(token_array_index.size());

  const bool is_non_ja_locale = !locale.empty() && !locale.starts_with("ja");

  uint8_t attributes = 0;
  if (is_non_ja_locale) {
    attributes |= UserPos::Token::NON_JA_LOCALE;
  }

  const TokenArrayData first = GetTokenArrayData(token_array_index.front());
  auto dest = tokens.begin();

  if (tokens.size() == 1) {  // no conjugation
    strings::Assign(dest->key, key);
    strings::Assign(dest->value, value);
    dest->id = first.conjugation_id;
    dest->attributes = attributes;
    dest->set_pos_type(pos_type);
  } else {
    // expand all other forms
    absl::string_view key_stem = key;
    absl::string_view value_stem = value;
    // assume that conjugation_form[0] contains the suffix of "base form".
    absl::string_view base_key_suffix = string_array_[first.key_suffix_index];
    absl::string_view base_value_suffix =
        string_array_[first.value_suffix_index];

    if (base_key_suffix.size() < key.size() &&
        base_value_suffix.size() < value.size() &&
        key.ends_with(base_key_suffix) && value.ends_with(base_value_suffix)) {
      key_stem.remove_suffix(base_key_suffix.size());
      value_stem.remove_suffix(base_value_suffix.size());
    }
    for (int i = 0; i < tokens.size(); ++i) {
      const TokenArrayData src = GetTokenArrayData(token_array_index[i]);
      absl::string_view key_suffix = string_array_[src.key_suffix_index];
      absl::string_view value_suffix = string_array_[src.value_suffix_index];
      dest->key = absl::StrCat(key_stem, key_suffix);
      dest->value = absl::StrCat(value_stem, value_suffix);
      dest->id = src.conjugation_id;
      dest->attributes = attributes;
      dest->set_pos_type(pos_type);
      ++dest;
    }
  }

  return tokens;
}

std::unique_ptr<UserPos> UserPos::CreateFromDataManager(
    const DataManager& manager) {
  absl::string_view token_array_data, string_array_data;
  manager.GetUserPosData(&token_array_data, &string_array_data);
  return std::make_unique<UserPos>(token_array_data, string_array_data);
}

// static
absl::string_view UserPos::GetStringPosType(
    user_dictionary::UserDictionary::PosType pos_type) {
  if (user_dictionary::UserDictionary::PosType_IsValid(pos_type)) {
    return kPosTypeStringTable[pos_type].first;
  }
  return {};
}

// static
uint16_t UserPos::GetCostFromPosType(
    user_dictionary::UserDictionary::PosType pos_type) {
  int cost = 0;
  if (user_dictionary::UserDictionary::PosType_IsValid(pos_type)) {
    cost = kPosTypeStringTable[pos_type].second;
  }
  static constexpr uint16_t kDefaultCost = 5000;
  return cost > 0 ? cost : kDefaultCost;
}

// static
user_dictionary::UserDictionary::PosType UserPos::ToPosType(
    absl::string_view string_pos_type) {
  // make a static reverse lookup from string_pos to pos_type.
  static const absl::NoDestructor<
      absl::flat_hash_map<absl::string_view, uint8_t>>
      kPosTypeMap([&] {
        absl::flat_hash_map<absl::string_view, uint8_t> pos_type_map;
        for (int i = 0; i < std::size(kPosTypeStringTable); ++i) {
          pos_type_map[kPosTypeStringTable[i].first] = i;
        }
        return pos_type_map;  // moved
      }());

  int index = -1;
  if (const auto it = kPosTypeMap->find(string_pos_type);
      it != kPosTypeMap->end()) {
    index = it->second;
  }

  return static_cast<user_dictionary::UserDictionary::PosType>(index);
}

}  // namespace dictionary
}  // namespace mozc
