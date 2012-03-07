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

#include "sync/user_dictionary_adapter.h"

#include <map>
#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "sync/inprocess_service.h"
#include "sync/sync.pb.h"
#include "sync/syncer.h"
#include "sync/sync_util.h"
#include "sync/user_dictionary_sync_util.h"
#include "storage/registry.h"
#include "storage/tiny_storage.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {

class UserDictionaryAdapterTest : public testing::Test {
 public:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

    config::Config config = config::ConfigHandler::GetConfig();
    config::SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_user_dictionary_sync(true);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    storage::Registry::SetStorage(NULL);
  }
};

TEST_F(UserDictionaryAdapterTest, BucketSize) {
  UserDictionaryAdapter adapter;
  EXPECT_EQ(256, adapter.bucket_size());
}

TEST_F(UserDictionaryAdapterTest, BucketId) {
  UserDictionaryAdapter adapter;
  EXPECT_TRUE(adapter.SetBucketId(0));
  EXPECT_EQ(1, adapter.GetNextBucketId());

  EXPECT_TRUE(adapter.SetBucketId(100));
  EXPECT_EQ(101, adapter.GetNextBucketId());

  EXPECT_TRUE(adapter.SetBucketId(adapter.bucket_size() - 1));
  EXPECT_EQ(0, adapter.GetNextBucketId());

  // too big.
  EXPECT_TRUE(adapter.SetBucketId(10000));
  EXPECT_EQ(0, adapter.GetNextBucketId());
}

TEST_F(UserDictionaryAdapterTest, UserDictionaryFileName) {
  UserDictionaryAdapter adapter;
  const string filename = "test";
  adapter.SetUserDictionaryFileName(filename);
  EXPECT_EQ(filename, adapter.GetUserDictionaryFileName());
  EXPECT_NE(string::npos,
            adapter.GetLastSyncedUserDictionaryFileName().find(filename));
  EXPECT_NE(string::npos,
            adapter.GetTempLastSyncedUserDictionaryFileName().find(filename));
  EXPECT_NE(adapter.GetUserDictionaryFileName(),
            adapter.GetLastSyncedUserDictionaryFileName());
  EXPECT_NE(adapter.GetUserDictionaryFileName(),
            adapter.GetTempLastSyncedUserDictionaryFileName());
  EXPECT_NE(adapter.GetLastSyncedUserDictionaryFileName(),
            adapter.GetTempLastSyncedUserDictionaryFileName());
}

TEST_F(UserDictionaryAdapterTest, SetDownloadedItemsEmptyItems) {
  ime_sync::SyncItems items;
  UserDictionaryAdapter adapter;
  EXPECT_TRUE(adapter.SetDownloadedItems(items));
}

namespace {
bool AddSyncEntry(UserDictionaryStorage *storage) {
  CHECK(storage);
  storage->EnsureSyncDictionaryExists();
  for (int i = 0; i < storage->dictionaries_size(); ++i) {
    UserDictionarySyncUtil::UserDictionary *dict =
        storage->mutable_dictionaries(i);
    if (dict->syncable()) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry =
          dict->add_entries();
      DCHECK(entry);
      entry->set_key(SyncUtil::GenRandomString(5));
      entry->set_value(SyncUtil::GenRandomString(5));
      entry->set_pos(SyncUtil::GenRandomString(5));
      return true;
    }
  }
  return false;
}
}

TEST_F(UserDictionaryAdapterTest, SetDownloadedItems) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_dic");

  UserDictionaryAdapter adapter;
  adapter.SetUserDictionaryFileName(filename);
  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());

  UserDictionaryStorage prev(adapter.GetLastSyncedUserDictionaryFileName());
  prev.Load();
  AddSyncEntry(&prev);
  EXPECT_TRUE(prev.Lock());
  EXPECT_TRUE(prev.Save());
  EXPECT_TRUE(prev.UnLock());

  UserDictionaryStorage expected("");
  expected.CopyFrom(prev);
  AddSyncEntry(&expected);

  UserDictionaryStorage storage(adapter.GetUserDictionaryFileName());
  storage.CopyFrom(prev);
  EXPECT_TRUE(storage.Lock());  // keep Locking
  EXPECT_TRUE(storage.Save());

  ime_sync::SyncItems items;
  ime_sync::SyncItem *item = items.Add();
  CHECK(item);

  item->set_component(adapter.component_id());

  sync::UserDictionaryKey *key =
      item->mutable_key()->MutableExtension(
          sync::UserDictionaryKey::ext);
  sync::UserDictionaryValue *value =
      item->mutable_value()->MutableExtension(
          sync::UserDictionaryValue::ext);
  CHECK(key);
  CHECK(value);

  key->set_bucket_id(10);
  UserDictionarySyncUtil::UserDictionaryStorageBase *remote_update =
      value->mutable_user_dictionary_storage();
  CHECK(remote_update);

  // Obtain local update.
  UserDictionarySyncUtil::CreateUpdate(prev, expected,
                                       remote_update);

  // storage is locked
  EXPECT_FALSE(adapter.SetDownloadedItems(items));

  storage.UnLock();

  // storage is not locked.
  EXPECT_TRUE(adapter.SetDownloadedItems(items));

  storage.Load();
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(expected, storage));

  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());
}

TEST_F(UserDictionaryAdapterTest, SetDownloadedItemsSnapshot) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_dic");

  UserDictionaryAdapter adapter;
  adapter.SetUserDictionaryFileName(filename);
  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());

  UserDictionaryStorage prev(adapter.GetLastSyncedUserDictionaryFileName());
  EXPECT_FALSE(prev.Load());
  AddSyncEntry(&prev);
  EXPECT_TRUE(prev.Lock());
  EXPECT_TRUE(prev.Save());
  EXPECT_TRUE(prev.UnLock());

  UserDictionaryStorage expected("");
  expected.EnsureSyncDictionaryExists();
  AddSyncEntry(&expected);

  UserDictionaryStorage storage(adapter.GetUserDictionaryFileName());
  storage.CopyFrom(prev);
  EXPECT_TRUE(storage.Lock());  // keep Locking
  EXPECT_TRUE(storage.Save());

  ime_sync::SyncItems items;
  ime_sync::SyncItem *item = items.Add();
  CHECK(item);

  item->set_component(adapter.component_id());

  sync::UserDictionaryKey *key =
      item->mutable_key()->MutableExtension(
          sync::UserDictionaryKey::ext);
  sync::UserDictionaryValue *value =
      item->mutable_value()->MutableExtension(
          sync::UserDictionaryValue::ext);
  CHECK(key);
  CHECK(value);

  key->set_bucket_id(0);
  UserDictionarySyncUtil::UserDictionaryStorageBase *remote_update =
      value->mutable_user_dictionary_storage();
  CHECK(remote_update);

  // Obtain remote update.
  UserDictionarySyncUtil::CreateSnapshot(expected, remote_update);

  // storage is locked
  EXPECT_FALSE(adapter.SetDownloadedItems(items));

  storage.UnLock();

  // storage is not locked.
  EXPECT_TRUE(adapter.SetDownloadedItems(items));

  storage.Load();
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(expected, storage));

  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());
}

// there are both local and remote update.
TEST_F(UserDictionaryAdapterTest, SetDownloadedItemsConflicts) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_dic");

  UserDictionaryAdapter adapter;
  adapter.SetUserDictionaryFileName(filename);
  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());

  // Create seed
  UserDictionaryStorage seed(adapter.GetLastSyncedUserDictionaryFileName());
  EXPECT_FALSE(seed.Load());
  AddSyncEntry(&seed);
  EXPECT_TRUE(seed.Lock());
  EXPECT_TRUE(seed.Save());
  EXPECT_TRUE(seed.UnLock());

  // Create local update
  UserDictionaryStorage storage(adapter.GetUserDictionaryFileName());
  storage.CopyFrom(seed);
  AddSyncEntry(&storage);
  EXPECT_TRUE(storage.Lock());  // keep Locking
  EXPECT_TRUE(storage.Save());
  UserDictionarySyncUtil::UserDictionaryStorageBase local_update;
  UserDictionarySyncUtil::CreateUpdate(seed, storage, &local_update);

  // Create remote update
  UserDictionaryStorage remote("");
  remote.CopyFrom(seed);
  AddSyncEntry(&remote);

  ime_sync::SyncItems items;
  ime_sync::SyncItem *item = items.Add();
  CHECK(item);

  item->set_component(adapter.component_id());

  sync::UserDictionaryKey *key =
      item->mutable_key()->MutableExtension(
          sync::UserDictionaryKey::ext);
  sync::UserDictionaryValue *value =
      item->mutable_value()->MutableExtension(
          sync::UserDictionaryValue::ext);
  CHECK(key);
  CHECK(value);

  key->set_bucket_id(0);
  UserDictionarySyncUtil::UserDictionaryStorageBase *remote_update =
      value->mutable_user_dictionary_storage();
  CHECK(remote_update);

  // Obtain remote update.
  UserDictionarySyncUtil::CreateUpdate(seed, remote, remote_update);

  // storage is locked
  EXPECT_FALSE(adapter.SetDownloadedItems(items));

  storage.UnLock();

  // storage is not locked.
  EXPECT_TRUE(adapter.SetDownloadedItems(items));

  // Here emulate the coflicts resolve.
  UserDictionarySyncUtil::MergeUpdate(*remote_update, &seed);
  UserDictionarySyncUtil::MergeUpdate(local_update, &seed);

  storage.Load();
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(seed, storage));

  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());
}

// Local and remote updates makes 'prev_dict' storage exceed its limit.
TEST_F(UserDictionaryAdapterTest, TemporaryFileExceeds) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_dic_exceed");

  UserDictionaryAdapter adapter;
  adapter.SetUserDictionaryFileName(filename);
  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());

  // Set up sync environment.
  ime_sync::SyncItems items;
  ime_sync::SyncItem *item = items.Add();
  CHECK(item);
  item->set_component(adapter.component_id());
  sync::UserDictionaryKey *key =
      item->mutable_key()->MutableExtension(sync::UserDictionaryKey::ext);
  CHECK(key);
  sync::UserDictionaryValue *value =
      item->mutable_value()->MutableExtension(sync::UserDictionaryValue::ext);
  CHECK(value);
  key->set_bucket_id(0);

  // Create a user dictionary
  UserDictionaryStorage storage(adapter.GetUserDictionaryFileName());
  for (size_t i = 0; i < UserDictionaryStorage::max_sync_entry_size() - 1;
       ++i) {
    AddSyncEntry(&storage);
  }
  EXPECT_TRUE(storage.Lock());
  EXPECT_TRUE(storage.Save());
  EXPECT_TRUE(storage.UnLock());

  // Create a last synced dictionary
  UserDictionaryStorage prev_storage(
      adapter.GetLastSyncedUserDictionaryFileName());
  prev_storage.CopyFrom(storage);
  AddSyncEntry(&prev_storage);
  EXPECT_TRUE(prev_storage.Lock());
  EXPECT_TRUE(prev_storage.Save());
  EXPECT_TRUE(prev_storage.UnLock());

  // Create local update, which has 1 removed entry.
  UserDictionarySyncUtil::UserDictionaryStorageBase local_update;
  UserDictionarySyncUtil::CreateUpdate(prev_storage, storage, &local_update);

  // Create remote update, which has 1 more entry.
  UserDictionaryStorage remote("");
  remote.CopyFrom(storage);
  AddSyncEntry(&remote);
  UserDictionarySyncUtil::UserDictionaryStorageBase *remote_update =
      value->mutable_user_dictionary_storage();
  CHECK(remote_update);
  UserDictionarySyncUtil::CreateUpdate(storage, remote, remote_update);

  // The number of entries in prev_storage must exceed its limit.
  EXPECT_TRUE(adapter.SetDownloadedItems(items));

  // Here emulate the coflicts resolve.
  UserDictionarySyncUtil::MergeUpdate(*remote_update, &prev_storage);
  UserDictionarySyncUtil::MergeUpdate(local_update, &prev_storage);

  storage.Load();
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(prev_storage, storage));
  prev_storage.Load();

  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());
}

TEST_F(UserDictionaryAdapterTest, GetItemsToUpload) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_dic");

  UserDictionaryAdapter adapter;
  adapter.SetUserDictionaryFileName(filename);
  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());

  UserDictionaryStorage prev(adapter.GetLastSyncedUserDictionaryFileName());
  EXPECT_FALSE(prev.Load());
  AddSyncEntry(&prev);
  EXPECT_TRUE(prev.Lock());
  EXPECT_TRUE(prev.Save());
  EXPECT_TRUE(prev.UnLock());

  UserDictionaryStorage storage(adapter.GetUserDictionaryFileName());
  storage.CopyFrom(prev);
  EXPECT_TRUE(storage.Lock());
  EXPECT_TRUE(storage.Save());
  EXPECT_TRUE(storage.UnLock());

  // Now prev == storage.
  {
    ime_sync::SyncItems items;
    EXPECT_TRUE(adapter.GetItemsToUpload(&items));
    EXPECT_EQ(0, items.size());
  }

  // add modifications in sync dictionary.
  AddSyncEntry(&storage);
  EXPECT_TRUE(storage.Lock());
  EXPECT_TRUE(storage.Save());
  EXPECT_TRUE(storage.UnLock());

  // get the update in advance
  UserDictionarySyncUtil::UserDictionaryStorageBase update;
  UserDictionarySyncUtil::CreateUpdate(prev, storage, &update);

  // Upload success.
  {
    ime_sync::SyncItems items;
    EXPECT_TRUE(adapter.SetBucketId(123));  // set bucket Id
    EXPECT_TRUE(adapter.GetItemsToUpload(&items));

    EXPECT_EQ(1, items.size());
    const ime_sync::SyncItem &item = items.Get(0);

    EXPECT_EQ(adapter.component_id(), item.component());
    const sync::UserDictionaryKey &key =
        item.key().GetExtension(sync::UserDictionaryKey::ext);
    const sync::UserDictionaryValue &value =
        item.value().GetExtension(sync::UserDictionaryValue::ext);

    // next bucket id is 123 + 1 == 124.
    EXPECT_EQ(124, key.bucket_id());

    // update is encoded in value.user_dictionary_storage().
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(
        update,
        value.user_dictionary_storage()));

    // update success.
    EXPECT_TRUE(adapter.MarkUploaded(item, true));

    EXPECT_TRUE(Util::IsEqualFile(
        adapter.GetUserDictionaryFileName(),
        adapter.GetLastSyncedUserDictionaryFileName()));

    // next bucket id is 124 + 1 == 125.
    EXPECT_EQ(125, adapter.GetNextBucketId());
  }

  prev.Clear();
  prev.EnsureSyncDictionaryExists();
  AddSyncEntry(&prev);
  EXPECT_TRUE(prev.Lock());
  EXPECT_TRUE(prev.Save());
  EXPECT_TRUE(prev.UnLock());

  // add modifications.
  storage.CopyFrom(prev);
  AddSyncEntry(&storage);
  EXPECT_TRUE(storage.Lock());
  EXPECT_TRUE(storage.Save());
  EXPECT_TRUE(storage.UnLock());

  // get the update in advance
  update.Clear();
  UserDictionarySyncUtil::CreateUpdate(prev, storage, &update);

  // Upload failed.
  {
    ime_sync::SyncItems items;
    EXPECT_TRUE(adapter.SetBucketId(200));  // set bucket Id
    EXPECT_TRUE(adapter.GetItemsToUpload(&items));

    EXPECT_EQ(1, items.size());
    const ime_sync::SyncItem &item = items.Get(0);

    EXPECT_EQ(adapter.component_id(), item.component());
    const sync::UserDictionaryKey &key =
        item.key().GetExtension(sync::UserDictionaryKey::ext);
    const sync::UserDictionaryValue &value =
        item.value().GetExtension(sync::UserDictionaryValue::ext);

    // next bucket id is 200 + 1 == 201.
    EXPECT_EQ(201, key.bucket_id());

    // update is encoded in value.user_dictionary_storage().
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(
        update,
        value.user_dictionary_storage()));

    // update failed.
    EXPECT_TRUE(adapter.MarkUploaded(item, false));

    EXPECT_FALSE(Util::IsEqualFile(
        adapter.GetUserDictionaryFileName(),
        adapter.GetLastSyncedUserDictionaryFileName()));

    // next bucket id is not updated.
    EXPECT_EQ(201, adapter.GetNextBucketId());
  }

  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());
}

TEST_F(UserDictionaryAdapterTest, GetItemsToUploadSnapShot) {
  const string filename =
      Util::JoinPath(FLAGS_test_tmpdir, "test_dic2");

  UserDictionaryAdapter adapter;
  adapter.SetUserDictionaryFileName(filename);

  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());

  UserDictionaryStorage prev(adapter.GetLastSyncedUserDictionaryFileName());
  EXPECT_FALSE(prev.Load());
  AddSyncEntry(&prev);
  EXPECT_TRUE(prev.Lock());
  EXPECT_TRUE(prev.Save());
  EXPECT_TRUE(prev.UnLock());

  UserDictionaryStorage storage(adapter.GetUserDictionaryFileName());
  EXPECT_FALSE(storage.Load());
  storage.CopyFrom(prev);
  AddSyncEntry(&storage);
  EXPECT_TRUE(storage.Lock());
  EXPECT_TRUE(storage.Save());
  EXPECT_TRUE(storage.UnLock());

  {
    ime_sync::SyncItems items;
    EXPECT_TRUE(adapter.SetBucketId(255));  // set bucket Id
    EXPECT_TRUE(adapter.GetItemsToUpload(&items));

    EXPECT_EQ(1, items.size());
    const ime_sync::SyncItem &item = items.Get(0);

    EXPECT_EQ(adapter.component_id(), item.component());
    const sync::UserDictionaryKey &key =
        item.key().GetExtension(sync::UserDictionaryKey::ext);
    const sync::UserDictionaryValue &value =
        item.value().GetExtension(sync::UserDictionaryValue::ext);

    EXPECT_EQ(0, key.bucket_id());

    // when bucket_id is 0, snapshot is created.
    UserDictionarySyncUtil::UserDictionaryStorageBase update;
    UserDictionarySyncUtil::CreateSnapshot(storage, &update);
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(
        update,
        value.user_dictionary_storage()));

    // update success.
    EXPECT_TRUE(adapter.MarkUploaded(item, true));

    EXPECT_TRUE(Util::IsEqualFile(
        adapter.GetUserDictionaryFileName(),
        adapter.GetLastSyncedUserDictionaryFileName()));
  }

  // add more than 1024 diffs in a sync dictionary.
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    UserDictionarySyncUtil::UserDictionary *dict =
        storage.mutable_dictionaries(i);
    if (dict->syncable()) {
      for (int j = 0; j < 1500; ++j) {
        dict->add_entries();
      }
      break;
    }
  }

  EXPECT_TRUE(storage.Lock());
  EXPECT_TRUE(storage.Save());
  EXPECT_TRUE(storage.UnLock());

  {
    ime_sync::SyncItems items;
    // Even if the id is not 255, snapshot is created.
    EXPECT_TRUE(adapter.SetBucketId(100));  // set bucket Id
    EXPECT_TRUE(adapter.GetItemsToUpload(&items));

    EXPECT_EQ(1, items.size());
    const ime_sync::SyncItem &item = items.Get(0);

    EXPECT_EQ(adapter.component_id(), item.component());
    const sync::UserDictionaryKey &key =
        item.key().GetExtension(sync::UserDictionaryKey::ext);
    const sync::UserDictionaryValue &value =
        item.value().GetExtension(sync::UserDictionaryValue::ext);

    EXPECT_EQ(0, key.bucket_id());

    // when bucket_id is 0, snapshot is created.
    UserDictionarySyncUtil::UserDictionaryStorageBase update;
    UserDictionarySyncUtil::CreateSnapshot(storage, &update);
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(
        update,
        value.user_dictionary_storage()));

    // update success.
    EXPECT_TRUE(adapter.MarkUploaded(item, true));

    EXPECT_TRUE(Util::IsEqualFile(
        adapter.GetUserDictionaryFileName(),
        adapter.GetLastSyncedUserDictionaryFileName()));
  }

  Util::Unlink(adapter.GetUserDictionaryFileName());
  Util::Unlink(adapter.GetLastSyncedUserDictionaryFileName());
}

namespace {
// On memory storage for dependency injection.
// This class might be useful for other test cases.
// TODO(taku): Move this class in storage/ directory.
class MemoryStorage : public storage::StorageInterface {
 public:
  virtual bool Open(const string &filename) {
    return true;
  }

  virtual bool Sync() {
    return true;
  }

  virtual bool Lookup(const string &key, string *value) const {
    CHECK(value);
    map<string, string>::const_iterator it = data_.find(key);
    if (it == data_.end()) {
      return false;
    }
    *value = it->second;
    return true;
  }

  virtual bool Insert(const string &key, const string &value) {
    data_[key] = value;
    return true;
  }

  virtual bool Erase(const string &key) {
    map<string, string>::iterator it = data_.find(key);
    if (it != data_.end()) {
      data_.erase(it);
      return true;
    }
    return false;
  }

  virtual bool Clear() {
    data_.clear();
    return true;
  }

  virtual size_t Size() const {
    return data_.size();
  }

  MemoryStorage() {}
  virtual ~MemoryStorage() {}

 private:
  map<string, string> data_;
};
}  // namespace

TEST_F(UserDictionaryAdapterTest, RealScenarioTest) {
  const int kClientsSize = 10;

  // Only exist one service, which emulates the sync server.
  InprocessService service;

  vector<string> filenames;
  vector<Syncer *> syncers;
  vector<UserDictionaryAdapter *> adapters;
  vector<MemoryStorage *> memory_storages;

  // create 10 clients
  for (int i = 0; i < kClientsSize; ++i) {
    Syncer *syncer = new Syncer(&service);
    CHECK(syncer);
    MemoryStorage *memory_storage = new MemoryStorage;
    CHECK(memory_storage);
    UserDictionaryAdapter *adapter = new UserDictionaryAdapter;
    CHECK(adapter);
    const string filename =
        Util::JoinPath(FLAGS_test_tmpdir,
                       "client." + Util::SimpleItoa(i));
    adapter->SetUserDictionaryFileName(filename);
    syncer->RegisterAdapter(adapter);
    syncers.push_back(syncer);
    adapters.push_back(adapter);
    memory_storages.push_back(memory_storage);
    filenames.push_back(filename);
  }

  CHECK_EQ(filenames.size(), adapters.size());
  CHECK_EQ(syncers.size(), adapters.size());

  bool reload_required = false;

  for (int n = 0; n < 300; ++n) {
    // User modifies dictionary on |client_id|-th PC.
    const int client_id = Util::Random(kClientsSize);

    CHECK(client_id >= 0 && client_id < kClientsSize);
    UserDictionaryStorage storage(filenames[client_id]);
    storage.Load();
    EXPECT_TRUE(storage.Lock());
    AddSyncEntry(&storage);
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());

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

  // Check all clients have the same storage.
  UserDictionaryStorage target(filenames[0]);
  target.Load();
  for (int i = 1; i < kClientsSize; ++i) {
    UserDictionaryStorage storage(filenames[i]);
    storage.Load();
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(target,
                                                       storage));
  }

  for (int i = 0; i < kClientsSize; ++i) {
    Util::Unlink(filenames[i]);
    delete syncers[i];
    delete adapters[i];
    delete memory_storages[i];
  }
}

}  // sync
}  // mozc
