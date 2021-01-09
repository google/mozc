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

#include "storage/louds/louds_trie.h"

#include <vector>

#include "base/port.h"
#include "storage/louds/louds_trie_builder.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace storage {
namespace louds {
namespace {

class RecordCallbackArgs {
 public:
  struct CallbackArgs {
    absl::string_view key;
    size_t prefix_len;
    const LoudsTrie *trie;
    LoudsTrie::Node node;
  };

  explicit RecordCallbackArgs(std::vector<CallbackArgs> *output)
      : output_(output) {}

  void operator()(absl::string_view key, size_t prefix_len,
                  const LoudsTrie &trie, LoudsTrie::Node node) {
    CallbackArgs args;
    args.key = key;
    args.prefix_len = prefix_len;
    args.trie = &trie;
    args.node = node;
    output_->push_back(args);
  }

 private:
  std::vector<CallbackArgs> *output_;
};

LoudsTrie::Node Traverse(const LoudsTrie &trie, absl::string_view key) {
  LoudsTrie::Node node;
  trie.Traverse(key, &node);
  return node;
}

struct CacheSizeParam {
  CacheSizeParam(size_t lb0, size_t lb1, size_t s0, size_t s1, size_t term_lb1)
      : louds_lb0_cache_size(lb0),
        louds_lb1_cache_size(lb1),
        louds_select0_cache_size(s0),
        louds_select1_cache_size(s1),
        termvec_lb1_cache_size(term_lb1) {}

  size_t louds_lb0_cache_size;
  size_t louds_lb1_cache_size;
  size_t louds_select0_cache_size;
  size_t louds_select1_cache_size;
  size_t termvec_lb1_cache_size;
};

class LoudsTrieTest : public ::testing::TestWithParam<CacheSizeParam> {};

#define INSTANTIATE_TEST_CASE(Generator)                                \
  INSTANTIATE_TEST_SUITE_P(                                             \
      Generator, LoudsTrieTest,                                         \
      ::testing::Values(                                                \
          CacheSizeParam(0, 0, 0, 0, 0), CacheSizeParam(0, 0, 0, 0, 1), \
          CacheSizeParam(0, 0, 0, 1, 0), CacheSizeParam(0, 0, 0, 1, 1), \
          CacheSizeParam(0, 0, 1, 0, 0), CacheSizeParam(0, 0, 1, 0, 1), \
          CacheSizeParam(0, 0, 1, 1, 0), CacheSizeParam(0, 0, 1, 1, 1), \
          CacheSizeParam(0, 1, 0, 0, 0), CacheSizeParam(0, 1, 0, 0, 1), \
          CacheSizeParam(0, 1, 0, 1, 0), CacheSizeParam(0, 1, 0, 1, 1), \
          CacheSizeParam(0, 1, 1, 0, 0), CacheSizeParam(0, 1, 1, 0, 1), \
          CacheSizeParam(0, 1, 1, 1, 0), CacheSizeParam(0, 1, 1, 1, 1), \
          CacheSizeParam(1, 0, 0, 0, 0), CacheSizeParam(1, 0, 0, 0, 1), \
          CacheSizeParam(1, 0, 0, 1, 0), CacheSizeParam(1, 0, 0, 1, 1), \
          CacheSizeParam(1, 0, 1, 0, 0), CacheSizeParam(1, 0, 1, 0, 1), \
          CacheSizeParam(1, 0, 1, 1, 0), CacheSizeParam(1, 0, 1, 1, 1), \
          CacheSizeParam(1, 1, 0, 0, 0), CacheSizeParam(1, 1, 0, 0, 1), \
          CacheSizeParam(1, 1, 0, 1, 0), CacheSizeParam(1, 1, 0, 1, 1), \
          CacheSizeParam(1, 1, 1, 0, 0), CacheSizeParam(1, 1, 1, 0, 1), \
          CacheSizeParam(1, 1, 1, 1, 0), CacheSizeParam(1, 1, 1, 1, 1), \
          CacheSizeParam(2, 2, 2, 2, 2), CacheSizeParam(8, 8, 8, 8, 8), \
          CacheSizeParam(1024, 1024, 1024, 1024, 1024)));

TEST_P(LoudsTrieTest, NodeBasedApis) {
  // Create the following trie (* stands for non-terminal nodes):
  //
  //        *          Key   ID
  //     a/   \b       ---------
  //    0      *       a     0
  //  a/ \b     \d     aa    1
  //  1   2      3     ab    2
  //    c/ \d          bd    3
  //    *   4          abd   4
  //   d|              abcd  5
  //    5
  LoudsTrieBuilder builder;
  builder.Add("a");
  builder.Add("aa");
  builder.Add("ab");
  builder.Add("abcd");
  builder.Add("abd");
  builder.Add("bd");
  builder.Build();

  const CacheSizeParam &param = GetParam();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()),
            param.louds_lb0_cache_size, param.louds_lb1_cache_size,
            param.louds_select0_cache_size, param.louds_select1_cache_size,
            param.termvec_lb1_cache_size);

  char buf[LoudsTrie::kMaxDepth + 1];  // for RestoreKeyString().

  // Walk the trie in BFS order and check properties at each node.

  // Root node
  const LoudsTrie::Node root;
  ASSERT_TRUE(trie.IsValidNode(root));
  EXPECT_FALSE(trie.IsTerminalNode(root));
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("", &node));
    EXPECT_EQ(root, node);
  }

  // There's no right sibling for root.
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToNextSibling(root)));

  // Terminal node for "a".
  const LoudsTrie::Node node_a = trie.MoveToFirstChild(root);
  ASSERT_TRUE(trie.IsValidNode(node_a));
  ASSERT_TRUE(trie.IsTerminalNode(node_a));
  EXPECT_EQ('a', trie.GetEdgeLabelToParentNode(node_a));
  EXPECT_EQ(0, trie.GetKeyIdOfTerminalNode(node_a));
  EXPECT_EQ(node_a, trie.GetTerminalNodeFromKeyId(0));
  EXPECT_EQ("a", trie.RestoreKeyString(0, buf));
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.MoveToChildByLabel('a', &node));
    EXPECT_EQ(node_a, node);
  }
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("a", &node));
    EXPECT_EQ(node_a, node);
  }

  // Non-terminal node for "b".
  const LoudsTrie::Node node_b = trie.MoveToNextSibling(node_a);
  ASSERT_TRUE(trie.IsValidNode(node_b));
  EXPECT_FALSE(trie.IsTerminalNode(node_b));
  EXPECT_EQ('b', trie.GetEdgeLabelToParentNode(node_b));
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.MoveToChildByLabel('b', &node));
    EXPECT_EQ(node_b, node);
  }
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("b", &node));
    EXPECT_EQ(node_b, node);
  }

  // There's no right sibling for "b".
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToNextSibling(node_b)));

  // Terminal node (leaf) for "aa".
  const LoudsTrie::Node node_aa = trie.MoveToFirstChild(node_a);
  ASSERT_TRUE(trie.IsValidNode(node_aa));
  ASSERT_TRUE(trie.IsTerminalNode(node_aa));
  EXPECT_EQ('a', trie.GetEdgeLabelToParentNode(node_aa));
  EXPECT_EQ(1, trie.GetKeyIdOfTerminalNode(node_aa));
  EXPECT_EQ(node_aa, trie.GetTerminalNodeFromKeyId(1));
  EXPECT_EQ("aa", trie.RestoreKeyString(1, buf));
  {
    LoudsTrie::Node node = node_a;
    EXPECT_TRUE(trie.MoveToChildByLabel('a', &node));
    EXPECT_EQ(node_aa, node);
  }
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("aa", &node));
    EXPECT_EQ(node_aa, node);
  }

  // There's no child for "aa".
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToFirstChild(node_aa)));

  // Terminal node for "ab".
  const LoudsTrie::Node node_ab = trie.MoveToNextSibling(node_aa);
  ASSERT_TRUE(trie.IsValidNode(node_ab));
  ASSERT_TRUE(trie.IsTerminalNode(node_ab));
  EXPECT_EQ('b', trie.GetEdgeLabelToParentNode(node_ab));
  EXPECT_EQ(2, trie.GetKeyIdOfTerminalNode(node_ab));
  EXPECT_EQ(node_ab, trie.GetTerminalNodeFromKeyId(2));
  EXPECT_EQ("ab", trie.RestoreKeyString(2, buf));
  {
    LoudsTrie::Node node = node_a;
    EXPECT_TRUE(trie.MoveToChildByLabel('b', &node));
    EXPECT_EQ(node_ab, node);
  }
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("ab", &node));
    EXPECT_EQ(node_ab, node);
  }

  // There's no right sibling for "ab".
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToNextSibling(node_ab)));

  // Terminal node (leaf) for "bd".
  const LoudsTrie::Node node_bd = trie.MoveToFirstChild(node_b);
  ASSERT_TRUE(trie.IsValidNode(node_bd));
  ASSERT_TRUE(trie.IsTerminalNode(node_bd));
  EXPECT_EQ('d', trie.GetEdgeLabelToParentNode(node_bd));
  EXPECT_EQ(3, trie.GetKeyIdOfTerminalNode(node_bd));
  EXPECT_EQ(node_bd, trie.GetTerminalNodeFromKeyId(3));
  EXPECT_EQ("bd", trie.RestoreKeyString(3, buf));
  {
    LoudsTrie::Node node = node_b;
    EXPECT_TRUE(trie.MoveToChildByLabel('d', &node));
    EXPECT_EQ(node_bd, node);
  }
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("bd", &node));
    EXPECT_EQ(node_bd, node);
  }

  // There is no child nor right sibling for "bd".
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToFirstChild(node_bd)));
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToNextSibling(node_bd)));

  // Non-terminal node for "abc".
  const LoudsTrie::Node node_abc = trie.MoveToFirstChild(node_ab);
  ASSERT_TRUE(trie.IsValidNode(node_abc));
  EXPECT_FALSE(trie.IsTerminalNode(node_abc));
  EXPECT_EQ('c', trie.GetEdgeLabelToParentNode(node_abc));
  {
    LoudsTrie::Node node = node_ab;
    EXPECT_TRUE(trie.MoveToChildByLabel('c', &node));
    EXPECT_EQ(node_abc, node);
  }
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("abc", &node));
    EXPECT_EQ(node_abc, node);
  }

  // Terminal node (leaf) for "abd".
  const LoudsTrie::Node node_abd = trie.MoveToNextSibling(node_abc);
  ASSERT_TRUE(trie.IsValidNode(node_abd));
  ASSERT_TRUE(trie.IsTerminalNode(node_abd));
  EXPECT_EQ('d', trie.GetEdgeLabelToParentNode(node_abd));
  EXPECT_EQ(4, trie.GetKeyIdOfTerminalNode(node_abd));
  EXPECT_EQ(node_abd, trie.GetTerminalNodeFromKeyId(4));
  EXPECT_EQ("abd", trie.RestoreKeyString(4, buf));
  {
    LoudsTrie::Node node = node_ab;
    EXPECT_TRUE(trie.MoveToChildByLabel('d', &node));
    EXPECT_EQ(node_abd, node);
  }
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("abd", &node));
    EXPECT_EQ(node_abd, node);
  }

  // There is no child nor right sibling for "abd".
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToFirstChild(node_abd)));
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToNextSibling(node_abd)));

  // Terminal node (leaf) for "abcd".
  const LoudsTrie::Node node_abcd = trie.MoveToFirstChild(node_abc);
  ASSERT_TRUE(trie.IsValidNode(node_abcd));
  ASSERT_TRUE(trie.IsTerminalNode(node_abcd));
  EXPECT_EQ('d', trie.GetEdgeLabelToParentNode(node_abcd));
  EXPECT_EQ(5, trie.GetKeyIdOfTerminalNode(node_abcd));
  EXPECT_EQ(node_abcd, trie.GetTerminalNodeFromKeyId(5));
  EXPECT_EQ("abcd", trie.RestoreKeyString(5, buf));
  {
    LoudsTrie::Node node = node_abc;
    EXPECT_TRUE(trie.MoveToChildByLabel('d', &node));
    EXPECT_EQ(node_abcd, node);
  }
  {
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("abcd", &node));
    EXPECT_EQ(node_abcd, node);
  }

  // There is no child nor right sibling for "abcd".
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToFirstChild(node_abcd)));
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToNextSibling(node_abcd)));

  // Try moving to non-existing nodes by a label.
  {
    LoudsTrie::Node node;
    EXPECT_FALSE(trie.MoveToChildByLabel('x', &node));
    EXPECT_FALSE(trie.IsValidNode(node));
  }

  // Traverse for some non-existing keys.
  {
    LoudsTrie::Node node;
    EXPECT_FALSE(trie.Traverse("x", &node));
    EXPECT_FALSE(trie.Traverse("xyz", &node));
  }
}
INSTANTIATE_TEST_CASE(GenNodeBasedApisTest);

TEST_P(LoudsTrieTest, HasKey) {
  LoudsTrieBuilder builder;
  builder.Add("a");
  builder.Add("abc");
  builder.Add("abcd");
  builder.Add("ae");
  builder.Add("aecd");
  builder.Add("b");
  builder.Add("bcx");
  builder.Build();

  const CacheSizeParam &param = GetParam();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()),
            param.louds_lb0_cache_size, param.louds_lb1_cache_size,
            param.louds_select0_cache_size, param.louds_select1_cache_size,
            param.termvec_lb1_cache_size);

  EXPECT_TRUE(trie.HasKey("a"));
  EXPECT_TRUE(trie.HasKey("abc"));
  EXPECT_TRUE(trie.HasKey("abcd"));
  EXPECT_TRUE(trie.HasKey("ae"));
  EXPECT_TRUE(trie.HasKey("aecd"));
  EXPECT_TRUE(trie.HasKey("b"));
  EXPECT_TRUE(trie.HasKey("bcx"));
  EXPECT_FALSE(trie.HasKey(""));
  EXPECT_FALSE(trie.HasKey("ab"));
  EXPECT_FALSE(trie.HasKey("aa"));
  EXPECT_FALSE(trie.HasKey("aec"));
  EXPECT_FALSE(trie.HasKey("aecx"));
  EXPECT_FALSE(trie.HasKey("aecdf"));
  EXPECT_FALSE(trie.HasKey("abcdefghi"));
  EXPECT_FALSE(trie.HasKey("bc"));
  EXPECT_FALSE(trie.HasKey("bca"));
  EXPECT_FALSE(trie.HasKey("bcxyz"));
}

TEST(LoudsTrieTest, ExactSearch) {
  LoudsTrieBuilder builder;
  builder.Add("a");
  builder.Add("abc");
  builder.Add("abcd");
  builder.Add("ae");
  builder.Add("aecd");
  builder.Add("b");
  builder.Add("bcx");

  builder.Build();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  EXPECT_EQ(builder.GetId("a"), trie.ExactSearch("a"));
  EXPECT_EQ(builder.GetId("abc"), trie.ExactSearch("abc"));
  EXPECT_EQ(builder.GetId("abcd"), trie.ExactSearch("abcd"));
  EXPECT_EQ(builder.GetId("ae"), trie.ExactSearch("ae"));
  EXPECT_EQ(builder.GetId("aecd"), trie.ExactSearch("aecd"));
  EXPECT_EQ(builder.GetId("b"), trie.ExactSearch("b"));
  EXPECT_EQ(builder.GetId("bcx"), trie.ExactSearch("bcx"));
  EXPECT_EQ(-1, trie.ExactSearch(""));
  EXPECT_EQ(-1, trie.ExactSearch("ab"));
  EXPECT_EQ(-1, trie.ExactSearch("aa"));
  EXPECT_EQ(-1, trie.ExactSearch("aec"));
  EXPECT_EQ(-1, trie.ExactSearch("aecx"));
  EXPECT_EQ(-1, trie.ExactSearch("aecdf"));
  EXPECT_EQ(-1, trie.ExactSearch("abcdefghi"));
  EXPECT_EQ(-1, trie.ExactSearch("bc"));
  EXPECT_EQ(-1, trie.ExactSearch("bca"));
  EXPECT_EQ(-1, trie.ExactSearch("bcxyz"));
  trie.Close();
}
INSTANTIATE_TEST_CASE(GenHasKeyTest);

TEST_P(LoudsTrieTest, PrefixSearch) {
  LoudsTrieBuilder builder;
  builder.Add("aa");
  builder.Add("ab");
  builder.Add("abc");
  builder.Add("abcd");
  builder.Add("abd");
  builder.Add("ebd");
  builder.Add("\x01\xFF");
  builder.Add("\x01\xFF\xFF");
  builder.Build();

  const CacheSizeParam &param = GetParam();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()),
            param.louds_lb0_cache_size, param.louds_lb1_cache_size,
            param.louds_select0_cache_size, param.louds_select1_cache_size,
            param.termvec_lb1_cache_size);
  {
    const absl::string_view kKey = "abc";
    std::vector<RecordCallbackArgs::CallbackArgs> actual;
    trie.PrefixSearch(kKey, RecordCallbackArgs(&actual));

    ASSERT_EQ(2, actual.size());

    // "ab"
    EXPECT_EQ(kKey, actual[0].key);
    EXPECT_EQ(2, actual[0].prefix_len);
    EXPECT_EQ(&trie, actual[0].trie);
    EXPECT_EQ(Traverse(trie, "ab"), actual[0].node);

    // "abc"
    EXPECT_EQ(kKey, actual[1].key);
    EXPECT_EQ(3, actual[1].prefix_len);
    EXPECT_EQ(&trie, actual[1].trie);
    EXPECT_EQ(Traverse(trie, "abc"), actual[1].node);
  }
  {
    const absl::string_view kKey = "abxxxxxxx";
    std::vector<RecordCallbackArgs::CallbackArgs> actual;
    trie.PrefixSearch(kKey, RecordCallbackArgs(&actual));

    ASSERT_EQ(1, actual.size());

    // "ab"
    EXPECT_EQ(kKey, actual[0].key);
    EXPECT_EQ(2, actual[0].prefix_len);
    EXPECT_EQ(&trie, actual[0].trie);
    EXPECT_EQ(Traverse(trie, "ab"), actual[0].node);
  }
  {
    // Make sure that it works for non-ascii characters too.
    const absl::string_view kKey = "\x01\xFF\xFF";
    std::vector<RecordCallbackArgs::CallbackArgs> actual;
    trie.PrefixSearch(kKey, RecordCallbackArgs(&actual));

    ASSERT_EQ(2, actual.size());

    // "\x01\xFF"
    EXPECT_EQ(kKey, actual[0].key);
    EXPECT_EQ(2, actual[0].prefix_len);
    EXPECT_EQ(&trie, actual[0].trie);
    EXPECT_EQ(Traverse(trie, "\x01\xFF"), actual[0].node);

    // "\x01\xFF\xFF"
    EXPECT_EQ(kKey, actual[1].key);
    EXPECT_EQ(3, actual[1].prefix_len);
    EXPECT_EQ(&trie, actual[1].trie);
    EXPECT_EQ(Traverse(trie, "\x01\xFF\xFF"), actual[1].node);
  }
  {
    std::vector<RecordCallbackArgs::CallbackArgs> actual;
    trie.PrefixSearch("xyz", RecordCallbackArgs(&actual));
    EXPECT_TRUE(actual.empty());
  }
}
INSTANTIATE_TEST_CASE(GenPrefixSearchTest);

TEST_P(LoudsTrieTest, RestoreKeyString) {
  LoudsTrieBuilder builder;
  builder.Add("aa");
  builder.Add("ab");
  builder.Add("abc");
  builder.Add("abcd");
  builder.Add("abcde");
  builder.Add("abcdef");
  builder.Add("abcea");
  builder.Add("abcef");
  builder.Add("abd");
  builder.Add("ebd");
  builder.Build();

  const CacheSizeParam &param = GetParam();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()),
            param.louds_lb0_cache_size, param.louds_lb1_cache_size,
            param.louds_select0_cache_size, param.louds_select1_cache_size,
            param.termvec_lb1_cache_size);

  char buffer[LoudsTrie::kMaxDepth + 1];
  EXPECT_EQ("aa", trie.RestoreKeyString(builder.GetId("aa"), buffer));
  EXPECT_EQ("ab", trie.RestoreKeyString(builder.GetId("ab"), buffer));
  EXPECT_EQ("abc", trie.RestoreKeyString(builder.GetId("abc"), buffer));
  EXPECT_EQ("abcd", trie.RestoreKeyString(builder.GetId("abcd"), buffer));
  EXPECT_EQ("abcde", trie.RestoreKeyString(builder.GetId("abcde"), buffer));
  EXPECT_EQ("abcdef", trie.RestoreKeyString(builder.GetId("abcdef"), buffer));
  EXPECT_EQ("abcea", trie.RestoreKeyString(builder.GetId("abcea"), buffer));
  EXPECT_EQ("abcef", trie.RestoreKeyString(builder.GetId("abcef"), buffer));
  EXPECT_EQ("abd", trie.RestoreKeyString(builder.GetId("abd"), buffer));
  EXPECT_EQ("ebd", trie.RestoreKeyString(builder.GetId("ebd"), buffer));
  EXPECT_EQ("", trie.RestoreKeyString(-1, buffer));
  trie.Close();
}
INSTANTIATE_TEST_CASE(GenRestoreKeyStringTest);

}  // namespace
}  // namespace louds
}  // namespace storage
}  // namespace mozc
