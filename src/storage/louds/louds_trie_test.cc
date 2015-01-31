// Copyright 2010-2015, Google Inc.
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

#include <limits>
#include <string>

#include "base/port.h"
#include "storage/louds/key_expansion_table.h"
#include "storage/louds/louds_trie_builder.h"
#include "testing/base/public/gunit.h"

// undef max macro if defined.
#ifdef max
#undef max
#endif  // max

namespace mozc {
namespace storage {
namespace louds {
namespace {

class LoudsTrieTest : public ::testing::Test {
};

class TestCallback : public LoudsTrie::Callback {
 public:
  TestCallback() : current_index_(0), limit_(numeric_limits<size_t>::max()) {
  }

  void AddExpectation(
      const string &s, size_t len, int id, ResultType result_type) {
    expectation_list_.push_back(Expectation());
    Expectation &expectation = expectation_list_.back();
    expectation.s = s;
    expectation.len = len;
    expectation.id = id;
    expectation.result_type = result_type;
  }

  virtual ResultType Run(const char *s, size_t len, int id) {
    if (current_index_ >= expectation_list_.size()) {
      ADD_FAILURE() << "Too many invocations: " << expectation_list_.size()
                    << " vs " << current_index_;
      // Quit the callback.
      return SEARCH_DONE;
    }
    const Expectation &expectation = expectation_list_[current_index_];
    EXPECT_EQ(expectation.s, string(s, len)) << current_index_;
    EXPECT_EQ(expectation.len, len) << current_index_;
    EXPECT_EQ(expectation.id, id) << current_index_;
    ++current_index_;
    return expectation.result_type;
  }

  size_t num_invoked() const { return current_index_; }

 private:
  struct Expectation {
    string s;
    size_t len;
    int id;
    ResultType result_type;
  };
  vector<Expectation> expectation_list_;
  size_t current_index_;
  size_t limit_;

  DISALLOW_COPY_AND_ASSIGN(TestCallback);
};

TEST_F(LoudsTrieTest, NodeBasedApis) {
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

  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

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
    LoudsTrie::Node node;
    EXPECT_TRUE(trie.Traverse("abcd", &node));
    EXPECT_EQ(node_abcd, node);
  }

  // There is no child nor right sibling for "abcd".
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToFirstChild(node_abcd)));
  EXPECT_FALSE(trie.IsValidNode(trie.MoveToNextSibling(node_abcd)));

  // Traverse for some non-existing keys.
  LoudsTrie::Node node;
  EXPECT_FALSE(trie.Traverse("x", &node));
  EXPECT_FALSE(trie.Traverse("xyz", &node));
}

TEST_F(LoudsTrieTest, HasKey) {
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

TEST_F(LoudsTrieTest, ExactSearch) {
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

TEST_F(LoudsTrieTest, PrefixSearch) {
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

  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  {
    TestCallback callback;
    callback.AddExpectation(
        "ab", 2, builder.GetId("ab"), LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation(
        "abc", 3, builder.GetId("abc"), LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PrefixSearch("abc", &callback);
    EXPECT_EQ(2, callback.num_invoked());
  }

  {
    TestCallback callback;
    callback.AddExpectation(
        "ab", 2, builder.GetId("ab"), LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PrefixSearch("abxxxxxxx", &callback);
    EXPECT_EQ(1, callback.num_invoked());
  }

  {
    // Make sure that non-ascii characters can be found, too.
    TestCallback callback;
    callback.AddExpectation("\x01\xFF", 2, builder.GetId("\x01\xFF"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("\x01\xFF\xFF", 3, builder.GetId("\x01\xFF\xFF"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PrefixSearch("\x01\xFF\xFF", &callback);
    EXPECT_EQ(2, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, PrefixSearchWithLimit) {
  LoudsTrieBuilder builder;
  builder.Add("aa");
  builder.Add("ab");
  builder.Add("abc");
  builder.Add("abcd");
  builder.Add("abcde");
  builder.Add("abcdef");
  builder.Add("abd");
  builder.Add("ebd");

  builder.Build();

  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  {
    TestCallback callback;
    callback.AddExpectation("ab", 2, builder.GetId("ab"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_DONE);

    trie.PrefixSearch("abcdef", &callback);
    EXPECT_EQ(2, callback.num_invoked());
  }

  {
    TestCallback callback;
    callback.AddExpectation("ab", 2, builder.GetId("ab"),
                            LoudsTrie::Callback::SEARCH_DONE);

    trie.PrefixSearch("abdxxx", &callback);
    EXPECT_EQ(1, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, PrefixSearchWithKeyExpansion) {
  LoudsTrieBuilder builder;
  builder.Add("abc");
  builder.Add("adc");
  builder.Add("cbc");
  builder.Add("ddc");

  builder.Build();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  KeyExpansionTable key_expansion_table;
  key_expansion_table.Add('b', "d");

  {
    TestCallback callback;
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("adc", 3, builder.GetId("adc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PrefixSearchWithKeyExpansion("abc", key_expansion_table, &callback);
    EXPECT_EQ(2, callback.num_invoked());
  }

  {
    TestCallback callback;
    callback.AddExpectation("adc", 3, builder.GetId("adc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    trie.PrefixSearchWithKeyExpansion("adc", key_expansion_table, &callback);
    EXPECT_EQ(1, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, PrefixSearchCulling) {
  LoudsTrieBuilder builder;
  builder.Add("a");
  builder.Add("ab");
  builder.Add("abc");
  builder.Add("abcd");
  builder.Add("ae");
  builder.Add("aec");
  builder.Add("aecd");

  builder.Build();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  KeyExpansionTable key_expansion_table;
  key_expansion_table.Add('b', "e");

  {
    TestCallback callback;
    callback.AddExpectation("a", 1, builder.GetId("a"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("ab", 2, builder.GetId("ab"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_CULL);
    // No callback for abcd, but ones for ae... should be found.
    callback.AddExpectation("ae", 2, builder.GetId("ae"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("aec", 3, builder.GetId("aec"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("aecd", 4, builder.GetId("aecd"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PrefixSearchWithKeyExpansion("abcd", key_expansion_table, &callback);
    EXPECT_EQ(6, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, PredictiveSearch) {
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
  builder.Add("\x01\xFF");
  builder.Add("\x01\xFF\xFF");

  builder.Build();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  {
    TestCallback callback;
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcd", 4, builder.GetId("abcd"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcde", 5, builder.GetId("abcde"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcdef", 6, builder.GetId("abcdef"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcea", 5, builder.GetId("abcea"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcef", 5, builder.GetId("abcef"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PredictiveSearch("abc", &callback);
    EXPECT_EQ(6, callback.num_invoked());
  }
  {
    TestCallback callback;
    callback.AddExpectation("aa", 2, builder.GetId("aa"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("ab", 2, builder.GetId("ab"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcd", 4, builder.GetId("abcd"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcde", 5, builder.GetId("abcde"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcdef", 6, builder.GetId("abcdef"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcea", 5, builder.GetId("abcea"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcef", 5, builder.GetId("abcef"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abd", 3, builder.GetId("abd"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PredictiveSearch("a", &callback);
    EXPECT_EQ(9, callback.num_invoked());
  }
  {
    // Make sure non-ascii characters can be found.
    TestCallback callback;
    callback.AddExpectation("\x01\xFF", 2, builder.GetId("\x01\xFF"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("\x01\xFF\xFF", 3, builder.GetId("\x01\xFF\xFF"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PredictiveSearch("\x01", &callback);
    EXPECT_EQ(2, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, PredictiveSearchWithLimit) {
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
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  {
    TestCallback callback;
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abcd", 4, builder.GetId("abcd"),
                            LoudsTrie::Callback::SEARCH_DONE);

    trie.PredictiveSearch("abc", &callback);
    EXPECT_EQ(2, callback.num_invoked());
  }
  {
    TestCallback callback;
    callback.AddExpectation("aa", 2, builder.GetId("aa"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("ab", 2, builder.GetId("ab"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_DONE);

    trie.PredictiveSearch("a", &callback);
    EXPECT_EQ(3, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, PredictiveSearchWithKeyExpansion) {
  LoudsTrieBuilder builder;
  builder.Add("abc");
  builder.Add("adc");
  builder.Add("cbc");
  builder.Add("ddc");

  builder.Build();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  KeyExpansionTable key_expansion_table;
  key_expansion_table.Add('b', "d");

  {
    TestCallback callback;
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("adc", 3, builder.GetId("adc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PredictiveSearchWithKeyExpansion(
        "ab", key_expansion_table, &callback);
    EXPECT_EQ(2, callback.num_invoked());
  }

  {
    TestCallback callback;
    callback.AddExpectation("adc", 3, builder.GetId("adc"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    trie.PredictiveSearchWithKeyExpansion(
        "ad", key_expansion_table, &callback);
    EXPECT_EQ(1, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, PredictiveSearchCulling) {
  LoudsTrieBuilder builder;
  builder.Add("a");
  builder.Add("ab");
  builder.Add("abc");
  builder.Add("abcd");
  builder.Add("ae");
  builder.Add("aec");
  builder.Add("aecd");

  builder.Build();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  {
    TestCallback callback;
    callback.AddExpectation("a", 1, builder.GetId("a"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("ab", 2, builder.GetId("ab"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_CULL);
    // No callback for abcd.
    callback.AddExpectation("ae", 2, builder.GetId("ae"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("aec", 3, builder.GetId("aec"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("aecd", 4, builder.GetId("aecd"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PredictiveSearch("a", &callback);
    EXPECT_EQ(6, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, PredictiveSearchCulling2) {
  LoudsTrieBuilder builder;
  builder.Add("abc");
  builder.Add("abcd");
  builder.Add("abcde");
  builder.Add("abxy");
  builder.Add("abxyz");

  builder.Build();
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

  {
    TestCallback callback;
    callback.AddExpectation("abc", 3, builder.GetId("abc"),
                            LoudsTrie::Callback::SEARCH_CULL);
    callback.AddExpectation("abxy", 4, builder.GetId("abxy"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);
    callback.AddExpectation("abxyz", 5, builder.GetId("abxyz"),
                            LoudsTrie::Callback::SEARCH_CONTINUE);

    trie.PredictiveSearch("a", &callback);
    EXPECT_EQ(3, callback.num_invoked());
  }

  trie.Close();
}

TEST_F(LoudsTrieTest, RestoreKeyString) {
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
  LoudsTrie trie;
  trie.Open(reinterpret_cast<const uint8 *>(builder.image().data()));

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

}  // namespace
}  // namespace louds
}  // namespace storage
}  // namespace mozc
