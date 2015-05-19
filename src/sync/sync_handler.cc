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
#include "sync/logging.h"
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
  SYNC_VLOG(1) << "SyncHandler::Sync is called by Scheduler";
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
  SYNC_VLOG(1) << "SyncHandler::Clear is called by Scheduler";
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
  SYNC_VLOG(1) << "notifiying named event: " << kEventName;
  if (!notifier.Notify()) {
    LOG(WARNING) << "cannot notify event: " << kEventName;
  }
}

void SendReloadCommand() {
  scoped_ptr<client::ClientInterface> client(
      client::ClientFactory::NewClient());
  DCHECK(client.get());
  SYNC_VLOG(1) << "realoading server...";
  client->Reload();
  SYNC_VLOG(1) << "done. reloaded";
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

    SYNC_VLOG(1) << "current_timestamp: " << current_timestamp
                 << " last_sync_timestamp: " << last_sync_timestamp_;

    if (command_type_ == SYNC &&
        current_timestamp - last_sync_timestamp_ < FLAGS_min_sync_interval) {
      SYNC_VLOG(1) << "New Sync command must be executed after "
                   << FLAGS_min_sync_interval << " interval sec.";
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::WAITSYNC);
      Util::Sleep(1000 * (FLAGS_min_sync_interval -
                          current_timestamp + last_sync_timestamp_));
      // TODO(taku):
      // If possible, we might want to have another commands::CloudSyncStatus
      // like commands::CloudSyncStatus::WAITSYNC_COMPLETED.
      // when a crash happens at |oauth2_util_.RefreshAccessToken()|, the
      // sync log or global data says that we are still in
      // commands::CloudSyncStatus::WAITSYNC state despite that we completed
      // wait time successfully.
      // We should avoid such confusion by making a new state or carefully
      // coding this part with leaving some comments.
    }

    // We don't care the failure of refreshing token because the
    // existing token may be valid.
    oauth2_util_.RefreshAccessToken();

    // Clear sync errors other than authorization error before stacking new
    // errors in syncers' works.
    {
      commands::CloudSyncStatus sync_status;
      sync_status_manager_->GetLastSyncStatus(&sync_status);
      sync_status_manager_->NewSyncStatusSession();
      for (size_t i = 0; i < sync_status.sync_errors_size(); ++i) {
        const commands::CloudSyncStatus::SyncError &sync_error =
            sync_status.sync_errors(i);
        if (sync_error.error_code() ==
            commands::CloudSyncStatus::AUTHORIZATION_FAIL) {
          sync_status_manager_->AddSyncErrorWithTimestamp(
              sync_error.error_code(), sync_error.timestamp());
        }
      }
    }

    switch (command_type_) {
      case SYNC:
        {
          commands::CloudSyncStatus sync_status;
          sync_status_manager_->GetLastSyncStatus(&sync_status);
          if (sync_status.global_status() ==
              commands::CloudSyncStatus::NOSYNC) {
            break;
          }

          bool reload_required = false;
          bool sync_succeed = true;
          if (SyncerFactory::GetSyncer()->Sync(&reload_required)) {
            if (reload_required) {
              SYNC_VLOG(1) << "sending reload command to the converter.";
              SendReloadCommand();
            }
          } else {
            SYNC_VLOG(1) << "SyncerInterface::Sync() failed";
            sync_succeed = false;
          }

          {
            scoped_lock lock(&status_mutex_);
            sync_status_manager_->SetSyncGlobalStatus(
                sync_succeed ? commands::CloudSyncStatus::SYNC_SUCCESS :
                commands::CloudSyncStatus::SYNC_FAILURE);
            if (sync_succeed) {
              SYNC_VLOG(1) << "Updating last_synced_timestamp "
                           << "for sync_status_manager";
              sync_status_manager_->SetLastSyncedTimestamp(current_timestamp);
            }
          }
          // Update last_sync_timestamp_
          last_sync_timestamp_ = current_timestamp;
        }
        break;
      case CLEAR:
        if (!SyncerFactory::GetSyncer()->Clear()) {
          SYNC_VLOG(1) << "SyncerInterface::Clear() failed";
          // Invokes the clear command later in case of failure.
          // AddJob just ignores if there's already the same job.
          SYNC_VLOG(1) << "adding clear-job from Scheduler";
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
          SYNC_VLOG(1) << "clearing auth token";
          commands::Input::AuthorizationInfo dummy_auth;
          SetAuthorization(dummy_auth);
        }

        SYNC_VLOG(1) << "removing clear-job from Scheduler";
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
    SYNC_VLOG(1) << "saving new sync status";
    sync_status_manager_->SaveSyncStatus();

    // Emit a notification event to the caller of Sync|Clear method.
    SYNC_VLOG(1) << "sending notification event";
    NotifyEvent();

    SYNC_VLOG(1) << "last_sync_timestamp is updated: " << last_sync_timestamp_;
  }

  enum CommandType {
    SYNC,
    CLEAR,
  };

  bool StartSync() {
    SYNC_VLOG(1) << "start StartSync";

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
    SYNC_VLOG(1) << "start StartClearSync";

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
    SYNC_VLOG(1) << "Waiting syncer thread...";
    Join();
    SYNC_VLOG(1) << "done";
  }

  void GetCloudSyncStatus(commands::CloudSyncStatus *cloud_sync_status) {
    DCHECK(cloud_sync_status);
    VLOG(1) << "GetCloudSyncStatus is called";
    VLOG_IF(1, IsRunning()) << "cloud sync is running now";
    sync_status_manager_->GetLastSyncStatus(cloud_sync_status);
  }

  void SetAuthorization(
      const commands::Input::AuthorizationInfo &authorization_info) {
    SYNC_VLOG(1) << "SetAuthorization is called";
    if (authorization_info.has_auth_code() &&
        !authorization_info.auth_code().empty()) {
      SYNC_VLOG(1) << "setting authorization_info";
      LOG(INFO) << authorization_info.DebugString();
      if (oauth2_util_.RequestAccessToken(authorization_info.auth_code())) {
        sync_status_manager_->NewSyncStatusSession();
        sync_status_manager_->SetSyncGlobalStatus(
            commands::CloudSyncStatus::INSYNC);
      } else {
        SYNC_VLOG(1) << "authorization failed";
        sync_status_manager_->AddSyncError(
            commands::CloudSyncStatus::AUTHORIZATION_FAIL);
        sync_status_manager_->SetSyncGlobalStatus(
            commands::CloudSyncStatus::NOSYNC);
      }
    } else {
      SYNC_VLOG(1) << "clearing authorization_info";
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
