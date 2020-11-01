// Copyright 2010-2020, Google Inc.
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

#include "converter/lattice.h"

#include <set>
#include <string>

#include "base/port.h"
#include "converter/node.h"
#include "testing/base/public/gunit.h"

namespace mozc {

TEST(LatticeTest, LatticeTest) {
  Lattice lattice;

  EXPECT_EQ("", lattice.key());
  EXPECT_FALSE(lattice.has_lattice());

  lattice.SetKey("this is a test");
  EXPECT_TRUE(lattice.has_lattice());

  lattice.set_history_end_pos(4);
  EXPECT_EQ(4, lattice.history_end_pos());

  EXPECT_NE(nullptr, lattice.bos_nodes());
  EXPECT_NE(nullptr, lattice.eos_nodes());

  lattice.Clear();
  EXPECT_EQ("", lattice.key());
  EXPECT_FALSE(lattice.has_lattice());
  EXPECT_EQ(0, lattice.history_end_pos());
}

TEST(LatticeTest, NewNodeTest) {
  Lattice lattice;
  Node *node = lattice.NewNode();
  EXPECT_TRUE(node != nullptr);
  EXPECT_EQ(0, node->lid);
  EXPECT_EQ(0, node->rid);
}

TEST(LatticeTest, InsertTest) {
  Lattice lattice;

  lattice.SetKey("test");

  {
    Node *node = lattice.NewNode();
    node->value = "ho";
    node->key = "es";
    lattice.Insert(1, node);

    Node *node2 = lattice.begin_nodes(1);
    EXPECT_EQ(node2, node);

    Node *node3 = lattice.end_nodes(3);
    EXPECT_EQ(node3, node);
  }

  {
    Node *node = lattice.NewNode();
    node->value = "o";
    node->key = "s";
    lattice.Insert(2, node);

    Node *node2 = lattice.begin_nodes(2);
    EXPECT_EQ(node2, node);

    int size = 0;
    Node *node3 = lattice.end_nodes(3);
    for (; node3 != nullptr; node3 = node3->enext) {
      ++size;
    }
    EXPECT_EQ(2, size);
  }
}

namespace {

// set cache_info[i] to (key.size() - i)
void UpdateCacheInfo(Lattice *lattice) {
  const size_t key_size = lattice->key().size();
  for (size_t i = 0; i < key_size; ++i) {
    lattice->SetCacheInfo(i, key_size - i);
  }
}

// add nodes whose key is key.substr(i, key.size() - i)
void InsertNodes(Lattice *lattice) {
  const size_t key_size = lattice->key().size();
  for (size_t i = 0; i < key_size; ++i) {
    Node *node = lattice->NewNode();
    node->key.assign(lattice->key(), i, key_size - i);
    lattice->Insert(i, node);
  }
}
}  // namespace

TEST(LatticeTest, AddSuffixTest) {
  Lattice lattice;

  const std::string kKey = "test";

  lattice.SetKey("");
  for (size_t len = 1; len <= kKey.size(); ++len) {
    lattice.AddSuffix(kKey.substr(len - 1, 1));
    InsertNodes(&lattice);
    UpdateCacheInfo(&lattice);

    // check BOS & EOS
    EXPECT_TRUE(lattice.has_lattice());
    EXPECT_NE(nullptr, lattice.bos_nodes());
    EXPECT_NE(0, lattice.bos_nodes()->node_type & Node::BOS_NODE);
    EXPECT_NE(nullptr, lattice.eos_nodes());
    EXPECT_NE(0, lattice.eos_nodes()->node_type & Node::EOS_NODE);

    // check cache_info
    const size_t key_size = lattice.key().size();
    for (size_t i = 0; i < key_size; ++i) {
      EXPECT_LE(lattice.cache_info(i), key_size - i);
    }

    // check whether lattice have nodes for all substrings
    for (size_t i = 0; i <= key_size; ++i) {
      // check for begin_nodes
      if (i < key_size) {
        std::set<int> lengths;
        for (Node *node = lattice.begin_nodes(i); node != nullptr;
             node = node->bnext) {
          lengths.insert(node->key.size());
        }
        EXPECT_EQ(lengths.size(), key_size - i);
      }
      // check for end_nodes
      if (i > 0) {
        std::set<int> lengths;
        for (Node *node = lattice.end_nodes(i); node != nullptr;
             node = node->enext) {
          lengths.insert(node->key.size());
        }
        EXPECT_EQ(lengths.size(), i);
      }
    }
  }
}

TEST(LatticeTest, ShrinkKeyTest) {
  Lattice lattice;

  const std::string kKey = "test";
  for (size_t len = 1; len <= kKey.size(); ++len) {
    lattice.AddSuffix(kKey.substr(len - 1, 1));
    InsertNodes(&lattice);
    UpdateCacheInfo(&lattice);
  }

  for (size_t len = kKey.size(); len >= 1; --len) {
    lattice.ShrinkKey(len);

    // check BOS & EOS
    EXPECT_TRUE(lattice.has_lattice());
    EXPECT_NE(nullptr, lattice.bos_nodes());
    EXPECT_NE(0, lattice.bos_nodes()->node_type & Node::BOS_NODE);
    EXPECT_NE(nullptr, lattice.eos_nodes());
    EXPECT_NE(0, lattice.eos_nodes()->node_type & Node::EOS_NODE);

    // check cache_info
    const size_t key_size = lattice.key().size();
    for (size_t i = 0; i < key_size; ++i) {
      EXPECT_LE(lattice.cache_info(i), key_size - i);
    }

    // check whether lattice have nodes for all substrings
    for (size_t i = 0; i <= key_size; ++i) {
      // check for begin_nodes
      if (i < key_size) {
        std::set<int> lengths;
        for (Node *node = lattice.begin_nodes(i); node != nullptr;
             node = node->bnext) {
          lengths.insert(node->key.size());
        }
        EXPECT_EQ(lengths.size(), key_size - i);
      }
      // check for end_nodes
      if (i > 0) {
        std::set<int> lengths;
        for (Node *node = lattice.end_nodes(i); node != nullptr;
             node = node->enext) {
          lengths.insert(node->key.size());
        }
        EXPECT_EQ(lengths.size(), i);
      }
    }
  }
}
}  // namespace mozc
