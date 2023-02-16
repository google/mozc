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

#include "storage/louds/louds.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/port.h"
#include "testing/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace storage {
namespace louds {
namespace {

std::vector<uint8_t> MakeSequence(absl::string_view s) {
  std::vector<uint8_t> seq;
  int bit_len = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (bit_len % 8 == 0) {
      seq.push_back(0);
    }
    switch (s[i]) {
      case '0':
        seq.back() >>= 1;
        break;
      case '1':
        seq.back() = (seq.back() >> 1) | 0x80;
        break;
      default:
        continue;
    }
    ++bit_len;
  }
  // Add padding so that seq.size() % 4 == 0.
  const size_t reminder = seq.size() % 4;
  if (reminder != 0) {
    for (size_t i = 0; i < 4 - reminder; ++i) {
      seq.push_back(0);
    }
  }
  return seq;
}

#define EXPECT_LEAF(louds, node)          \
  do {                                    \
    ASSERT_TRUE(louds.IsValidNode(node)); \
    Louds::Node tmp = node;               \
    louds.MoveToFirstChild(&tmp);         \
    EXPECT_FALSE(louds.IsValidNode(tmp)); \
  } while (false)

#define EXPECT_NO_SIBLING(louds, node)    \
  do {                                    \
    ASSERT_TRUE(louds.IsValidNode(node)); \
    Louds::Node tmp = node;               \
    louds.MoveToNextSibling(&tmp);        \
    EXPECT_FALSE(louds.IsValidNode(tmp)); \
  } while (false)

struct CacheSizeParam {
  CacheSizeParam(size_t lb0, size_t lb1, size_t s0, size_t s1)
      : bitvec_lb0_cache_size(lb0),
        bitvec_lb1_cache_size(lb1),
        select0_cache_size(s0),
        select1_cache_size(s1) {}

  size_t bitvec_lb0_cache_size;
  size_t bitvec_lb1_cache_size;
  size_t select0_cache_size;
  size_t select1_cache_size;
};

class LoudsTest : public ::testing::TestWithParam<CacheSizeParam> {};

TEST_P(LoudsTest, Basic) {
  const CacheSizeParam &param = GetParam();

  // Test with the trie illustrated in louds.h.
  const std::vector<uint8_t> kSeq = MakeSequence("10 110 0 110 0 0");
  Louds louds;
  louds.Init(kSeq.data(), kSeq.size(), param.bitvec_lb0_cache_size,
             param.bitvec_lb1_cache_size, param.select0_cache_size,
             param.select1_cache_size);

  // root -> 2 -> 3 -> 4 -> 5
  {
    Louds::Node node;
    EXPECT_TRUE(louds.IsRoot(node));
    EXPECT_NO_SIBLING(louds, node);
    EXPECT_EQ(node.node_id(), 1);

    louds.MoveToFirstChild(&node);
    EXPECT_LEAF(louds, node);
    EXPECT_EQ(node.node_id(), 2);

    louds.MoveToNextSibling(&node);
    EXPECT_NO_SIBLING(louds, node);
    EXPECT_EQ(node.node_id(), 3);

    louds.MoveToFirstChild(&node);
    EXPECT_LEAF(louds, node);
    EXPECT_EQ(node.node_id(), 4);

    louds.MoveToNextSibling(&node);
    EXPECT_LEAF(louds, node);
    EXPECT_NO_SIBLING(louds, node);
    EXPECT_EQ(node.node_id(), 5);
  }

  // 4 -> 3 -> 1
  {
    Louds::Node node;
    louds.InitNodeFromNodeId(4, &node);
    EXPECT_LEAF(louds, node);
    EXPECT_EQ(node.node_id(), 4);

    louds.MoveToParent(&node);
    EXPECT_EQ(node.node_id(), 3);

    louds.MoveToParent(&node);
    EXPECT_EQ(node.node_id(), 1);
    EXPECT_TRUE(louds.IsRoot(node));
  }
}

INSTANTIATE_TEST_SUITE_P(
    GenLoudsTest, LoudsTest,
    ::testing::Values(CacheSizeParam(0, 0, 0, 0), CacheSizeParam(0, 0, 0, 1),
                      CacheSizeParam(0, 0, 1, 0), CacheSizeParam(0, 0, 1, 1),
                      CacheSizeParam(0, 1, 0, 0), CacheSizeParam(0, 1, 0, 1),
                      CacheSizeParam(0, 1, 1, 0), CacheSizeParam(0, 1, 1, 1),
                      CacheSizeParam(1, 0, 0, 0), CacheSizeParam(1, 0, 0, 1),
                      CacheSizeParam(1, 0, 1, 0), CacheSizeParam(1, 0, 1, 1),
                      CacheSizeParam(1, 1, 0, 0), CacheSizeParam(1, 1, 0, 1),
                      CacheSizeParam(1, 1, 1, 0), CacheSizeParam(1, 1, 1, 1),
                      CacheSizeParam(2, 2, 2, 2), CacheSizeParam(8, 8, 8, 8),
                      CacheSizeParam(1024, 1024, 1024, 1024)));

}  // namespace
}  // namespace louds
}  // namespace storage
}  // namespace mozc
