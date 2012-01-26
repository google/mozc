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

#include <algorithm>

#include "base/file_stream.h"
#include "base/hash_tables.h"
#include "base/mmap.h"
#include "base/util.h"
#include "dictionary/rx/rx_trie.h"
#include "dictionary/rx/rx_trie_builder.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace rx {
namespace {

class RxTrieTest : public testing::Test {
 protected:
  RxTrieTest()
      : test_rx_(FLAGS_test_tmpdir + "/test_rx") {}

  virtual void SetUp() {
    Util::Unlink(test_rx_);
  }

  virtual void TearDown() {
    Util::Unlink(test_rx_);
  }

  void WriteToFile(const RxTrieBuilder &builder) {
    OutputFileStream ofs(test_rx_.c_str(), ios::binary|ios::out);
    builder.WriteImage(&ofs);
    EXPECT_TRUE(Util::FileExists(test_rx_));
  }

  void ReadFromFile(RxTrie *trie) {
    DCHECK(trie);
    EXPECT_TRUE(Util::FileExists(test_rx_));
    mapping_.reset(new Mmap<char>());
    mapping_->Open(test_rx_.c_str());
    const char *ptr = mapping_->begin();
    EXPECT_TRUE(trie->OpenImage(
        reinterpret_cast<const unsigned char *>(ptr)));
  }
  scoped_ptr<Mmap<char> > mapping_;

  const string test_rx_;
};

TEST_F(RxTrieTest, BasicTest) {
  int a_id = -1;
  {
    RxTrieBuilder builder;
    builder.AddKey("a");
    builder.AddKey("b");
    builder.AddKey("c");
    builder.AddKey("aa");
    builder.AddKey("aaa");
    builder.AddKey("aaa");
    builder.AddKey("aaa");
    builder.AddKey("aaa");
    builder.AddKey("ab");
    builder.Build();
    WriteToFile(builder);
    a_id = builder.GetIdFromKey("a");
    EXPECT_NE(-1, a_id);
  }
  RxTrie trie;
  ReadFromFile(&trie);
  {
    vector<RxEntry> results;
    trie.PrefixSearch("aaa", &results);
    // aaa, aa, a
    EXPECT_EQ(3, results.size());
  }
  {
    vector<RxEntry> results;
    trie.PredictiveSearch("a", &results);
    // a, aa, aaa, ab
    EXPECT_EQ(4, results.size());
  }
  {
    string key;
    trie.ReverseLookup(a_id, &key);
    EXPECT_EQ("a", key);
  }
}

struct CmpRxEntry {
  bool operator()(const RxEntry &lhs, const RxEntry &rhs) {
    if (lhs.id != rhs.id) {
      return lhs.id < rhs.id;
    } else {
      return lhs.key < rhs.key;
    }
  }
};

TEST_F(RxTrieTest, RandomTest) {
  const int kTestSize = 1000000;
  hash_map<string, int> inserted;
  {
    srand(0);
    RxTrieBuilder builder;
    for (size_t i = 0; i < kTestSize; ++i) {
      const string key = Util::SimpleItoa(
          static_cast<int>(kTestSize * (1.0 * rand() / RAND_MAX)));
      builder.AddKey(key);
      inserted.insert(make_pair(key, -1));
    }
    builder.Build();
    for (hash_map<string, int>::iterator itr = inserted.begin();
         itr != inserted.end(); ++itr) {
      const int id = builder.GetIdFromKey(itr->first);
      EXPECT_NE(-1, id);
      itr->second = id;
    }
    WriteToFile(builder);
  }

  RxTrie trie;
  ReadFromFile(&trie);
  {
    // Find prefix for "111111"
    vector<RxEntry> expected;
    for (size_t i = 0; i < 6; ++i) {
      const string key = string(i + 1, '1');
      const hash_map<string, int>::const_iterator itr = inserted.find(key);
      if (itr != inserted.end()) {
        RxEntry entry;
        entry.key = itr->first;
        entry.id = itr->second;
        expected.push_back(entry);
      }
    }
    vector<RxEntry> results;
    trie.PrefixSearch("111111", &results);
    EXPECT_EQ(expected.size(), results.size());
    sort(expected.begin(), expected.end(), CmpRxEntry());
    sort(results.begin(), results.end(), CmpRxEntry());
    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i].key, results[i].key);
      EXPECT_EQ(expected[i].id, results[i].id);
      string key;
      trie.ReverseLookup(results[i].id, &key);
      EXPECT_EQ(results[i].key, key);
    }
  }
  {
    // Find predictive for "11111"
    vector<RxEntry> expected;
    for (hash_map<string, int>::const_iterator itr = inserted.begin();
         itr != inserted.end(); ++itr) {
      if (itr->first.find("11111") == 0) {
        RxEntry entry;
        entry.key = itr->first;
        entry.id = itr->second;
        expected.push_back(entry);
      }
    }
    vector<RxEntry> results;
    trie.PredictiveSearch("11111", &results);
    EXPECT_EQ(expected.size(), results.size());
    sort(expected.begin(), expected.end(), CmpRxEntry());
    sort(results.begin(), results.end(), CmpRxEntry());
    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i].key, results[i].key);
      EXPECT_EQ(expected[i].id, results[i].id);
      string key;
      trie.ReverseLookup(results[i].id, &key);
      EXPECT_EQ(results[i].key, key);
    }
  }
  {
    for (size_t i = 0; i < (kTestSize / 1000); ++i) {
      const string test_key = Util::SimpleItoa(
          static_cast<int>(kTestSize * (1.0 * rand() / RAND_MAX)));
      const hash_map<string, int>::const_iterator itr = inserted.find(test_key);
      string key;
      if (itr != inserted.end()) {
        trie.ReverseLookup(itr->second, &key);
        EXPECT_EQ(itr->first, key);
      }
    }
  }
}

TEST_F(RxTrieTest, LimitTest) {
  const int kTestSize = 100;
  const int kLimit = 3;
  hash_map<string, int> inserted;
  {
    srand(0);
    RxTrieBuilder builder;
    for (size_t i = 0; i < kTestSize; ++i) {
      const string key = string(i + 1, '1');
      builder.AddKey(key);
      inserted.insert(make_pair(key, -1));
    }
    builder.Build();
    for (hash_map<string, int>::iterator itr = inserted.begin();
         itr != inserted.end(); ++itr) {
      const int id = builder.GetIdFromKey(itr->first);
      EXPECT_NE(-1, id);
      itr->second = id;
    }
    WriteToFile(builder);
  }

  RxTrie trie;
  ReadFromFile(&trie);
  {
    // Find prefix for "111111"
    vector<RxEntry> expected;
    for (size_t i = 0; i < 6; ++i) {
      const string key = string(i + 1, '1');
      const hash_map<string, int>::const_iterator itr = inserted.find(key);
      if (itr != inserted.end()) {
        RxEntry entry;
        entry.key = itr->first;
        entry.id = itr->second;
        expected.push_back(entry);
      }
    }
    vector<RxEntry> results;
    trie.PrefixSearchWithLimit("111111", kLimit, &results);
    EXPECT_LE(kLimit, expected.size());
    EXPECT_EQ(kLimit, results.size());
    sort(expected.begin(), expected.end(), CmpRxEntry());
    sort(results.begin(), results.end(), CmpRxEntry());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(expected[i].key, results[i].key);
      EXPECT_EQ(expected[i].id, results[i].id);
      string key;
      trie.ReverseLookup(results[i].id, &key);
      EXPECT_EQ(results[i].key, key);
    }
  }
  {
    // Find predictive for "11111"
    vector<RxEntry> expected;
    for (hash_map<string, int>::const_iterator itr = inserted.begin();
         itr != inserted.end(); ++itr) {
      if (itr->first.find("11111") == 0) {
        RxEntry entry;
        entry.key = itr->first;
        entry.id = itr->second;
        expected.push_back(entry);
      }
    }
    vector<RxEntry> results;
    trie.PredictiveSearchWithLimit("11111", kLimit, &results);
    EXPECT_LE(kLimit, expected.size());
    EXPECT_EQ(kLimit, results.size());
    sort(expected.begin(), expected.end(), CmpRxEntry());
    sort(results.begin(), results.end(), CmpRxEntry());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(expected[i].key, results[i].key);
      EXPECT_EQ(expected[i].id, results[i].id);
      string key;
      trie.ReverseLookup(results[i].id, &key);
      EXPECT_EQ(results[i].key, key);
    }
  }
}

}  // namespace
}  // namespace rx
}  // namespace mozc
