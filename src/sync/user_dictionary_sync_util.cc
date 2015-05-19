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
#include <map>
#include <set>
#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/protobuf/protobuf.h"
#include "base/protobuf/repeated_field.h"
#include "base/protobuf/unknown_field_set.h"
#include "base/singleton.h"
#include "base/util.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "sync/sync_status_manager.h"
#include "sync/sync_util.h"

namespace mozc {
namespace sync {

using mozc::protobuf::Reflection;
using mozc::protobuf::RepeatedPtrField;
using mozc::protobuf::UnknownFieldSet;

namespace {

void CreateStorageSet(
    const UserDictionarySyncUtil::UserDictionaryStorageBase &storage,
    set<uint64> *contains) {
  DCHECK(contains);
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    const UserDictionarySyncUtil::UserDictionary &dic = storage.dictionaries(i);
    if (!dic.syncable()) {
      continue;
    }
    const string &name = dic.name();
    const bool enabled = dic.enabled();
    contains->insert(Util::Fingerprint(name) + '\t'
                     + (enabled ? '1' : '0'));
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
      contains->insert(fp);
    }
  }
}
}  // anonymous namespace

bool UserDictionarySyncUtil::IsEqualStorage(
    const UserDictionaryStorageBase &storage1,
    const UserDictionaryStorageBase &storage2) {
  if (UserDictionaryStorage::CountSyncableDictionaries(&storage1) !=
      UserDictionaryStorage::CountSyncableDictionaries(&storage2)) {
    return false;
  }
  set<uint64> storage_set1, storage_set2;
  CreateStorageSet(storage1, &storage_set1);
  CreateStorageSet(storage2, &storage_set2);
  return storage_set1 == storage_set2;
}

uint64 UserDictionarySyncUtil::EntryFingerprint(
    const UserDictionarySyncUtil::UserDictionaryEntry &entry) {
  return Util::Fingerprint(entry.key() + '\t' +
                           entry.value() + '\t' +
                           static_cast<char>(entry.pos()) + '\t' +
                           entry.comment());
}

namespace {
void CreateEntriesSet(const UserDictionarySyncUtil::UserDictionary &dictionary,
                      set<uint64> *contains) {
  CHECK(contains);
  contains->clear();
  for (int i = 0; i < dictionary.entries_size(); ++i) {
    contains->insert(UserDictionarySyncUtil::EntryFingerprint(
        dictionary.entries(i)));
  }
}

int FindDictionary(
    const UserDictionarySyncUtil::UserDictionaryStorageBase &storage,
    const string &name) {
  for (int i = 0; i < storage.dictionaries_size(); ++i) {
    const user_dictionary::UserDictionary &dict = storage.dictionaries(i);
    if (dict.syncable() && name == dict.name()) {
      return i;
    }
  }
  return -1;
}
}  // anonymous namaespace

bool UserDictionarySyncUtil::CreateSnapshot(
    const UserDictionaryStorageBase &storage_new,
    UserDictionaryStorageBase *update) {
  CHECK(update);
  update->CopyFrom(storage_new);
  update->set_storage_type(UserDictionaryStorageBase::SNAPSHOT);
  return true;
}

bool UserDictionarySyncUtil::CreateDictionaryUpdate(
    const UserDictionary &dictionary_old,
    const UserDictionary &dictionary_new,
    UserDictionary *update) {
  CHECK(update);
  update->Clear();
  DCHECK_EQ(dictionary_new.name(), dictionary_old.name());
  update->set_name(dictionary_new.name());
  update->set_syncable(dictionary_new.syncable());

  set<uint64> contains_old, contains_new;
  CreateEntriesSet(dictionary_old, &contains_old);
  CreateEntriesSet(dictionary_new, &contains_new);

  // Find entries added.
  for (int i = 0; i < dictionary_new.entries_size(); ++i) {
    const UserDictionaryEntry &entry_new = dictionary_new.entries(i);
    if (contains_old.find(EntryFingerprint(entry_new)) == contains_old.end()) {
      update->add_entries()->CopyFrom(entry_new);
    }
  }

  // Find entries removed.
  for (int i = 0; i < dictionary_old.entries_size(); ++i) {
    const UserDictionaryEntry &entry_old = dictionary_old.entries(i);
    if (contains_new.find(EntryFingerprint(entry_old)) == contains_new.end()) {
      UserDictionaryEntry *entry = update->add_entries();
      DCHECK(entry);
      entry->CopyFrom(entry_old);
      entry->set_removed(true);
    }
  }

  return true;
}

bool UserDictionarySyncUtil::ShouldCreateSnapshot(
    const UserDictionaryStorageBase &update) {
  // Create a snapshot if the #(updated entries)
  // exceeds |kSnapShotThreshold|.
  const uint32 kSnapShotThreshold = 1024;
  uint32 num_updated_entries = 0;
  for (int i = 0; i < update.dictionaries_size(); ++i) {
    ++num_updated_entries;
    num_updated_entries += update.dictionaries(i).entries_size();
  }

  return num_updated_entries > kSnapShotThreshold;
}

bool UserDictionarySyncUtil::CreateUpdate(
    const UserDictionaryStorageBase &storage_old,
    const UserDictionaryStorageBase &storage_new,
    UserDictionaryStorageBase *update) {
  CHECK(update);
  update->Clear();

  // Find newly added sync dictionaries.
  for (int i = 0; i < storage_new.dictionaries_size(); ++i) {
    if (!storage_new.dictionaries(i).syncable()) {
      continue;
    }
    const int index = FindDictionary(storage_old,
                                     storage_new.dictionaries(i).name());
    if (index == -1) {
      UserDictionary *dictionary = update->add_dictionaries();
      DCHECK(dictionary);
      dictionary->CopyFrom(storage_new.dictionaries(i));
    }
  }

  // Find removed sync dictionaries.
  for (int i = 0; i < storage_old.dictionaries_size(); ++i) {
    if (!storage_old.dictionaries(i).syncable()) {
      continue;
    }
    const int index = FindDictionary(storage_new,
                                     storage_old.dictionaries(i).name());
    if (index == -1) {
      LOG(WARNING) << "We cannot delete sync dictionaries.";
      UserDictionary *dictionary = update->add_dictionaries();
      DCHECK(dictionary);
      dictionary->set_name(storage_old.dictionaries(i).name());
      dictionary->set_removed(true);   // set removed flag.
      dictionary->set_syncable(true);
    }
  }

  // Find dictionaries both in |storage_old| and |storage_new|.
  for (int i = 0; i < storage_new.dictionaries_size(); ++i) {
    if (!storage_new.dictionaries(i).syncable()) {
      continue;
    }
    const int index_old = FindDictionary(storage_old,
                                         storage_new.dictionaries(i).name());
    if (index_old >= 0) {
      if (!storage_old.dictionaries(index_old).syncable()) {
        continue;
      }
      const int index_new = i;
      UserDictionary *dictionary = update->add_dictionaries();
      DCHECK(dictionary);
      // name_new == name_old
      DCHECK_EQ(storage_new.dictionaries(index_new).name(),
                storage_old.dictionaries(index_old).name());
      CreateDictionaryUpdate(storage_old.dictionaries(index_old),
                             storage_new.dictionaries(index_new),
                             dictionary);
      // If there is no update, remove the last dictionary.
      if (dictionary->entries_size() == 0) {
        update->mutable_dictionaries()->RemoveLast();
      }
    }
  }

  update->set_storage_type(UserDictionaryStorageBase::UPDATE);

  return true;
}

namespace {
// merge |update| to the |dictionary|
void MergeDictionary(
    const UserDictionarySyncUtil::UserDictionary &update,
    UserDictionarySyncUtil::UserDictionary *dictionary) {
  DCHECK(dictionary);
  set<uint64> removed_set;
  for (int i = 0; i < update.entries_size(); ++i) {
    if (update.entries(i).removed()) {
      removed_set.insert(
          UserDictionarySyncUtil::EntryFingerprint(
              update.entries(i)));
    } else {
      // TODO(taku): if the entry is already in the
      // dictionary, we don't need to call add_entries().
      dictionary->add_entries()->CopyFrom(update.entries(i));
    }
  }

  if (removed_set.empty()) {
    VLOG(1) << "No removed entries found";
    return;
  }

  // Create a new dictionary which reflects |removed_set|.
  // TODO(taku): making a new dictionary from scratch might NOT be
  // an optimal solution when the size of remvoed_set is 1 or 2.
  UserDictionarySyncUtil::UserDictionary new_dictionary;
  new_dictionary.set_name(dictionary->name());
  new_dictionary.set_enabled(dictionary->enabled());
  new_dictionary.set_syncable(dictionary->syncable());

  for (int i = 0; i < dictionary->entries_size(); ++i) {
    if (removed_set.find(
            UserDictionarySyncUtil::EntryFingerprint(
                dictionary->entries(i))) == removed_set.end()) {
      new_dictionary.add_entries()->CopyFrom(dictionary->entries(i));
    }
  }

  dictionary->CopyFrom(new_dictionary);
}

// Delete |delete_index|-th dictionary from |storage|.
void DeleteDictionary(
    int delete_index,
    UserDictionarySyncUtil::UserDictionaryStorageBase *storage) {
  DCHECK(storage);
  RepeatedPtrField<UserDictionarySyncUtil::UserDictionary> *dics =
      storage->mutable_dictionaries();
  DCHECK(dics);

  UserDictionarySyncUtil::UserDictionary **data = dics->mutable_data();
  DCHECK(data);
  for (int i = delete_index; i < storage->dictionaries_size() - 1; ++i) {
    swap(data[i], data[i + 1]);
  }

  dics->RemoveLast();
}
}  // anonymous namespace

// Merge one |update| to the current |storage|
bool UserDictionarySyncUtil::MergeUpdate(
    const UserDictionaryStorageBase &update,
    UserDictionaryStorageBase *storage) {
  for (int i = 0; i < update.dictionaries_size(); ++i) {
    const UserDictionarySyncUtil::UserDictionary &update_dictionary
        = update.dictionaries(i);
    const int target_index = FindDictionary(*storage,
                                            update_dictionary.name());
    if (target_index >= 0) {   // found in the storage.
      UserDictionarySyncUtil::UserDictionary *dictionary =
          storage->mutable_dictionaries(target_index);
      DCHECK(dictionary);
      DCHECK_EQ(update_dictionary.name(), dictionary->name());

      if (update_dictionary.removed()) {
        LOG(WARNING) << "update is inconsistent. "
                     << "we cannot delete sync dicitonaries.";
        dictionary->clear_entries();
        dictionary->set_removed(true);
      } else {
        MergeDictionary(update_dictionary, dictionary);
      }
    } else {   // not found in the storage
      if (update_dictionary.removed()) {
        LOG(WARNING) << "update is incosistent. "
                     << "cannot find dictionary: " << update_dictionary.name();
      } else {
        UserDictionarySyncUtil::UserDictionary *dictionary
            = storage->add_dictionaries();
        DCHECK(dictionary);
        dictionary->CopyFrom(update_dictionary);
      }
    }
  }

  for (int i = storage->dictionaries_size() - 1; i >= 0 ; --i) {
    if (storage->dictionaries(i).removed()) {
      LOG(WARNING) << "We cannot remove sync dictionary.";
      // This function must fail.
      DeleteDictionary(i, storage);
    }
  }

  return true;
}

bool UserDictionarySyncUtil::MergeUpdates(
    const vector<UserDictionaryStorageBase *> &updates,
    UserDictionaryStorageBase *storage) {
  DCHECK(storage);

  if (updates.empty()) {
    return true;
  }

  int last_snapshot_index = -1;
  for (int i = static_cast<int>(updates.size()) - 1; i >= 0; --i) {
    if (updates[i]->storage_type() == UserDictionaryStorageBase::SNAPSHOT) {
      last_snapshot_index = i;
      break;
    }
  }

  // apply snapshot.
  if (last_snapshot_index != -1) {
    const UserDictionaryStorageBase *update = updates[last_snapshot_index];
    for (int i = 0; i < storage->dictionaries_size(); ++i) {
      UserDictionary *dictionary = storage->mutable_dictionaries(i);
      // sync dictionaries only for sync.
      if (!dictionary->syncable()) {
        continue;
      }

      int update_index = FindDictionary(*update, dictionary->name());
      if (update_index >= 0) {
        dictionary->CopyFrom(update->dictionaries(update_index));
      }
    }
  }

  // merge other updates.
  if (last_snapshot_index + 1 < updates.size()) {
    for (int i = last_snapshot_index + 1; i < updates.size(); ++i) {
      MergeUpdate((*updates[i]), storage);
    }
  }

  return true;
}

bool UserDictionarySyncUtil::VerifyLockAndSaveStorage(
    UserDictionaryStorage *storage) {
  DCHECK(storage);

  // Check dictionary storage condition.
  for (int i = 0; i < storage->dictionaries_size(); ++i) {
    const UserDictionary &dict = storage->dictionaries(i);
    if (dict.syncable() &&
        dict.entries_size() > UserDictionaryStorage::max_sync_entry_size()) {
      // This singleton is also used in sync_handler.cc.
      Singleton<SyncStatusManager>::get()->AddSyncError(
          commands::CloudSyncStatus::USER_DICTIONARY_NUM_ENTRY_EXCEEDED);
      LOG(ERROR) << "a sync dictionary has " << dict.entries_size()
                 << " entries which exceeds the limit.";
      return false;
    }
  }

  return LockAndSaveStorage(storage);
}

bool UserDictionarySyncUtil::LockAndSaveStorage(
    UserDictionaryStorage *storage) {
  DCHECK(storage);

  if (!storage->Lock()) {
    LOG(ERROR) << "cannot lock the storage: " << storage->filename();
    return false;
  }
  if (!storage->SaveCore()) {
    LOG(ERROR) << "cannot save the storage: " << storage->filename();
    storage->UnLock();
    return false;
  }
  storage->UnLock();
  return true;
}

}  // namespace sync
}  // namespace mozc
