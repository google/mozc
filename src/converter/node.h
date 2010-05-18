// Copyright 2010, Google Inc.
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

#ifndef MOZC_CONVERTER_NODE_H_
#define MOZC_CONVERTER_NODE_H_

#include <string>
#include "base/base.h"

namespace mozc {

class Segment;

struct Node {
  enum NodeType {
    BOS_NODE,  // BOS (beginning of sentence)
    EOS_NODE,  // EOS (end of sentence)
    NOR_NODE,  // normal node in dictionary
    UNK_NODE,  // unknown node not in dictionary
    CON_NODE,  // constrained node
    HIS_NODE,  // history node
    UNU_NODE   // unused node
  };

  // CharacterForm setting.
  // if you don't want to modify the form
  // of this node, please set NO_NORMALIZATION.
  // Basically, the words registered in user dictionary should
  // have NO_NORMALIZATION
  enum NormalizationType {
    DEFAULT_NORMALIZATION,
    NO_NORMALIZATION
  };

  Node     *prev;
  Node     *next;
  Node     *bnext;
  Node     *enext;
  // if it is not NULL, transition cost
  // from constrained_prev to current node is defined,
  // other transition is set to be infinite
  Node     *constrained_prev;
  const Segment  *con_segment;
  uint16    rid;
  uint16    lid;
  uint16    begin_pos;
  uint16    end_pos;
  int16     wcost;
  int32     cost;
  NodeType  node_type;
  NormalizationType normalization_type;
  bool      is_weak_connected;
  string    key;
  string    value;

  inline void Init() {
    prev = next = bnext = enext = constrained_prev = NULL;
    key.clear();
    value.clear();
    rid = lid = begin_pos = end_pos = 0;
    node_type = NOR_NODE;
    normalization_type = DEFAULT_NORMALIZATION;
    con_segment = NULL;
    wcost = 0;
    cost = 0;
    is_weak_connected = false;
  }
};

class NodeAllocatorInterface {
 public:
  NodeAllocatorInterface() : max_nodes_size_(8192) {}
  virtual ~NodeAllocatorInterface() {}

  virtual Node *NewNode() = 0;

  virtual size_t max_nodes_size() const {
    return max_nodes_size_;
  }

  virtual void set_max_nodes_size(size_t max_nodes_size) {
    max_nodes_size_ = max_nodes_size;
  }

 private:
  size_t max_nodes_size_;
};
}
#endif  // MOZC_CONVERTER_NODE_H_;
