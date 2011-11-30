// Copyright 2010-2011, Google Inc.
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

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "converter/node.h"

namespace mozc {
namespace {
static Node *LookupInternal(map<string, list<DictionaryMock::NodeData> > dic,
                     const char *str,
                     size_t size, NodeAllocatorInterface *allocator,
                     bool is_prefix_search) {
  CHECK(str);

  const string lookup_str(str, size);
  Node *cur_node = NULL;

  size_t start_len;
  if (is_prefix_search) {
    start_len = 1;
  } else {
    start_len = lookup_str.length();
  }
  const size_t end_len = lookup_str.size();

  for (size_t i = start_len; i <= end_len; ++i) {
    const string prefix = lookup_str.substr(0, i);
    if (dic.find(prefix) != dic.end()) {
      list<DictionaryMock::NodeData> &node_list = dic[prefix];
      list<DictionaryMock::NodeData>::iterator it_list = node_list.begin();
      for (; it_list != node_list.end(); ++it_list) {
        DictionaryMock::NodeData& node_data = *it_list;
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
          new_node->rid = 1;
          new_node->lid = 1;
          new_node->bnext = cur_node;
          cur_node = new_node;
        }
      }
    }
  }
  return cur_node;
}
}  // namespace

DictionaryMock::DictionaryMock() {
  LOG(INFO) << "DictionaryMock is created";
}

DictionaryMock::~DictionaryMock() {
}

Node *DictionaryMock::LookupPredictive(
    const char *str, int size,
    NodeAllocatorInterface *allocator) const {
  CHECK_GT(size, 0);
  return LookupInternal(predictive_dictionary_, str,
                        static_cast<size_t>(size), allocator, false);
}

Node *DictionaryMock::LookupPrefixWithLimit(
    const char *str, int size,
    const Limit &limit,
    NodeAllocatorInterface *allocator) const {
  // DictionaryMock doesn't support a limitation
  CHECK_GT(size, 0);
  return LookupInternal(prefix_dictionary_, str,
                        static_cast<size_t>(size), allocator, true);
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
  NodeData node_data(str, key, value, attributes);
  prefix_dictionary_[str].push_back(node_data);
}

void DictionaryMock::AddLookupReverse(const string &str,
                                      const string &key, const string &value,
                                      uint32 attributes) {
  NodeData node_data(str, key, value, attributes);
  reverse_dictionary_[str].push_back(node_data);
}

DictionaryMock *DictionaryMock::GetDictionaryMock() {
  return Singleton<DictionaryMock>::get();
}

void DictionaryMock::ClearAll() {
  predictive_dictionary_.clear();
  prefix_dictionary_.clear();
  reverse_dictionary_.clear();
}
}  // namespace mozc
