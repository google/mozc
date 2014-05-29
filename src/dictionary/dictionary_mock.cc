// Copyright 2010-2014, Google Inc.
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

#include "dictionary/dictionary_mock.h"

#include <climits>
#include <map>
#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"

namespace mozc {
namespace {

const int kDummyPosId = 1;

bool HasValueInternal(const map<string, vector<Token *> > &dic,
                      StringPiece value) {
  typedef vector<Token *> TokenPtrVector;
  for (map<string, vector<Token *> >::const_iterator map_it = dic.begin();
       map_it != dic.end(); ++map_it) {
    const TokenPtrVector &v = map_it->second;
    for (TokenPtrVector::const_iterator it = v.begin(); it != v.end(); ++it) {
      if ((*it)->value == value) {
        return true;
      }
    }
  }
  return false;
}

Token *CreateToken(const string &str, const string &key, const string &value,
                   Token::AttributesBitfield attributes) {
  scoped_ptr<Token> token(new Token());
  token->key = key;
  token->value = value;
  // TODO(noriyukit): Currently, we cannot set cost and POS IDs.
  token->cost = 0;
  token->lid = token->rid = kDummyPosId;
  token->attributes = attributes;
  return token.release();
}

void DeletePtrs(map<string, vector<Token *> > *m) {
  for (map<string, vector<Token *> >::iterator iter = m->begin();
       iter != m->end(); ++iter) {
    STLDeleteElements(&iter->second);
  }
}

}  // namespace

DictionaryMock::DictionaryMock() {
  LOG(INFO) << "DictionaryMock is created";
}

DictionaryMock::~DictionaryMock() {
  DeletePtrs(&prefix_dictionary_);
  DeletePtrs(&exact_dictionary_);
  DeletePtrs(&reverse_dictionary_);
  DeletePtrs(&predictive_dictionary_);
}

bool DictionaryMock::HasValue(StringPiece value) const {
  return HasValueInternal(predictive_dictionary_, value) ||
         HasValueInternal(prefix_dictionary_, value) ||
         HasValueInternal(reverse_dictionary_, value) ||
         HasValueInternal(exact_dictionary_, value);
}

void DictionaryMock::LookupPredictive(
    StringPiece key,
    bool,  // use_kana_modifier_insensitive_lookup
    Callback *callback) const {
  map<string, vector<Token *> >::const_iterator vector_iter =
      predictive_dictionary_.find(key.as_string());
  if (vector_iter == predictive_dictionary_.end()) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE ||
      callback->OnActualKey(key, key, false) != Callback::TRAVERSE_CONTINUE) {
    return;
  }
  for (vector<Token *>::const_iterator iter = vector_iter->second.begin();
       iter != vector_iter->second.end(); ++iter) {
    if (callback->OnToken(key, key, **iter) != Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

void DictionaryMock::LookupPrefix(
    StringPiece key,
    bool,  // use_kana_modifier_insensitive_lookup
    Callback *callback) const {
  CHECK(!key.empty());

  string prefix;
  for (size_t len = 1; len <= key.size(); ++len) {
    key.substr(0, len).CopyToString(&prefix);
    map<string, vector<Token *> >::const_iterator iter =
        prefix_dictionary_.find(prefix);
    if (iter == prefix_dictionary_.end()) {
      continue;
    }
    switch (callback->OnKey(prefix)) {
      case Callback::TRAVERSE_DONE:
      case Callback::TRAVERSE_CULL:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      default:
        break;
    }
    switch (callback->OnActualKey(prefix, prefix, false)) {
      case Callback::TRAVERSE_DONE:
      case Callback::TRAVERSE_CULL:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      default:
        break;
    }
    for (vector<Token *>::const_iterator token_iter = iter->second.begin();
         token_iter != iter->second.end(); ++token_iter) {
      Callback::ResultType ret =
          callback->OnToken(prefix, prefix, **token_iter);
      if (ret == Callback::TRAVERSE_DONE || ret == Callback::TRAVERSE_CULL) {
        return;
      }
      if (ret == Callback::TRAVERSE_NEXT_KEY) {
        break;
      }
    }
  }
}

void DictionaryMock::LookupExact(StringPiece key, Callback *callback) const {
  map<string, vector<Token *> >::const_iterator iter =
      exact_dictionary_.find(key.as_string());
  if (iter == exact_dictionary_.end()) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }
  for (vector<Token *>::const_iterator token_iter = iter->second.begin();
       token_iter != iter->second.end(); ++token_iter) {
    if (callback->OnToken(key, key, **token_iter) !=
        Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

void DictionaryMock::LookupReverse(StringPiece str,
                                   NodeAllocatorInterface *allocator,
                                   Callback *callback) const {
  CHECK(!str.empty());

  for (int i = 1; i <= str.size(); ++i) {
    StringPiece prefix = str.substr(0, i);

    map<string, vector<Token *> >::const_iterator iter =
        reverse_dictionary_.find(prefix.as_string());
    if (iter == reverse_dictionary_.end()) {
      continue;
    }

    if (callback->OnKey(prefix) != Callback::TRAVERSE_CONTINUE) {
      return;
    }
    for (vector<Token *>::const_iterator token_iter = iter->second.begin();
         token_iter != iter->second.end(); ++token_iter) {
      if (callback->OnToken(prefix, prefix, **token_iter) !=
          Callback::TRAVERSE_CONTINUE) {
        return;
      }
    }
  }
}

void DictionaryMock::AddLookupPredictive(const string &str,
                                         const string &key, const string &value,
                                         Token::AttributesBitfield attributes) {
  predictive_dictionary_[str].push_back(
      CreateToken(str, key, value, attributes));
}

void DictionaryMock::AddLookupPrefix(const string &str,
                                     const string &key, const string &value,
                                     Token::AttributesBitfield attributes) {
  prefix_dictionary_[str].push_back(
      CreateToken(str, key, value, attributes));
}

void DictionaryMock::AddLookupReverse(const string &str,
                                      const string &key, const string &value,
                                      Token::AttributesBitfield attributes) {
  reverse_dictionary_[str].push_back(
      CreateToken(str, key, value, attributes));
}

void DictionaryMock::AddLookupExact(const string &str,
                                    const string &key, const string &value,
                                    Token::AttributesBitfield attributes) {
  exact_dictionary_[str].push_back(
      CreateToken(str, key, value, attributes));
}

}  // namespace mozc
