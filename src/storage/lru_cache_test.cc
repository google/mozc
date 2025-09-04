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

#include "storage/lru_cache.h"

#include <cstddef>
#include <vector>

#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace storage {
namespace {

using ::testing::ElementsAre;

template <typename Key, typename Value>
size_t SizeOfFreeList(const LruCache<Key, Value>& cache) {
  size_t size = 0;
  for (const typename LruCache<Key, Value>::Element* e =
           cache.FreeListForTesting();
       e; e = e->next) {
    ++size;
  }
  return size;
}

template <typename Key, typename Value>
std::vector<Key> GetOrderedKeys(const LruCache<Key, Value>& cache) {
  std::vector<Key> keys;
  keys.reserve(cache.Size());
  for (const typename LruCache<Key, Value>::Element& elem : cache) {
    keys.push_back(elem.key);
  }
  return keys;
}

TEST(LruCacheTest, Insert) {
  LruCache<int, int> cache(3);
  EXPECT_EQ(cache.Size(), 0);

  cache.Insert(0, 0);
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(0));
  cache.Insert(1, 1);
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(1, 0));
  cache.Insert(2, 2);
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2, 1, 0));
  cache.Insert(3, 3);
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(3, 2, 1));
  cache.Insert(4, 4);
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(4, 3, 2));
  cache.Insert(5, 5);
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(5, 4, 3));
}

TEST(LruCacheTest, Lookup) {
  LruCache<int, int> cache(5);
  for (int i = 0; i < 3; ++i) {
    cache.Insert(i, i);
  }
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2, 1, 0));

  // Looked up elements are moved to the head.
  EXPECT_NE(cache.Lookup(0), nullptr);
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(0, 2, 1));
  EXPECT_NE(cache.Lookup(1), nullptr);
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(1, 0, 2));

  EXPECT_EQ(cache.Lookup(-1), nullptr);
  EXPECT_EQ(cache.Lookup(3), nullptr);
}

TEST(LruCacheTest, LookupWithoutInsert) {
  LruCache<int, int> cache(5);
  for (int i = 0; i < 3; ++i) {
    cache.Insert(i, i);
  }
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2, 1, 0));

  // Unlike Lookup, LRU order shouldn't change.
  for (int i = 0; i < 3; ++i) {
    EXPECT_NE(cache.LookupWithoutInsert(i), nullptr);
    EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2, 1, 0));
  }
  EXPECT_EQ(cache.LookupWithoutInsert(-1), nullptr);
  EXPECT_EQ(cache.LookupWithoutInsert(3), nullptr);
}

TEST(LruCacheTest, Erase) {
  LruCache<int, int> cache(5);
  for (int i = 0; i < 3; ++i) {
    cache.Insert(i, i);
  }
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2, 1, 0));

  EXPECT_FALSE(cache.Erase(-1));
  EXPECT_FALSE(cache.Erase(5));

  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2, 1, 0));
  EXPECT_TRUE(cache.Erase(1));
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2, 0));
  EXPECT_TRUE(cache.Erase(0));
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2));
  EXPECT_TRUE(cache.Erase(2));
  EXPECT_EQ(cache.Size(), 0);
}

TEST(LruCacheTest, Clear) {
  LruCache<int, int> cache(5);
  for (int i = 0; i < 3; ++i) {
    cache.Insert(i, i);
  }
  EXPECT_THAT(GetOrderedKeys(cache), ElementsAre(2, 1, 0));
  EXPECT_EQ(SizeOfFreeList(cache), 2);
  cache.Clear();
  EXPECT_EQ(cache.Size(), 0);
  EXPECT_EQ(SizeOfFreeList(cache), 5);
}

TEST(LruCacheTest, LargeCapacity) {
  constexpr int kCapacity = 1000000;
  LruCache<int, int> cache(kCapacity);
  for (int i = 0; i < 3 * kCapacity; ++i) {
    cache.Insert(i, i);
    EXPECT_TRUE(cache.HasKey(i));
    EXPECT_EQ(cache.Head()->key, i);
    EXPECT_GE(kCapacity, cache.Size());
  }
}

}  // namespace
}  // namespace storage
}  // namespace mozc
