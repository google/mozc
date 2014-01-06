// Copyright 2010-2014, Google Inc.
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
#include "base/base.h"
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

TEST_F(LoudsTrieTest, Reverse) {
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
  EXPECT_STREQ("aa", trie.Reverse(builder.GetId("aa"), buffer));
  EXPECT_STREQ("ab", trie.Reverse(builder.GetId("ab"), buffer));
  EXPECT_STREQ("abc", trie.Reverse(builder.GetId("abc"), buffer));
  EXPECT_STREQ("abcd", trie.Reverse(builder.GetId("abcd"), buffer));
  EXPECT_STREQ("abcde", trie.Reverse(builder.GetId("abcde"), buffer));
  EXPECT_STREQ("abcdef", trie.Reverse(builder.GetId("abcdef"), buffer));
  EXPECT_STREQ("abcea", trie.Reverse(builder.GetId("abcea"), buffer));
  EXPECT_STREQ("abcef", trie.Reverse(builder.GetId("abcef"), buffer));
  EXPECT_STREQ("abd", trie.Reverse(builder.GetId("abd"), buffer));
  EXPECT_STREQ("ebd", trie.Reverse(builder.GetId("ebd"), buffer));
  EXPECT_STREQ("", trie.Reverse(-1, buffer));
  trie.Close();
}

}  // namespace
}  // namespace louds
}  // namespace storage
}  // namespace mozc
