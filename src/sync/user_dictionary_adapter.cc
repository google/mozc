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

#include "sync/user_dictionary_adapter.h"

#include <algorithm>
#include <string>

#include "base/base.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "storage/registry.h"
#include "sync/logging.h"
#include "sync/sync.pb.h"
#include "sync/sync_util.h"
#include "sync/user_dictionary_sync_util.h"

namespace mozc {
namespace sync {
namespace {

const uint32 kBucketSize = 256;
const char kLastBucketIdKey[] = "sync.user_dictionary_last_bucket_id";

class UserDictionaryStorageDeleter {
 public:
  explicit UserDictionaryStorageDeleter(
      vector<UserDictionarySyncUtil::UserDictionaryStorageBase *> *vec)
      : vec_(vec) {
  }
  ~UserDictionaryStorageDeleter() {
    STLDeleteElements(vec_);
  }
 private:
  vector<UserDictionarySyncUtil::UserDictionaryStorageBase *> *vec_;
  DISALLOW_COPY_AND_ASSIGN(UserDictionaryStorageDeleter);
};

}  // anonymous namespace

UserDictionaryAdapter::UserDictionaryAdapter()
    // set default user dictionary
    : user_dictionary_filename_(
        UserDictionaryUtil::GetUserDictionaryFileName()) {
}

UserDictionaryAdapter::~UserDictionaryAdapter() {}

bool UserDictionaryAdapter::SetDownloadedItems(
    const ime_sync::SyncItems &items) {
  vector<UserDictionarySyncUtil::UserDictionaryStorageBase *>
      remote_updates;
  UserDictionaryStorageDeleter deleter(&remote_updates);

  SYNC_VLOG(1) << "Start SetDownloadedItems: "
               << items.size() << " items";

  if (items.size() == 0) {
    SYNC_VLOG(1) << "No items found";
    return true;
  }

  // Aggregate all remote updates.
  uint32 bucket_id = kuint32max;
  for (size_t i = 0; i < items.size(); ++i) {
    const ime_sync::SyncItem &item = items.Get(i);
    if (item.component() != component_id() ||
        !item.key().HasExtension(sync::UserDictionaryKey::ext) ||
        !item.value().HasExtension(sync::UserDictionaryValue::ext)) {
      continue;
    }
    const sync::UserDictionaryKey &key =
        item.key().GetExtension(sync::UserDictionaryKey::ext);
    const sync::UserDictionaryValue &value =
        item.value().GetExtension(sync::UserDictionaryValue::ext);

    if (!value.has_user_dictionary_storage()) {
      SYNC_VLOG(1) << "value has no user_dictionary_storage";
      continue;
    }

    if (!key.has_bucket_id()) {
      SYNC_VLOG(1) << "value has no bucket_id";
      continue;
    }

    remote_updates.push_back(
        new UserDictionarySyncUtil::UserDictionaryStorageBase(
            value.user_dictionary_storage()));
    bucket_id = key.bucket_id();
  }

  if (bucket_id != kuint32max && !SetBucketId(bucket_id)) {
    SYNC_VLOG(1) << "cannot save bucket id";
    return false;
  }

  // Run migration code, because the incoming data from the server may in
  // the older format.
  for (size_t i = 0; i < remote_updates.size(); ++i) {
    UserDictionaryUtil::ResolveUnknownFieldSet(remote_updates[i]);
  }

  SYNC_VLOG(1) << "current backet_id=" << bucket_id;

  const string prev_file = GetLastSyncedUserDictionaryFileName();
  const string &cur_file = user_dictionary_filename();

  SYNC_VLOG(1) << "comparing " << prev_file << " with " << cur_file;
  if (FileUtil::IsEqualFile(prev_file, cur_file)) {
    if (remote_updates.empty()) {
      SYNC_VLOG(1) << "no local_update and no remote_updates.";
      return true;
    }

    SYNC_VLOG(1) << "no local_update and has remote_updates.";
    UserDictionaryStorage cur_storage(cur_file);
    cur_storage.Load();
    SYNC_VLOG(1) << "merging remote_updates to current storage.";
    UserDictionarySyncUtil::MergeUpdates(remote_updates, &cur_storage);
    if (!UserDictionarySyncUtil::VerifyLockAndSaveStorage(&cur_storage)) {
      SYNC_VLOG(1) << "cannot save cur_storage.";
      return false;
    }
    SYNC_VLOG(1) << "copying " << cur_file << " to " << prev_file;
    if (!SyncUtil::CopyLastSyncedFile(cur_file, prev_file)) {
      SYNC_VLOG(1) << "cannot copy " << cur_file << " to " << prev_file;
      return false;
    }
  } else {   // Updates found on the local.
    if (remote_updates.empty()) {
      SYNC_VLOG(1) << "has local_update and no remote_updates.";
      return true;
    }

    // In this case, we simply merge the |local_update| and |remote_updates|.
    SYNC_VLOG(1) << "has local_update and has remote_updates.";

    SYNC_VLOG(1) << "loading " << prev_file;
    UserDictionaryStorage prev_storage(prev_file);
    prev_storage.Load();

    SYNC_VLOG(1) << "loading " << cur_file;
    UserDictionaryStorage cur_storage(cur_file);
    cur_storage.Load();

    // Obtain local update.
    SYNC_VLOG(1) << "making local update";
    UserDictionarySyncUtil::UserDictionaryStorageBase local_update;
    UserDictionarySyncUtil::CreateUpdate(prev_storage, cur_storage,
                                         &local_update);

    if (local_update.dictionaries_size() == 0) {
      SYNC_VLOG(1) << "has no local_update in actual.";
      // no updates are found on the local.
      UserDictionarySyncUtil::MergeUpdates(remote_updates, &cur_storage);
      if (!UserDictionarySyncUtil::VerifyLockAndSaveStorage(&cur_storage)) {
        SYNC_VLOG(1) << "cannot save cur_storage.";
        return false;
      }
      SYNC_VLOG(1) << "copying " << cur_file << " to " << prev_file;
      if (!SyncUtil::CopyLastSyncedFile(cur_file, prev_file)) {
        SYNC_VLOG(1) << "cannot copy " << cur_file << " to " << prev_file;
        return false;
      }
    } else {
      // This case causes a conflict, so we make a backup just in case.
      SYNC_VLOG(1) << "making a backup " << cur_storage.filename() << ".bak";
      if (!FileUtil::CopyFile(cur_storage.filename(),
                              cur_storage.filename() + ".bak")) {
        SYNC_VLOG(1) << "cannot make backup file";
      }

      // First, apply the |remote_updates| to the previous storage.
      // |prev_storage| only reflects the |remote_updates|.
      SYNC_VLOG(1) << "merging remote_updates into prev_storage";
      UserDictionarySyncUtil::MergeUpdates(remote_updates, &prev_storage);

      // We apply the |remote_updates| and |local_update| to
      // the prev_storage. It can be seen as an approximation of
      // mixing |remote_updates| and |local_update|, it is not
      // perfect though.
      SYNC_VLOG(1) << "coping prev_storage into cur_storage";
      cur_storage.CopyFrom(prev_storage);

      SYNC_VLOG(1) << "merging local_update to cur_storage";
      UserDictionarySyncUtil::MergeUpdate(local_update, &cur_storage);

      SYNC_VLOG(1) << "saving cur_storage";
      if (!UserDictionarySyncUtil::VerifyLockAndSaveStorage(&cur_storage)) {
        SYNC_VLOG(1) << "cannot save cur_storage.";
        return false;
      }
      // Even if a sync dictionary of |prev_storage| exceeds its limit after
      // applying |remote_update| on prev_storage, we must save it. So we use
      // LockAndSaveStorage() without verifications. Please refer
      // http://b/5948831 for details.
      SYNC_VLOG(1) << "saving prev_storage";
      if (!UserDictionarySyncUtil::LockAndSaveStorage(&prev_storage)) {
        SYNC_VLOG(1) << "cannot save prev_storage.";
        return false;
      }
    }
  }

  return true;
}

bool UserDictionaryAdapter::GetItemsToUpload(ime_sync::SyncItems *items) {
  DCHECK(items);
  SYNC_VLOG(1) << "Start GetItemsToUpload()";

  if (!FileUtil::FileExists(user_dictionary_filename())) {
    SYNC_VLOG(1) << user_dictionary_filename() << " does not exist.";
    return true;
  }

  const string prev_file = GetLastSyncedUserDictionaryFileName();
  const string &cur_file = user_dictionary_filename();

  // No updates found on the local.
  if (FileUtil::IsEqualFile(prev_file, cur_file)) {
    SYNC_VLOG(1) << "No changes found in local dictionary files.";
    return true;
  }

  // Load raw data, (i.e. without migration code), because it should be the
  // data on the server.
  UserDictionaryStorage prev_storage(prev_file);
  prev_storage.LoadWithoutMigration();

  UserDictionaryStorage cur_storage(cur_file);
  cur_storage.Load();

  // No updates found on the local.
  if (UserDictionarySyncUtil::IsEqualStorage(prev_storage, cur_storage)) {
    SYNC_VLOG(1) << "No need to upload updates.";
    return true;
  }

  // tmp_file is a 'pending' last synced dictionary.
  // Here we make a temporary file so that we can rollback the
  // last synced file if upload is failed.
  const string tmp_file = GetTempLastSyncedUserDictionaryFileName();
  if (!SyncUtil::CopyLastSyncedFile(cur_file, tmp_file)) {
    SYNC_VLOG(1) << "cannot copy " << cur_file << " to " << tmp_file;
    return false;
  }


  UserDictionarySyncUtil::UserDictionaryStorageBase local_update;

  // Obtain local update.
  UserDictionarySyncUtil::CreateUpdate(
      prev_storage, cur_storage, &local_update);

  // No need to update the file.
  if (local_update.dictionaries_size() == 0) {
    SYNC_VLOG(1) << "No local update";
    FileUtil::Unlink(tmp_file);
    return true;
  }
  UserDictionaryUtil::FillDesktopDeprecatedPosField(&local_update);

  ime_sync::SyncItem *item = items->Add();
  CHECK(item);

  item->set_component(component_id());
  sync::UserDictionaryKey *key =
      item->mutable_key()->MutableExtension(sync::UserDictionaryKey::ext);
  CHECK(key);

  sync::UserDictionaryValue *value =
      item->mutable_value()->MutableExtension(sync::UserDictionaryValue::ext);
  CHECK(value);
  value->mutable_user_dictionary_storage()->Swap(&local_update);

  uint32 next_bucket_id = GetNextBucketId();

  // If the diff is too big or next_bucket_id is 0,
  // create a snapshot instead.
  if (next_bucket_id == 0 ||
      UserDictionarySyncUtil::ShouldCreateSnapshot(
          value->user_dictionary_storage())) {
    SYNC_VLOG(1) << "Start creating snapshot";
    // 0 is reserved for snapshot.
    next_bucket_id = 0;
    UserDictionarySyncUtil::CreateSnapshot(
        cur_storage, value->mutable_user_dictionary_storage());
    UserDictionaryUtil::FillDesktopDeprecatedPosField(
        value->mutable_user_dictionary_storage());
  }

  key->set_bucket_id(next_bucket_id);

  return true;
}

bool UserDictionaryAdapter::MarkUploaded(
    const ime_sync::SyncItem& item, bool uploaded) {
  SYNC_VLOG(1) << "Start MarkUploaded() uploaded=" << uploaded;

  const string prev_file = GetLastSyncedUserDictionaryFileName();
  const string tmp_file = GetTempLastSyncedUserDictionaryFileName();

  if (!uploaded) {
    // Rollback the last synced file by removing the pending file.
    SYNC_VLOG(1) << "rollbacking the last synced file: " << tmp_file;
    FileUtil::Unlink(tmp_file);
    return true;
  }

  // Push the pending last synced file atomically.
  SYNC_VLOG(1) << "AtomicRename " << tmp_file << " to " << prev_file;
  if (!FileUtil::AtomicRename(tmp_file, prev_file)) {
    SYNC_VLOG(1) << "cannot update: " << prev_file;
    return false;
  }

  const uint32 next_bucket_id = GetNextBucketId();
  SYNC_VLOG(1) << "updating next_bucket_id=" << next_bucket_id;
  if (!SetBucketId(next_bucket_id)) {
    SYNC_VLOG(1) << "cannot set bucket id";
    return false;
  }

  return true;
}

bool UserDictionaryAdapter::Clear() {
  SYNC_VLOG(1) << "start Clear()";
  FileUtil::Unlink(GetLastSyncedUserDictionaryFileName());
  FileUtil::Unlink(GetTempLastSyncedUserDictionaryFileName());
  return true;
}

ime_sync::Component UserDictionaryAdapter::component_id() const {
  return ime_sync::MOZC_USER_DICTIONARY;
}

string UserDictionaryAdapter::GetLastSyncedUserDictionaryFileName() const {
  const char kSuffix[] = ".last_synced";
#ifdef OS_WIN
  return user_dictionary_filename() + kSuffix;
#else
  const string dirname = FileUtil::Dirname(user_dictionary_filename());
  const string basename = FileUtil::Basename(user_dictionary_filename());
  return FileUtil::JoinPath(dirname, "." + basename + kSuffix);
#endif
}

string UserDictionaryAdapter::GetTempLastSyncedUserDictionaryFileName() const {
  const char kSuffix[] = ".pending";
  return GetLastSyncedUserDictionaryFileName() + kSuffix;
}

// static
uint32 UserDictionaryAdapter::bucket_size() const {
  return kBucketSize;
}

bool UserDictionaryAdapter::SetBucketId(uint32 bucket_id) {
  if (bucket_id >= bucket_size()) {
    LOG(ERROR) << "invalid bucket_id is given. reset to default";
  }
  if (!mozc::storage::Registry::Insert(kLastBucketIdKey, bucket_id) ||
      !mozc::storage::Registry::Sync()) {
    LOG(ERROR) << "cannot save: "
               << kLastBucketIdKey << " " << bucket_id;
    return false;
  }
  return true;
}

uint32 UserDictionaryAdapter::GetNextBucketId() const {
  uint32 value = 0;
  if (!mozc::storage::Registry::Lookup(kLastBucketIdKey, &value)) {
    LOG(ERROR) << "cannot read: " << kLastBucketIdKey;
    return static_cast<uint32>(0);
  }
  if (value >= bucket_size()) {
    LOG(ERROR) << "invalid bucket_id is saved. reset to default";
    return static_cast<uint32>(0);
  }
  return (value + 1) % bucket_size();
}

}  // namespace sync
}  // namespace mozc
