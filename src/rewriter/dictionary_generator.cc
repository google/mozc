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

// Classes managing dictionary entries.

#include "rewriter/dictionary_generator.h"

#include <cstdint>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "data_manager/data_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_pos.h"

namespace mozc {
namespace rewriter {
namespace {

void MergeTokenString(std::string &base, std::string &new_string) {
  if (!new_string.empty()) {
    base = std::move(new_string);
  }
}

}  // namespace

void Token::MergeFrom(Token new_token) {
  sorting_key =
      new_token.sorting_key == 0 ? sorting_key : new_token.sorting_key;
  MergeTokenString(key, new_token.key);
  MergeTokenString(value, new_token.value);
  MergeTokenString(pos, new_token.pos);
  MergeTokenString(description, new_token.description);
  MergeTokenString(additional_description, new_token.additional_description);
}

bool operator==(const Token &lhs, const Token &rhs) {
  return std::tie(lhs.key, lhs.value, lhs.pos) ==
         std::tie(rhs.key, rhs.value, rhs.pos);
}

bool operator<(const Token &lhs, const Token &rhs) {
  return std::tie(lhs.key, lhs.sorting_key, lhs.value) <
         std::tie(rhs.key, rhs.sorting_key, rhs.value);
}

DictionaryGenerator::DictionaryGenerator(const DataManager &data_manager) {
  const dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());
  open_bracket_id_ = pos_matcher.GetOpenBracketId();
  close_bracket_id_ = pos_matcher.GetCloseBracketId();
  user_pos_ = dictionary::UserPos::CreateFromDataManager(data_manager);
}

const Token &DictionaryGenerator::AddToken(Token token) {
  // Take the token as node_type of tokens_.
  auto node = tokens_.extract(token);
  if (node.empty()) {
    return *tokens_.insert(std::move(token)).first;
  } else {
    node.value().MergeFrom(std::move(token));
    return *tokens_.insert(std::move(node.value())).first;
  }
}

bool DictionaryGenerator::Output(std::ostream &os) const {
  uint32_t num_same_keys = 0;
  std::string prev_key;
  for (const Token &token : tokens_) {
    absl::string_view pos = token.pos;

    // Update the number of the sequence of the same keys
    if (prev_key == token.key) {
      ++num_same_keys;
    } else {
      num_same_keys = 0;
      prev_key = token.key;
    }
    const uint32_t cost = 10 * num_same_keys;

    uint16_t id;
    if (pos == "括弧開") {
      id = open_bracket_id_;
    } else if (pos == "括弧閉") {
      id = close_bracket_id_;
    } else {
      CHECK(user_pos_->GetPosIds(pos, &id)) << "Unknown POS type: " << pos;
    }

    // Output in mozc dictionary format
    os << token.key << "\t" << id << "\t" << id << "\t" << cost << "\t"
       << token.value << "\t" << token.description << "\t"
       << token.additional_description << std::endl;
  }
  return true;
}

}  // namespace rewriter
}  // namespace mozc
