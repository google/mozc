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

#include <algorithm>
#include <string>

#include "base/base.h"
#include "base/util.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "storage/registry.h"
#include "sync/user_dictionary_sync_util.h"
#include "sync/sync.pb.h"
#include "sync/sync_util.h"

namespace mozc {
namespace sync {
namespace {
const uint32 kBucketSize = 256;
const char kLastBucketIdKey[] = "sync.user_dictionary_last_bucket_id";
}  // anonymous namespace

UserDictionaryAdapter::UserDictionaryAdapter() {
  // set default user dictionary
  SetUserDictionaryFileName(UserDictionaryUtil::GetUserDictionaryFileName());
}

UserDictionaryAdapter::~UserDictionaryAdapter() {}

bool UserDictionaryAdapter::SetDownloadedItems(
    const ime_sync::SyncItems &items) {
  vector<const UserDictionarySyncUtil::UserDictionaryStorageBase *>
      remote_updates;

  VLOG(1) << "Start SetDownloadedItems: " << items.size() << " items";

  if (items.size() == 0) {
    LOG(WARNING) << "No items found";
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
      LOG(ERROR) << "value has no user_dictionary_storage";
      continue;
    }

    if (!key.has_bucket_id()) {
      LOG(ERROR) << "value has no bucket_id";
      continue;
    }

    remote_updates.push_back(&(value.user_dictionary_storage()));
    bucket_id = key.bucket_id();
  }

  if (bucket_id != kuint32max && !SetBucketId(bucket_id)) {
    LOG(ERROR) << "cannot save bucket id";
    return false;
  }

  const string prev_file = GetLastSyncedUserDictionaryFileName();
  const string cur_file = GetUserDictionaryFileName();

  if (Util::IsEqualFile(prev_file, cur_file)) {
    if (remote_updates.empty()) {
      VLOG(1) << "No local_update and no remote_updates.";
      return true;
    }

    VLOG(1) << "No local_update and has remote_updates.";
    UserDictionaryStorage cur_storage(cur_file);
    cur_storage.Load();
    UserDictionarySyncUtil::MergeUpdates(remote_updates, &cur_storage);
    if (!UserDictionarySyncUtil::LockAndSaveStorage(&cur_storage)) {
      return false;
    }
    if (!SyncUtil::CopyLastSyncedFile(cur_file, prev_file)) {
      LOG(ERROR) << "cannot copy " << cur_file << " to " << prev_file;
      return false;
    }
  } else {   // Updates found on the local.
    if (remote_updates.empty()) {
      VLOG(1) << "Has local_update and no remote_updates.";
      return true;
    }

    // In this case, we simply merge the |local_update| and |remote_updates|.
    VLOG(1) << "Has local_update and has remote_updates.";

    UserDictionaryStorage prev_storage(prev_file);
    prev_storage.Load();

    UserDictionaryStorage cur_storage(cur_file);
    cur_storage.Load();

    // Obtain local update.
    UserDictionarySyncUtil::UserDictionaryStorageBase local_update;
    UserDictionarySyncUtil::CreateUpdate(prev_storage, cur_storage,
                                         &local_update);

    if (local_update.dictionaries_size() == 0) {
      // no updates are found on the local.
      UserDictionarySyncUtil::MergeUpdates(remote_updates, &cur_storage);
      if (!UserDictionarySyncUtil::LockAndSaveStorage(&cur_storage)) {
        return false;
      }
      VLOG(1) << "copying " << cur_file << " to " << prev_file;
      if (!SyncUtil::CopyLastSyncedFile(cur_file, prev_file)) {
        LOG(ERROR) << "cannot copy " << cur_file << " to " << prev_file;
        return false;
      }
    } else {
      // This case causes a conflict, so we make a backup just in case.
      if (!Util::CopyFile(cur_storage.filename(),
                          cur_storage.filename() + ".bak")) {
        LOG(ERROR) << "cannot make backup file";
      }

      // First, apply the |remote_updates| to the previous storage.
      // |prev_storage| only reflects the |remote_updates|.
      UserDictionarySyncUtil::MergeUpdates(remote_updates, &prev_storage);

      // We apply the |remote_updates| and |local_update| to
      // the prev_storage. It can be seen as an approximation of
      // mixing |remote_updates| and |local_update|, it is not
      // perfect though.
      cur_storage.CopyFrom(prev_storage);
      UserDictionarySyncUtil::MergeUpdate(local_update, &cur_storage);

      if (!UserDictionarySyncUtil::LockAndSaveStorage(&cur_storage)) {
        return false;
      }
      if (!UserDictionarySyncUtil::LockAndSaveStorage(&prev_storage)) {
        return false;
      }
    }
  }

  return true;
}

bool UserDictionaryAdapter::GetItemsToUpload(ime_sync::SyncItems *items) {
  DCHECK(items);
  VLOG(1) << "Start GetItemsToUpload()";

  if (!Util::FileExists(GetUserDictionaryFileName())) {
    LOG(WARNING) << GetUserDictionaryFileName() << " does not exist.";
    return true;
  }

  const string prev_file = GetLastSyncedUserDictionaryFileName();
  const string cur_file = GetUserDictionaryFileName();

  // No updates found on the local.
  if (Util::IsEqualFile(prev_file, cur_file)) {
    VLOG(1) << "No changes found in local dictionary files.";
    return true;
  }

  UserDictionaryStorage prev_storage(prev_file);
  prev_storage.Load();

  UserDictionaryStorage cur_storage(cur_file);
  cur_storage.Load();

  // No updates found on the local.
  if (UserDictionarySyncUtil::IsEqualStorage(prev_storage, cur_storage)) {
    VLOG(1) << "No need to upload updates.";
    return true;
  }

  // tmp_file is a 'pending' last synced dictionary.
  // Here we make a temporary file so that we can rollback the
  // last synced file if upload is failed.
  const string tmp_file = GetTempLastSyncedUserDictionaryFileName();
  if (!SyncUtil::CopyLastSyncedFile(cur_file, tmp_file)) {
    LOG(ERROR) << "cannot copy " << cur_file << " to " << tmp_file;
    return false;
  }

  ime_sync::SyncItem *item = items->Add();
  CHECK(item);

  item->set_component(component_id());

  sync::UserDictionaryKey *key =
      item->mutable_key()->MutableExtension(
          sync::UserDictionaryKey::ext);
  sync::UserDictionaryValue *value =
      item->mutable_value()->MutableExtension(
          sync::UserDictionaryValue::ext);
  CHECK(key);
  CHECK(value);

  UserDictionarySyncUtil::UserDictionaryStorageBase *local_update =
      value->mutable_user_dictionary_storage();
  CHECK(local_update);

  // Obtain local update.
  UserDictionarySyncUtil::CreateUpdate(prev_storage, cur_storage,
                                       local_update);

  // No need to update the file.
  if (local_update->dictionaries_size() == 0) {
    VLOG(1) << "No local update";
    Util::Unlink(tmp_file);
    items->RemoveLast();
    return true;
  }

  uint32 next_bucket_id = GetNextBucketId();

  // If the diff is too big or next_bucket_id is 0,
  // create a snapshot instead.
  if (next_bucket_id == 0 ||
      UserDictionarySyncUtil::ShouldCreateSnapshot(*local_update)) {
    VLOG(1) << "Start creating snapshot";
    // 0 is reserved for snapshot.
    next_bucket_id = 0;
    UserDictionarySyncUtil::CreateSnapshot(cur_storage, local_update);
  }

  key->set_bucket_id(next_bucket_id);

  return true;
}

bool UserDictionaryAdapter::MarkUploaded(
    const ime_sync::SyncItem& item, bool uploaded) {
  VLOG(1) << "Start MarkUploaded() uploaded=" << uploaded;

  const string prev_file = GetLastSyncedUserDictionaryFileName();
  const string tmp_file = GetTempLastSyncedUserDictionaryFileName();

  if (!uploaded) {
    // Rollback the last synced file by removing the pending file.
    Util::Unlink(tmp_file);
    return true;
  }

  // Push the pending last synced file atomically.
  VLOG(1) << "AtomicRename " << tmp_file << " to " << prev_file;
  if (!Util::AtomicRename(tmp_file, prev_file)) {
    LOG(ERROR) << "cannot update: " << prev_file;
    return false;
  }

  const uint32 next_bucket_id = GetNextBucketId();
  if (!SetBucketId(next_bucket_id)) {
    LOG(ERROR) << "cannot set bucket id";
    return false;
  }

  return true;
}

bool UserDictionaryAdapter::Clear() {
  Util::Unlink(GetLastSyncedUserDictionaryFileName());
  Util::Unlink(GetTempLastSyncedUserDictionaryFileName());
  return true;
}

ime_sync::Component UserDictionaryAdapter::component_id() const {
  return ime_sync::MOZC_USER_DICTIONARY;
}

void UserDictionaryAdapter::SetUserDictionaryFileName(const string &filename) {
  VLOG(1) << "Setting UserDictionaryFileName: " << filename;
  user_dictionary_filename_ = filename;
}

string UserDictionaryAdapter::GetUserDictionaryFileName() const {
  return user_dictionary_filename_;
}

string UserDictionaryAdapter::GetLastSyncedUserDictionaryFileName() const {
  const char kSuffix[] = ".last_synced";
#ifdef OS_WINDOWS
  return GetUserDictionaryFileName() + kSuffix;
#else
  const string dirname = Util::Dirname(GetUserDictionaryFileName());
  const string basename = Util::Basename(GetUserDictionaryFileName());
  return Util::JoinPath(dirname, "." + basename + kSuffix);
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
