// Copyright 2010-2013, Google Inc.
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

#include "storage/lru_storage.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/util.h"
#include "storage/lru_cache.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace storage {
namespace {

string GenRandomString(int size) {
  string result;
  const size_t len = Util::Random(size) + 1;
  for (int i = 0; i < len; ++i) {
    const uint16 l = Util::Random(0xFFFF) + 1;
    Util::UCS2ToUTF8Append(l, &result);
  }
  return result;
}

void RunTest(LRUStorage *storage, uint32 size) {
  mozc::storage::LRUCache<string, uint32> cache(size);
  set<string> used;
  vector<pair<string, uint32> > values;
  for (int i = 0; i < size * 2; ++i) {
    const string key = GenRandomString(20);
    const uint32 value = static_cast<uint32>(Util::Random(10000000));
    if (used.find(key) != used.end()) {
      continue;
    }
    used.insert(key);
    values.push_back(make_pair(key, value));
    cache.Insert(key, value);
    storage->Insert(key, reinterpret_cast<const char *>(&value));
  }

  reverse(values.begin(), values.end());

  vector<string> value_list;
  EXPECT_TRUE(storage->GetAllValues(&value_list));

  uint32 last_access_time;
  for (int i = 0; i < size; ++i) {
    const uint32 *v1 = cache.Lookup(values[i].first);
    const uint32 *v2 = reinterpret_cast<const uint32*>(
        storage->Lookup(values[i].first, &last_access_time));
    const uint32 *v3 = reinterpret_cast<const uint32*>(value_list[i].data());
    EXPECT_TRUE(v1 != NULL);
    EXPECT_EQ(*v1, values[i].second);
    EXPECT_TRUE(v2 != NULL);
    EXPECT_EQ(*v2, values[i].second);
    EXPECT_TRUE(v3 != NULL);
    EXPECT_EQ(*v3, values[i].second);
  }

  for (int i = size; i < values.size(); ++i) {
    const uint32 *v1 = cache.Lookup(values[i].first);
    const uint32 *v2 = reinterpret_cast<const uint32*>(
        storage->Lookup(values[i].first, &last_access_time));
    EXPECT_TRUE(v1 == NULL);
    EXPECT_TRUE(v2 == NULL);
  }
}
}  // namespace

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
    if (FileUtil::FileExists(path)) {
      FileUtil::Unlink(path);
    }
  }

  static string GetTemporaryFilePath() {
    // This name should be unique to each test.
    return FileUtil::JoinPath(FLAGS_test_tmpdir, "LRUStorageTest_test.db");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LRUStorageTest);
};

TEST_F(LRUStorageTest, LRUStorageTest) {
  const int kSize[] = {10, 100, 1000, 10000};
  const string file = GetTemporaryFilePath();
  for (int i = 0; i < arraysize(kSize); ++i) {
    LRUStorage::CreateStorageFile(file.c_str(), 4, kSize[i], 0x76fef);
    LRUStorage storage;
    EXPECT_TRUE(storage.Open(file.c_str()));
    EXPECT_EQ(file, storage.filename());
    EXPECT_EQ(kSize[i], storage.size());
    EXPECT_EQ(4, storage.value_size());
    EXPECT_EQ(0x76fef, storage.seed());
    RunTest(&storage, kSize[i]);
  }
}

struct Entry {
  uint64 key;
  uint32 last_access_time;
  string value;
};

TEST_F(LRUStorageTest, ReadWriteTest) {
  const int kSize[] = {10, 100, 1000, 10000};
  const string file = GetTemporaryFilePath();
  for (int i = 0; i < arraysize(kSize); ++i) {
    LRUStorage::CreateStorageFile(file.c_str(), 4, kSize[i], 0x76fef);
    LRUStorage storage;
    EXPECT_TRUE(storage.Open(file.c_str()));
    EXPECT_EQ(file, storage.filename());
    EXPECT_EQ(kSize[i], storage.size());
    EXPECT_EQ(4, storage.value_size());
    EXPECT_EQ(0x76fef, storage.seed());

    vector<Entry> entries;

    const size_t size = kSize[i];
    for (int j = 0; j < size; ++j) {
      Entry entry;
      entry.key = Util::Random(RAND_MAX);
      const int n = Util::Random(RAND_MAX);
      entry.value.assign(reinterpret_cast<const char *>(&n), 4);
      entry.last_access_time = Util::Random(100000);
      entries.push_back(entry);
      storage.Write(j, entry.key, entry.value, entry.last_access_time);
    }

    for (int j = 0; j < size; ++j) {
      uint64 key;
      string value;
      uint32 last_access_time;
      storage.Read(j, &key, &value, &last_access_time);
      EXPECT_EQ(entries[j].key, key);
      EXPECT_EQ(entries[j].value, value);
      EXPECT_EQ(entries[j].last_access_time, last_access_time);
    }
  }
}

TEST_F(LRUStorageTest, Merge) {
  const string file1 = GetTemporaryFilePath() + ".tmp1";
  const string file2 = GetTemporaryFilePath() + ".tmp2";

  // Can merge
  {
    LRUStorage::CreateStorageFile(file1.c_str(), 4, 100, 0x76fef);
    LRUStorage::CreateStorageFile(file2.c_str(), 4, 100, 0x76fef);
    LRUStorage storage;
    EXPECT_TRUE(storage.Open(file1.c_str()));
    EXPECT_TRUE(storage.Merge(file2.c_str()));
  }

  // different entry size
  {
    LRUStorage::CreateStorageFile(file1.c_str(), 4, 100, 0x76fef);
    LRUStorage::CreateStorageFile(file2.c_str(), 4, 200, 0x76fef);
    LRUStorage storage;
    EXPECT_TRUE(storage.Open(file1.c_str()));
    EXPECT_TRUE(storage.Merge(file2.c_str()));
  }

  // seed is different
  {
    LRUStorage::CreateStorageFile(file1.c_str(), 4, 100, 0x76fef);
    LRUStorage::CreateStorageFile(file2.c_str(), 4, 200, 0x76fee);
    LRUStorage storage;
    EXPECT_TRUE(storage.Open(file1.c_str()));
    EXPECT_FALSE(storage.Merge(file2.c_str()));
  }

  // value size is different
  {
    LRUStorage::CreateStorageFile(file1.c_str(), 4, 100, 0x76fef);
    LRUStorage::CreateStorageFile(file2.c_str(), 8, 200, 0x76fef);
    LRUStorage storage;
    EXPECT_TRUE(storage.Open(file1.c_str()));
    EXPECT_FALSE(storage.Merge(file2.c_str()));
  }

  {
    LRUStorage::CreateStorageFile(file1.c_str(), 4, 8, 0x76fef);
    LRUStorage::CreateStorageFile(file2.c_str(), 4, 4, 0x76fef);
    LRUStorage storage1;
    EXPECT_TRUE(storage1.Open(file1.c_str()));
    storage1.Write(0, 0, "test", 0);
    storage1.Write(1, 1, "test", 10);
    storage1.Write(2, 2, "test", 20);
    storage1.Write(3, 3, "test", 30);

    LRUStorage storage2;
    EXPECT_TRUE(storage2.Open(file2.c_str()));
    storage2.Write(0, 4, "test", 0);
    storage2.Write(1, 5, "test", 50);

    EXPECT_TRUE(storage1.Merge(storage2));

    uint64 fp;
    string value;
    uint32 last_access_time;

    storage1.Read(0, &fp, &value, &last_access_time);
    EXPECT_EQ(5, fp);
    EXPECT_EQ(50, last_access_time);

    storage1.Read(1, &fp, &value, &last_access_time);
    EXPECT_EQ(3, fp);
    EXPECT_EQ(30, last_access_time);

    storage1.Read(2, &fp, &value, &last_access_time);
    EXPECT_EQ(2, fp);
    EXPECT_EQ(20, last_access_time);

    storage1.Read(3, &fp, &value, &last_access_time);
    EXPECT_EQ(1, fp);
    EXPECT_EQ(10, last_access_time);
  }

  // same FP
  {
    LRUStorage::CreateStorageFile(file1.c_str(), 4, 8, 0x76fef);
    LRUStorage::CreateStorageFile(file2.c_str(), 4, 4, 0x76fef);
    LRUStorage storage1;
    EXPECT_TRUE(storage1.Open(file1.c_str()));
    storage1.Write(0, 0, "test", 0);
    storage1.Write(1, 1, "test", 10);
    storage1.Write(2, 2, "test", 20);
    storage1.Write(3, 3, "test", 30);

    LRUStorage storage2;
    EXPECT_TRUE(storage2.Open(file2.c_str()));
    storage2.Write(0, 2, "new1", 0);
    storage2.Write(1, 3, "new2", 50);

    EXPECT_TRUE(storage1.Merge(storage2));

    uint64 fp;
    string value;
    uint32 last_access_time;

    storage1.Read(0, &fp, &value, &last_access_time);
    EXPECT_EQ(3, fp);
    EXPECT_EQ(50, last_access_time);
    EXPECT_EQ("new2", value);

    storage1.Read(1, &fp, &value, &last_access_time);
    EXPECT_EQ(2, fp);
    EXPECT_EQ(20, last_access_time);
    EXPECT_EQ("test", value);

    storage1.Read(2, &fp, &value, &last_access_time);
    EXPECT_EQ(1, fp);
    EXPECT_EQ(10, last_access_time);

    storage1.Read(3, &fp, &value, &last_access_time);
    EXPECT_EQ(0, fp);
    EXPECT_EQ(0, last_access_time);
  }

  FileUtil::Unlink(file1);
  FileUtil::Unlink(file2);
}

TEST_F(LRUStorageTest, InvalidFileOpenTest) {
  LRUStorage storage;
  EXPECT_FALSE(storage.Insert("test", NULL));

  const string filename = GetTemporaryFilePath();
  FileUtil::Unlink(filename);

  // cannot open
  EXPECT_FALSE(storage.Open(filename.c_str()));
  EXPECT_FALSE(storage.Insert("test", NULL));
}

class LRUStorageOpenOrCreateTest : public testing::Test {
 protected:
  LRUStorageOpenOrCreateTest() {}

  virtual void SetUp() {
    UnlinkDBFileIfExists();
  }

  virtual void TearDown() {
    UnlinkDBFileIfExists();
  }

  static void UnlinkDBFileIfExists() {
    const string path = GetTemporaryFilePath();
    if (FileUtil::FileExists(path)) {
      FileUtil::Unlink(path);
    }
  }

  static string GetTemporaryFilePath() {
    // This name should be unique to each test.
    return FileUtil::JoinPath(FLAGS_test_tmpdir,
                              "LRUStorageOpenOrCreateTest_test.db");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LRUStorageOpenOrCreateTest);
};

TEST_F(LRUStorageOpenOrCreateTest, OpenOrCreateTest) {
  const string file = GetTemporaryFilePath();
  {
    OutputFileStream ofs(file.c_str());
    ofs << "test";
  }

  {
    LRUStorage storage;
    EXPECT_FALSE(storage.Open(file.c_str()))
        << "Corrupted file should be detected as an error.";
  }

  {
    LRUStorage storage;
    EXPECT_TRUE(storage.OpenOrCreate(file.c_str(), 4, 10, 0x76fef))
        << "Corrupted file should be replaced with new one.";
    uint32 v = 823;
    storage.Insert("test", reinterpret_cast<const char *>(&v));
    const uint32 *result =
        reinterpret_cast<const uint32 *>(storage.Lookup("test"));
    CHECK_EQ(v, *result);
  }
}

}  // namespace storage
}  // namespace mozc
