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

#include "converter/converter_data.h"

#include <cstring>
#include "base/base.h"
#include "converter/key_corrector.h"

namespace mozc {

class NodeAllocator : public NodeAllocatorInterface {
 public:
  NodeAllocator(): node_freelist_(1024) {}
  virtual ~NodeAllocator() {}

  virtual Node *NewNode() {
    Node *node = node_freelist_.Alloc();
    DCHECK(node);
    node->Init();
    return node;
  }

  // Free all nodes allocateed by NewNode()
  void Free(){
    node_freelist_.Free();
  }

 private:
  FreeList<Node> node_freelist_;
};

ConverterData::ConverterData() : bos_node_(NULL),
                                 eos_node_(NULL),
                                 key_corrector_(new KeyCorrector),
                                 node_allocator_(new NodeAllocator) {}

ConverterData::~ConverterData() {}

NodeAllocatorInterface *ConverterData::node_allocator() const {
  return node_allocator_.get();
}

Node *ConverterData::NewNode() {
  return node_allocator_->NewNode();
}

Node **ConverterData::begin_nodes_list() {
  return &begin_nodes_list_[0];
}

Node **ConverterData::end_nodes_list() {
  return &end_nodes_list_[0];
}

const KeyCorrector &ConverterData::key_corrector() const {
  return *(key_corrector_.get());
}

void ConverterData::set_key(const string &key,
                            KeyCorrector::InputMode mode) {
  key_ = key;
  const size_t size = key.size();
  begin_nodes_list_.resize(size + 4);
  end_nodes_list_.resize(size + 4);
  fill(begin_nodes_list_.begin(), begin_nodes_list_.end(),
       static_cast<Node *>(NULL));
  fill(end_nodes_list_.begin(), end_nodes_list_.end(),
       static_cast<Node *>(NULL));
  key_corrector_->CorrectKey(key, mode);
}

const string &ConverterData::key() const {
  return key_;
}

bool ConverterData::has_lattice() const {
  return !begin_nodes_list_.empty();
}

void ConverterData::clear_lattice() {
  key_.clear();
  begin_nodes_list_.clear();
  end_nodes_list_.clear();
  node_allocator_->Free();
}
}  // namespace mozc
