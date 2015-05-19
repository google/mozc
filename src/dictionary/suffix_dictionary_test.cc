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

#include "dictionary/suffix_dictionary.h"

#include "base/base.h"
#include "base/trie.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "testing/base/public/gunit.h"

namespace mozc {

TEST(SuffixDictionaryTest, BasicTest) {
  // Test the SuffixDictionary with mock data.
  testing::MockDataManager manager;
  const DictionaryInterface *d = manager.GetSuffixDictionary();
  NodeAllocator allocator;

  // doesn't support prefix/reverse lookup.
  EXPECT_TRUE(NULL == d->LookupPrefix("test", 4, &allocator));
  EXPECT_TRUE(NULL == d->LookupReverse("test", 4, &allocator));

  // only support predictive lookup.
  // Also, SuffixDictionary returns non-NULL value even
  // for empty request.
  EXPECT_FALSE(NULL == d->LookupPredictive(NULL, 0, &allocator));
  EXPECT_FALSE(NULL == d->LookupPredictive("", 0, &allocator));

  {
    // empty request.
    Node *node = d->LookupPredictive("", 0, &allocator);
    for (; node != NULL; node = node->bnext) {
      EXPECT_FALSE(node->key.empty());
      EXPECT_FALSE(node->value.empty());
      EXPECT_NE(node->lid, 0);
      EXPECT_NE(node->rid, 0);
    }
  }

  {
    // "た"
    const char kQuery[] = "\xE3\x81\x9F";
    Node *node = d->LookupPredictive(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      EXPECT_FALSE(node->key.empty());
      EXPECT_FALSE(node->value.empty());
      EXPECT_NE(node->lid, 0);
      EXPECT_NE(node->rid, 0);
      EXPECT_TRUE(Util::StartsWith(node->key, kQuery));
    }
  }

  {
    // "だ"
    const char kQuery[] = "\xe3\x81\xa0";
    DictionaryInterface::Limit limit;
    Trie<string> trie;
    // "ね"
    trie.AddEntry("\xe3\x81\xad", "");
    // "よ"
    trie.AddEntry("\xe3\x82\x88", "");
    limit.begin_with_trie = &trie;

    Node *node = d->LookupPredictiveWithLimit(kQuery, strlen(kQuery),
                                              limit, &allocator);
    for (; node != NULL; node = node->bnext) {
      // "だね"
      // "だよ"
      EXPECT_TRUE(Util::StartsWith(node->key, "\xe3\x81\xa0\xe3\x81\xad") ||
                  Util::StartsWith(node->key, "\xe3\x81\xa0\xe3\x82\x88"));
      // "だな"
      EXPECT_FALSE(Util::StartsWith(node->key, "\xe3\x81\xa0\xe3\x81\xaa"));
      EXPECT_NE(node->lid, 0);
      EXPECT_NE(node->rid, 0);
    }
  }

  {
    DictionaryInterface::Limit limit;
    Trie<string> trie;
    // "だ"
    trie.AddEntry("\xe3\x81\xa0", "");
    // "で"
    trie.AddEntry("\xe3\x81\xa7", "");
    limit.begin_with_trie = &trie;

    Node *node = d->LookupPredictiveWithLimit("", 0, limit, &allocator);
    for (; node != NULL; node = node->bnext) {
      // "だ"
      // "で"
      EXPECT_TRUE(Util::StartsWith(node->key, "\xe3\x81\xa0") ||
                  Util::StartsWith(node->key, "\xe3\x81\xa7"));
      EXPECT_NE(node->lid, 0);
      EXPECT_NE(node->rid, 0);
    }
  }
}
}  // namespace mozc
