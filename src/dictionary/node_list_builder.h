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

#ifndef MOZC_DICTIONARY_NODE_LIST_BUILDER_H_
#define MOZC_DICTIONARY_NODE_LIST_BUILDER_H_

#include "base/port.h"
#include "converter/node.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"

namespace mozc {

// The cost is 500 * log(30): 30 times in freq.
static const int32 kKanaModifierInsensitivePenalty = 1700;

// Provides basic functionality for building a list of nodes.
// This class is defined inline because it contributes to the performance of
// dictionary lookup.
class BaseNodeListBuilder : public DictionaryInterface::Callback {
 public:
  BaseNodeListBuilder(NodeAllocatorInterface *allocator, int limit)
      : allocator_(allocator), limit_(limit), penalty_(0), result_(NULL) {
  }

  // Determines a penalty for tokens of this (key, actual_key) pair.
  virtual ResultType OnActualKey(StringPiece key,
                                 StringPiece actual_key,
                                 bool is_expanded) {
    penalty_ = is_expanded ? kKanaModifierInsensitivePenalty : 0;
    return TRAVERSE_CONTINUE;
  }

  // Creates a new node and prepends it to the current list.
  virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                             const Token &token) {
    Node *new_node = NewNodeFromToken(token);
    PrependNode(new_node);
    return (limit_ <= 0) ? TRAVERSE_DONE : TRAVERSE_CONTINUE;
  }

  int limit() const { return limit_; }
  int penalty() const { return penalty_; }
  Node *result() const { return result_; }
  NodeAllocatorInterface *allocator() { return allocator_; }

  Node *NewNodeFromToken(const Token &token) {
    Node *new_node = NULL;
    if (allocator_) {
      new_node = allocator_->NewNode();
    } else {
      new_node = new Node();  // For test
    }
    new_node->InitFromToken(token);
    new_node->wcost += penalty_;
    return new_node;
  }

  void PrependNode(Node *node) {
    node->bnext = result_;
    result_ = node;
    --limit_;
  }

 protected:
  NodeAllocatorInterface *allocator_;
  int limit_;
  int penalty_;
  Node *result_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BaseNodeListBuilder);
};

// Implements key filtering rule for LookupPrefix().
// This class is also defined inline.
class NodeListBuilderForLookupPrefix : public BaseNodeListBuilder {
 public:
  NodeListBuilderForLookupPrefix(NodeAllocatorInterface *allocator,
                                 int limit, size_t min_key_length)
      : BaseNodeListBuilder(allocator, limit),
        min_key_length_(min_key_length) {}

  virtual ResultType OnKey(StringPiece key) {
    return key.size() < min_key_length_ ? TRAVERSE_NEXT_KEY : TRAVERSE_CONTINUE;
  }

 protected:
  const size_t min_key_length_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NodeListBuilderForLookupPrefix);
};

}  // namespace mozc

#endif  // MOZC_DICTIONARY_NODE_LIST_BUILDER_H_
