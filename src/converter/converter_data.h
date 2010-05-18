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

#ifndef MOZC_CONVERTER_CONVERTER_DATA_H_
#define MOZC_CONVERTER_CONVERTER_DATA_H_

#include <vector>
#include <string>
#include "base/freelist.h"
#include "converter/node.h"
#include "converter/nbest_generator.h"
#include "converter/key_corrector.h"

namespace mozc {

class NBestGenerator;
class NodeAllocator;

class ConverterData {
 public:
  NodeAllocatorInterface *node_allocator() const;

  // Old interface for compatibility
  Node *NewNode();

  Node **begin_nodes_list();
  Node **end_nodes_list();

  const KeyCorrector &key_corrector() const;

  Node *bos_node() const {
    return bos_node_;
  }

  Node *eos_node() const {
    return eos_node_;
  }

  void set_bos_node(Node *bos_node) {
    bos_node_ = bos_node;
  }

  void set_eos_node(Node *eos_node) {
    eos_node_ = eos_node;
  }

  void set_key(const string &key, KeyCorrector::InputMode mode);
  const string& key() const;

  void clear_lattice();
  bool has_lattice() const;

  ConverterData();
  virtual ~ConverterData();

 private:
  string key_;
  Node *bos_node_;
  Node *eos_node_;
  scoped_ptr<KeyCorrector> key_corrector_;
  scoped_ptr<NodeAllocator> node_allocator_;
  vector<Node *> begin_nodes_list_;
  vector<Node *> end_nodes_list_;
  // Limit of node count returned by each dictionary backend. Total number of
  // nodes may exceed this.
};
}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_DATA_H_
