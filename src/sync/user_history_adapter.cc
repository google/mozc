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
#include <string>
#include <vector>

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "storage/registry.h"
#include "sync/user_history_sync_util.h"
#include "sync/sync.pb.h"
#include "sync/sync_util.h"

namespace mozc {
namespace sync {
namespace {
const uint32 kBucketSize = 1024;
const uint32 kMaxEntriesSize = 256;
const char kLastDownloadTimestampKey[] = "sync.user_history_last_download_time";

class RealClockTimer : public ClockTimerInterface {
 public:
  uint64 GetCurrentTime() const {
    return static_cast<uint64>(time(NULL));
  }
};
}  // anonymous namespace

UserHistoryAdapter::UserHistoryAdapter()
    : clock_timer_(NULL), local_update_time_(0) {
  // set default user dictionary
  SetUserHistoryFileName(UserHistoryPredictor::GetUserHistoryFileName());
}

UserHistoryAdapter::~UserHistoryAdapter() {}

bool UserHistoryAdapter::SetDownloadedItems(
    const ime_sync::SyncItems &items) {
  vector<const UserHistorySyncUtil::UserHistory *> remote_updates;

  VLOG(1) << "Start SetDownloadedItems: " << items.size() << " items";

  if (items.size() == 0) {
    LOG(WARNING) << "No items found";
    return true;
  }

  // Aggregate all remote updates.
  for (size_t i = 0; i < items.size(); ++i) {
    const ime_sync::SyncItem &item = items.Get(i);
    if (item.component() != component_id() ||
        !item.key().HasExtension(sync::UserHistoryKey::ext) ||
        !item.value().HasExtension(sync::UserHistoryValue::ext)) {
      continue;
    }
    const sync::UserHistoryValue &value =
        item.value().GetExtension(sync::UserHistoryValue::ext);

    if (!value.has_user_history()) {
      continue;
    }

    remote_updates.push_back(&(value.user_history()));
  }

  VLOG(1) << remote_updates.size() << " remote_updates found";

  if (remote_updates.empty()) {
    return true;
  }

  UserHistoryStorage storage(GetUserHistoryFileName());
  storage.Load();

  UserHistorySyncUtil::MergeUpdates(remote_updates, &storage);

  if (!storage.Save()) {
    LOG(ERROR) << "cannot save new storage";
    return false;
  }

  return true;
}

bool UserHistoryAdapter::GetItemsToUpload(ime_sync::SyncItems *items) {
  DCHECK(items);

  local_update_time_ =
      clock_timer_ != NULL ?
      clock_timer_ ->GetCurrentTime() :
      Singleton<RealClockTimer>::get()->GetCurrentTime();

  if (!Util::FileExists(GetUserHistoryFileName())) {
    LOG(WARNING) << GetUserHistoryFileName() << " does not exist.";
    return true;
  }

  // Obtain local update.
  UserHistorySyncUtil::UserHistory local_update;
  {
    UserHistoryStorage storage(GetUserHistoryFileName());
    storage.Load();
    const uint64 last_download_time = GetLastDownloadTimestamp();
    UserHistorySyncUtil::CreateUpdate(storage, last_download_time,
                                      &local_update);
  }

  // No need to update the file.
  if (local_update.entries_size() == 0) {
    VLOG(1) << "No update found on the local.";
    return true;
  }

  sync::UserHistoryKey *key = NULL;
  sync::UserHistoryValue *value = NULL;
  DCHECK_GT(kMaxEntriesSize, 0);

  // Split all |local_updates| into small chunks. Each chunk has
  // at most |kMaxEntriesSize| entries. This treatment is required
  // for avoidng the case where one item has lots of entries.
  for (int i = 0; i < local_update.entries_size(); ++i) {
    if (i % kMaxEntriesSize == 0) {
      ime_sync::SyncItem *item = items->Add();
      DCHECK(item);
      item->set_component(component_id());
      key = item->mutable_key()->MutableExtension(
          sync::UserHistoryKey::ext);
      value = item->mutable_value()->MutableExtension(
          sync::UserHistoryValue::ext);
      DCHECK(key);
      DCHECK(value);
      key->set_bucket_id(GetNextBucketId());
    }
    DCHECK(value);
    value->mutable_user_history()->add_entries()->CopyFrom(
        local_update.entries(i));
  }

  return true;
}

bool UserHistoryAdapter::MarkUploaded(
    const ime_sync::SyncItem& item, bool uploaded) {
  VLOG(1) << "Start MarkUploaded() uploaded=" << uploaded;

  if (item.component() != component_id() ||
      !item.key().HasExtension(sync::UserHistoryKey::ext) ||
      !item.value().HasExtension(sync::UserHistoryValue::ext)) {
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

bool UserHistoryAdapter::Clear() {
  if (!mozc::storage::Registry::Erase(kLastDownloadTimestampKey)) {
    LOG(ERROR) << "cannot erase: " << kLastDownloadTimestampKey;
  }
  return true;
}

void UserHistoryAdapter::SetUserHistoryFileName(const string &filename) {
  VLOG(1) << "Setting UserHistoryFileName: " << filename;
  user_history_filename_ = filename;
}

string UserHistoryAdapter::GetUserHistoryFileName() const {
  return user_history_filename_;
}

uint32 UserHistoryAdapter::bucket_size() const {
  DCHECK_GT(kBucketSize, 0);
  return kBucketSize;
}

uint32 UserHistoryAdapter::GetNextBucketId() const {
  // randomly select one bucket.
  // TODO(taku): have to care the case where duplicated ids are used.
  uint64 id = 0;
  if (!Util::GetSecureRandomSequence(
          reinterpret_cast<char *>(&id), sizeof(id))) {
    LOG(ERROR) << "GetSecureRandomSequence() failed. use rand()";
    id = static_cast<uint64>(rand());
  }

  return static_cast<uint32>(id % bucket_size());
}

bool UserHistoryAdapter::SetLastDownloadTimestamp(uint64 last_download_time) {
  if (!mozc::storage::Registry::Insert(kLastDownloadTimestampKey,
                                       last_download_time) ||
      !mozc::storage::Registry::Sync()) {
    LOG(ERROR) << "cannot save: "
               << kLastDownloadTimestampKey << " " << last_download_time;
    return false;
  }
  return true;
}

uint64 UserHistoryAdapter::GetLastDownloadTimestamp() const {
  uint64 last_download_time = 0;
  if (!mozc::storage::Registry::Lookup(kLastDownloadTimestampKey,
                                       &last_download_time)) {
    LOG(ERROR) << "cannot read: " << kLastDownloadTimestampKey;
    return static_cast<uint64>(0);
  }
  return last_download_time;
}

void UserHistoryAdapter::SetClockTimerInterface(
    ClockTimerInterface *clock_timer) {
  DCHECK(clock_timer);
  clock_timer_ = clock_timer;
}

ime_sync::Component UserHistoryAdapter::component_id() const {
  return ime_sync::MOZC_USER_HISTORY_PREDICTION;
}
}  // namespace sync
}  // namespace mozc
