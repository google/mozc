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

#include "dictionary/louds/louds_trie_adapter.h"

#include <algorithm>

#include "base/hash_tables.h"
#include "base/number_util.h"
#include "base/util.h"
#include "storage/louds/louds_trie_builder.h"
#include "testing/base/public/gunit.h"

namespace {

using ::mozc::NumberUtil;
using ::mozc::Util;
using ::mozc::dictionary::louds::Entry;
using ::mozc::dictionary::louds::LoudsTrieAdapter;
using ::mozc::storage::louds::LoudsTrieBuilder;

class LoudsTrieAdapterTest : public ::testing::Test {
};

TEST_F(LoudsTrieAdapterTest, BasicTest) {
  LoudsTrieBuilder builder;
  builder.Add("a");
  builder.Add("b");
  builder.Add("c");
  builder.Add("aa");
  builder.Add("aaa");
  builder.Add("aaa");
  builder.Add("aaa");
  builder.Add("aaa");
  builder.Add("ab");
  builder.Build();

  const int a_id = builder.GetId("a");
  ASSERT_NE(-1, a_id);

  LoudsTrieAdapter trie;
  trie.OpenImage(reinterpret_cast<const uint8 *>(builder.image().data()));

  {
    vector<Entry> results;
    trie.PrefixSearch("aaa", &results);
    // aaa, aa, a
    EXPECT_EQ(3, results.size());
  }
  {
    vector<Entry> results;
    trie.PredictiveSearch("a", &results);
    // a, aa, aaa, ab
    EXPECT_EQ(4, results.size());
  }
  {
    string key;
    trie.ReverseLookup(a_id, &key);
    EXPECT_EQ("a", key);
  }
  {
    const int id = trie.GetIdFromKey("a");
    EXPECT_EQ(a_id, id);
  }
  {
    const int id = trie.GetIdFromKey("x");
    EXPECT_EQ(-1, id);
  }
}

struct CmpEntry {
  bool operator()(const Entry &lhs, const Entry &rhs) {
    if (lhs.id != rhs.id) {
      return lhs.id < rhs.id;
    } else {
      return lhs.key < rhs.key;
    }
  }
};

TEST_F(LoudsTrieAdapterTest, RandomTest) {
  const int kTestSize = 1000000;
  hash_map<string, int> inserted;
  LoudsTrieBuilder builder;

  {
    Util::SetRandomSeed(0);
    for (size_t i = 0; i < kTestSize; ++i) {
      const string key = NumberUtil::SimpleItoa(Util::Random(kTestSize));
      builder.Add(key);
      inserted.insert(make_pair(key, -1));
    }
    builder.Build();
    for (hash_map<string, int>::iterator iter = inserted.begin();
         iter != inserted.end(); ++iter) {
      const int id = builder.GetId(iter->first);
      ASSERT_NE(-1, id);
      iter->second = id;
    }
  }

  LoudsTrieAdapter trie;
  trie.OpenImage(reinterpret_cast<const uint8 *>(builder.image().data()));

  // Find prefix for "111111"
  {
    vector<Entry> expected;
    for (size_t i = 0; i < 6; ++i) {
      const string key = string(i + 1, '1');
      const hash_map<string, int>::const_iterator iter = inserted.find(key);
      if (iter != inserted.end()) {
        Entry entry;
        entry.key = iter->first;
        entry.id = iter->second;
        expected.push_back(entry);
      }
    }

    vector<Entry> results;
    trie.PrefixSearch("111111", &results);
    ASSERT_EQ(expected.size(), results.size());
    sort(expected.begin(), expected.end(), CmpEntry());
    sort(results.begin(), results.end(), CmpEntry());
    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i].key, results[i].key);
      EXPECT_EQ(expected[i].id, results[i].id);
      string key;
      trie.ReverseLookup(results[i].id, &key);
      EXPECT_EQ(results[i].key, key);
      const int id = trie.GetIdFromKey(results[i].key);
      EXPECT_EQ(results[i].id, id);
    }
  }

  // Find predictive for "11111"
  {
    vector<Entry> expected;
    for (hash_map<string, int>::const_iterator iter = inserted.begin();
         iter != inserted.end(); ++iter) {
      if (iter->first.find("11111") == 0) {
        Entry entry;
        entry.key = iter->first;
        entry.id = iter->second;
        expected.push_back(entry);
      }
    }

    vector<Entry> results;
    trie.PredictiveSearch("11111", &results);
    ASSERT_EQ(expected.size(), results.size());
    sort(expected.begin(), expected.end(), CmpEntry());
    sort(results.begin(), results.end(), CmpEntry());
    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i].key, results[i].key);
      EXPECT_EQ(expected[i].id, results[i].id);
      string key;
      trie.ReverseLookup(results[i].id, &key);
      EXPECT_EQ(results[i].key, key);
      const int id = trie.GetIdFromKey(results[i].key);
      EXPECT_EQ(results[i].id, id);
    }
  }

  {
    for (size_t i = 0; i < (kTestSize / 1000); ++i) {
      const string test_key = NumberUtil::SimpleItoa(
          Util::Random(kTestSize));
      const hash_map<string, int>::const_iterator iter =
          inserted.find(test_key);
      string key;
      if (iter != inserted.end()) {
        trie.ReverseLookup(iter->second, &key);
        EXPECT_EQ(iter->first, key);
        const int id = trie.GetIdFromKey(iter->first);
        EXPECT_EQ(iter->second, id);
      }
    }
  }
}

TEST_F(LoudsTrieAdapterTest, LimitTest) {
  const int kTestSize = 100;
  const int kLimit = 3;
  hash_map<string, int> inserted;
  LoudsTrieBuilder builder;
  {
    Util::SetRandomSeed(0);
    for (size_t i = 0; i < kTestSize; ++i) {
      const string key = string(i + 1, '1');
      builder.Add(key);
      inserted.insert(make_pair(key, -1));
    }
    builder.Build();
    for (hash_map<string, int>::iterator iter = inserted.begin();
         iter != inserted.end(); ++iter) {
      const int id = builder.GetId(iter->first);
      EXPECT_NE(-1, id);
      iter->second = id;
    }
  }

  LoudsTrieAdapter trie;
  trie.OpenImage(reinterpret_cast<const uint8 *>(builder.image().data()));

  // Find prefix for "111111"
  {
    vector<Entry> expected;
    for (size_t i = 0; i < 6; ++i) {
      const string key = string(i + 1, '1');
      const hash_map<string, int>::const_iterator iter = inserted.find(key);
      if (iter != inserted.end()) {
        Entry entry;
        entry.key = iter->first;
        entry.id = iter->second;
        expected.push_back(entry);
      }
    }

    vector<Entry> results;
    trie.PrefixSearchWithLimit("111111", kLimit, &results);
    ASSERT_LE(kLimit, expected.size());
    ASSERT_EQ(kLimit, results.size());
    sort(expected.begin(), expected.end(), CmpEntry());
    sort(results.begin(), results.end(), CmpEntry());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(expected[i].key, results[i].key);
      EXPECT_EQ(expected[i].id, results[i].id);
      string key;
      trie.ReverseLookup(results[i].id, &key);
      EXPECT_EQ(results[i].key, key);
      const int id = trie.GetIdFromKey(results[i].key);
      EXPECT_EQ(results[i].id, id);
    }
  }

  // Find predictive for "11111"
  {
    vector<Entry> expected;
    for (hash_map<string, int>::const_iterator iter = inserted.begin();
         iter != inserted.end(); ++iter) {
      if (iter->first.find("11111") == 0) {
        Entry entry;
        entry.key = iter->first;
        entry.id = iter->second;
        expected.push_back(entry);
      }
    }

    vector<Entry> results;
    trie.PredictiveSearchWithLimit("11111", kLimit, &results);
    ASSERT_LE(kLimit, expected.size());
    ASSERT_EQ(kLimit, results.size());
    sort(expected.begin(), expected.end(), CmpEntry());
    sort(results.begin(), results.end(), CmpEntry());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(expected[i].key, results[i].key);
      EXPECT_EQ(expected[i].id, results[i].id);
      string key;
      trie.ReverseLookup(results[i].id, &key);
      EXPECT_EQ(results[i].key, key);
      const int id = trie.GetIdFromKey(results[i].key);
      EXPECT_EQ(results[i].id, id);
    }
  }
}

}  // namespace
