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

#ifndef MOZC_CONVERTER_NODE_LIST_BUILDER_H_
#define MOZC_CONVERTER_NODE_LIST_BUILDER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "protocol/commands.pb.h"

namespace mozc {

// Spatial cost penalty per modification. We are going to
// use the moderate penalty to increase the coverage, and re-calculate
// the actual penalty with new typing correction module.
// Per-modification penalty allows to recompute the added penalty
// from the actual output of key by counting different characters.
inline int32_t GetPerExpansionSpatialCostPenalty() {
  static constexpr int32_t kPerExpansionSpatialCostPenalty = 2500;
  return kPerExpansionSpatialCostPenalty;
}

inline int32_t GetSpatialCostPenalty(int num_expanded) {
  return num_expanded * GetPerExpansionSpatialCostPenalty();
}

// Provides basic functionality for building a list of nodes.
// This class is defined inline because it contributes to the performance of
// dictionary lookup.
class BaseNodeListBuilder : public dictionary::DictionaryInterface::Callback {
 public:
  BaseNodeListBuilder(mozc::NodeAllocator* allocator, int limit)
      : allocator_(allocator), limit_(limit), penalty_(0) {
    result_.reserve(64);
    DCHECK(allocator_) << "Allocator must not be nullptr";
  }

  BaseNodeListBuilder(const BaseNodeListBuilder&) = delete;
  BaseNodeListBuilder& operator=(const BaseNodeListBuilder&) = delete;

  // Determines a penalty for tokens of this (key, actual_key) pair.
  ResultType OnActualKey(absl::string_view key, absl::string_view actual_key,
                         int num_expanded) override {
    penalty_ = GetSpatialCostPenalty(num_expanded);
    return TRAVERSE_CONTINUE;
  }

  // Creates a new node and prepends it to the current list.
  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const dictionary::Token& token) override {
    Node* new_node = NewNodeFromToken(token);
    DCHECK(new_node);
    AppendToResult(new_node);
    return (result_.size() > limit_) ? TRAVERSE_DONE : TRAVERSE_CONTINUE;
  }

  int limit() const { return limit_; }
  int penalty() const { return penalty_; }
  NodeAllocator* allocator() { return allocator_; }

  absl::Span<Node* const> result_view() const {
    return absl::MakeSpan(result_);
  }
  std::vector<Node*> result() { return result_; }

  Node* NewNodeFromToken(const dictionary::Token& token) {
    Node* new_node = allocator_->NewNode();
    new_node->InitFromToken(token);
    new_node->wcost += penalty_;
    if (penalty_ > 0) new_node->attributes |= Node::KEY_EXPANDED;
    return new_node;
  }

  void AppendToResult(Node* node) {
    DCHECK(node);
    result_.push_back(node);
  }

 private:
  NodeAllocator* allocator_ = nullptr;
  const int limit_ = 0;
  int penalty_ = 0;
  std::vector<Node*> result_;
};

// Implements key filtering rule for LookupPrefix().
// This class is also defined inline.
class NodeListBuilderForLookupPrefix : public BaseNodeListBuilder {
 public:
  NodeListBuilderForLookupPrefix(mozc::NodeAllocator* allocator, int limit,
                                 size_t min_key_length)
      : BaseNodeListBuilder(allocator, limit),
        min_key_length_(min_key_length) {}

  NodeListBuilderForLookupPrefix(const NodeListBuilderForLookupPrefix&) =
      delete;
  NodeListBuilderForLookupPrefix& operator=(
      const NodeListBuilderForLookupPrefix&) = delete;

  ResultType OnKey(absl::string_view key) override {
    return key.size() < min_key_length_ ? TRAVERSE_NEXT_KEY : TRAVERSE_CONTINUE;
  }

 protected:
  const size_t min_key_length_ = 0;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_NODE_LIST_BUILDER_H_
