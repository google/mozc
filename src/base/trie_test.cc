// Copyright 2010-2012, Google Inc.
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

#include "base/base.h"
#include "base/trie.h"
#include "testing/base/public/gunit.h"

TEST(TrieTest, Trie) {
  mozc::Trie<string> trie;

  enum TestType {
    LOOKUP,
    ADD,
    REMOVE,
  };
  static const struct TestCase {
    const int type;
    const string key;
    const string value;
    const bool expected_found;
    const string expected_value;
  } test_cases[] = {
    { ADD, "abc", "data_abc", true, "data_abc" },
    { ADD, "abd", "data_abd", true, "data_abd" },
    { ADD, "abcd", "data_abcd", true, "data_abcd" },
    { ADD, "abc", "data_abc2", true, "data_abc2" },
    { ADD, "bcd", "data_bcd", true, "data_bcd" },
    { LOOKUP, "abc", "", true, "data_abc2" },
    { LOOKUP, "abd", "", true, "data_abd" },
    { LOOKUP, "abcd", "", true, "data_abcd" },
    { LOOKUP, "bcd", "", true, "data_bcd" },
    { LOOKUP, "xyz", "", false, "" },
    { LOOKUP, "abcde", "", false, "" },
    { REMOVE, "bcd", "", false, "" },
    { REMOVE, "abd", "", false, "" },
    { LOOKUP, "abc", "", true, "data_abc2" },
    { LOOKUP, "abcd", "", true, "data_abcd" },
    { REMOVE, "abc", "", false, "" },
    { LOOKUP, "abcd", "", true, "data_abcd" },
    { REMOVE, "xyz", "", false, "" },
  };
  const int size = ARRAYSIZE_UNSAFE(test_cases);
  for (int i = 0; i < size; ++i) {
    const TestCase& test = test_cases[i];
    switch (test.type) {
    case ADD:
      trie.AddEntry(test.key, test.value);
      break;
    case REMOVE:
      trie.DeleteEntry(test.key);
      break;
    case LOOKUP:
    default:
      // do nothing
      break;
    }
    string data;
    const bool found = trie.LookUp(test.key, &data);
    EXPECT_EQ(found, test.expected_found);
    if (found) {
      EXPECT_EQ(data, test.expected_value);
    }
  }
}

TEST(TrieTest, LookUpPrefix) {
  mozc::Trie<string> trie;
  trie.AddEntry("abc", "[ABC]");
  trie.AddEntry("abd", "[ABD]");
  trie.AddEntry("a", "[A]");

  string value;
  size_t key_length = 0;
  bool has_subtrie = false;
  EXPECT_TRUE(trie.LookUpPrefix("abc", &value, &key_length, &has_subtrie));
  EXPECT_EQ("[ABC]", value);
  EXPECT_EQ(3, key_length);

  value.clear();
  key_length = 0;
  EXPECT_TRUE(trie.LookUpPrefix("abcd", &value, &key_length, &has_subtrie));
  EXPECT_EQ("[ABC]", value);
  EXPECT_EQ(3, key_length);

  value.clear();
  key_length = 0;
  EXPECT_FALSE(trie.LookUpPrefix("abe", &value, &key_length, &has_subtrie));

  value.clear();
  key_length = 0;
  EXPECT_TRUE(trie.LookUpPrefix("ac", &value, &key_length, &has_subtrie));
  EXPECT_EQ("[A]", value);
  EXPECT_EQ(1, key_length);

  value.clear();
  key_length = 0;
  EXPECT_FALSE(trie.LookUpPrefix("xyz", &value, &key_length, &has_subtrie));
}

TEST(TrieTest, Empty) {
  mozc::Trie<string> trie;
  {
    vector<string> values;
    trie.LookUpPredictiveAll("a", &values);
    EXPECT_EQ(0, values.size());
  }

  {
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_FALSE(trie.LookUpPrefix("a", &value, &key_length, &has_subtrie));
    EXPECT_EQ("", value);
    EXPECT_EQ(0, key_length);
  }
}

TEST(TrieTest, UTF8LookUpPrefix) {
  mozc::Trie<string> trie;
  // "きゃ"
  trie.AddEntry("\xe3\x81\x8d\xe3\x82\x83", "");
  // "きゅ"
  trie.AddEntry("\xe3\x81\x8d\xe3\x82\x85", "");
  // "きょ"
  trie.AddEntry("\xe3\x81\x8d\xe3\x82\x87", "");
  // "っ"
  trie.AddEntry("\xe3\x81\xa3", "");
  // "か"
  trie.AddEntry("\xe3\x81\x8b", "");
  // "き"
  trie.AddEntry("\xe3\x81\x8d", "");
  // "く"
  trie.AddEntry("\xe3\x81\x8f", "");
  // "け"
  trie.AddEntry("\xe3\x81\x91", "");
  // "こ"
  trie.AddEntry("\xe3\x81\x93", "");

  string value;
  size_t key_length = 0;
  bool has_subtrie = false;
  {
    key_length = 0;
    // "か"
    const string query = "\xe3\x81\x8b";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    // "きゅ"
    const string query = "\xe3\x81\x8d\xe3\x82\x85";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    // "くぁ"
    const string query = "\xe3\x81\x8f\xe3\x81\x81";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    // "っあ"
    const string query = "\xe3\x81\xa3\xe3\x81\x82";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    // "き"
    const string query = "\xe3\x81\x8d";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    // "かかかかかか"
    const string query = "\xe3\x81\x8b\xe3\x81\x8b\xe3\x81\x8b\xe3\x81\x8b\xe3\x81\x8b\xe3\x81\x8b";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    // "きゅあああ"
    const string query = "\xe3\x81\x8d\xe3\x82\x85\xe3\x81\x82\xe3\x81\x82\xe3\x81\x82";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    // "きあああ"
    const string query = "\xe3\x81\x8d\xe3\x81\x82\xe3\x81\x82\xe3\x81\x82";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    // "も"
    const string query = "\xe3\x82\x82";
    EXPECT_FALSE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
}

namespace {
bool HasData(const vector<string> &values, const string &value) {
  for (size_t i = 0; i < values.size(); ++i) {
    if (values[i] == value) {
      return true;
    }
  }
  return false;
}
}  // namespace

TEST(TrieTest, LookUpPredictiveAll) {
  mozc::Trie<string> trie;
  trie.AddEntry("abc", "[ABC]");
  trie.AddEntry("abd", "[ABD]");
  trie.AddEntry("a", "[A]");

  {
    vector<string> values;
    trie.LookUpPredictiveAll("a", &values);
    EXPECT_EQ(3, values.size());
    EXPECT_TRUE(HasData(values, "[ABC]"));
    EXPECT_TRUE(HasData(values, "[ABD]"));
    EXPECT_TRUE(HasData(values, "[A]"));
  }

  {
    vector<string> values;
    trie.LookUpPredictiveAll("ab", &values);
    EXPECT_EQ(2, values.size());
    EXPECT_TRUE(HasData(values, "[ABC]"));
    EXPECT_TRUE(HasData(values, "[ABD]"));
  }

  {
    vector<string> values;
    trie.LookUpPredictiveAll("abc", &values);
    EXPECT_EQ(1, values.size());
    EXPECT_TRUE(HasData(values, "[ABC]"));
  }

  {
    vector<string> values;
    trie.LookUpPredictiveAll("", &values);
    EXPECT_EQ(3, values.size());
    EXPECT_TRUE(HasData(values, "[ABC]"));
    EXPECT_TRUE(HasData(values, "[ABD]"));
    EXPECT_TRUE(HasData(values, "[A]"));
  }

  {
    vector<string> values;
    trie.LookUpPredictiveAll("x", &values);
    EXPECT_EQ(0, values.size());
  }
}
