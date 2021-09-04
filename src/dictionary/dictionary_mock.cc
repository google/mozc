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

#include "dictionary/dictionary_mock.h"

#include <climits>
#include <map>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {
namespace {

bool HasKeyInternal(
    const std::map<std::string, std::vector<std::unique_ptr<Token>>> &dic,
    absl::string_view key) {
  for (const auto &kv : dic) {
    for (const auto &token_ptr : kv.second) {
      if (token_ptr->key == key) {
        return true;
      }
    }
  }
  return false;
}

bool HasValueInternal(
    const std::map<std::string, std::vector<std::unique_ptr<Token>>> &dic,
    absl::string_view value) {
  for (const auto &kv : dic) {
    for (const auto &token_ptr : kv.second) {
      if (token_ptr->value == value) {
        return true;
      }
    }
  }
  return false;
}

std::unique_ptr<Token> CreateToken(const std::string &key,
                                   const std::string &value,
                                   Token::AttributesBitfield attributes) {
  return dictionary::CreateToken(key, value, DictionaryMock::kDefaultCost,
                                 DictionaryMock::kDummyPosId,
                                 DictionaryMock::kDummyPosId, attributes);
}

}  // namespace

std::unique_ptr<Token> CreateToken(const std::string &key,
                                   const std::string &value, int cost, int lid,
                                   int rid,
                                   Token::AttributesBitfield attributes) {
  std::unique_ptr<Token> token(new Token());
  token->key = key;
  token->value = value;
  token->cost = cost;
  token->lid = lid;
  token->rid = rid;
  token->attributes = attributes;
  return token;
}

const int DictionaryMock::kDefaultCost = 0;
const int DictionaryMock::kDummyPosId = 1;

DictionaryMock::DictionaryMock() { LOG(INFO) << "DictionaryMock is created"; }

DictionaryMock::~DictionaryMock() = default;

bool DictionaryMock::HasKey(absl::string_view key) const {
  return HasKeyInternal(predictive_dictionary_, key) ||
         HasKeyInternal(prefix_dictionary_, key) ||
         HasKeyInternal(reverse_dictionary_, key) ||
         HasKeyInternal(exact_dictionary_, key);
}

bool DictionaryMock::HasValue(absl::string_view value) const {
  return HasValueInternal(predictive_dictionary_, value) ||
         HasValueInternal(prefix_dictionary_, value) ||
         HasValueInternal(reverse_dictionary_, value) ||
         HasValueInternal(exact_dictionary_, value);
}

void DictionaryMock::LookupPredictive(
    absl::string_view key, const ConversionRequest &conversion_request,
    Callback *callback) const {
  const auto vector_iter = predictive_dictionary_.find((std::string(key)));
  if (vector_iter == predictive_dictionary_.end()) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE ||
      callback->OnActualKey(key, key, false) != Callback::TRAVERSE_CONTINUE) {
    return;
  }
  for (const auto &token_ptr : vector_iter->second) {
    if (callback->OnToken(key, key, *token_ptr) !=
        Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

void DictionaryMock::LookupPrefix(absl::string_view key,
                                  const ConversionRequest &conversion_request,
                                  Callback *callback) const {
  CHECK(!key.empty());

  std::string prefix;
  for (size_t len = 1; len <= key.size(); ++len) {
    prefix.assign(key.data(), len);
    const auto iter = prefix_dictionary_.find(prefix);
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
    for (const auto &token_ptr : iter->second) {
      Callback::ResultType ret = callback->OnToken(prefix, prefix, *token_ptr);
      if (ret == Callback::TRAVERSE_DONE || ret == Callback::TRAVERSE_CULL) {
        return;
      }
      if (ret == Callback::TRAVERSE_NEXT_KEY) {
        break;
      }
    }
  }
}

void DictionaryMock::LookupExact(absl::string_view key,
                                 const ConversionRequest &conversion_request,
                                 Callback *callback) const {
  const auto iter = exact_dictionary_.find((std::string(key)));
  if (iter == exact_dictionary_.end()) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }
  for (const auto &token_ptr : iter->second) {
    if (callback->OnToken(key, key, *token_ptr) !=
        Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

void DictionaryMock::LookupReverse(absl::string_view str,
                                   const ConversionRequest &conversion_request,
                                   Callback *callback) const {
  CHECK(!str.empty());

  for (int i = 1; i <= str.size(); ++i) {
    absl::string_view prefix = str.substr(0, i);

    const auto iter = reverse_dictionary_.find((std::string(prefix)));
    if (iter == reverse_dictionary_.end()) {
      continue;
    }

    if (callback->OnKey(prefix) != Callback::TRAVERSE_CONTINUE) {
      return;
    }
    for (const auto &token_ptr : iter->second) {
      if (callback->OnToken(prefix, prefix, *token_ptr) !=
          Callback::TRAVERSE_CONTINUE) {
        return;
      }
    }
  }
}

void DictionaryMock::AddLookupPredictive(const std::string &str,
                                         const std::string &key,
                                         const std::string &value, int cost,
                                         int lid, int rid,
                                         Token::AttributesBitfield attributes) {
  predictive_dictionary_[str].push_back(
      CreateToken(key, value, cost, lid, rid, attributes));
}

void DictionaryMock::AddLookupPrefix(const std::string &str,
                                     const std::string &key,
                                     const std::string &value,
                                     Token::AttributesBitfield attributes) {
  prefix_dictionary_[str].push_back(CreateToken(key, value, attributes));
}

void DictionaryMock::AddLookupReverse(const std::string &str,
                                      const std::string &key,
                                      const std::string &value,
                                      Token::AttributesBitfield attributes) {
  reverse_dictionary_[str].push_back(CreateToken(key, value, attributes));
}

void DictionaryMock::AddLookupExact(const std::string &str,
                                    const std::string &key,
                                    const std::string &value,
                                    Token::AttributesBitfield attributes) {
  exact_dictionary_[str].push_back(CreateToken(key, value, attributes));
}

}  // namespace dictionary
}  // namespace mozc
