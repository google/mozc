// Copyright 2010, Google Inc.
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
#include <string>
#include <vector>
#include <set>
#include <utility>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "storage/lru_cache.h"
#include "storage/lru_storage.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {
int Random(int size) {
  return 1 + static_cast<int> (1.0 * size * rand() / (RAND_MAX + 1.0));
}

string GenRandomString(int size) {
  string result;
  const size_t len = Random(size);
  for (int i = 0; i < len; ++i) {
    const uint16 l = Random(0xFFFF);
    Util::UCS2ToUTF8Append(l, &result);
  }
  return result;
}

void RunTest(LRUStorage *storage, uint32 size) {
  LRUCache<string, uint32> cache(size);
  set<string> used;
  vector<pair<string, uint32> > values;
  for (int i = 0; i < size * 2; ++i) {
    const string key = GenRandomString(20);
    const uint32 value = static_cast<uint32>(Random(10000000));
    if (used.find(key) != used.end()) {
      continue;
    }
    used.insert(key);
    values.push_back(make_pair(key, value));
    cache.Insert(key, value);
    storage->Insert(key, reinterpret_cast<const char *>(&value));
  }

  reverse(values.begin(), values.end());

  uint32 last_access_time;
  for (int i = 0; i < size; ++i) {
    const uint32 *v1 = cache.Lookup(values[i].first);
    const uint32 *v2 = reinterpret_cast<const uint32*>(
        storage->Lookup(values[i].first, &last_access_time));
    EXPECT_TRUE(v1 != NULL);
    EXPECT_EQ(*v1, values[i].second);
    EXPECT_TRUE(v2 != NULL);
    EXPECT_EQ(*v2, values[i].second);
  }

  for (int i = size; i < values.size(); ++i) {
    const uint32 *v1 = cache.Lookup(values[i].first);
    const uint32 *v2 = reinterpret_cast<const uint32*>(
        storage->Lookup(values[i].first, &last_access_time));
    EXPECT_TRUE(v1 == NULL);
    EXPECT_TRUE(v2 == NULL);
  }
}
}  // anonymous namespace

class LRUStorageTest : public testing::Test {
 protected:
  LRUStorageTest() {}

  virtual void SetUp() {
    UnlinkDBFileIfExists();
  }

  virtual void TearDown() {
    UnlinkDBFileIfExists();
  }

  static void UnlinkDBFileIfExists() {
    const string path = GetTemporaryFilePath();
    if (Util::FileExists(path)) {
      Util::Unlink(path);
    }
  }

  static string GetTemporaryFilePath() {
    // This name should be unique to each test.
    return Util::JoinPath(FLAGS_test_tmpdir, "LRUStorageTest_test.db");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LRUStorageTest);
};

TEST_F(LRUStorageTest, LRUStorageTest) {
  static const int kSize[] = {10, 100, 1000, 10000};
  const string file = GetTemporaryFilePath();
  for (int i = 0; i < arraysize(kSize); ++i) {
    LRUStorage::CreateStorageFile(file.c_str(), 4, kSize[i], 0x76fef);
    LRUStorage storage;
    EXPECT_TRUE(storage.Open(file.c_str()));
    EXPECT_EQ(storage.size(), kSize[i]);
    EXPECT_EQ(storage.value_size(), 4);
    RunTest(&storage, kSize[i]);
  }
}

class LRUStoragOpenOrCreateTest : public testing::Test {
 protected:
  LRUStoragOpenOrCreateTest() {}

  virtual void SetUp() {
    UnlinkDBFileIfExists();
  }

  virtual void TearDown() {
    UnlinkDBFileIfExists();
  }

  static void UnlinkDBFileIfExists() {
    const string path = GetTemporaryFilePath();
    if (Util::FileExists(path)) {
      Util::Unlink(path);
    }
  }

  static string GetTemporaryFilePath() {
    // This name should be unique to each test.
    return Util::JoinPath(FLAGS_test_tmpdir,
                          "LRUStoragOpenOrCreateTest_test.db");
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(LRUStoragOpenOrCreateTest);
};

TEST_F(LRUStoragOpenOrCreateTest, LRUStoragOpenOrCreateTest) {
  const string file = GetTemporaryFilePath();
  {
    OutputFileStream ofs(file.c_str());
    ofs << "test";
  }

  {
    LRUStorage storage;
    EXPECT_FALSE(storage.Open(file.c_str()));
  }

  {
    LRUStorage storage;
    EXPECT_TRUE(storage.OpenOrCreate(file.c_str(), 4, 10, 0x76fef));
    uint32 v = 823;
    storage.Insert("test", reinterpret_cast<const char *>(&v));
    const uint32 *result =
        reinterpret_cast<const uint32 *>(storage.Lookup("test"));
    CHECK_EQ(v, *result);
  }
}
}  // namespace mozc
