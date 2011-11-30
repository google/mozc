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

#include "sync/user_history_adapter.h"

#include <cmath>
#include <map>
#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/freelist.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "sync/inprocess_service.h"
#include "sync/sync.pb.h"
#include "sync/syncer.h"
#include "sync/sync_util.h"
#include "sync/user_history_sync_util.h"
#include "storage/registry.h"
#include "storage/tiny_storage.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {

class TestClockTimer : public ClockTimerInterface {
 public:
  uint64 GetCurrentTime() const {
    return ++current_time_ + base_time_;
  }

  void SetBaseTime(uint64 base_time) {
    base_time_ = base_time;
  }

  TestClockTimer()
      : current_time_(0), base_time_(0) {}

 private:
  mutable uint64 current_time_;
  uint64 base_time_;
};

class UserHistoryAdapterTest : public testing::Test {
 public:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

    config::Config config = config::ConfigHandler::GetConfig();
    config::SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_user_history_sync(true);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    storage::Registry::SetStorage(NULL);
  }
};

TEST_F(UserHistoryAdapterTest, BucketSize) {
  UserHistoryAdapter adapter;
  EXPECT_EQ(1024, adapter.bucket_size());
}

TEST_F(UserHistoryAdapterTest, BucketId) {
  UserHistoryAdapter adapter;
  for (int i = 0; i < 1000; ++i) {
    const uint32 id = adapter.GetNextBucketId();
    EXPECT_GT(adapter.bucket_size(), id);
    EXPECT_LE(0, id);
  }
}

TEST_F(UserHistoryAdapterTest, UserHistoryFileName) {
  UserHistoryAdapter adapter;
  const string filename = "test";
  adapter.SetUserHistoryFileName(filename);
  EXPECT_EQ(filename, adapter.GetUserHistoryFileName());
}

TEST_F(UserHistoryAdapterTest, LastDownloadTimestamp) {
  UserHistoryAdapter adapter;
  EXPECT_TRUE(adapter.SetLastDownloadTimestamp(1234));
  EXPECT_EQ(1234, adapter.GetLastDownloadTimestamp());

  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(adapter.SetLastDownloadTimestamp(i));
    EXPECT_EQ(i, adapter.GetLastDownloadTimestamp());
  }
}

TEST_F(UserHistoryAdapterTest, SetDownloadedItems) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_history");

  UserHistoryAdapter adapter;
  adapter.SetUserHistoryFileName(filename);
  adapter.SetLastDownloadTimestamp(0);

  UserHistorySyncUtil::UserHistory expected;
  TestClockTimer clock_timer;

  {
    UserHistoryStorage local_history(filename);
    UserHistorySyncUtil::AddRandomUpdates(&local_history,
                                          &clock_timer);
    EXPECT_TRUE(local_history.Save());
    expected.CopyFrom(local_history);
  }

  FreeList<UserHistorySyncUtil::UserHistory> freelist(100);

  vector<const UserHistorySyncUtil::UserHistory *> updates;
  for (int i = 0; i < 10; ++i) {
    UserHistorySyncUtil::UserHistory *update = freelist.Alloc();
    UserHistorySyncUtil::AddRandomUpdates(update,
                                          &clock_timer);
    updates.push_back(update);
  }


  EXPECT_TRUE(UserHistorySyncUtil::MergeUpdates(updates,
                                                &expected));

  ime_sync::SyncItems items;

  for (size_t i = 0; i < updates.size(); ++i) {
    ime_sync::SyncItem *item = items.Add();
    CHECK(item);
    item->set_component(adapter.component_id());
    sync::UserHistoryKey *key =
        item->mutable_key()->MutableExtension(
            sync::UserHistoryKey::ext);
    sync::UserHistoryValue *value =
        item->mutable_value()->MutableExtension(
            sync::UserHistoryValue::ext);
    CHECK(key);
    CHECK(value);
    key->set_bucket_id(i);
    value->mutable_user_history()->CopyFrom(*(updates[i]));
  }

  EXPECT_TRUE(adapter.SetDownloadedItems(items));

  UserHistoryStorage local_history(filename);
  EXPECT_TRUE(local_history.Load());
  EXPECT_EQ(expected.DebugString(), local_history.DebugString());
}

TEST_F(UserHistoryAdapterTest, GetItemsToUpload) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_history");

  UserHistoryAdapter adapter;
  adapter.SetUserHistoryFileName(filename);
  TestClockTimer clock_timer;

  UserHistoryStorage local_history(filename);
  UserHistorySyncUtil::AddRandomUpdates(&local_history,
                                        &clock_timer);
  UserHistorySyncUtil::AddRandomUpdates(&local_history,
                                        &clock_timer);
  UserHistorySyncUtil::AddRandomUpdates(&local_history,
                                        &clock_timer);
  EXPECT_GT(local_history.entries_size(), 0);
  EXPECT_TRUE(local_history.Save());

  // No update.
  {
    adapter.SetLastDownloadTimestamp(kuint64max);
    ime_sync::SyncItems items;
    EXPECT_TRUE(adapter.GetItemsToUpload(&items));
    EXPECT_EQ(0, items.size());
  }

  // upload all data.
  {
    adapter.SetLastDownloadTimestamp(0);
    ime_sync::SyncItems items;
    EXPECT_TRUE(adapter.GetItemsToUpload(&items));
    EXPECT_EQ(1, items.size());

    const ime_sync::SyncItem &item = items.Get(0);

    EXPECT_EQ(adapter.component_id(), item.component());
    const sync::UserHistoryValue &value =
        item.value().GetExtension(sync::UserHistoryValue::ext);
    EXPECT_EQ(value.user_history().DebugString(),
              local_history.DebugString());
  }

  // updates which are newer than last_access_time.
  {
    const uint32 last_access_time =
        local_history.entries(0).last_access_time();
    UserHistorySyncUtil::UserHistory expected;
    UserHistorySyncUtil::CreateUpdate(local_history, last_access_time,
                                      &expected);
    adapter.SetLastDownloadTimestamp(last_access_time);
    ime_sync::SyncItems items;
    EXPECT_TRUE(adapter.GetItemsToUpload(&items));
    EXPECT_EQ(1, items.size());

    const ime_sync::SyncItem &item = items.Get(0);

    EXPECT_EQ(adapter.component_id(), item.component());
    const sync::UserHistoryValue &value =
        item.value().GetExtension(sync::UserHistoryValue::ext);
    EXPECT_EQ(value.user_history().DebugString(),
              expected.DebugString());
  }
}

TEST_F(UserHistoryAdapterTest, MarkUploaded) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_history");

  UserHistoryAdapter adapter;
  adapter.SetUserHistoryFileName(filename);

  ime_sync::SyncItem item;
  item.set_component(adapter.component_id());
  sync::UserHistoryKey *key =
      item.mutable_key()->MutableExtension(
          sync::UserHistoryKey::ext);
  sync::UserHistoryValue *value =
      item.mutable_value()->MutableExtension(
          sync::UserHistoryValue::ext);
  CHECK(key);
  CHECK(value);
  key->set_bucket_id(0);

  // last_access_time is not updated.
  adapter.SetLastDownloadTimestamp(1234);
  adapter.MarkUploaded(item, false);
  EXPECT_EQ(1234, adapter.GetLastDownloadTimestamp());

  // last_access_time is updated.
  const uint64 synced_time = static_cast<uint64>(time(NULL));
  ime_sync::SyncItems items;
  adapter.GetItemsToUpload(&items);
  adapter.MarkUploaded(item, true);
  const int diff = abs(static_cast<int>(synced_time -
                                        adapter.GetLastDownloadTimestamp()));
  EXPECT_LE(diff, 2);
}

namespace {
// On memory storage for dependency injection.
// This class might be useful for other test cases.
// TODO(taku): Move this class in storage/ directory.
class MemoryStorage : public storage::StorageInterface {
 public:
  bool Open(const string &filename) {
    return true;
  }

  bool OpenOrCreate(const string &filename) {
    return true;
  }

  bool Sync() {
    return true;
  }

  bool Lookup(const string &key, string *value) const {
    CHECK(value);
    map<string, string>::const_iterator it = data_.find(key);
    if (it == data_.end()) {
      return false;
    }
    *value = it->second;
    return true;
  }

  bool Insert(const string &key, const string &value) {
    data_[key] = value;
    return true;
  }

  bool Erase(const string &key) {
    map<string, string>::iterator it = data_.find(key);
    if (it != data_.end()) {
      data_.erase(it);
      return true;
    }
    return false;
  }

  bool Clear() {
    data_.clear();
    return true;
  }

  size_t Size() const {
    return data_.size();
  }

  MemoryStorage() {}
  virtual ~MemoryStorage() {}

 private:
  map<string, string> data_;
};
}  // namespace

TEST_F(UserHistoryAdapterTest, RealScenarioTest) {
  const int kClientsSize = 10;

  // Only exist one service, which emulates the sync server.
  InprocessService service;

  vector<string> filenames;
  vector<Syncer *> syncers;
  vector<UserHistoryAdapter *> adapters;
  vector<storage::StorageInterface *> memory_storages;
  TestClockTimer clock_timer;

  // create 10 clients
  for (int i = 0; i < kClientsSize; ++i) {
    Syncer *syncer = new Syncer(&service);
    CHECK(syncer);
    storage::StorageInterface *memory_storage = new MemoryStorage;
    CHECK(memory_storage);

    UserHistoryAdapter *adapter = new UserHistoryAdapter;
    CHECK(adapter);
    const string filename =
        Util::JoinPath(FLAGS_test_tmpdir,
                       "client." + Util::SimpleItoa(i));
    adapter->SetUserHistoryFileName(filename);
    adapter->SetClockTimerInterface(&clock_timer);
    syncer->RegisterAdapter(adapter);
    syncers.push_back(syncer);
    adapters.push_back(adapter);
    memory_storages.push_back(memory_storage);
    filenames.push_back(filename);
  }

  CHECK_EQ(filenames.size(), adapters.size());
  CHECK_EQ(syncers.size(), adapters.size());

  bool reload_required = false;

  for (int n = 0; n < 100; ++n) {
    // User modifies dictionary on |client_id|-th PC.
    const int client_id = Util::Random(kClientsSize);
    CHECK(client_id >= 0 && client_id < kClientsSize);
    UserHistoryStorage history(filenames[client_id]);
    history.Load();
    clock_timer.SetBaseTime(1000 * n + 100 * client_id);
    UserHistorySyncUtil::AddRandomUpdates(&history,
                                          &clock_timer);
    EXPECT_TRUE(history.Save());

    for (int i = 0; i < kClientsSize; ++i) {
      // Switch internal storage. A little tricky.
      storage::Registry::SetStorage(memory_storages[i]);
      syncers[i]->Sync(&reload_required);
    }
  }

  // Do sync on every client just in case.
  for (int i = 0; i < kClientsSize; ++i) {
    storage::Registry::SetStorage(memory_storages[i]);
    syncers[i]->Sync(&reload_required);
  }

  // Check all clients have the similear entries.
  UserHistoryStorage target(filenames[0]);
  target.Load();
  int same_history_num = 0;
  for (int i = 1; i < kClientsSize; ++i) {
    UserHistoryStorage storage(filenames[i]);
    storage.Load();
    EXPECT_GT(storage.entries_size(), 0);
    if (target.entries(0).DebugString() ==
        storage.entries(0).DebugString()) {
      ++same_history_num;
    }
  }

  // 7/9 entries should have the same top entries.
  // TODO(taku): Compare all entities.
  EXPECT_GE(same_history_num, 7);

  for (int i = 0; i < kClientsSize; ++i) {
    Util::Unlink(filenames[i]);
    delete syncers[i];
    delete adapters[i];
    delete memory_storages[i];
  }
}
}  // sync
}  // mozc
