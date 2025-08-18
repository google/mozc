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

#include "storage/lru_storage.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/time/time.h"
#include "base/clock_mock.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "base/random.h"
#include "storage/lru_cache.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace storage {
namespace {

constexpr uint32_t kSeed = 0x76fef;  // Seed for fingerprint.

void RunTest(LruStorage* storage, uint32_t size) {
  mozc::storage::LruCache<std::string, uint32_t> cache(size);
  std::set<std::string, std::less<>> used;
  std::vector<std::pair<std::string, uint32_t>> values;
  mozc::Random random;
  for (int i = 0; i < size * 2; ++i) {
    const std::string key = random.Utf8String(
        absl::Uniform(absl::IntervalClosed, random, 1, 20), 1, 0x20000);
    const uint32_t value = absl::Uniform(random, 0u, 10000000u);
    if (used.find(key) != used.end()) {
      continue;
    }
    used.insert(key);
    values.push_back(std::make_pair(key, value));
    cache.Insert(key, value);
    storage->Insert(key, reinterpret_cast<const char*>(&value));
  }

  std::reverse(values.begin(), values.end());

  std::vector<std::string> value_list;
  storage->GetAllValues(&value_list);

  uint32_t last_access_time;
  for (int i = 0; i < size; ++i) {
    const uint32_t* v1 = cache.Lookup(values[i].first);
    const uint32_t* v2 = reinterpret_cast<const uint32_t*>(
        storage->Lookup(values[i].first, &last_access_time));
    const uint32_t* v3 =
        reinterpret_cast<const uint32_t*>(value_list[i].data());
    EXPECT_TRUE(v1 != nullptr);
    EXPECT_EQ(*v1, values[i].second);
    EXPECT_TRUE(v2 != nullptr);
    EXPECT_EQ(*v2, values[i].second);
    EXPECT_TRUE(v3 != nullptr);
    EXPECT_EQ(*v3, values[i].second);
  }

  for (int i = size; i < values.size(); ++i) {
    const uint32_t* v1 = cache.Lookup(values[i].first);
    const uint32_t* v2 = reinterpret_cast<const uint32_t*>(
        storage->Lookup(values[i].first, &last_access_time));
    EXPECT_TRUE(v1 == nullptr);
    EXPECT_TRUE(v2 == nullptr);
  }
}

std::vector<std::string> GetValuesInStorageOrder(const LruStorage& storage) {
  std::vector<std::string> ret;
  for (size_t i = 0; i < storage.used_size(); ++i) {
    uint64_t fp;
    std::string value;
    uint32_t last_access_time;
    storage.Read(i, &fp, &value, &last_access_time);
    ret.push_back(value);
  }
  return ret;
}

}  // namespace

class LruStorageTest : public ::testing::Test {};

TEST_F(LruStorageTest, LruStorageTest) {
  constexpr int kSize[] = {10, 100, 1000, 10000};
  for (int i = 0; i < std::size(kSize); ++i) {
    TempFile file(testing::MakeTempFileOrDie());
    LruStorage::CreateStorageFile(file.path().c_str(), 4, kSize[i], kSeed);
    LruStorage storage;
    EXPECT_TRUE(storage.Open(file.path().c_str()));
    EXPECT_EQ(storage.filename(), file.path());
    EXPECT_EQ(storage.size(), kSize[i]);
    EXPECT_EQ(storage.value_size(), 4);
    EXPECT_EQ(storage.seed(), kSeed);
    RunTest(&storage, kSize[i]);
  }
}

struct Entry {
  uint64_t key;
  uint32_t last_access_time;
  std::string value;
};

TEST_F(LruStorageTest, ReadWriteTest) {
  constexpr int kSize[] = {10, 100, 1000, 10000};
  absl::BitGen gen;

  for (int i = 0; i < std::size(kSize); ++i) {
    TempFile file(testing::MakeTempFileOrDie());
    LruStorage::CreateStorageFile(file.path().c_str(), 4, kSize[i], kSeed);
    LruStorage storage;
    EXPECT_TRUE(storage.Open(file.path().c_str()));
    EXPECT_EQ(storage.filename(), file.path());
    EXPECT_EQ(storage.size(), kSize[i]);
    EXPECT_EQ(storage.value_size(), 4);
    EXPECT_EQ(storage.seed(), kSeed);

    std::vector<Entry> entries;

    const size_t size = kSize[i];
    for (int j = 0; j < size; ++j) {
      Entry entry;
      entry.key = absl::Uniform<uint64_t>(gen);
      const int n = absl::Uniform(gen, 0, std::numeric_limits<int>::max());
      entry.value.assign(reinterpret_cast<const char*>(&n), 4);
      entry.last_access_time = absl::Uniform(gen, 0, 100000);
      entries.push_back(entry);
      storage.Write(j, entry.key, entry.value, entry.last_access_time);
    }

    for (int j = 0; j < size; ++j) {
      uint64_t key;
      std::string value;
      uint32_t last_access_time;
      storage.Read(j, &key, &value, &last_access_time);
      EXPECT_EQ(key, entries[j].key);
      EXPECT_EQ(value, entries[j].value);
      EXPECT_EQ(last_access_time, entries[j].last_access_time);
    }
  }
}

TEST_F(LruStorageTest, Merge) {
  TempFile file1(testing::MakeTempFileOrDie());
  TempFile file2(testing::MakeTempFileOrDie());

  // Can merge
  {
    LruStorage::CreateStorageFile(file1.path().c_str(), 4, 100, kSeed);
    LruStorage::CreateStorageFile(file2.path().c_str(), 4, 100, kSeed);
    LruStorage storage;
    EXPECT_TRUE(storage.Open(file1.path().c_str()));
    EXPECT_TRUE(storage.Merge(file2.path().c_str()));
  }

  // different entry size
  {
    LruStorage::CreateStorageFile(file1.path().c_str(), 4, 100, kSeed);
    LruStorage::CreateStorageFile(file2.path().c_str(), 4, 200, kSeed);
    LruStorage storage;
    EXPECT_TRUE(storage.Open(file1.path().c_str()));
    EXPECT_TRUE(storage.Merge(file2.path().c_str()));
  }

  // seed is different
  {
    LruStorage::CreateStorageFile(file1.path().c_str(), 4, 100, kSeed);
    LruStorage::CreateStorageFile(file2.path().c_str(), 4, 200, 0x76fee);
    LruStorage storage;
    EXPECT_TRUE(storage.Open(file1.path().c_str()));
    EXPECT_FALSE(storage.Merge(file2.path().c_str()));
  }

  // value size is different
  {
    LruStorage::CreateStorageFile(file1.path().c_str(), 4, 100, kSeed);
    LruStorage::CreateStorageFile(file2.path().c_str(), 8, 200, kSeed);
    LruStorage storage;
    EXPECT_TRUE(storage.Open(file1.path().c_str()));
    EXPECT_FALSE(storage.Merge(file2.path().c_str()));
  }

  {
    // Need to mock clock because old entries are removed on Open.  The maximum
    // timestamp set below is 50, so set the current time to 100.
    ScopedClockMock clock(absl::FromUnixSeconds(100));

    LruStorage::CreateStorageFile(file1.path().c_str(), 4, 8, kSeed);
    LruStorage::CreateStorageFile(file2.path().c_str(), 4, 4, kSeed);
    LruStorage storage1;
    EXPECT_TRUE(storage1.Open(file1.path().c_str()));
    storage1.Write(0, 0, "test", 1);
    storage1.Write(1, 1, "test", 10);
    storage1.Write(2, 2, "test", 20);
    storage1.Write(3, 3, "test", 30);

    LruStorage storage2;
    EXPECT_TRUE(storage2.Open(file2.path().c_str()));
    storage2.Write(0, 4, "test", 2);
    storage2.Write(1, 5, "test", 50);

    EXPECT_TRUE(storage1.Merge(storage2));

    uint64_t fp;
    std::string value;
    uint32_t last_access_time;

    storage1.Read(0, &fp, &value, &last_access_time);
    EXPECT_EQ(fp, 5);
    EXPECT_EQ(last_access_time, 50);

    storage1.Read(1, &fp, &value, &last_access_time);
    EXPECT_EQ(fp, 3);
    EXPECT_EQ(last_access_time, 30);

    storage1.Read(2, &fp, &value, &last_access_time);
    EXPECT_EQ(fp, 2);
    EXPECT_EQ(last_access_time, 20);

    storage1.Read(3, &fp, &value, &last_access_time);
    EXPECT_EQ(fp, 1);
    EXPECT_EQ(last_access_time, 10);
  }

  // same FP
  {
    // Need to mock clock because old entries are removed on Open.  The maximum
    // timestamp set below is 50, so set the current time to 100.
    ScopedClockMock clock(absl::FromUnixSeconds(100));

    LruStorage::CreateStorageFile(file1.path().c_str(), 4, 8, kSeed);
    LruStorage::CreateStorageFile(file2.path().c_str(), 4, 4, kSeed);
    LruStorage storage1;
    EXPECT_TRUE(storage1.Open(file1.path().c_str()));
    storage1.Write(0, 0, "test", 0);
    storage1.Write(1, 1, "test", 10);
    storage1.Write(2, 2, "test", 20);
    storage1.Write(3, 3, "test", 30);

    LruStorage storage2;
    EXPECT_TRUE(storage2.Open(file2.path().c_str()));
    storage2.Write(0, 2, "new1", 0);
    storage2.Write(1, 3, "new2", 50);

    EXPECT_TRUE(storage1.Merge(storage2));

    uint64_t fp;
    std::string value;
    uint32_t last_access_time;

    storage1.Read(0, &fp, &value, &last_access_time);
    EXPECT_EQ(fp, 3);
    EXPECT_EQ(last_access_time, 50);
    EXPECT_EQ(value, "new2");

    storage1.Read(1, &fp, &value, &last_access_time);
    EXPECT_EQ(fp, 2);
    EXPECT_EQ(last_access_time, 20);
    EXPECT_EQ(value, "test");

    storage1.Read(2, &fp, &value, &last_access_time);
    EXPECT_EQ(fp, 1);
    EXPECT_EQ(last_access_time, 10);

    storage1.Read(3, &fp, &value, &last_access_time);
    EXPECT_EQ(fp, 0);
    EXPECT_EQ(last_access_time, 0);
  }
}

TEST_F(LruStorageTest, InvalidFileOpenTest) {
  LruStorage storage;
  EXPECT_FALSE(storage.Insert("test", nullptr));

  TempFile file(testing::MakeTempFileOrDie());

  // cannot open
  EXPECT_FALSE(storage.Open(file.path().c_str()));
  EXPECT_FALSE(storage.Insert("test", nullptr));
}

TEST_F(LruStorageTest, OpenOrCreateTest) {
  TempFile file(testing::MakeTempFileOrDie());
  ASSERT_OK(FileUtil::SetContents(file.path(), "test"));

  {
    LruStorage storage;
    EXPECT_FALSE(storage.Open(file.path().c_str()))
        << "Corrupted file should be detected as an error.";
  }

  {
    LruStorage storage;
    EXPECT_TRUE(storage.OpenOrCreate(file.path().c_str(), 4, 10, kSeed))
        << "Corrupted file should be replaced with new one.";
    uint32_t v = 823;
    storage.Insert("test", reinterpret_cast<const char*>(&v));
    const uint32_t* result =
        reinterpret_cast<const uint32_t*>(storage.Lookup("test"));
    CHECK_EQ(v, *result);
  }
}

TEST_F(LruStorageTest, Delete) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));
  clock->AutoAdvance(absl::Seconds(1));

  constexpr size_t kValueSize = 4;
  constexpr size_t kNumElements = 4;
  LruStorage storage;
  TempFile file(testing::MakeTempFileOrDie());
  ASSERT_TRUE(storage.OpenOrCreate(file.path().c_str(), kValueSize,
                                   kNumElements, kSeed));

  EXPECT_TRUE(storage.Delete("nothing to delete"));

  struct {
    const char* key;
    const char* value;
  } kElements[kNumElements] = {
      // Elements are inserted into the storage in this order.
      {"0000", "aaaa"},
      {"1111", "bbbb"},
      {"2222", "cccc"},
      {"3333", "dddd"},
  };
  for (const auto& kv : kElements) {
    EXPECT_TRUE(storage.Insert(kv.key, kv.value));
  }

  // Test the case where the element to be deleted is at the last slot of
  // mmapped region.
  EXPECT_TRUE(storage.Delete("3333"));
  std::vector<std::string> expected = {"aaaa", "bbbb", "cccc"};
  EXPECT_EQ(GetValuesInStorageOrder(storage), expected);
  EXPECT_EQ(storage.used_size(), 3);
  EXPECT_EQ(storage.LookupAsString("0000"), "aaaa");
  EXPECT_EQ(storage.LookupAsString("1111"), "bbbb");
  EXPECT_EQ(storage.LookupAsString("2222"), "cccc");
  EXPECT_TRUE(storage.Lookup("3333") == nullptr);

  // Remove the element ("1111", "bbbb") in the middle.  The current
  // last element, ("2222", "cccc") should be moved to keep contiguity.
  EXPECT_TRUE(storage.Delete("1111"));
  EXPECT_EQ(storage.used_size(), 2);
  EXPECT_EQ(storage.LookupAsString("0000"), "aaaa");
  EXPECT_EQ(storage.LookupAsString("2222"), "cccc");
  EXPECT_TRUE(storage.Lookup("1111") == nullptr);
  EXPECT_TRUE(storage.Lookup("3333") == nullptr);
  expected = {"aaaa", "cccc"};
  EXPECT_EQ(GetValuesInStorageOrder(storage), expected);

  // Insert a new element ("4444", "eeee").
  EXPECT_TRUE(storage.Insert("4444", "eeee"));
  EXPECT_EQ(storage.used_size(), 3);
  EXPECT_EQ(storage.LookupAsString("0000"), "aaaa");
  EXPECT_EQ(storage.LookupAsString("2222"), "cccc");
  EXPECT_EQ(storage.LookupAsString("4444"), "eeee");
  EXPECT_TRUE(storage.Lookup("1111") == nullptr);
  EXPECT_TRUE(storage.Lookup("3333") == nullptr);
  expected = {"aaaa", "cccc", "eeee"};
  EXPECT_EQ(GetValuesInStorageOrder(storage), expected);

  // Remove the beginning ("0000", "aaaa").
  EXPECT_TRUE(storage.Delete("0000"));
  EXPECT_EQ(storage.used_size(), 2);
  EXPECT_EQ(storage.LookupAsString("2222"), "cccc");
  EXPECT_EQ(storage.LookupAsString("4444"), "eeee");
  EXPECT_TRUE(storage.Lookup("0000") == nullptr);
  EXPECT_TRUE(storage.Lookup("1111") == nullptr);
  EXPECT_TRUE(storage.Lookup("3333") == nullptr);
  expected = {"eeee", "cccc"};  // "eeee" was moved to the position of "aaaa".
  EXPECT_EQ(GetValuesInStorageOrder(storage), expected);

  // Remove ("4444", "eeee")
  EXPECT_TRUE(storage.Delete("4444"));
  EXPECT_EQ(storage.used_size(), 1);
  EXPECT_TRUE(storage.Lookup("0000") == nullptr);
  EXPECT_TRUE(storage.Lookup("1111") == nullptr);
  EXPECT_TRUE(storage.Lookup("3333") == nullptr);
  EXPECT_TRUE(storage.Lookup("4444") == nullptr);
  expected = {"cccc"};
  EXPECT_EQ(GetValuesInStorageOrder(storage), expected);

  EXPECT_TRUE(storage.Delete("2222"));
  EXPECT_EQ(storage.used_size(), 0);
  EXPECT_TRUE(storage.Lookup("0000") == nullptr);
  EXPECT_TRUE(storage.Lookup("1111") == nullptr);
  EXPECT_TRUE(storage.Lookup("2222") == nullptr);
  EXPECT_TRUE(storage.Lookup("3333") == nullptr);
  EXPECT_TRUE(storage.Lookup("4444") == nullptr);
}

TEST_F(LruStorageTest, DeleteElementsBefore) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  constexpr size_t kValueSize = 4;
  constexpr size_t kNumElements = 4;
  LruStorage storage;
  TempFile file(testing::MakeTempFileOrDie());
  ASSERT_TRUE(storage.OpenOrCreate(file.path().c_str(), kValueSize,
                                   kNumElements, kSeed));

  // Auto advance clock after opening the file; otherwise OpenOrCreate()
  // advances the clock.
  clock->AutoAdvance(absl::Seconds(1));

  struct {
    const char* key;
    const char* value;
  } kElements[kNumElements] = {
      // Elements are inserted into the storage in this order.  The above clock
      // mock gives timestamps 1 to 4.
      {"1111", "aaaa"},  // Timestamp 1
      {"2222", "bbbb"},  // Timestamp 2
      {"3333", "cccc"},  // Timestamp 3
      {"4444", "dddd"},  // Timestamp 4
  };
  for (const auto& kv : kElements) {
    storage.Insert(kv.key, kv.value);
  }

  std::vector<std::string> values;
  storage.GetAllValues(&values);
  const std::vector<std::string> kExpectedAfterInsert = {
      "dddd",
      "cccc",
      "bbbb",
      "aaaa",
  };
  EXPECT_EQ(values, kExpectedAfterInsert);

  // Should remove "1111" and "2222".
  EXPECT_EQ(storage.DeleteElementsBefore(3), 2);

  values.clear();
  storage.GetAllValues(&values);
  const std::vector<std::string> kExpectedAfterDelete = {
      "dddd",
      "cccc",
  };
  EXPECT_EQ(values, kExpectedAfterDelete);
}

TEST_F(LruStorageTest, DeleteElementsUntouchedFor62Days) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  constexpr size_t kValueSize = 4;
  constexpr size_t kNumElements = 4;
  LruStorage storage;
  TempFile file(testing::MakeTempFileOrDie());
  ASSERT_TRUE(storage.OpenOrCreate(file.path().c_str(), kValueSize,
                                   kNumElements, kSeed));

  // Auto advance clock after opening the file; otherwise OpenOrCreate()
  // advances the clock.
  clock->Advance(absl::Seconds(1));

  storage.Insert("1111", "aaaa");
  storage.Insert("2222", "bbbb");

  // Advance clock for 63 days.
  clock->Advance(absl::Hours(63 * 24));

  // Insert newer elements.
  storage.Insert("3333", "cccc");
  storage.Insert("4444", "dddd");

  EXPECT_EQ(storage.DeleteElementsUntouchedFor62Days(), 2);

  std::vector<std::string> values;
  storage.GetAllValues(&values);
  const std::vector<std::string> kExpectedAfterDelete = {
      "dddd",
      "cccc",
  };
  EXPECT_EQ(values, kExpectedAfterDelete);
}

TEST_F(LruStorageTest, OldDataAreNotLookedUp) {
  ScopedClockMock clock(absl::FromUnixSeconds(1));

  constexpr size_t kValueSize = 4;
  constexpr size_t kNumElements = 4;
  LruStorage storage;
  TempFile file(testing::MakeTempFileOrDie());
  ASSERT_TRUE(storage.OpenOrCreate(file.path().c_str(), kValueSize,
                                   kNumElements, kSeed));

  EXPECT_TRUE(storage.Insert("1111", "aaaa"));
  EXPECT_TRUE(storage.Insert("2222", "bbbb"));

  // The two elements can be looked up as they are still not 62-day old.
  EXPECT_EQ(storage.LookupAsString("1111"), "aaaa");
  EXPECT_EQ(storage.LookupAsString("2222"), "bbbb");
  EXPECT_TRUE(storage.Touch("1111"));
  EXPECT_TRUE(storage.Touch("2222"));

  // Advance clock for 63 days.
  clock->Advance(absl::Hours(63 * 24));

  // Insert new elements.
  EXPECT_TRUE(storage.Insert("3333", "cccc"));
  EXPECT_TRUE(storage.Insert("4444", "dddd"));

  // The old two cannot be looked up.
  EXPECT_TRUE(storage.Lookup("1111") == nullptr);
  EXPECT_TRUE(storage.Lookup("2222") == nullptr);
  EXPECT_FALSE(storage.Touch("1111"));
  EXPECT_FALSE(storage.Touch("2222"));

  // But the new ones are accessible.
  EXPECT_EQ(storage.LookupAsString("3333"), "cccc");
  EXPECT_EQ(storage.LookupAsString("4444"), "dddd");
  EXPECT_TRUE(storage.Touch("3333"));
  EXPECT_TRUE(storage.Touch("4444"));
}

}  // namespace storage
}  // namespace mozc
