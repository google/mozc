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

#ifndef MOZC_CONVERTER_LATTICE_H_
#define MOZC_CONVERTER_LATTICE_H_

#include <vector>
#include <string>
#include "base/base.h"
#include "base/freelist.h"
#include "converter/node.h"

namespace mozc {

class NodeAllocatorInterface;
class NodeAllocator;

class Lattice {
 public:
  NodeAllocatorInterface *node_allocator() const;

  // set key and initalizes lattice with key.
  void SetKey(const string &key);

  // return key.
  const string& key() const;

  // allocate new node.
  Node *NewNode();

  // return nodes (linked list) starting with |pos|.
  // To traverse all nodes, use Node::bnext member.
  Node *begin_nodes(size_t pos) const;

  // return nodes (linked list) ending at |pos|.
  // To traverse all nodes, use Node::enext member.
  Node *end_nodes(size_t pos) const;

  // return bos nodes.
  // alias of end_nodes(0).
  Node *bos_nodes() const;

  // return eos nodes.
  // alias of begin_nodes(key.size()).
  Node *eos_nodes() const;

  // inset nodes (linked list) to the position |pos|.
  void Insert(size_t pos, Node *node);

  // clear all lattice and nodes allocated with NewNode method.
  void Clear();

  // return true if this instance has a valid lattice.
  bool has_lattice() const;

  Lattice();
  virtual ~Lattice();

 private:
  string key_;
  vector<Node *> begin_nodes_;
  vector<Node *> end_nodes_;
  scoped_ptr<NodeAllocator> node_allocator_;
};
}  // namespace mozc

#endif  // MOZC_CONVERTER_LATTICE_H_
