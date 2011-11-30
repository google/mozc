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

#include "sync/user_dictionary_sync_util.h"

#include <string>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/freelist.h"
#include "base/util.h"
#include "dictionary/user_dictionary_storage.h"
#include "sync/sync_util.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace sync {

TEST(UserDictionarySyncUtilTest, IsEqualStorage) {
  UserDictionarySyncUtil::UserDictionaryStorageBase storage1, storage2;

  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));

  {
    UserDictionarySyncUtil::UserDictionary *dic = storage1.add_dictionaries();
    dic->set_name("dic");
    dic->set_syncable(true);
    for (int i = 0; i < 10; ++i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = Util::SimpleItoa(i);
      entry->set_key("key" + suffix);
      entry->set_value("value" + suffix);
      entry->set_pos("pos" + suffix);
    }
  }
  EXPECT_FALSE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));

  {
    UserDictionarySyncUtil::UserDictionary *dic = storage2.add_dictionaries();
    dic->set_name("dic");
    dic->set_syncable(true);
    // Order is different.
    for (int i = 9; i >= 0; --i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = Util::SimpleItoa(i);
      entry->set_key("key" + suffix);
      entry->set_value("value" + suffix);
      entry->set_pos("pos" + suffix);
    }
  }
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));

  {
    // Add duplicates
    UserDictionarySyncUtil::UserDictionary *dic =
        storage1.mutable_dictionaries(0);
    for (int i = 0; i < 4; ++i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = Util::SimpleItoa(i);
      entry->set_key("key" + suffix);
      entry->set_value("value" + suffix);
      entry->set_pos("pos" + suffix);
    }
  }
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));

  {
    // additional unsyncable dictionary doesn't affect.
    UserDictionarySyncUtil::UserDictionary *dic =
        storage2.add_dictionaries();
    dic->set_name("dic2");
    dic->set_syncable(false);
    for (int i = 0; i < 10; ++i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = Util::SimpleItoa(i);
      entry->set_key("key2" + suffix);
      entry->set_value("value2" + suffix);
      entry->set_pos("pos2" + suffix);
    }
  }
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));
}

TEST(UserDictionarySyncUtilTest, EntryFingerprint) {
  UserDictionarySyncUtil::UserDictionaryEntry entry1, entry2;

  EXPECT_EQ(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry1.set_key("key");
  entry1.set_value("value");
  entry1.set_pos("pos");
  entry1.set_pos("comment");
  entry2.CopyFrom(entry1);

  EXPECT_EQ(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry2.set_key("key2");
  EXPECT_NE(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry2.set_key("key");
  entry2.set_value("value2");
  EXPECT_NE(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry2.set_value("value");
  entry2.set_pos("pos2");
  EXPECT_NE(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry2.set_pos("pos");
  entry2.set_comment("comment2");
  EXPECT_NE(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));
}

namespace {
typedef FreeList<UserDictionarySyncUtil::UserDictionaryStorageBase>
    UserDictionaryStorageAllocator;

// Add random modifications to |storage|. Sync dictionary must have any
// midification. This function is used for unittesting.
void AddRandomUpdates(UserDictionaryStorage *storage) {
  CHECK(storage);

  // In 10% cases, clean out the storage.
  if (Util::Random(10) == 0) {
    storage->Clear();
    storage->EnsureSyncDictionaryExists();
  }

  // In 50% cases, add an unsync dictionary.
  if (Util::Random(2) == 0) {
    UserDictionarySyncUtil::UserDictionary *dic = storage->add_dictionaries();
    DCHECK(dic);
    dic->set_name(SyncUtil::GenRandomString(100));
    dic->set_syncable(false);
  }

  // Modify dictionaries.
  for (int i = 0; i < storage->dictionaries_size(); ++i) {
    UserDictionarySyncUtil::UserDictionary *dic =
        storage->mutable_dictionaries(i);
    // In 50% cases, remove entries.
    if (Util::Random(2) == 0) {
      UserDictionarySyncUtil::UserDictionary tmp_dic;
      tmp_dic.set_name(dic->name());
      tmp_dic.set_syncable(dic->syncable());
      dic->CopyFrom(tmp_dic);
    }
    // In 50% cases, add an entry.
    if (Util::Random(2) == 0) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      DCHECK(entry);
      entry->set_key(SyncUtil::GenRandomString(5));
      entry->set_value(SyncUtil::GenRandomString(5));
      entry->set_pos(SyncUtil::GenRandomString(5));
    }
  }

  CHECK_GT(storage->dictionaries_size(), 0);
}
}  // namespace

TEST(UserDictionarySyncUtilTest, CreateAndMergeTest) {
  UserDictionaryStorage storage_orig("");
  UserDictionaryStorage storage_cur("");
  UserDictionarySyncUtil::UserDictionaryStorageBase storage_prev;

  // Create a seed storage
  storage_orig.EnsureSyncDictionaryExists();

  // repeat 100 times.
  for (int i = 0; i < 100; ++i) {
    const int num_updates = Util::Random(100) + 1;
    storage_cur.CopyFrom(storage_orig);
    UserDictionaryStorageAllocator allocator(100);
    vector<const UserDictionarySyncUtil::UserDictionaryStorageBase *> updates;
    // Emulate the scenario that client sends |num_updaes| updates to the
    // cloud.
    for (int n = 0; n < num_updates; ++n) {
      storage_prev.CopyFrom(storage_cur);
      AddRandomUpdates(&storage_cur);
      UserDictionarySyncUtil::UserDictionaryStorageBase *update =
          allocator.Alloc(1);
      // Get the diff between storage_cur and storage_prev,
      UserDictionarySyncUtil::CreateUpdate(storage_prev, storage_cur, update);
      updates.push_back(update);
    }
    // Apply the updates to the storage_orig
    UserDictionarySyncUtil::MergeUpdates(updates, &storage_orig);

    // Compare only syncable dictionaries.
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage_orig,
                                                       storage_cur));

    // The number of syncable dictionaries must be 1.
    int num_sync_dict =
        UserDictionaryStorage::CountSyncableDictionaries(&storage_orig);
    // Check maximum number of dictionaries.
    EXPECT_GE(UserDictionaryStorage::max_sync_dictionary_size(), num_sync_dict);
    // Check minimum number of dictionaries.
    EXPECT_LT(0, num_sync_dict);
  }
}

TEST(UserDictionarySyncUtilTest, DuplicatedSyncDictionaryNameTest) {
  UserDictionarySyncUtil::UserDictionaryStorageBase
      storage_orig, storage_cur, storage_new;

  {
    // Create an unsyncable dictionary
    UserDictionarySyncUtil::UserDictionary *dic =
        storage_orig.add_dictionaries();
    dic->set_name("dic");
    dic->set_syncable(false);
    for (int i = 0; i < 10; ++i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = Util::SimpleItoa(i);
      entry->set_key("key" + suffix);
      entry->set_value("value" + suffix);
      entry->set_pos("pos" + suffix);
    }
  }
  {
    // Create a syncable dictionary with the same name
    UserDictionarySyncUtil::UserDictionary *dic =
        storage_orig.add_dictionaries();
    dic->set_name("dic");
    dic->set_syncable(true);
    for (int i = 0; i < 10; ++i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = Util::SimpleItoa(i);
      entry->set_key("key_sync" + suffix);
      entry->set_value("value_sync" + suffix);
      entry->set_pos("pos_sync" + suffix);
    }
  }

  storage_cur.CopyFrom(storage_orig);
  storage_new.CopyFrom(storage_orig);

  {
    // Create an update to the normal dictionary.
    UserDictionarySyncUtil::UserDictionary *dic =
        storage_cur.mutable_dictionaries(0);
    EXPECT_FALSE(dic->syncable());  // just in case
    UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
    entry->set_key("new_key");
    entry->set_value("new_value");
    entry->set_pos("new_pos");
  }

  {
    // Create an update to the syncable dictionary.
    UserDictionarySyncUtil::UserDictionary *dic =
        storage_cur.mutable_dictionaries(1);
    EXPECT_TRUE(dic->syncable());  // just in case
    UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
    entry->set_key("new_synced_key");
    entry->set_value("new_synced_value");
    entry->set_pos("new_synced_pos");
  }

  UserDictionarySyncUtil::UserDictionaryStorageBase update;
  vector<const UserDictionarySyncUtil::UserDictionaryStorageBase *> updates;
  UserDictionarySyncUtil::CreateUpdate(storage_orig, storage_cur, &update);
  updates.push_back(&update);
  UserDictionarySyncUtil::MergeUpdates(updates, &storage_new);

  // Sync is affected, so new is different from the original.
  EXPECT_FALSE(
      UserDictionarySyncUtil::IsEqualStorage(storage_orig, storage_new));
  // No changes on the unsyncable dictionary
  EXPECT_EQ(storage_orig.dictionaries(0).DebugString(),
            storage_new.dictionaries(0).DebugString());
  // Updates are propagated to the synced dictionary, so storage_cur
  // and storage_new should be same in the sync point of view.
  EXPECT_TRUE(
      UserDictionarySyncUtil::IsEqualStorage(storage_cur, storage_new));
}

TEST(UserDictionarySyncUtilTest, ShouldCreateSnapshot) {
  UserDictionarySyncUtil::UserDictionaryStorageBase update;
  EXPECT_FALSE(UserDictionarySyncUtil::ShouldCreateSnapshot(update));

  for (int i = 0; i < 2000; ++i) {
    update.add_dictionaries();
  }

  EXPECT_TRUE(UserDictionarySyncUtil::ShouldCreateSnapshot(update));

  update.clear_dictionaries();

  EXPECT_FALSE(UserDictionarySyncUtil::ShouldCreateSnapshot(update));

  UserDictionarySyncUtil::UserDictionary *dic = update.add_dictionaries();

  for (int i = 0; i < 1000; ++i) {
    dic->add_entries();
  }

  EXPECT_FALSE(UserDictionarySyncUtil::ShouldCreateSnapshot(update));

  for (int i = 0; i < 1000; ++i) {
    dic->add_entries();
  }

  EXPECT_TRUE(UserDictionarySyncUtil::ShouldCreateSnapshot(update));
}

namespace {
// Emulate cloud download.
void DownloadUpdates(
    int timestamp,
    const vector<const UserDictionarySyncUtil::UserDictionaryStorageBase *>
    &updates,
    vector<const UserDictionarySyncUtil::UserDictionaryStorageBase *>
    *new_updates) {
  for (size_t i = 0; i < updates.size(); ++i) {
    if (timestamp <= i) {   // newer than timestamp.
      new_updates->push_back(updates[i]);
    }
  }
}
}  // namespace

TEST(UserDictionarySyncUtilTest, RealScenarioTest) {
  const size_t kClientsSize = 10;

  // Make sure that every storage have a sync dictionary.
  vector<UserDictionaryStorage *> storages(kClientsSize);
  for (int i = 0; i < kClientsSize; ++i) {
    storages[i] = new UserDictionaryStorage("");
    DCHECK(storages[i]);
    storages[i]->EnsureSyncDictionaryExists();
  }

  vector<int> timestamps(kClientsSize);
  fill(timestamps.begin(), timestamps.end(), 0);
  vector<const UserDictionarySyncUtil::UserDictionaryStorageBase *> updates;

  UserDictionaryStorageAllocator allocator(100);

  for (int n = 0; n < 1000; ++n) {
    // User modifies dictionary on |client_id|-th PC.
    const int client_id = Util::Random(kClientsSize);
    CHECK(client_id >= 0 && client_id < kClientsSize);
    UserDictionaryStorage *storage = storages[client_id];

    UserDictionarySyncUtil::UserDictionaryStorageBase prev;
    prev.CopyFrom(*storage);
    AddRandomUpdates(storage);
    UserDictionarySyncUtil::UserDictionaryStorageBase *update =
        allocator.Alloc(1);
    UserDictionarySyncUtil::CreateUpdate(prev, *storage, update);
    updates.push_back(update);
    timestamps[client_id] = updates.size();

    // Start Sync procedure on every machine.
    // Here we assume that no conflicts occur.
    for (int i = 0; i < kClientsSize; ++i) {
      // Download updates from the cloud
      vector<const UserDictionarySyncUtil::UserDictionaryStorageBase *>
          new_updates;
      DownloadUpdates(timestamps[i], updates, &new_updates);
      timestamps[i] = updates.size();   // update timestamp.
      UserDictionarySyncUtil::MergeUpdates(new_updates, storages[i]);
    }
  }

  // All machine should have the same dictionary.
  for (int i = 1; i < kClientsSize; ++i) {
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(*storages[0],
                                                       *storages[i]));
  }

  for (int i = 0; i < kClientsSize; ++i) {
    delete (storages[i]);
  }
}

}  // sync
}  // mozc
