// Copyright 2010-2011, Google Inc.
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

#include <sstream>
#include <string>

#include "base/hash_tables.h"
#include "base/util.h"
#include "dictionary/rx/rx_trie_builder.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace rx {
namespace {
TEST(RxTrieBuilderTest, BasicTest) {
  // 6 unique entries
  const char *kKeys[] = {
    "a",
    "b",
    "c",
    "aa",
    "aaa",
    "aab",
    "a",
  };
  RxTrieBuilder builder;
  for (size_t i = 0; i < arraysize(kKeys); ++i) {
    builder.AddKey(kKeys[i]);
  }
  builder.Build();
  hash_set<int> seen;
  for (size_t i = 0; i < arraysize(kKeys); ++i) {
    const int id = builder.GetIdFromKey(kKeys[i]);
    EXPECT_GE(id, 0);
    seen.insert(id);
  }
  EXPECT_EQ(6, seen.size());
  EXPECT_EQ(-1, builder.GetIdFromKey("unknown"));
}

TEST(RxTrieBuilderTest, RandomTest) {
  srand(0);
  const int kTestSize = 1000000;
  hash_set<string> inserted;
  RxTrieBuilder builder;
  for (size_t i = 0; i < kTestSize; ++i) {
    const string key = Util::SimpleItoa(
        static_cast<int>(1.0 * kTestSize * rand() / (RAND_MAX + 1.0)));
    builder.AddKey(key);
    inserted.insert(key);
  }
  builder.Build();
  for (size_t i = 0; i < kTestSize; ++i) {
    const string key = Util::SimpleItoa(
        static_cast<int>(1.0 * kTestSize * rand() / (RAND_MAX + 1.0)));
    if (inserted.find(key) != inserted.end()) {
      EXPECT_GE(builder.GetIdFromKey(key), 0);
    } else {
      EXPECT_EQ(-1, builder.GetIdFromKey(key));
    }
  }
  hash_set<int> ids;
  for (size_t i = 0; i < kTestSize; ++i) {
    const string key = Util::SimpleItoa(i);
    const int id = builder.GetIdFromKey(key);
    if (id != -1) {
      ids.insert(id);
    }
  }
  EXPECT_EQ(inserted.size(), ids.size());
}
}  // namespace
}  // namespace rx
}  // namespace mozc
