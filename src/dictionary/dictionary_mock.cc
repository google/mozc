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
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/node_list_builder.h"

namespace mozc {
namespace {

const int kDummyPosId = 1;

uint8 ConvertNodeAttrToTokenAttr(uint32 node_attributes) {
  uint8 attr = Token::NONE;
  if (node_attributes & Node::SPELLING_CORRECTION) {
    attr |= Token::SPELLING_CORRECTION;
  }
  if (node_attributes & Node::USER_DICTIONARY) {
    attr |= Token::USER_DICTIONARY;
  }
  return attr;
}

typedef list<DictionaryMock::NodeData> NodeDataList;

static Node *LookupInternal(const map<string, NodeDataList> &dic,
                            const char *str,
                            size_t size, NodeAllocatorInterface *allocator,
                            bool is_prefix_search) {
  CHECK(str);

  const StringPiece lookup_str(str, size);
  Node *cur_node = NULL;

  size_t start_len;
  if (is_prefix_search) {
    start_len = 1;
  } else {
    start_len = lookup_str.length();
  }
  const size_t end_len = lookup_str.size();

  for (size_t i = start_len; i <= end_len; ++i) {
    const string prefix(str, i);
    map<string, NodeDataList>::const_iterator iter = dic.find(prefix);
    if (iter == dic.end()) {
      continue;
    }

    const NodeDataList &node_list = iter->second;
    NodeDataList::const_iterator it_list = node_list.begin();
    for (; it_list != node_list.end(); ++it_list) {
      const DictionaryMock::NodeData &node_data = *it_list;
      if (node_data.lookup_str == prefix) {
        Node *new_node = NULL;
        if (allocator != NULL) {
          new_node = allocator->NewNode();
        } else {
          // for test
          new_node = new Node();
        }
        new_node->key = node_data.key;
        new_node->value = node_data.value;
        new_node->attributes = node_data.attributes;
        // As for now, we don't have an interface to change rid/lid.
        // TODO(manabe): Implement it when need be.
        new_node->rid = kDummyPosId;
        new_node->lid = kDummyPosId;
        new_node->bnext = cur_node;
        cur_node = new_node;
      }
    }
  }
  return cur_node;
}

bool HasValueInternal(const map<string, list<DictionaryMock::NodeData> > &dic,
                      const StringPiece value) {
  typedef list<DictionaryMock::NodeData> NodeDataList;
  for (map<string, NodeDataList>::const_iterator map_it = dic.begin();
       map_it != dic.end(); ++map_it) {
    const NodeDataList &l = map_it->second;
    for (NodeDataList::const_iterator list_it = l.begin();
         list_it != l.end(); ++list_it) {
      if (list_it->value == value) {
        return true;
      }
    }
  }
  return false;
}

bool HasValueInternalForToken(const map<string, vector<Token *> > &dic,
                              const StringPiece value) {
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
                   uint32 attributes) {
  scoped_ptr<Token> token(new Token());
  token->key = key;
  token->value = value;
  // TODO(noriyukit): Currently, we cannot set cost and POS IDs.
  token->cost = 0;
  token->lid = token->rid = kDummyPosId;
  // TODO(noriyukit): Now we can directly take Token's attribute in
  // |attributes|.
  token->attributes = ConvertNodeAttrToTokenAttr(attributes);
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
}

bool DictionaryMock::HasValue(const StringPiece value) const {
  return HasValueInternal(predictive_dictionary_, value) ||
         HasValueInternalForToken(prefix_dictionary_, value) ||
         HasValueInternal(reverse_dictionary_, value) ||
         HasValueInternalForToken(exact_dictionary_, value);
}

Node *DictionaryMock::LookupPredictiveWithLimit(
    const char *str, int size,
    const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  // DictionaryMock doesn't support a limitation
  return LookupPredictive(str, size, allocator);
}

Node *DictionaryMock::LookupPredictive(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  CHECK_GT(size, 0);
  return LookupInternal(predictive_dictionary_, str,
                        static_cast<size_t>(size), allocator, false);
}

void DictionaryMock::LookupPrefix(
    StringPiece key,
    bool /*use_kana_modifier_insensitive_lookup*/,
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

Node *DictionaryMock::LookupReverse(const char *str, int size,
                            NodeAllocatorInterface *allocator) const {
  CHECK_GT(size, 0);
  return LookupInternal(reverse_dictionary_, str,
                        static_cast<size_t>(size), allocator, true);
}

DictionaryMock::NodeData::NodeData(const string &lookup_str,
                                   const string &key, const string &value,
                                   uint32 attributes) {
  this->lookup_str = lookup_str;
  this->key = key;
  this->value = value;
  this->attributes = attributes;
}

void DictionaryMock::AddLookupPredictive(const string &str,
                                         const string &key, const string &value,
                                         uint32 attributes) {
  NodeData node_data(str, key, value, attributes);
  predictive_dictionary_[str].push_back(node_data);
}

void DictionaryMock::AddLookupPrefix(const string &str,
                                     const string &key, const string &value,
                                     uint32 attributes) {
  prefix_dictionary_[str].push_back(
      CreateToken(str, key, value, attributes));
}

void DictionaryMock::AddLookupReverse(const string &str,
                                      const string &key, const string &value,
                                      uint32 attributes) {
  NodeData node_data(str, key, value, attributes);
  reverse_dictionary_[str].push_back(node_data);
}

void DictionaryMock::AddLookupExact(const string &str,
                                    const string &key, const string &value,
                                    uint32 attributes) {
  exact_dictionary_[str].push_back(
      CreateToken(str, key, value, attributes));
}

}  // namespace mozc
