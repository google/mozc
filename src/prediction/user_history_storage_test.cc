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

#include "prediction/user_history_storage.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/file/temp_dir.h"
#include "base/file_util.h"
#include "base/thread.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc::prediction {
namespace {

using Entry = UserHistoryStorage::Entry;

Entry MakeEntry(int i) {
  Entry entry;
  entry.set_key(absl::StrCat("key", i));
  entry.set_value(absl::StrCat("value", i));
  return entry;
}

class UserHistoryStorageTest : public testing::TestWithTempUserProfile {};

TEST_F(UserHistoryStorageTest, BasicTest) {
  UserHistoryStorage storage;

  EXPECT_TRUE(storage.IsEmpty());
  EXPECT_FALSE(storage.Head());

  std::vector<Entry> entries;

  // Insert new entries.
  for (int i = 0; i < 100; ++i) {
    Entry entry = MakeEntry(i);
    entries.push_back(entry);
    storage.Insert(std::move(entry));
  }

  EXPECT_FALSE(storage.IsEmpty());

  // Serialization.
  storage.AsyncSave();
  EXPECT_TRUE(storage.IsSyncerRunning());
  storage.Wait();
  EXPECT_FALSE(storage.IsSyncerRunning());

  storage.AsyncLoad();
  EXPECT_TRUE(storage.IsSyncerRunning());
  storage.Wait();
  EXPECT_FALSE(storage.IsSyncerRunning());
  EXPECT_FALSE(storage.IsEmpty());

  // immutable lookup. LRU order is not updated.
  for (const Entry& entry : entries) {
    const uint64_t fp = UserHistoryStorage::EntryFingerprint(entry);
    EXPECT_TRUE(storage.Contains(fp));
    const auto snapshot = storage.Lookup(fp);
    EXPECT_TRUE(snapshot);
    EXPECT_EQ(snapshot->key(), entry.key());
    EXPECT_EQ(snapshot->value(), entry.value());
  }

  // mutable lookup. LRU order is not updated.
  for (const Entry& entry : entries) {
    const uint64_t fp = UserHistoryStorage::EntryFingerprint(entry);
    EXPECT_TRUE(storage.Contains(fp));
    auto snapshot = storage.MutableLookup(fp);
    EXPECT_TRUE(snapshot);
    EXPECT_EQ(snapshot->key(), entry.key());
    EXPECT_EQ(snapshot->value(), entry.value());
    snapshot->set_removed(true);

    // const_snapshot refers to the same entry.
    const auto const_snapshot = storage.Lookup(fp);
    EXPECT_EQ(const_snapshot.get(), snapshot.get());
    EXPECT_TRUE(const_snapshot->removed());
    snapshot->set_removed(false);
    EXPECT_FALSE(const_snapshot->removed());
  }

  // Head
  {
    auto head = storage.Head();
    EXPECT_TRUE(head);
    EXPECT_EQ(head->key(), "key99");
    EXPECT_EQ(head->value(), "value99");
  }

  // ForEach.
  {
    auto it = entries.rbegin();
    storage.ForEach([&](uint64_t fp, const Entry& entry) {
      EXPECT_TRUE(storage.Contains(fp));
      auto snapshot = storage.MutableLookup(fp);
      EXPECT_EQ(snapshot->key(), it->key());
      EXPECT_EQ(snapshot->value(), it->value());
      ++it;
      return true;
    });

    int num_iterations = 0;
    storage.ForEach([&](uint64_t fp, const Entry& entry) {
      EXPECT_TRUE(storage.Contains(fp));
      return ++num_iterations < 10;
    });
    EXPECT_EQ(num_iterations, 10);
  }

  // Insert new entry.
  {
    const Entry entry = MakeEntry(10000);
    storage.Insert(entry);
    auto head = storage.Head();
    EXPECT_EQ(entry.key(), head->key());
    EXPECT_EQ(entry.value(), head->value());
  }

  // Delete entries.
  {
    std::vector<uint64_t> fps;
    for (int i = 0; i < 50; ++i) {
      Entry entry = MakeEntry(i);
      fps.emplace_back(UserHistoryStorage::EntryFingerprint(entry));
    }

    storage.Erase(fps);

    for (uint64_t fp : fps) {
      EXPECT_FALSE(storage.Contains(fp));
    }

    for (int i = 50; i < 100; ++i) {
      const uint64_t fp = UserHistoryStorage::EntryFingerprint(MakeEntry(i));
      EXPECT_TRUE(storage.Contains(fp));
    }
  }

  // Clears. empty data must be serialized.
  storage.Clear();
  EXPECT_TRUE(storage.IsEmpty());

  storage.AsyncLoad();
  storage.Wait();
  EXPECT_TRUE(storage.IsEmpty());
}

TEST_F(UserHistoryStorageTest, MultiThreadsTest) {
  TempFile file(testing::MakeTempFileOrDie());
  UserHistoryStorage storage(file.path());

  std::vector<Thread> threads;

  constexpr int kNumThreads = 32;

  // Access the storage from multiple threads.
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(Thread([&, i] {
      for (int repeat = 0; repeat < 10; ++repeat) {
        storage.AsyncSave();

        for (int n = 0; n < 100; ++n) {
          Entry entry = MakeEntry(i * n);
          storage.Insert(std::move(entry));
        }

        for (int n = 0; n < 100; ++n) {
          Entry entry = MakeEntry(i * n);
          const uint64_t fp = UserHistoryStorage::EntryFingerprint(entry);
          if (storage.Contains(fp)) {
            const auto snapshot = storage.Lookup(fp);
            if (snapshot) {
              EXPECT_EQ(entry.key(), snapshot->key());
              EXPECT_EQ(entry.value(), snapshot->value());
            }
          }
        }
      }

      storage.ForEach([&](uint64_t fp, const Entry& entry) {
        EXPECT_TRUE(storage.Contains(fp));
        return true;
      });
    }));
  }

  for (Thread& thread : threads) {
    thread.Join();
  }
}

TEST_F(UserHistoryStorageTest, UniqueLockTest) {
  UserHistoryStorage storage;

  std::vector<Thread> threads;

  constexpr int kNumThreads = 32;

  // Access the storage from multiple threads.
  std::string buffer;  // std::string is not thread-safe
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back(Thread([&] {
      for (int n = 0; n < 1000; ++n) {
        auto lock = storage.AcquireUniqueLock();
        buffer += ' ';
      }
    }));
  }

  for (Thread& thread : threads) {
    thread.Join();
  }
}

TEST_F(UserHistoryStorageTest, SyncTest) {
  const TempFile file = testing::MakeTempFileOrDie();

  UserHistoryStorage storage(file.path());

  storage.Wait();
  storage.Insert(MakeEntry(0));
  storage.Save();

  // Removes the file
  FileUtil::UnlinkIfExists(file.path()).IgnoreError();

  storage.Save();

  // File doesn't exist as no mutable operation is executed.
  EXPECT_FALSE(FileUtil::FileExists(file.path()).ok());

  // File is created.
  storage.Insert(MakeEntry(1000));
  storage.Save();
  EXPECT_OK(FileUtil::FileExists(file.path()));

  // When cleared, file is removed.
  storage.Clear();
  EXPECT_FALSE(FileUtil::FileExists(file.path()).ok());
}

TEST_F(UserHistoryStorageTest, CancelTest) {
  const TempFile file = testing::MakeTempFileOrDie();

  {
    UserHistoryStorage storage(file.path());
    storage.Wait();
    for (int i = 0; i < 100; ++i) {
      Entry entry = MakeEntry(i);
      storage.Insert(std::move(entry));
    }
  }

  EXPECT_OK(FileUtil::FileExists(file.path()));

  auto check_serialized_data = [&]() {
    UserHistoryStorage storage(file.path());
    storage.Wait();
    for (int i = 0; i < 100; ++i) {
      const Entry entry = MakeEntry(i);
      EXPECT_TRUE(
          storage.Contains(UserHistoryStorage::EntryFingerprint(entry)));
    }
  };

  check_serialized_data();

  {
    // destructor is called immediately via cancel operation.
    // The file must be kept.
    UserHistoryStorage storage(file.path());
  }

  EXPECT_OK(FileUtil::FileExists(file.path()));

  check_serialized_data();
}

TEST_F(UserHistoryStorageTest, MigrateNextEntriesTest) {
  mozc::user_history_predictor::UserHistory proto;

  for (int i = 0; i < 100; ++i) {
    Entry* entry = proto.add_entries();
    *entry = MakeEntry(i);
    for (int k = i + 1; k < i + 5 && k < 100; ++k) {
      const uint32_t fp = UserHistoryStorage::FingerprintDepereated(
          absl::StrCat("key", k), absl::StrCat("value", k));
      entry->add_next_entries_deprecated()->set_entry_fp(fp);
    }
  }

  UserHistoryStorage::MigrateNextEntries(&proto);

  for (int i = 0; i < 100; ++i) {
    const auto& entry = proto.entries(i);
    EXPECT_EQ(entry.next_entries_deprecated_size(), 0);
    int s = 0;
    for (int k = i + 1; k < i + 5 && k < 100; ++k) {
      const uint64_t fp = UserHistoryStorage::Fingerprint(
          absl::StrCat("key", k), absl::StrCat("value", k));
      EXPECT_EQ(entry.next_entry_fps(s++), fp);
    }
    EXPECT_EQ(entry.next_entry_fps_size(), s);
  }
}

}  // namespace
}  // namespace mozc::prediction
