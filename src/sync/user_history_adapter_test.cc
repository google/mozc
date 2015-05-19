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

#include "sync/user_history_adapter.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>

#include "base/base.h"
#include "base/clock_mock.h"
#include "base/file_stream.h"
#include "base/freelist.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/stl_util.h"
#include "base/testing_util.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "storage/memory_storage.h"
#include "storage/registry.h"
#include "storage/tiny_storage.h"
#include "sync/inprocess_service.h"
#include "sync/sync.pb.h"
#include "sync/sync_util.h"
#include "sync/syncer.h"
#include "sync/user_history_sync_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {

class UserHistoryAdapterTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

    clock_mock_.reset(new ClockMock(0, 0));
    clock_mock_->SetAutoPutClockForward(1, 0);
    Util::SetClockHandler(clock_mock_.get());

    config::Config config = config::ConfigHandler::GetConfig();
    config::SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_user_history_sync(true);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    storage::Registry::SetStorage(NULL);
    Util::SetClockHandler(NULL);
  }

  scoped_ptr<ClockMock> clock_mock_;
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

  {
    UserHistoryStorage local_history(filename);
    UserHistorySyncUtil::AddRandomUpdates(&local_history);
    EXPECT_TRUE(local_history.Save());
    expected.CopyFrom(local_history);
  }

  FreeList<UserHistorySyncUtil::UserHistory> freelist(100);

  vector<const UserHistorySyncUtil::UserHistory *> updates;
  for (int i = 0; i < 10; ++i) {
    UserHistorySyncUtil::UserHistory *update = freelist.Alloc();
    UserHistorySyncUtil::AddRandomUpdates(update);
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

  UserHistoryStorage local_history(filename);
  UserHistorySyncUtil::AddRandomUpdates(&local_history);
  UserHistorySyncUtil::AddRandomUpdates(&local_history);
  UserHistorySyncUtil::AddRandomUpdates(&local_history);
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
  const uint64 synced_time = Util::GetTime();
  ime_sync::SyncItems items;
  adapter.GetItemsToUpload(&items);
  adapter.MarkUploaded(item, true);
  const int diff = abs(static_cast<int>(synced_time -
                                        adapter.GetLastDownloadTimestamp()));
  EXPECT_LE(diff, 2);
}

namespace {

string DumpUserHistoryStorage(const string &fname) {
  UserHistoryStorage storage(fname);
  storage.Load();
  return storage.Utf8DebugString();
}

void FillEntryWithDefaultValueIfNotExist(UserHistorySyncUtil::Entry *entry) {
  const UserHistorySyncUtil::Entry &default_entry =
      UserHistorySyncUtil::Entry::default_instance();
  if (!entry->has_suggestion_freq()) {
    entry->set_suggestion_freq(default_entry.suggestion_freq());
  }
  if (!entry->has_conversion_freq()) {
    entry->set_conversion_freq(default_entry.conversion_freq());
  }
  if (!entry->has_last_access_time()) {
    entry->set_last_access_time(default_entry.last_access_time());
  }
  if (!entry->has_removed()) {
    entry->set_removed(default_entry.removed());
  }
}

void FillEachEntryWithDefaultValueIfNotExist(UserHistoryStorage *storage) {
  for (int i = 0; i < storage->entries_size(); ++i) {
    FillEntryWithDefaultValueIfNotExist(storage->mutable_entries(i));
  }
}

}  // namespace

TEST_F(UserHistoryAdapterTest, RealScenarioTest) {
  const int kClientsSize = 10;

  // Only exist one service, which emulates the sync server.
  InprocessService service;

  vector<string> filenames;
  vector<Syncer *> syncers;
  vector<UserHistoryAdapter *> adapters;
  vector<storage::StorageInterface *> memory_storages;

  // create 10 clients
  for (int i = 0; i < kClientsSize; ++i) {
    Syncer *syncer = new Syncer(&service);
    CHECK(syncer);
    storage::StorageInterface *memory_storage = storage::MemoryStorage::New();
    CHECK(memory_storage);

    UserHistoryAdapter *adapter = new UserHistoryAdapter;
    CHECK(adapter);
    const string filename =
        Util::JoinPath(FLAGS_test_tmpdir,
                       "client." + NumberUtil::SimpleItoa(i));
    adapter->SetUserHistoryFileName(filename);
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
    UserHistorySyncUtil::AddRandomUpdates(&history);
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

  // Check all clients have the same entries.
  UserHistoryStorage target(filenames[0]);
  target.Load();
  FillEachEntryWithDefaultValueIfNotExist(&target);
  for (int i = 1; i < kClientsSize; ++i) {
    UserHistoryStorage storage(filenames[i]);
    storage.Load();
    EXPECT_EQ(target.entries_size(), storage.entries_size());

    if (target.entries_size() == storage.entries_size()) {
      // Ensure all entries have all elements.
      FillEachEntryWithDefaultValueIfNotExist(&storage);
      EXPECT_EQ(target.DebugString(), storage.DebugString());
    }
  }

  for (int i = 0; i < kClientsSize; ++i) {
    Util::Unlink(filenames[i]);
  }
  STLDeleteElements(&syncers);
  STLDeleteElements(&adapters);
  STLDeleteElements(&memory_storages);
}

// TODO(team): In case where add/edit and clean occur at the same time on
// different clients, sync behavior is undefined because entry is sorted only in
// order of timestamp, i.e., the order of key-value and CLEAN_ALL_EVENT depends
// on sorting algorithm and initial order. Although such situation would be
// rare, we should fix the behavior of sync. Currently test is added as DISABLED
// and to be fixed.
TEST_F(UserHistoryAdapterTest, DISABLED_EditAndDeleteAtTheSameTime) {
  const int kClientsSize = 2;

  // Emulates sync server.
  InprocessService service;

  vector<string> filenames;
  vector<Syncer *> syncers;
  vector<UserHistoryAdapter *> adapters;
  vector<storage::StorageInterface *> memory_storages;

  // Create sync clients.
  for (int i = 0; i < kClientsSize; ++i) {
    Syncer *syncer = new Syncer(&service);
    CHECK(syncer);
    storage::StorageInterface *memory_storage = storage::MemoryStorage::New();
    CHECK(memory_storage);

    UserHistoryAdapter *adapter = new UserHistoryAdapter;
    CHECK(adapter);
    const string filename =
        Util::JoinPath(FLAGS_test_tmpdir,
                       "client." + NumberUtil::SimpleItoa(i));
    adapter->SetUserHistoryFileName(filename);
    syncer->RegisterAdapter(adapter);
    syncers.push_back(syncer);
    adapters.push_back(adapter);
    memory_storages.push_back(memory_storage);
    filenames.push_back(filename);
  }
  ASSERT_EQ(filenames.size(), adapters.size());
  ASSERT_EQ(syncers.size(), adapters.size());

  // Add a history entry as initial state.
  {
    const uint64 kTime0 = 0;
    UserHistoryStorage history(filenames[0]);
    history.Load();
    UserHistorySyncUtil::Entry *entry = history.add_entries();
    entry->set_key("start");
    entry->set_value("start");
    entry->set_conversion_freq(10);
    entry->set_suggestion_freq(20);
    entry->set_last_access_time(kTime0);
    EXPECT_TRUE(history.Save());

    // Do sync on every client to share the initial state.
    clock_mock_->SetTime(kTime0, 0);
    for (int i = 0; i < kClientsSize; ++i) {
      storage::Registry::SetStorage(memory_storages[i]);
      bool reload_required = false;
      syncers[i]->Sync(&reload_required);
      VLOG(1) << "UserHistoryStorage of " << i << ": "
              << DumpUserHistoryStorage(filenames[i]);
    }
  }

  const int kNumEntriesToAdd = 10;
  {
    // Set the clock forward 1000 time units to make it easy to understand sync
    // behavior by VLOG dump.
    const uint64 kTime1 = Util::GetTime() + 1000;
    // Change on client 0
    {
      // Adds new entries at time 1
      UserHistoryStorage history(filenames[0]);
      history.Load();
      for (int i = 0; i < kNumEntriesToAdd; ++i) {
        UserHistorySyncUtil::Entry *entry = history.add_entries();
        entry->set_key(Util::StringPrintf("newentry%d", i));
        entry->set_value(Util::StringPrintf("newentry%d", i));
        entry->set_conversion_freq(30);
        entry->set_suggestion_freq(40);
        entry->set_last_access_time(kTime1);
      }

      // Modifies entry "start" at time 1.
      for (int i = 0; i < history.entries_size(); ++i) {
        if (history.entries(i).key() != "start") {
          continue;
        }
        UserHistorySyncUtil::Entry *entry = history.mutable_entries(i);
        entry->set_conversion_freq(1000);
        entry->set_last_access_time(kTime1);
        break;
      }
      EXPECT_TRUE(history.Save());
    }

    // Client 1 clears history.
    {
      UserHistoryStorage history(filenames[1]);
      history.Load();
      UserHistorySyncUtil::Entry *entry = history.add_entries();
      entry->set_entry_type(UserHistorySyncUtil::Entry::CLEAN_ALL_EVENT);
      entry->set_last_access_time(kTime1);
      EXPECT_TRUE(history.Save());
    }
  }

  {
    const uint64 kTime2 = Util::GetTime() + 1000;
    clock_mock_->SetTime(kTime2, 0);

    // Do sync on every client in random order.
    vector<int> order(kClientsSize);
    for (int i = 0; i < kClientsSize; ++i) {
      order[i] = i;
    }
    random_shuffle(order.begin(), order.end(), Util::Random);

    for (int i = 0; i < kClientsSize; ++i) {
      const int client_id = order[i];
      storage::Registry::SetStorage(memory_storages[client_id]);
      bool reload_required = false;
      syncers[client_id]->Sync(&reload_required);
    }
  }

  {
    const uint64 kTime3 = Util::GetTime() + 1000;
    clock_mock_->SetTime(kTime3, 0);

    // Do sync on every client to share the final server state.
    for (int i = 0; i < kClientsSize; ++i) {
      storage::Registry::SetStorage(memory_storages[i]);
      bool reload_required = false;
      syncers[i]->Sync(&reload_required);
      VLOG(1) << "Final state of client " << i << ": "
              << DumpUserHistoryStorage(filenames[i]);
    }
  }

  // Check if every client shares the same state.
  UserHistoryStorage storage_0(filenames[0]);
  storage_0.Load();
  FillEachEntryWithDefaultValueIfNotExist(&storage_0);
  for (int i = 1; i < kClientsSize; ++i) {
    UserHistoryStorage storage_i(filenames[i]);
    storage_i.Load();
    FillEachEntryWithDefaultValueIfNotExist(&storage_i);
    EXPECT_PROTO_EQ(storage_0, storage_i);
  }

  // Check if the initial entry "start" modified at time kTime1 and entries
  // added at time kTime1 exist (i.e., if entries are added/modified locally on
  // a client and local entries on another client are cleared at exactly the
  // same time, we may want to keep those entries.
  for (int i = 0; i < kClientsSize; ++i) {
    UserHistoryStorage storage(filenames[i]);
    storage.Load();
    set<string> keys;
    for (int j = 0; j < storage.entries_size(); ++j) {
      keys.insert(storage.entries(i).key());
    }
    EXPECT_TRUE(keys.find("start") != keys.end())
        << "start is not in the storage of client " << i;
    for (int j = 0; j < kNumEntriesToAdd; ++j) {
      const string key = Util::StringPrintf("newentry%d", j);
      EXPECT_TRUE(keys.find(key) != keys.end())
          << key << " is not in the storage of client " << i;
    }
  }

  for (int i = 0; i < kClientsSize; ++i) {
    Util::Unlink(filenames[i]);
  }
  STLDeleteElements(&syncers);
  STLDeleteElements(&adapters);
  STLDeleteElements(&memory_storages);
}

}  // namespace sync
}  // namespace mozc
