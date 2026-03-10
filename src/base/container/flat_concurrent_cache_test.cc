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

#include "base/container/flat_concurrent_cache.h"

#include <functional>
#include <string>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/strings/str_cat.h"
#include "base/thread.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using TestCache = FlatConcurrentCache<std::string, int>;

TEST(FlatConcurrentCacheTest, BasicPutAndGet) {
  TestCache cache(10);
  int value = 0;

  cache.Insert("key1", 100);
  cache.Insert("key2", 200);

  EXPECT_TRUE(cache.Lookup("key1", &value));
  EXPECT_EQ(value, 100);

  EXPECT_TRUE(cache.Lookup("key2", &value));
  EXPECT_EQ(value, 200);

  EXPECT_FALSE(cache.Lookup("unknown", &value));
}

TEST(FlatConcurrentCacheTest, OverwriteExistingKey) {
  TestCache cache(10);
  int value = 0;

  cache.Insert("key1", 100);
  cache.Insert("key1", 999);  // Update with the same key.

  EXPECT_TRUE(cache.Lookup("key1", &value));
  EXPECT_EQ(value, 999);
}

TEST(FlatConcurrentCacheTest, EvictionPolicy) {
  // Force to use the bucket of size 1.
  FlatConcurrentCache<int, int, absl::Hash<int>, std::equal_to<int>, 4>
      small_cache(1);

  // Inserts 4 items.
  for (int i = 0; i < 4; ++i) small_cache.Insert(i, i * 10);

  // Access the item 0 so increase access_clock.
  int value = 0;
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(small_cache.Lookup(0, &value));
  }

  // Insert next item.
  small_cache.Insert(100, 1000);

  // The item 0 should be survived.
  EXPECT_TRUE(small_cache.Lookup(0, &value));
  EXPECT_EQ(value, 0);
}

TEST(FlatConcurrentCacheTest, ClearCache) {
  TestCache cache(100);
  cache.Insert("a", 1);
  cache.Insert("b", 2);
  cache.Clear();

  int value = 0;
  EXPECT_FALSE(cache.Lookup("a", &value));
  EXPECT_FALSE(cache.Lookup("b", &value));
  EXPECT_FALSE(cache.Lookup("c", &value));
}

TEST(FlatConcurrentCacheTest, ConcurrencyStressTest) {
  const int kNumThreads = 8;
  const int kOpsPerThread = 10000;
  TestCache cache(1000);

  std::vector<Thread> threads;
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&cache, t]() {
      for (int i = 0; i < kOpsPerThread; ++i) {
        const std::string key = absl::StrCat("key_", i % 100);  // Conflicts
        if (i % 2 == 0) {
          cache.Insert(key, i + t);
        } else {
          int value = 0;
          cache.Lookup(key, &value);
        }
      }
    });
  }

  for (Thread& th : threads) {
    th.Join();
  }
}

}  // namespace
}  // namespace mozc
