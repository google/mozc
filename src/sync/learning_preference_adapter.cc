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

#include "sync/learning_preference_adapter.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "storage/registry.h"
#include "sync/learning_preference_sync_util.h"
#include "sync/sync.pb.h"
#include "sync/sync_util.h"

namespace mozc {
namespace sync {
namespace {
const uint32 kBucketSize = 512;
const uint32 kMaxEntriesSize = 128;
const char kLastDownloadTimestampKey[] =
    "sync.learning_preference_last_download_time";
}  // anonymous namespace

LearningPreferenceAdapter::LearningPreferenceAdapter()
    : local_update_time_(0) {
  ClearStorage();

  // TODO(peria): Make sure that these GetStorage() do not return NULL
  //     if FLAGS_user_history_rewriter in rewriter/rewriter.cc is true.
  //     It means UserSegmentHistoryRewriter and userBoundaryHistoryRewriter
  //     must be created before the call of this constructor.
  AddStorage(LearningPreference::Entry::USER_SEGMENT_HISTORY,
             UserSegmentHistoryRewriter::GetStorage());
  AddStorage(LearningPreference::Entry::USER_BOUNDARY_HISTORY,
             UserBoundaryHistoryRewriter::GetStorage());
}

LearningPreferenceAdapter::~LearningPreferenceAdapter() {}

bool LearningPreferenceAdapter::Start() {
  local_update_.Clear();

  const uint64 last_access_time = GetLastDownloadTimestamp();

  local_update_time_ = Util::GetTime();

  for (size_t i = 0; i < GetStorageSize(); ++i) {
    const Storage &storage = GetStorage(i);
    DCHECK(storage.lru_storage);
    if (storage.lru_storage == NULL) {
      LOG(ERROR) << "storage: " << storage.type << " is NULL";
      continue;
    }
    // TODO(taku): since storage.lru_storage is managed by
    // UserHistorySegmentRewriter or UserBoundaryHistoryRewriter,
    // storages may become NULL or invalid if the ower of them
    // are deleted. We have to care this case.
    LearningPreferenceSyncUtil::CreateUpdate(
        *(storage.lru_storage),
        storage.type,
        last_access_time,
        &local_update_);
  }

  return true;
}

bool LearningPreferenceAdapter::SetDownloadedItems(
    const ime_sync::SyncItems &items) {
  VLOG(1) << "Start SetDownloadedItems: " << items.size() << " items";

  if (items.size() == 0) {
    LOG(WARNING) << "No items found";
    return true;
  }

  LearningPreference remote_update;

  // Aggregate all remote updates.
  for (size_t i = 0; i < items.size(); ++i) {
    const ime_sync::SyncItem &item = items.Get(i);
    if (item.component() != component_id() ||
        !item.key().HasExtension(sync::LearningPreferenceKey::ext) ||
        !item.value().HasExtension(sync::LearningPreferenceValue::ext)) {
      continue;
    }
    const sync::LearningPreferenceValue &value =
        item.value().GetExtension(sync::LearningPreferenceValue::ext);

    if (!value.has_learning_preference()) {
      continue;
    }

    remote_update.MergeFrom(value.learning_preference());
  }

  if (remote_update.entries_size() == 0) {
    VLOG(1) << "No remote updates";
    return true;
  }

  for (size_t i = 0; i < GetStorageSize(); ++i) {
    const Storage &storage = GetStorage(i);
    if (storage.lru_storage == NULL) {
      LOG(ERROR) << "storage: " << storage.type << " is NULL";
      continue;
    }
    // make *.merge_pending file at this stage, as this logic
    // is executed outside of the main converter thread.
    // After sync finishes, the sync thread sends Reload commands
    // to the main converter thread. Main converter thread merges
    // the *.merge_pending files.
    //
    // TODO(taku): since storage.lru_storage is managed by
    // UserHistorySegmentRewriter or UserBoundaryHistoryRewriter,
    // storages may become NULL or invalid if the ower of them
    // are deleted. We have to care this case.
    LearningPreferenceSyncUtil::CreateMergePendingFile(
        *(storage.lru_storage),
        storage.type,
        remote_update);
  }

  return true;
}

bool LearningPreferenceAdapter::GetItemsToUpload(ime_sync::SyncItems *items) {
  DCHECK(items);

  // No need to update the file.
  if (local_update_.entries_size() == 0) {
    VLOG(1) << "No update found on the local.";
    return true;
  }

  sync::LearningPreferenceKey *key = NULL;
  sync::LearningPreferenceValue *value = NULL;
  DCHECK_GT(kMaxEntriesSize, 0);

  // Split all |local_update_| into small chunks. Each chunk has
  // at most |kMaxEntriesSize| entries. This treatment is required
  // for avoidng the case where one item has lots of entries.
  for (int i = 0; i < local_update_.entries_size(); ++i) {
    if (i % kMaxEntriesSize == 0) {
      ime_sync::SyncItem *item = items->Add();
      DCHECK(item);
      item->set_component(component_id());
      key = item->mutable_key()->MutableExtension(
          sync::LearningPreferenceKey::ext);
      value = item->mutable_value()->MutableExtension(
          sync::LearningPreferenceValue::ext);
      DCHECK(key);
      DCHECK(value);
      key->set_bucket_id(GetNextBucketId());
    }
    DCHECK(value);
    value->mutable_learning_preference()->add_entries()->CopyFrom(
        local_update_.entries(i));
  }

  local_update_.Clear();

  return true;
}

bool LearningPreferenceAdapter::MarkUploaded(
    const ime_sync::SyncItem& item, bool uploaded) {
  VLOG(1) << "Start MarkUploaded() uploaded=" << uploaded;

  if (item.component() != component_id() ||
      !item.key().HasExtension(sync::LearningPreferenceKey::ext) ||
      !item.value().HasExtension(sync::LearningPreferenceValue::ext)) {
    return false;
  }

  if (!uploaded) {
    return true;
  }

  if (!SetLastDownloadTimestamp(local_update_time_)) {
    LOG(ERROR) << "Cannot set synced time";
    return false;
  }

  return true;
}

bool LearningPreferenceAdapter::Clear() {
  if (!mozc::storage::Registry::Erase(kLastDownloadTimestampKey)) {
    LOG(ERROR) << "cannot erase: " << kLastDownloadTimestampKey;
  }
  return true;
}

void LearningPreferenceAdapter::AddStorage(LearningPreference::Entry::Type type,
                                           const LRUStorage *lru_storage) {
  if (lru_storage == NULL) {
    LOG(ERROR) << "LRUStorage is NULL";
    return;
  }

  Storage storage;
  storage.type = type;
  storage.lru_storage = lru_storage;
  storages_.push_back(storage);
}

void LearningPreferenceAdapter::ClearStorage() {
  storages_.clear();
}

size_t LearningPreferenceAdapter::GetStorageSize() const {
  return storages_.size();
}

const LearningPreferenceAdapter::Storage &
LearningPreferenceAdapter::GetStorage(size_t i) const {
  return storages_[i];
}

uint32 LearningPreferenceAdapter::bucket_size() const {
  DCHECK_GT(kBucketSize, 0);
  return kBucketSize;
}

const LearningPreference &
LearningPreferenceAdapter::local_update() const {
  return local_update_;
}

LearningPreference *
LearningPreferenceAdapter::mutable_local_update() {
  return &local_update_;
}

uint32 LearningPreferenceAdapter::GetNextBucketId() const {
  // randomly select one bucket.
  // TODO(taku): have to care the case where duplicated ids are used.
  uint64 id = 0;
  if (!Util::GetSecureRandomSequence(
          reinterpret_cast<char *>(&id), sizeof(id))) {
    LOG(ERROR) << "GetSecureRandomSequence() failed. use rand()";
    id = static_cast<uint64>(Util::Random(RAND_MAX));
  }

  return static_cast<uint32>(id % bucket_size());
}

bool LearningPreferenceAdapter::SetLastDownloadTimestamp(
    uint64 last_download_time) {
  if (!mozc::storage::Registry::Insert(kLastDownloadTimestampKey,
                                       last_download_time) ||
      !mozc::storage::Registry::Sync()) {
    LOG(ERROR) << "cannot save: "
               << kLastDownloadTimestampKey << " " << last_download_time;
    return false;
  }
  return true;
}

uint64 LearningPreferenceAdapter::GetLastDownloadTimestamp() const {
  uint64 last_download_time = 0;
  if (!mozc::storage::Registry::Lookup(kLastDownloadTimestampKey,
                                       &last_download_time)) {
    LOG(ERROR) << "cannot read: " << kLastDownloadTimestampKey;
    return static_cast<uint64>(0);
  }
  return last_download_time;
}

ime_sync::Component LearningPreferenceAdapter::component_id() const {
  return ime_sync::MOZC_LEARNING_PREFERENCE;
}
}  // namespace sync
}  // namespace mozc
