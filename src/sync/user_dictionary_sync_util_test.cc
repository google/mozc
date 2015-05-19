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

#include "sync/user_dictionary_sync_util.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/clock_mock.h"
#include "base/file_util.h"
#include "base/flags.h"
#include "base/freelist.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "dictionary/user_dictionary_storage.h"
#include "session/commands.pb.h"
#include "sync/sync_status_manager.h"
#include "sync/sync_status_manager_interface.h"
#include "sync/sync_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {

using user_dictionary::UserDictionary;

namespace {

class UserDictionarySyncUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    original_user_profile_dir_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    Util::SetClockHandler(NULL);
    SyncStatusReset();
  }

  virtual void TearDown() {
    SyncStatusReset();
    Util::SetClockHandler(NULL);
    SystemUtil::SetUserProfileDirectory(original_user_profile_dir_);
  }

  void SyncStatusReset() {
    // Reset sync status assuming authorization succeeds.
    Singleton<SyncStatusManager>::get()->SetSyncGlobalStatus(
        commands::CloudSyncStatus::INSYNC);
    Singleton<SyncStatusManager>::get()->NewSyncStatusSession();
  }

 private:
  string original_user_profile_dir_;
};

vector<uint64> CreateStorageSortedVectorForUnsyncableDictionaries(
    const UserDictionarySyncUtil::UserDictionaryStorageBase &storage) {
  vector<uint64> result;
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    const UserDictionarySyncUtil::UserDictionary &dic = storage.dictionaries(i);
    if (dic.syncable()) {
      continue;
    }
    const string &name = dic.name();
    const bool enabled = dic.enabled();
    result.push_back(Util::Fingerprint(name) + '\t' + (enabled ? '1' : '0'));
    for (int j = 0; j < dic.entries_size(); ++j) {
      const UserDictionarySyncUtil::UserDictionaryEntry &entry = dic.entries(j);
      if (entry.removed()) {
        continue;
      }
      const uint64 fp = Util::Fingerprint(
          name + '\t' +
          entry.key() + '\t' +
          entry.value() + '\t' +
          static_cast<char>(entry.pos()) + '\t' +
          entry.comment());
      result.push_back(fp);
    }
  }
  sort(result.begin(), result.end());
  return result;
}

bool IsEqualUnsyncableDictionaries(
    UserDictionarySyncUtil::UserDictionaryStorageBase storage1,
    UserDictionarySyncUtil::UserDictionaryStorageBase storage2) {
  return
      CreateStorageSortedVectorForUnsyncableDictionaries(storage1) ==
      CreateStorageSortedVectorForUnsyncableDictionaries(storage2);
}

}  // namespace

TEST_F(UserDictionarySyncUtilTest, IsEqualStorage) {
  UserDictionarySyncUtil::UserDictionaryStorageBase storage1, storage2;

  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));

  {
    UserDictionarySyncUtil::UserDictionary *dic = storage1.add_dictionaries();
    dic->set_name("dic");
    dic->set_syncable(true);
    for (int i = 0; i < 10; ++i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = NumberUtil::SimpleItoa(i);
      entry->set_key("key" + suffix);
      entry->set_value("value" + suffix);
      entry->set_pos(UserDictionary::NOUN);
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
      const string suffix = NumberUtil::SimpleItoa(i);
      entry->set_key("key" + suffix);
      entry->set_value("value" + suffix);
      entry->set_pos(UserDictionary::NOUN);
    }
  }
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));

  {
    // Add duplicates
    UserDictionarySyncUtil::UserDictionary *dic =
        storage1.mutable_dictionaries(0);
    for (int i = 0; i < 4; ++i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = NumberUtil::SimpleItoa(i);
      entry->set_key("key" + suffix);
      entry->set_value("value" + suffix);
      entry->set_pos(UserDictionary::NOUN);
    }
  }
  EXPECT_FALSE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));

  storage2.CopyFrom(storage1);
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));
  {
    // additional unsyncable dictionary doesn't affect.
    UserDictionarySyncUtil::UserDictionary *dic =
        storage2.add_dictionaries();
    dic->set_name("dic2");
    dic->set_syncable(false);
    for (int i = 0; i < 10; ++i) {
      UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
      const string suffix = NumberUtil::SimpleItoa(i);
      entry->set_key("key2" + suffix);
      entry->set_value("value2" + suffix);
      entry->set_pos(UserDictionary::NOUN);
    }
  }
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage1, storage2));
}

TEST_F(UserDictionarySyncUtilTest, EntryFingerprint) {
  UserDictionarySyncUtil::UserDictionaryEntry entry1, entry2;

  EXPECT_EQ(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry1.set_key("key");
  entry1.set_value("value");
  entry1.set_pos(UserDictionary::NOUN);
  entry1.set_comment("comment");

  entry2.CopyFrom(entry1);
  EXPECT_EQ(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry2.CopyFrom(entry1);
  entry2.set_key("key2");
  EXPECT_NE(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry2.CopyFrom(entry1);
  entry2.set_value("value2");
  EXPECT_NE(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry2.CopyFrom(entry1);
  entry2.set_pos(UserDictionary::ADVERB);
  EXPECT_NE(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));

  entry2.CopyFrom(entry1);
  entry2.set_comment("comment2");
  EXPECT_NE(UserDictionarySyncUtil::EntryFingerprint(entry1),
            UserDictionarySyncUtil::EntryFingerprint(entry2));
}

TEST_F(UserDictionarySyncUtilTest, RemoveDuplicatedEntries) {
  UserDictionaryStorage storage("");
  {
    UserDictionary *dictionary = storage.add_dictionaries();
    dictionary->set_id(100);
    dictionary->set_name("dictionary_1");
    dictionary->set_syncable(true);
    UserDictionarySyncUtil::UserDictionaryEntry *entry1 =
        dictionary->add_entries();
    entry1->set_key("key1");
    entry1->set_value("value1");
    entry1->set_pos(UserDictionary::NOUN);
    entry1->set_comment("comment1");
    UserDictionarySyncUtil::UserDictionaryEntry *entry2 =
        dictionary->add_entries();
    entry2->set_key("key2");
    entry2->set_value("value2");
    entry2->set_pos(UserDictionary::ADVERB);
    entry2->set_comment("comment2");
  }
  {
    UserDictionary *dictionary = storage.add_dictionaries();
    dictionary->CopyFrom(storage.dictionaries(0));
    dictionary->set_id(200);
    dictionary->set_name("test_dictionary_2");
  }
  {
    UserDictionary *dictionary = storage.add_dictionaries();
    dictionary->CopyFrom(storage.dictionaries(0));
    dictionary->set_id(300);
    dictionary->set_name("test_dictionary_3");
    dictionary->set_syncable(false);
  }

  UserDictionaryStorage orig_storage("");
  orig_storage.CopyFrom(storage);
  ASSERT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage, orig_storage));

  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    storage.CopyFrom(orig_storage);

    UserDictionary *dictionary = storage.mutable_dictionaries(i);
    // Add a duplicated entry.
    dictionary->add_entries()->CopyFrom(dictionary->entries(0));

    const int orig_entries_size = orig_storage.dictionaries(i).entries_size();
    if (dictionary->syncable()) {
      EXPECT_NE(dictionary->entries_size(), orig_entries_size);
      EXPECT_FALSE(
          UserDictionarySyncUtil::IsEqualStorage(storage, orig_storage));
      UserDictionarySyncUtil::RemoveDuplicatedEntries(&storage);
      EXPECT_EQ(dictionary->entries_size(), orig_entries_size);
      EXPECT_TRUE(
          UserDictionarySyncUtil::IsEqualStorage(storage, orig_storage));
    } else {
      // IsEqualStorage() and RemoveDuplicatedEntries() doesn't do nothing to
      // non-syncable dictionaries.
      EXPECT_NE(dictionary->entries_size(), orig_entries_size);
      EXPECT_TRUE(
          UserDictionarySyncUtil::IsEqualStorage(storage, orig_storage));
      UserDictionarySyncUtil::RemoveDuplicatedEntries(&storage);
      EXPECT_NE(dictionary->entries_size(), orig_entries_size);
      EXPECT_TRUE(
          UserDictionarySyncUtil::IsEqualStorage(storage, orig_storage));
    }
  }
}

namespace {
typedef FreeList<UserDictionarySyncUtil::UserDictionaryStorageBase>
    UserDictionaryStorageAllocator;

// Add random modifications to |storage|. Sync dictionary must have any
// midification.
void AddRandomUpdates(UserDictionaryStorage *storage) {
  CHECK(storage);

  // In 10% cases, clean out the storage.
  if (Util::Random(10) == 0) {
    storage->Clear();
  }

  // In 20% cases, remove a dictionary.
  if (Util::Random(5) == 0) {
    const int dictionary_num = storage->dictionaries_size();
    if (dictionary_num != 0) {
      storage->mutable_dictionaries()->SwapElements(
          Util::Random(dictionary_num), dictionary_num - 1);
      storage->mutable_dictionaries()->RemoveLast();
    }
  }

  storage->EnsureSyncDictionaryExists();

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
      entry->set_pos(UserDictionary::NOUN);
    }
  }

  CHECK_GT(storage->dictionaries_size(), 0);
}
}  // namespace

TEST_F(UserDictionarySyncUtilTest, NumEntryExceedsTest) {
  const int kMaxNumEntry = UserDictionaryStorage::max_sync_entry_size();
  const uint64 kSecond = 123;
  const uint32 kMicroSecond = 456789;
  scoped_ptr<Util::ClockInterface>
      mock_clock(new ClockMock(kSecond, kMicroSecond));
  Util::SetClockHandler(mock_clock.get());
  SyncStatusManagerInterface *manager = Singleton<SyncStatusManager>::get();

  // Actual test.
  UserDictionaryStorage storage(FileUtil::JoinPath(FLAGS_test_tmpdir,
                                                   "test.db"));
  EXPECT_TRUE(storage.EnsureSyncDictionaryExists());
  UserDictionarySyncUtil::UserDictionary *dic = storage.mutable_dictionaries(0);
  EXPECT_TRUE(dic->syncable());
  EXPECT_EQ(0, dic->entries_size());
  for (int i = 0; i < kMaxNumEntry; ++i) {
    UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
    entry->set_key("key" + NumberUtil::SimpleItoa(i));
    entry->set_value("value" + NumberUtil::SimpleItoa(i));
  }

  commands::CloudSyncStatus status;
  manager->GetLastSyncStatus(&status);
  EXPECT_EQ(0, status.sync_errors_size());
  EXPECT_TRUE(UserDictionarySyncUtil::VerifyLockAndSaveStorage(&storage));

  // Check error log
  manager->GetLastSyncStatus(&status);
  EXPECT_EQ(0, status.sync_errors_size());

  // Newly add a few etnries, to exceed maximum number of entry.
  for (int i = 0; i < 10; ++i) {
    UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
    entry->set_key("key" + NumberUtil::SimpleItoa(i + kMaxNumEntry));
    entry->set_value("value" + NumberUtil::SimpleItoa(i + kMaxNumEntry));
  }
  EXPECT_FALSE(UserDictionarySyncUtil::VerifyLockAndSaveStorage(&storage));
  // Save without validation intentionally.
  EXPECT_TRUE(UserDictionarySyncUtil::LockAndSaveStorage(&storage));

  // Check error log
  manager->GetLastSyncStatus(&status);
  EXPECT_EQ(1, status.sync_errors_size());
  EXPECT_EQ(commands::CloudSyncStatus::USER_DICTIONARY_NUM_ENTRY_EXCEEDED,
            status.sync_errors(0).error_code());
  EXPECT_EQ(kSecond, status.sync_errors(0).timestamp());
}

TEST_F(UserDictionarySyncUtilTest, CreateAndMergeTest) {
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
    vector<UserDictionarySyncUtil::UserDictionaryStorageBase *> updates;
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
        UserDictionaryStorage::CountSyncableDictionaries(storage_orig);
    // Check maximum number of dictionaries.
    EXPECT_GE(UserDictionaryStorage::max_sync_dictionary_size(), num_sync_dict);
    // Check minimum number of dictionaries.
    EXPECT_LT(0, num_sync_dict);
  }
}

TEST_F(UserDictionarySyncUtilTest, MergeDuplicatedEntryTest) {
  UserDictionaryStorage storage("");
  storage.EnsureSyncDictionaryExists();

  for (int i = 0; i < 100; ++i) {
    UserDictionarySyncUtil::UserDictionaryStorageBase prev;
    prev.CopyFrom(storage);
    AddRandomUpdates(&storage);
    UserDictionarySyncUtil::UserDictionaryStorageBase update;
    EXPECT_TRUE(UserDictionarySyncUtil::CreateUpdate(prev, storage, &update));

    // Merge the update twice and second one does nothing.
    EXPECT_TRUE(UserDictionarySyncUtil::MergeUpdate(update, &prev));
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(prev, storage));
    EXPECT_TRUE(UserDictionarySyncUtil::MergeUpdate(update, &prev));
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(prev, storage));
  }
}

TEST_F(UserDictionarySyncUtilTest, DuplicatedSyncDictionaryNameTest) {
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
      const string suffix = NumberUtil::SimpleItoa(i);
      entry->set_key("key" + suffix);
      entry->set_value("value" + suffix);
      entry->set_pos(UserDictionary::NOUN);
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
      const string suffix = NumberUtil::SimpleItoa(i);
      entry->set_key("key_sync" + suffix);
      entry->set_value("value_sync" + suffix);
      entry->set_pos(UserDictionary::NOUN);
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
    entry->set_pos(UserDictionary::NOUN);
  }

  {
    // Create an update to the syncable dictionary.
    UserDictionarySyncUtil::UserDictionary *dic =
        storage_cur.mutable_dictionaries(1);
    EXPECT_TRUE(dic->syncable());  // just in case
    UserDictionarySyncUtil::UserDictionaryEntry *entry = dic->add_entries();
    entry->set_key("new_synced_key");
    entry->set_value("new_synced_value");
    entry->set_pos(UserDictionary::NOUN);
  }

  UserDictionarySyncUtil::UserDictionaryStorageBase update;
  vector<UserDictionarySyncUtil::UserDictionaryStorageBase *> updates;
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

TEST_F(UserDictionarySyncUtilTest, ShouldCreateSnapshot) {
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

TEST_F(UserDictionarySyncUtilTest, CreateSnapshot) {
  {
    UserDictionaryStorage storage(""), update("");
    update.add_dictionaries()->set_syncable(true);
    update.add_dictionaries()->set_syncable(false);
    ASSERT_EQ(0, storage.dictionaries_size());
    ASSERT_EQ(2, update.dictionaries_size());
    UserDictionarySyncUtil::CreateSnapshot(storage, &update);
    EXPECT_EQ(0, update.dictionaries_size());
    EXPECT_EQ(UserDictionaryStorage::SNAPSHOT, update.storage_type());
  }

  {
    UserDictionaryStorage storage(""), update("");
    storage.add_dictionaries()->set_syncable(true);
    storage.mutable_dictionaries(0)->add_entries()->set_key("aaa");
    storage.add_dictionaries()->set_syncable(false);
    ASSERT_EQ(2, storage.dictionaries_size());
    ASSERT_EQ(0, update.dictionaries_size());
    UserDictionarySyncUtil::CreateSnapshot(storage, &update);
    UserDictionarySyncUtil::IsEqualStorage(storage, update);
    EXPECT_EQ(UserDictionaryStorage::CountSyncableDictionaries(update),
              update.dictionaries_size());
    EXPECT_EQ(UserDictionaryStorage::SNAPSHOT, update.storage_type());
  }

  {
    UserDictionaryStorage storage(""), update("");
    for (int i = 0; i < 1000; ++i) {
      AddRandomUpdates(&storage);
      UserDictionarySyncUtil::CreateSnapshot(storage, &update);
      EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(storage, update));
      EXPECT_EQ(UserDictionaryStorage::CountSyncableDictionaries(update),
                update.dictionaries_size());
      EXPECT_EQ(UserDictionaryStorage::SNAPSHOT, update.storage_type());
    }
  }
}

TEST_F(UserDictionarySyncUtilTest, CopySyncableDictionaries) {
  UserDictionaryStorage from(""), to("");
  from.EnsureSyncDictionaryExists();

  for (int i = 0; i < 1000; ++i) {
    to.CopyFrom(from);
    AddRandomUpdates(&to);
    const bool is_unsyncable_changed = IsEqualUnsyncableDictionaries(from, to);
    UserDictionarySyncUtil::CopyDictionaries(from, &to);
    EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(from, to));
    EXPECT_EQ(is_unsyncable_changed, IsEqualUnsyncableDictionaries(from, to));
  }

  // Check the order of the destination dictionaries.
  // from:   a(syncable), b, c(syncable), d, e(syncable), f
  // to:     g, h(syncable), e(syncable), d, c(syncable), b
  // copied: g, e(syncable), d, c(syncable), b, a(syncable)
  // - The order of ["g", "e", "d", "c", "b"] is kept.
  // - "a" is added at the end since it is a syncable dictionary.
  // - "f" is not added since it is an unsyncable dictionary.
  // - "g" is not removed since it is an unsyncable dictionary.
  // - "h" is removed since it is a syncable dictionary.
  from.Clear();
  from.add_dictionaries()->set_name("a");
  from.add_dictionaries()->set_name("b");
  from.add_dictionaries()->set_name("c");
  from.add_dictionaries()->set_name("d");
  from.add_dictionaries()->set_name("e");
  from.add_dictionaries()->set_name("f");
  from.mutable_dictionaries(0)->set_syncable(true);
  from.mutable_dictionaries(2)->set_syncable(true);
  from.mutable_dictionaries(4)->set_syncable(true);
  to.Clear();
  to.add_dictionaries()->set_name("g");
  to.add_dictionaries()->set_name("h");
  to.add_dictionaries()->set_name("e");
  to.add_dictionaries()->set_name("d");
  to.add_dictionaries()->set_name("c");
  to.add_dictionaries()->set_name("b");
  to.mutable_dictionaries(1)->set_syncable(true);
  to.mutable_dictionaries(2)->set_syncable(true);
  to.mutable_dictionaries(4)->set_syncable(true);

  UserDictionarySyncUtil::CopyDictionaries(from, &to);
  EXPECT_TRUE(UserDictionarySyncUtil::IsEqualStorage(from, to));
  EXPECT_EQ(6, to.dictionaries_size());
  EXPECT_EQ("g", to.dictionaries(0).name());
  EXPECT_EQ("e", to.dictionaries(1).name());
  EXPECT_EQ("d", to.dictionaries(2).name());
  EXPECT_EQ("c", to.dictionaries(3).name());
  EXPECT_EQ("b", to.dictionaries(4).name());
  EXPECT_EQ("a", to.dictionaries(5).name());
}

TEST_F(UserDictionarySyncUtilTest, RemoveUnsyncableDictionaries) {
  UserDictionaryStorage storage("");

  for (int i = 0; i < 100; ++i) {
    AddRandomUpdates(&storage);
    UserDictionaryStorage original_storage("");
    original_storage.CopyFrom(storage);
    UserDictionarySyncUtil::RemoveUnsyncableDictionaries(&storage);
    EXPECT_TRUE(
        UserDictionarySyncUtil::IsEqualStorage(original_storage, storage));
  }

  storage.Clear();
  storage.add_dictionaries()->set_name("0");
  storage.add_dictionaries()->set_name("1");
  storage.add_dictionaries()->set_name("2");
  storage.add_dictionaries()->set_name("3");
  storage.add_dictionaries()->set_name("4");
  storage.add_dictionaries()->set_name("5");
  storage.add_dictionaries()->set_name("6");
  storage.add_dictionaries()->set_name("7");
  storage.mutable_dictionaries(1)->set_syncable(true);
  storage.mutable_dictionaries(2)->set_syncable(true);
  storage.mutable_dictionaries(5)->set_syncable(true);
  storage.mutable_dictionaries(7)->set_syncable(true);

  UserDictionarySyncUtil::RemoveUnsyncableDictionaries(&storage);
  EXPECT_EQ(4, storage.dictionaries_size());
  EXPECT_EQ("1", storage.dictionaries(0).name());
  EXPECT_EQ("2", storage.dictionaries(1).name());
  EXPECT_EQ("5", storage.dictionaries(2).name());
  EXPECT_EQ("7", storage.dictionaries(3).name());
}

namespace {
// Emulate cloud download.
void DownloadUpdates(
    int timestamp,
    const vector<UserDictionarySyncUtil::UserDictionaryStorageBase *> &updates,
    vector<UserDictionarySyncUtil::UserDictionaryStorageBase *> *new_updates) {
  for (size_t i = 0; i < updates.size(); ++i) {
    if (timestamp <= i) {   // newer than timestamp.
      new_updates->push_back(updates[i]);
    }
  }
}
}  // namespace

TEST_F(UserDictionarySyncUtilTest, RealScenarioTest) {
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
  vector<UserDictionarySyncUtil::UserDictionaryStorageBase *> updates;

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
      vector<UserDictionarySyncUtil::UserDictionaryStorageBase *> new_updates;
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

}  // namespace sync
}  // namespace mozc
