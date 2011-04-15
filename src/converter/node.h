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

#ifndef MOZC_CONVERTER_NODE_H_
#define MOZC_CONVERTER_NODE_H_

#include <map>
#include <string>
#include "base/base.h"

namespace mozc {

class Segment;

struct Node {
  enum NodeType {
    NOR_NODE,  // normal node
    BOS_NODE,  // BOS (beginning of sentence)
    EOS_NODE,  // EOS (end of sentence)
    CON_NODE,  // constrained node
    HIS_NODE,  // history node
  };

  enum Attribute {
    DEFAULT_ATTRIBUTE     = 0,
    SYSTEM_DICTIONARY     = 1,  // system dictionary (not used now)
    USER_DICTIONARY       = 2,  // user dictionary (not used now)
    NO_VARIANTS_EXPANSION = 4,  // no need to expand full/half
    WEAK_CONNECTED        = 8,  // internally used in the converter
    SPELLING_CORRECTION   = 16,  // "did you mean"
  };

  Node     *prev;
  Node     *next;
  Node     *bnext;
  Node     *enext;
  // if it is not NULL, transition cost
  // from constrained_prev to current node is defined,
  // other transition is set to be infinite
  Node     *constrained_prev;

  uint16    rid;
  uint16    lid;
  uint16    begin_pos;
  uint16    end_pos;
  int16     wcost;
  int32     cost;
  NodeType  node_type;
  uint32    attributes;
  string    key;
  string    value;

  inline void Init() {
    prev = NULL;
    next = NULL;
    bnext = NULL;
    enext = NULL;
    constrained_prev = NULL;
    rid = 0;
    lid = 0;
    begin_pos = 0;
    end_pos = 0;
    node_type = NOR_NODE;
    wcost = 0;
    cost = 0;
    attributes = 0;
    key.clear();
    value.clear();
  }
};

// this class keep multiple types of data.
// each type should inherit NodeAllocatorData::Data
class NodeAllocatorData {
 public:
  ~NodeAllocatorData() {
    clear();
  }

  bool has(const char *name) const {
    return (data_.find(name) != data_.end());
  }

  void erase(const char *name) {
    if (has(name)) {
      delete data_[name];
      data_.erase(name);
    }
  }

  void clear() {
    for (map<const char *, Data *>::iterator it = data_.begin();
         it != data_.end(); ++it) {
      delete it->second;
    }
    data_.clear();
  }

  template<typename Type> Type *get(const char *name) {
    if (!has(name)) {
      data_[name] = new Type;
    }
    return reinterpret_cast<Type *>(data_[name]);
  }

  class Data {
   public:
    virtual ~Data() {}
  };

 private:
  map<const char *, Data *> data_;
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

  // Backend specific data, like cache for look up.
  NodeAllocatorData *mutable_data() {
    return &data_;
  }
  const NodeAllocatorData &data() {
    return data_;
  }

 private:
  size_t max_nodes_size_;
  NodeAllocatorData data_;
};
}
#endif  // MOZC_CONVERTER_NODE_H_;
