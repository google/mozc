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

#include "base/container/trie.h"

#include <cstddef>
#include <cstring>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(TrieTest, Trie) {
  Trie<std::string> trie;

  enum TestType {
    LOOKUP,
    ADD,
  };
  const struct TestCase {
    const int type;
    const std::string key;
    const std::string value;
    const bool expected_found;
    const std::string expected_value;
  } test_cases[] = {
      {ADD, "abc", "data_abc", true, "data_abc"},
      {ADD, "abd", "data_abd", true, "data_abd"},
      {ADD, "abcd", "data_abcd", true, "data_abcd"},
      {ADD, "abc", "data_abc2", true, "data_abc2"},
      {ADD, "bcd", "data_bcd", true, "data_bcd"},
      {LOOKUP, "abc", "", true, "data_abc2"},
      {LOOKUP, "abd", "", true, "data_abd"},
      {LOOKUP, "abcd", "", true, "data_abcd"},
      {LOOKUP, "bcd", "", true, "data_bcd"},
      {LOOKUP, "xyz", "", false, ""},
      {LOOKUP, "abcde", "", false, ""},
  };
  const int size = std::size(test_cases);
  for (int i = 0; i < size; ++i) {
    const TestCase& test = test_cases[i];
    switch (test.type) {
      case ADD:
        trie.AddEntry(test.key, test.value);
        break;
      case LOOKUP:
      default:
        // do nothing
        break;
    }
    Trie<std::string> other;
    using std::swap;
    swap(trie, other);
    std::string data;
    const bool found = other.LookUp(test.key, &data);
    EXPECT_EQ(found, test.expected_found);
    if (found) {
      EXPECT_EQ(data, test.expected_value);
    }
    EXPECT_FALSE(trie.LookUp(test.key, &data));
    swap(trie, other);
  }
}

TEST(TrieTest, LongestMatch) {
  Trie<std::string> trie;
  trie.AddEntry("abc", "[ABC]");
  trie.AddEntry("abd", "[ABD]");
  trie.AddEntry("a", "[A]");

  std::string value;
  size_t key_length = 0;
  EXPECT_TRUE(trie.LongestMatch("abc", &value, &key_length));
  EXPECT_EQ("[ABC]", value);
  EXPECT_EQ(3, key_length);

  value.clear();
  key_length = 0;
  EXPECT_TRUE(trie.LongestMatch("abcd", &value, &key_length));
  EXPECT_EQ("[ABC]", value);
  EXPECT_EQ(3, key_length);

  value.clear();
  key_length = 0;
  EXPECT_TRUE(trie.LongestMatch("abe", &value, &key_length));
  EXPECT_EQ("[A]", value);
  EXPECT_EQ(1, key_length);

  value.clear();
  key_length = 0;
  EXPECT_TRUE(trie.LongestMatch("ac", &value, &key_length));
  EXPECT_EQ("[A]", value);
  EXPECT_EQ(1, key_length);

  value.clear();
  key_length = 0;
  EXPECT_FALSE(trie.LongestMatch("xyz", &value, &key_length));
}

TEST(TrieTest, LookUpPrefix) {
  Trie<std::string> trie;
  trie.AddEntry("abc", "[ABC]");
  trie.AddEntry("abd", "[ABD]");
  trie.AddEntry("a", "[A]");

  std::string value;
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
  Trie<std::string> trie;
  {
    std::vector<std::string> values;
    trie.LookUpPredictiveAll("a", &values);
    EXPECT_EQ(0, values.size());
  }

  {
    std::string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    EXPECT_FALSE(trie.LookUpPrefix("a", &value, &key_length, &has_subtrie));
    EXPECT_EQ("", value);
    EXPECT_EQ(0, key_length);
  }
}

TEST(TrieTest, UTF8LookUpPrefix) {
  Trie<std::string> trie;
  trie.AddEntry("きゃ", "");
  trie.AddEntry("きゅ", "");
  trie.AddEntry("きょ", "");
  trie.AddEntry("っ", "");
  trie.AddEntry("か", "");
  trie.AddEntry("き", "");
  trie.AddEntry("く", "");
  trie.AddEntry("け", "");
  trie.AddEntry("こ", "");

  std::string value;
  size_t key_length = 0;
  bool has_subtrie = false;
  {
    key_length = 0;
    const std::string query = "か";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    const std::string query = "きゅ";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    const std::string query = "くぁ";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    const std::string query = "っあ";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    const std::string query = "き";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    const std::string query = "かかかかかか";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    const std::string query = "きゅあああ";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    const std::string query = "きあああ";
    EXPECT_TRUE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
  {
    key_length = 0;
    const std::string query = "も";
    EXPECT_FALSE(trie.LookUpPrefix(query, &value, &key_length, &has_subtrie));
  }
}

TEST(TrieTest, UTF8LongestMatch) {
  Trie<std::string> trie;
  trie.AddEntry("あいう", "アイウ");
  trie.AddEntry("あいえ", "アイエ");
  trie.AddEntry("あ", "ア");

  std::string value;
  size_t key_length = 0;
  EXPECT_TRUE(trie.LongestMatch("あいう", &value, &key_length));
  EXPECT_EQ("アイウ", value);
  EXPECT_EQ(strlen("あいう"), key_length);

  value.clear();
  key_length = 0;
  EXPECT_TRUE(trie.LongestMatch("あいうえ", &value, &key_length));
  EXPECT_EQ("アイウ", value);
  EXPECT_EQ(strlen("あいう"), key_length);

  value.clear();
  key_length = 0;
  EXPECT_TRUE(trie.LongestMatch("あい", &value, &key_length));
  EXPECT_EQ("ア", value);
  EXPECT_EQ(strlen("あ"), key_length);

  value.clear();
  key_length = 0;
  EXPECT_TRUE(trie.LongestMatch("あお", &value, &key_length));
  EXPECT_EQ("ア", value);
  EXPECT_EQ(strlen("あ"), key_length);

  value.clear();
  key_length = 0;
  EXPECT_FALSE(trie.LongestMatch("お", &value, &key_length));
}

bool HasData(absl::Span<const std::string> values,
             const absl::string_view value) {
  return absl::c_find(values, value) != values.end();
}

TEST(TrieTest, LookUpPredictiveAll) {
  Trie<std::string> trie;
  trie.AddEntry("abc", "[ABC]");
  trie.AddEntry("abd", "[ABD]");
  trie.AddEntry("a", "[A]");

  {
    std::vector<std::string> values;
    trie.LookUpPredictiveAll("a", &values);
    EXPECT_EQ(3, values.size());
    EXPECT_TRUE(HasData(values, "[ABC]"));
    EXPECT_TRUE(HasData(values, "[ABD]"));
    EXPECT_TRUE(HasData(values, "[A]"));
  }

  {
    std::vector<std::string> values;
    trie.LookUpPredictiveAll("ab", &values);
    EXPECT_EQ(2, values.size());
    EXPECT_TRUE(HasData(values, "[ABC]"));
    EXPECT_TRUE(HasData(values, "[ABD]"));
  }

  {
    std::vector<std::string> values;
    trie.LookUpPredictiveAll("abc", &values);
    EXPECT_EQ(1, values.size());
    EXPECT_TRUE(HasData(values, "[ABC]"));
  }

  {
    std::vector<std::string> values;
    trie.LookUpPredictiveAll("", &values);
    EXPECT_EQ(3, values.size());
    EXPECT_TRUE(HasData(values, "[ABC]"));
    EXPECT_TRUE(HasData(values, "[ABD]"));
    EXPECT_TRUE(HasData(values, "[A]"));
  }

  {
    std::vector<std::string> values;
    trie.LookUpPredictiveAll("x", &values);
    EXPECT_EQ(0, values.size());
  }
}

TEST(TrieTest, EmptyKeyAndHasSubTrie) {
  Trie<std::string> trie;
  EXPECT_FALSE(trie.HasSubTrie(""));
  EXPECT_FALSE(trie.HasSubTrie("a"));

  trie.AddEntry("", "[ROOT]");
  EXPECT_FALSE(trie.HasSubTrie(""));

  std::string value;
  EXPECT_TRUE(trie.LookUp("", &value));
  EXPECT_EQ("[ROOT]", value);

  size_t key_length = 99;
  bool fixed = false;
  EXPECT_TRUE(trie.LookUpPrefix("", &value, &key_length, &fixed));
  EXPECT_EQ("[ROOT]", value);
  EXPECT_EQ(0, key_length);
  EXPECT_TRUE(fixed);

  EXPECT_TRUE(trie.LongestMatch("xyz", &value, &key_length));
  EXPECT_EQ("[ROOT]", value);
  EXPECT_EQ(0, key_length);

  trie.AddEntry("ab", "[AB]");
  EXPECT_TRUE(trie.HasSubTrie("a"));
  EXPECT_TRUE(trie.HasSubTrie("ab"));
  EXPECT_FALSE(trie.HasSubTrie("abc"));
  EXPECT_FALSE(trie.HasSubTrie(""));

  EXPECT_TRUE(trie.LookUpPrefix("", &value, &key_length, &fixed));
  EXPECT_FALSE(fixed);
}

TEST(TrieTest, LookUpPrefixKeyLengthAndFixedSemantics) {
  Trie<std::string> trie;
  trie.AddEntry("a", "[A]");
  trie.AddEntry("abc", "[ABC]");
  trie.AddEntry("abd", "[ABD]");

  std::string value = "unchanged";
  size_t key_length = 0;
  bool fixed = false;

  // "abe" matches edges "ab", where no value exists; must NOT fall back to "a".
  EXPECT_FALSE(trie.LookUpPrefix("abe", &value, &key_length, &fixed));
  EXPECT_EQ("unchanged", value);
  EXPECT_EQ(2, key_length);
  EXPECT_TRUE(fixed);

  EXPECT_TRUE(trie.LookUpPrefix("a", &value, &key_length, &fixed));
  EXPECT_EQ("[A]", value);
  EXPECT_EQ(1, key_length);
  EXPECT_FALSE(fixed);

  EXPECT_TRUE(trie.LookUpPrefix("abc", &value, &key_length, &fixed));
  EXPECT_EQ("[ABC]", value);
  EXPECT_EQ(3, key_length);
  EXPECT_TRUE(fixed);
}

TEST(TrieTest, ManyBranchesReallocationAndOverwrite) {
  Trie<std::string> trie;
  // Insert 150 multi-level branches so `nodes_` reallocates repeatedly while
  // single-child parent maps hold inline SOO entries.
  for (int i = 0; i < 150; ++i) {
    const std::string prefix = absl::StrCat("k", i, "_");
    trie.AddEntry(prefix, absl::StrCat("v_", i));
    trie.AddEntry(absl::StrCat(prefix, "leaf"), absl::StrCat("vl_", i));
  }

  std::string value;
  for (int i = 0; i < 150; ++i) {
    const std::string prefix = absl::StrCat("k", i, "_");
    EXPECT_TRUE(trie.LookUp(prefix, &value));
    EXPECT_EQ(absl::StrCat("v_", i), value);
    EXPECT_TRUE(trie.LookUp(absl::StrCat(prefix, "leaf"), &value));
    EXPECT_EQ(absl::StrCat("vl_", i), value);
  }

  trie.AddEntry("k0_", "overwritten");
  EXPECT_TRUE(trie.LookUp("k0_", &value));
  EXPECT_EQ("overwritten", value);
}

TEST(TrieTest, MultiByteUtf8LengthsAndDeepTree) {
  Trie<int> trie;
  // 1-byte ASCII + 2-byte U+00E9 + 3-byte U+3042 + 4-byte U+F0000 (PUA).
  const std::string mixed = "a\xc3\xa9\xe3\x81\x82\xf3\xb0\x80\x80";
  trie.AddEntry(mixed, 42);

  // Deep key and wide branching (>16 inline stack capacity) exercise iterative
  // traversal without recursion.
  const std::string deep(2000, 'z');
  trie.AddEntry(deep, 99);
  for (char ch = 'a'; ch <= 'z'; ++ch) {
    const char key[] = {'w', ch};
    trie.AddEntry(absl::string_view(key, 2), static_cast<int>(ch));
  }

  int val = 0;
  size_t key_len = 0;
  bool fixed = false;
  EXPECT_TRUE(
      trie.LookUpPrefix(absl::StrCat(mixed, "!"), &val, &key_len, &fixed));
  EXPECT_EQ(42, val);
  EXPECT_EQ(mixed.size(), key_len);
  EXPECT_TRUE(fixed);

  EXPECT_TRUE(trie.LookUp(deep, &val));
  EXPECT_EQ(99, val);

  std::vector<int> wide_values;
  trie.LookUpPredictiveAll("w", &wide_values);
  EXPECT_EQ(26, wide_values.size());

  std::vector<int> all;
  trie.LookUpPredictiveAll("", &all);
  EXPECT_EQ(28, all.size());
}

TEST(TrieTest, InvalidUtf8) {
  Trie<std::string> trie;
  trie.AddEntry(absl::string_view("\0", 1), "[NUL]");
  trie.AddEntry("a", "[A]");
  trie.AddEntry("abc", "[ABC]");

  // Invalid UTF-8 keys are ignored by AddEntry and must not alias U+0000 or
  // overwrite prefix values.
  trie.AddEntry("\xff", "[BAD]");
  trie.AddEntry("a\xff", "[BAD_A]");

  std::string value;
  EXPECT_FALSE(trie.LookUp("\xff", &value));
  EXPECT_FALSE(trie.LookUp("a\xff", &value));
  EXPECT_FALSE(trie.HasSubTrie("\xff"));
  EXPECT_FALSE(trie.HasSubTrie("a\xff"));
  EXPECT_TRUE(trie.LookUp(absl::string_view("\0", 1), &value));
  EXPECT_EQ("[NUL]", value);
  EXPECT_TRUE(trie.LookUp("a", &value));
  EXPECT_EQ("[A]", value);

  size_t key_length = 99;
  bool fixed = false;
  EXPECT_FALSE(trie.LongestMatch("\xff", &value, &key_length));
  EXPECT_EQ(0, key_length);
  EXPECT_FALSE(trie.LookUpPrefix("\xff", &value, &key_length, &fixed));
  EXPECT_EQ(0, key_length);
  EXPECT_TRUE(fixed);

  // Prefix matching stops cleanly before a trailing invalid UTF-8 byte.
  EXPECT_TRUE(trie.LongestMatch("a\xff", &value, &key_length));
  EXPECT_EQ("[A]", value);
  EXPECT_EQ(1, key_length);
  EXPECT_TRUE(trie.LookUpPrefix("a\xff", &value, &key_length, &fixed));
  EXPECT_EQ("[A]", value);
  EXPECT_EQ(1, key_length);
  EXPECT_FALSE(fixed);

  std::vector<std::string> predictive;
  trie.LookUpPredictiveAll("\xff", &predictive);
  EXPECT_TRUE(predictive.empty());
}

TEST(TrieTest, BoolSpecialization) {
  Trie<bool> trie;
  trie.AddEntry("ありがとう", true);
  trie.AddEntry("よろしく", false);

  bool val = false;
  size_t len = 0;
  EXPECT_TRUE(trie.LongestMatch("ありがとうございます", &val, &len));
  EXPECT_TRUE(val);
  EXPECT_EQ(strlen("ありがとう"), len);

  EXPECT_TRUE(trie.LookUp("よろしく", &val));
  EXPECT_FALSE(val);
}

}  // namespace
}  // namespace mozc
