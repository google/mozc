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

#ifndef MOZC_SYNC_SYNC_HANDLER_H_
#define MOZC_SYNC_SYNC_HANDLER_H_

#include "base/scheduler.h"
#include "session/commands.pb.h"

namespace mozc {
namespace sync {

class SyncHandler {
 public:
  // Sync() does the following four steps in sequence.
  // 1. calls SyncerInterface::Start() in the current thread.
  // 2  creates a new thread and executes SyncerInterface::Sync() method.
  //    Sync operation (network connections) is executed asynchronously.
  // 3. sends "Reload" IPC command to the main converter thread,
  //    if reload is required, i.e. there exists an update on the cloud.
  // 4. Signal process-wide named event. Config dialog can
  //    wait for the named event to know the time when the Sync()
  //    method finishes.
  // If a sync thread is already created in the step 2,
  // event is not be signaled, as currently running thread
  // will signal a event later.
  static bool Sync();

  // Clear() does the following two steps in sequence.
  // 1  creates a new thread and executes SyncerInterface::Clear() method.
  //    Clear operation (network connections) is executed asynchronously.
  // 2. Signal process-wide named event. Config dialog can
  //    wait for the named event
  //    to know the time when the Sync() method finishes.
  static bool Clear();

  // Wait Sync() or Clear() call until they finish.
  static void Wait();

  // Get the current cloud sync status and set it into the argument.
  static bool GetCloudSyncStatus(commands::CloudSyncStatus *cloud_sync_status);

  // Set the authorization information and store it into the local storage.
  static bool SetAuthorization(
      const commands::Input::AuthorizationInfo &authorization_info);

  // Setter and Getter for scheduler job
  static const Scheduler::JobSetting &GetSchedulerJobSetting();

  static void SetSchedulerJobSetting(const Scheduler::JobSetting *setting);

 private:
  SyncHandler() {}
  ~SyncHandler() {}
};
}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNCER_H_
