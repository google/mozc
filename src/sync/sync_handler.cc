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

#include "sync/sync_handler.h"

#include <string>
#include "base/base.h"
#include "base/mutex.h"
#include "base/scheduler.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "ipc/named_event.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_util.h"
#include "sync/sync_status_manager.h"
#include "sync/syncer_interface.h"
#include "client/client_interface.h"

DEFINE_int32(min_sync_interval, 120, "min sync interval");

namespace mozc {
namespace sync {
namespace {

// TODO(taku) move it to base/const.h
const char kEventName[] = "sync";

bool SyncFromScheduler(void *) {
  const config::Config &config = config::ConfigHandler::GetConfig();
  if (!config.has_sync_config()) {
    // In case of nosync, it just returns true to keep default backoff
    // (periodical running).
    return true;
  }
  return SyncHandler::Sync();
}

const uint32 kInterval = 3 * 60 * 60 * 1000;  // 3 hours
const uint32 kMaxInterval = 3 * 60 * 60 * 1000;  // 3 hours
const uint32 kDelay = 2 * 60 * 1000;  // 2 minutes
const uint32 kRandomDelay = 5 * 60 * 1000;  // 5 minutes

const Scheduler::JobSetting *g_sync_job_setting = NULL;

const Scheduler::JobSetting kDefaultSyncJobSetting = Scheduler::JobSetting(
    "CloudSync",
    kInterval,
    kMaxInterval,
    kDelay,
    kRandomDelay,
    &SyncFromScheduler,
    NULL);

const char kClearSyncName[] = "ClearCloudSync";

// This will block the scheduler thread randomly but not block the
// main thread.
bool ClearSyncFromScheduler(void *) {
  SyncHandler::Clear();
  SyncHandler::Wait();
  commands::CloudSyncStatus cloud_sync_status;
  SyncHandler::GetCloudSyncStatus(&cloud_sync_status);
  return cloud_sync_status.global_status() !=
      commands::CloudSyncStatus::SYNC_FAILURE;
}

const uint32 kDefaultIntervalForClear = 3 * 60 * 1000;  // 3 minutes
const uint32 kMaxIntervalForClear = 24 * 60 * 60 * 1000;  // 1 day = 24 hours
const uint32 kDelayForClear = 2 * 60 * 1000;  // 2 minutes
const uint32 kRandomDelayForClear = 1 * 60 * 1000;  // 1 minute

const Scheduler::JobSetting kClearSyncJobSetting = Scheduler::JobSetting(
    kClearSyncName,
    kDefaultIntervalForClear,
    kMaxIntervalForClear,
    kDelayForClear,
    kRandomDelayForClear,
    &ClearSyncFromScheduler,
    NULL);

void NotifyEvent() {
  NamedEventNotifier notifier(kEventName);
  VLOG(1) << "Notifiying named event: " << kEventName;
  if (!notifier.Notify()) {
    LOG(WARNING) << "cannot notify event: " << kEventName;
  }
}

void SendReloadCommand() {
  scoped_ptr<client::ClientInterface> client(
      client::ClientFactory::NewClient());
  DCHECK(client.get());
  client->Reload();
}

class SyncerThread: public Thread {
 public:
  SyncerThread()
      : oauth2_util_(OAuth2Client::GetDefaultClient()),
        last_sync_timestamp_(0) {
    SyncerFactory::SetOAuth2(&oauth2_util_);

    // Singleton of SyncStatusManager is also used in each sync_adapter and
    // each sync_util.
    sync_status_manager_ = Singleton<SyncStatusManager>::get();
  }

  virtual ~SyncerThread() {
    Join();
  }

  // This method is executed outside of the
  // main converter thread.
  void Run() {
    const uint64 current_timestamp = Util::GetTime();

    if (command_type_ == SYNC &&
        current_timestamp - last_sync_timestamp_ < FLAGS_min_sync_interval) {
      LOG(WARNING) << "New Sync command must be executed after "
                   << FLAGS_min_sync_interval << " interval sec.";
      NotifyEvent();
      return;
    }

    // We don't care the failure of refreshing token because the
    // existing token may be valid.
    oauth2_util_.RefreshAccessToken();

    // Clear sync errors before stacking new errors in syncers' works.
    sync_status_manager_->NewSyncStatusSession();

    switch (command_type_) {
      case SYNC:
        {
          sync_status_manager_->SetSyncGlobalStatus(
              commands::CloudSyncStatus::INSYNC);

          bool reload_required = false;
          bool sync_succeed = true;
          if (SyncerFactory::GetSyncer()->Sync(&reload_required)) {
            if (reload_required) {
              VLOG(1) << "Sending Reload command to the converter.";
              SendReloadCommand();
            }
          } else {
            LOG(WARNING) << "SyncerInterface::Sync() failed";
            sync_succeed = false;
          }

          {
            scoped_lock lock(&status_mutex_);
            sync_status_manager_->SetSyncGlobalStatus(
                sync_succeed ? commands::CloudSyncStatus::SYNC_SUCCESS :
                commands::CloudSyncStatus::SYNC_FAILURE);
            if (sync_succeed) {
              sync_status_manager_->SetLastSyncedTimestamp(current_timestamp);
            }
          }
          // Update last_sync_timestamp_
          last_sync_timestamp_ = current_timestamp;
        }
        break;
      case CLEAR:
        if (!SyncerFactory::GetSyncer()->Clear()) {
          LOG(WARNING) << "SyncerInterface::Clear() failed";
          // Invokes the clear command later in case of failure.
          // AddJob just ignores if there's already the same job.
          Scheduler::AddJob(kClearSyncJobSetting);
          sync_status_manager_->SetSyncGlobalStatus(
              commands::CloudSyncStatus::SYNC_FAILURE);

          // Set the command type to SYNC to allow the next Clear() method.
          command_type_ = SYNC;
          break;
        }

        {
          // Dummy auth does not have any specific authorization data,
          // so it clears the auth info compeletely and sets the sync
          // status to NOSYNC.
          commands::Input::AuthorizationInfo dummy_auth;
          SetAuthorization(dummy_auth);
        }
        sync_status_manager_->SetSyncGlobalStatus(
            commands::CloudSyncStatus::NOSYNC);
        Scheduler::RemoveJob(kClearSyncName);
        // Update last_sync_timestamp_
        last_sync_timestamp_ = current_timestamp;
        break;
      default:
        LOG(ERROR) << "Unknown command: " << command_type_;
        break;
    }

    // Save the final sync status in registry
    sync_status_manager_->SaveSyncStatus();

    // Emit a notification event to the caller of Sync|Clear method.
    NotifyEvent();
  }

  enum CommandType {
    SYNC,
    CLEAR,
  };

  bool StartSync() {
    if (IsRunning()) {
      LOG(WARNING) << "Sync|Clear command is already running";
      // Don't call NotifyEvent as currently running instance
      // will emit the event later.
      return true;
    }

    command_type_ = SYNC;

    if (!SyncerFactory::GetSyncer()->Start()) {
      LOG(ERROR) << "SyncerInterface::Start() failed";
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::SYNC_FAILURE);
      NotifyEvent();
      return false;
    }

    Start();
    return true;
  }

  bool StartClearSync() {
    if (command_type_ == CLEAR) {
      LOG(WARNING) << "Last command is already CLEAR.";
      return true;
    }

    if (IsRunning()) {
      LOG(INFO) << "SyncThread is running.  Wait for its end.";
      Wait();
    }

    commands::CloudSyncStatus cloud_sync_status;
    GetCloudSyncStatus(&cloud_sync_status);
    if (cloud_sync_status.global_status() ==
        commands::CloudSyncStatus::NOSYNC) {
      LOG(WARNING) << "Sync is not Running.";
      return true;
    }

    // Remove the sync config to stop further Sync after Clear.
    {
      config::Config config;
      config::ConfigHandler::GetConfig(&config);
      config.clear_sync_config();
      config::ConfigHandler::SetConfig(config);
    }

    command_type_ = CLEAR;
    Start();
    return true;
  }

  void Wait() {
    Join();
  }

  void GetCloudSyncStatus(commands::CloudSyncStatus *cloud_sync_status) {
    DCHECK(cloud_sync_status);
    if (IsRunning()) {
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::INSYNC);
    }
    sync_status_manager_->GetLastSyncStatus(cloud_sync_status);
  }

  void SetAuthorization(
      const commands::Input::AuthorizationInfo &authorization_info) {
    if (authorization_info.has_auth_code() &&
        !authorization_info.auth_code().empty()) {
      LOG(INFO) << authorization_info.DebugString();
      oauth2_util_.RequestAccessToken(authorization_info.auth_code());
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::INSYNC);
    } else {
      oauth2_util_.Clear();
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::NOSYNC);
    }
  }

 private:
  CommandType command_type_;
  Mutex status_mutex_;
  OAuth2Util oauth2_util_;
  uint64 last_sync_timestamp_;
  SyncStatusManagerInterface *sync_status_manager_;
};
}  // namespace

const Scheduler::JobSetting &SyncHandler::GetSchedulerJobSetting() {
  if (g_sync_job_setting == NULL) {
    return kDefaultSyncJobSetting;
  } else {
    return *g_sync_job_setting;
  }
}

void SyncHandler::SetSchedulerJobSetting(const Scheduler::JobSetting *setting) {
  g_sync_job_setting = setting;
}

bool SyncHandler::Sync() {
  return Singleton<SyncerThread>::get()->StartSync();
}

bool SyncHandler::Clear() {
  return Singleton<SyncerThread>::get()->StartClearSync();
}

void SyncHandler::Wait() {
  Singleton<SyncerThread>::get()->Wait();
}

bool SyncHandler::GetCloudSyncStatus(
    commands::CloudSyncStatus *cloud_sync_status) {
  Singleton<SyncerThread>::get()->GetCloudSyncStatus(cloud_sync_status);
  return true;
}

bool SyncHandler::SetAuthorization(
    const commands::Input::AuthorizationInfo &authorization_info) {
  Singleton<SyncerThread>::get()->SetAuthorization(authorization_info);
  return true;
}
}  // namespace sync
}  // namespace mozc
