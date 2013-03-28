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

#include "sync/sync_status_manager.h"

#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/process_mutex.h"
#include "base/util.h"
#include "storage/registry.h"

namespace mozc {
namespace sync {

namespace {
const char kLastSyncedDataKey[] = "sync.last_synced_data";
}  // namespace

SyncStatusManager::SyncStatusManager() {
  scoped_lock lock(&mutex_);

  // Read the last status from registry or set default status.
  string value;
  if (storage::Registry::Lookup(kLastSyncedDataKey, &value)) {
    sync_status_.ParseFromArray(value.data(), value.size());
  } else {
    LOG(WARNING) << "cannot read: " << kLastSyncedDataKey;
    sync_status_.Clear();
    sync_status_.set_global_status(commands::CloudSyncStatus::NOSYNC);
  }
}

SyncStatusManager::~SyncStatusManager() {
  SaveSyncStatus();
}

void SyncStatusManager::GetLastSyncStatus(
    commands::CloudSyncStatus *sync_status) {
  DCHECK(sync_status);
  scoped_lock lock(&mutex_);
  sync_status->CopyFrom(sync_status_);
}

void SyncStatusManager::SetLastSyncStatus(
    const commands::CloudSyncStatus &sync_status) {
  scoped_lock lock(&mutex_);
  sync_status_.CopyFrom(sync_status);
}

void SyncStatusManager::SaveSyncStatus() {
  scoped_lock lock(&mutex_);

  if (!storage::Registry::Insert(
          kLastSyncedDataKey, sync_status_.SerializeAsString())) {
    LOG(ERROR) << "cannot save: "<< kLastSyncedDataKey;
  }
  storage::Registry::Sync();
}

void SyncStatusManager::SetLastSyncedTimestamp(const int64 timestamp) {
  scoped_lock lock(&mutex_);
  sync_status_.set_last_synced_timestamp(timestamp);
}

void SyncStatusManager::SetSyncGlobalStatus(
    const commands::CloudSyncStatus::SyncGlobalStatus global_status) {
  scoped_lock lock(&mutex_);
  sync_status_.set_global_status(global_status);
}

void SyncStatusManager::AddSyncError(
    const commands::CloudSyncStatus::ErrorCode error_code) {
  AddSyncErrorWithTimestamp(error_code, Util::GetTime());
}

void SyncStatusManager::AddSyncErrorWithTimestamp(
    const commands::CloudSyncStatus::ErrorCode error_code,
    const int64 timestamp) {
  scoped_lock lock(&mutex_);
  commands::CloudSyncStatus::SyncError *error = sync_status_.add_sync_errors();
  error->set_error_code(error_code);
  error->set_timestamp(timestamp);
}

void SyncStatusManager::NewSyncStatusSession() {
  scoped_lock lock(&mutex_);
  sync_status_.mutable_sync_errors()->Clear();
}

}  // namespace sync
}  // namespace mozc
