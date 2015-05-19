// Copyright 2010-2012, Google Inc.
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

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/freelist.h"
#include "base/logging.h"
#include "base/util.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_pos.h"

namespace mozc {
namespace rewriter {

Token::Token()
    : sorting_key_(0) {}

Token::~Token() {}

void Token::MergeFrom(const Token &token) {
  if (token.sorting_key() != 0) {
    sorting_key_ = token.sorting_key();
  }
  if (!token.key().empty()) {
    key_ = token.key();
  }
  if (!token.value().empty()) {
    value_ = token.value();
  }
  if (!token.pos().empty()) {
    pos_ = token.pos();
  }
  if (!token.description().empty()) {
    description_ = token.description();
  }
  if (!token.additional_description().empty()) {
    additional_description_ = token.additional_description();
  }
}

uint64 Token::GetID() const {
  return Util::Fingerprint(key_ + "\t" + value_ + "\t" + pos_);
}

static const size_t kTokenSize = 1000;

DictionaryGenerator::DictionaryGenerator()
    : token_pool_(new ObjectPool<Token>(kTokenSize)),
      token_map_(new map<uint64, Token *>),
      user_pos_(UserPosManager::GetUserPosManager()->GetUserPOS()),
      open_bracket_id_(UserPosManager::GetUserPosManager()->GetPOSMatcher()
                       ->GetOpenBracketId()),
      close_bracket_id_(UserPosManager::GetUserPosManager()->GetPOSMatcher()
                        ->GetCloseBracketId()) {}

DictionaryGenerator::~DictionaryGenerator() {}

void DictionaryGenerator::AddToken(const Token &token) {
  Token *new_token = NULL;
  map<uint64, Token *>::const_iterator it = token_map_->find(token.GetID());
  if (it != token_map_->end()) {
    new_token = it->second;
  } else {
    new_token = token_pool_->Alloc();
    token_map_->insert(make_pair(token.GetID(), new_token));
  }
  new_token->MergeFrom(token);
}

namespace {
struct CompareToken {
  bool operator() (const Token *t1, const Token *t2) const {
    if (t1->key() != t2->key()) {
      // Sort by keys first.  Key represents the reading of the token.
      return (t1->key() < t2->key());
    } else if (t1->sorting_key() != t2->sorting_key()) {
      // If the keys are equal, sorting keys are used.  Sorting keys
      // are tipically character encodings like CP932, Unicode, etc.
      return (t1->sorting_key() < t2->sorting_key());
    }
    // If the both keys are equal, values (encoded in UTF8) are used.
    return (t1->value() < t2->value());
  }
};

void GetSortedTokens(const map<uint64, Token *> *token_map,
                     vector<const Token *> *tokens) {
  tokens->clear();
  for (map<uint64, Token *>::const_iterator it = token_map->begin();
       it != token_map->end();
       ++it) {
    tokens->push_back(it->second);
  }
  sort(tokens->begin(), tokens->end(), CompareToken());
}
}  // namespace

bool DictionaryGenerator::Output(const string &filename) const {
  mozc::OutputFileStream ofs(filename.c_str());
  if (!ofs) {
    LOG(ERROR) << "Failed to open: " << filename;
    return false;
  }

  vector<const Token *> tokens;
  GetSortedTokens(token_map_.get(), &tokens);

  uint32 num_same_keys = 0;
  string prev_key;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const Token &token = *(tokens[i]);
    const string &pos = token.pos();

    // Update the number of the sequence of the same keys
    if (prev_key == token.key()) {
      ++num_same_keys;
    } else {
      num_same_keys = 0;
      prev_key = token.key();
    }
    const uint32 cost = 10 * num_same_keys;

    uint16 id;
    if (pos == "\xE6\x8B\xAC\xE5\xBC\xA7\xE9\x96\x8B") {      // "括弧開"
      id = open_bracket_id_;
    } else if (pos == "\xE6\x8B\xAC\xE5\xBC\xA7\xE9\x96\x89") {  // "括弧閉"
      id = close_bracket_id_;
    } else {
      CHECK(user_pos_->GetPOSIDs(pos, &id)) << "Unknown POS type: " << pos;
    }

    // Output in mozc dictionary format
    ofs << token.key() << "\t"
        << id << "\t"
        << id << "\t"
        << cost << "\t"
        << token.value() << "\t"
        << (token.description().empty()? "": token.description()) << "\t"
        << (token.additional_description().empty()?
            "": token.additional_description());
    ofs << endl;
  }
  return true;
}


}  // namespace rewriter
}  // namespace mozc
