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

#ifndef MOZC_SYNC_SYNC_STATUS_MANAGER_INTERFACE_H_
#define MOZC_SYNC_SYNC_STATUS_MANAGER_INTERFACE_H_

#include "session/commands.pb.h"

namespace mozc {
namespace sync {

class SyncStatusManagerInterface {
 public:
  virtual ~SyncStatusManagerInterface() {}

  virtual void GetLastSyncStatus(commands::CloudSyncStatus *sync_status) = 0;

  // Updates the sync status as the one in the argument.
  virtual void SetLastSyncStatus(
      const commands::CloudSyncStatus &sync_status) = 0;

  // Save on-memory status into registry.
  virtual void SaveSyncStatus() = 0;

  // Each method below updates a part of sync status.
  // Set in |last_synced_timestamp|.
  virtual void SetLastSyncedTimestamp(const int64 timestamp) = 0;

  // Set in |sync_global_status|.
  virtual void SetSyncGlobalStatus(
      const commands::CloudSyncStatus::SyncGlobalStatus global_status) = 0;

  // Add a |sync_error| item, with error_code.
  virtual void AddSyncError(
      const commands::CloudSyncStatus::ErrorCode error_code) = 0;

  // Add a |sync_error| item, with error_code and a timestamp.
  virtual void AddSyncErrorWithTimestamp(
      const commands::CloudSyncStatus::ErrorCode error_code,
      const int64 timestamp) = 0;

  // Clear everything other than |sync_global_status| and
  // |last_synced_timestamp|.
  // TODO(peria): Stack whole status before clearing.
  virtual void NewSyncStatusSession() = 0;

  // TODO(peria): Stack sync_status for few syncs, and make methods to access
  // or to operate with old statuses.

 protected:
  SyncStatusManagerInterface() {}
};

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_STATUS_MANAGER_INTERFACE_H_
