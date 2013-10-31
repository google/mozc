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

#include "sync/sync_handler.h"

#include <string>

#define SYNC_VLOG_MODULENAME "SyncHandler"

#include "base/logging.h"
#include "base/mutex.h"
#include "base/port.h"
#include "base/scheduler.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "sync/config_adapter.h"
#include "sync/logging.h"
#include "sync/mock_syncer.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_server.h"
#include "sync/oauth2_util.h"
#include "sync/sync_status_manager.h"
#include "sync/syncer.h"
#include "sync/user_dictionary_adapter.h"
#if !defined(OS_ANDROID) && !defined(__native_client__)
#include "client/client_interface.h"
#endif  // !OS_ANDROID && !__native_client__

DEFINE_int32(min_sync_interval, 120, "min sync interval");
DEFINE_string(sync_url,
              "",
              "sync url");

namespace mozc {
namespace sync {
namespace {

const uint32 kDefaultInterval = 3 * 60 * 60 * 1000;  // 3 hours
const uint32 kMaxInterval = 3 * 60 * 60 * 1000;  // 3 hours
const uint32 kDelay = 2 * 60 * 1000;  // 2 minutes
const uint32 kRandomDelay = 5 * 60 * 1000;  // 5 minutes

const char kDefaultSyncName[] = "CloudSync";

bool SyncFromScheduler(void *data) {
  SyncHandler *sync_handler = static_cast<SyncHandler *>(data);

  const config::Config &config = config::ConfigHandler::GetConfig();
  if (!config.has_sync_config()) {
    // In case of nosync, it just returns true to keep default backoff
    // (periodical running).
    return true;
  }
  SYNC_VLOG(1) << "SyncHandler::Sync is called by Scheduler";
  return sync_handler->Sync();
}

void SendReloadCommand() {
#if !defined(OS_ANDROID) && !defined(__native_client__)
  scoped_ptr<client::ClientInterface> client(
      client::ClientFactory::NewClient());
  DCHECK(client.get());
  SYNC_VLOG(1) << "realoading server...";
  client->Reload();
  SYNC_VLOG(1) << "done. reloaded";
#endif  // !OS_ANDROID && !__native_client__
}

}  // namespace

SyncHandler::SyncHandler()
    : ALLOW_THIS_IN_INITIALIZER_LIST(cloud_sync_job_setting_(
          kDefaultSyncName,
          kDefaultInterval,
          kMaxInterval,
          kDelay,
          kRandomDelay,
          &SyncFromScheduler,
          this)),
      config_adapter_(new ConfigAdapter),
      user_dictionary_adapter_(new UserDictionaryAdapter),
      oauth2_util_(new OAuth2Util(OAuth2Client::GetDefaultInstance(),
                                  OAuth2Server::GetDefaultInstance())),
      last_sync_timestamp_(0),
      reload_required_timestamp_(0) {
  // Singleton of SyncStatusManager is also used in each sync_adapter and
  // each sync_util.
  sync_status_manager_ = Singleton<SyncStatusManager>::get();

  InitializeSyncer();
}

SyncHandler::~SyncHandler() {
  Join();
  Scheduler::RemoveJob(kDefaultSyncName);
}

void SyncHandler::InitializeSyncer() {
  syncer_.reset(new MockSyncer);
}

void SyncHandler::Run() {
  const uint64 current_timestamp = Util::GetTime();

  SYNC_VLOG(1) << "current_timestamp: " << current_timestamp
               << " last_sync_timestamp: " << last_sync_timestamp_;

  if (current_timestamp - last_sync_timestamp_ < FLAGS_min_sync_interval) {
    // TODO(hsumita): Register new scheduler job and exit instead of Sleep().
    SYNC_VLOG(1) << "New Sync command must be executed after "
                 << FLAGS_min_sync_interval << " interval sec.";
    sync_status_manager_->SetSyncGlobalStatus(
        commands::CloudSyncStatus::WAITSYNC);
    Util::Sleep(1000 * (FLAGS_min_sync_interval -
                        current_timestamp + last_sync_timestamp_));
    // TODO(taku):
    // If possible, we might want to have another commands::CloudSyncStatus
    // like commands::CloudSyncStatus::WAITSYNC_COMPLETED.
    // when a crash happens at |oauth2_util_->RefreshAccessToken()|, the
    // sync log or global data says that we are still in
    // commands::CloudSyncStatus::WAITSYNC state despite that we completed
    // wait time successfully.
    // We should avoid such confusion by making a new state or carefully
    // coding this part with leaving some comments.
  }

  OAuth2::Error error = OAuth2::kNone;

  if (oauth2_util_->GetClientType() == INSTALLED_APP) {
    // RefreshAccessToken is only needed for installed application client.
    error = oauth2_util_->RefreshAccessToken();
  }

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

  // Stop Sync if authorization fails.
  if (error == OAuth2::kInvalidGrant) {
    SYNC_VLOG(1) << "Refreshing tokens fails with invalid grant.";

    // Dummy auth does not have any specific authorization data,
    // so it clears the auth info compeletely and sets the sync
    // status to NOSYNC.
    SYNC_VLOG(1) << "clearing auth token, it is no more in use.";
    commands::Input::AuthorizationInfo dummy_auth;
    SetAuthorization(dummy_auth);

    // Don't stop scheduler job to re-enable sync feature with valid access
    // token.
    return;
  }

  commands::CloudSyncStatus sync_status;
  sync_status_manager_->GetLastSyncStatus(&sync_status);
  if (sync_status.global_status() != commands::CloudSyncStatus::NOSYNC) {
    bool reload_required = false;
    bool sync_succeed = true;
    if (!syncer_->Sync(&reload_required)) {
      SYNC_VLOG(1) << "SyncerInterface::Sync() failed";
      sync_succeed = false;
    }
    if (reload_required) {
      SYNC_VLOG(1) << "sending reload command to the converter.";
      SendReloadCommand();
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
      if (reload_required) {
        reload_required_timestamp_ = current_timestamp;
      }
    }
    // Update last_sync_timestamp_
    last_sync_timestamp_ = current_timestamp;
  }

  // Save the final sync status in registry
  SYNC_VLOG(1) << "saving new sync status";
  sync_status_manager_->SaveSyncStatus();

  SYNC_VLOG(1) << "last_sync_timestamp is updated: " << last_sync_timestamp_;
}

bool SyncHandler::Sync() {
  SYNC_VLOG(1) << "start Sync";

  if (IsRunning()) {
    LOG(WARNING) << "Sync is already running";
    return true;
  }

  // Acquire lock since Sync() is called by converter thread and scheduler.
  scoped_try_lock lock(&sync_mutex_);
  if (!lock.locked()) {
    LOG(WARNING) << "Failed to acquire lock. Sync is already running.";
    return true;
  }

  if (!syncer_->Start()) {
    LOG(ERROR) << "SyncerInterface::Start() failed";
    sync_status_manager_->SetSyncGlobalStatus(
        commands::CloudSyncStatus::SYNC_FAILURE);
    return false;
  }

  Start();
  return true;
}

void SyncHandler::Wait() {
  SYNC_VLOG(1) << "Waiting syncer thread...";
  Join();
  SYNC_VLOG(1) << "done";
}

bool SyncHandler::GetCloudSyncStatus(
    commands::CloudSyncStatus *cloud_sync_status) {
  DCHECK(cloud_sync_status);
  VLOG(1) << "GetCloudSyncStatus is called";
  VLOG_IF(1, IsRunning()) << "cloud sync is running now";
  sync_status_manager_->GetLastSyncStatus(cloud_sync_status);
  return true;
}

bool SyncHandler::SetAuthorization(
    const commands::Input::AuthorizationInfo &authorization_info) {
  SYNC_VLOG(1) << "SetAuthorization is called";
  if (oauth2_util_->GetClientType() == CHROME_APP) {
#ifdef __native_client__
    if (authorization_info.has_access_token() &&
        !authorization_info.access_token().empty()) {
      // In NaCl Mozc, access token which is gotten using Chrome Identity
      // JavaScript API is set in access_token.
      // But we don't need to save it. Just checks if it is not empty here.
      // TODO(horo): It may be better to check if this access_token is really
      //             valid using IME Sync API.
      sync_status_manager_->NewSyncStatusSession();
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::INSYNC);
    } else {
      syncer_->ClearLocal();
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::NOSYNC);
    }
    return true;
#else  // __native_client__
    LOG(FATAL) << "CHROME_APP client type is only supported in NaCl Mozc";
    return false;
#endif  // __native_client__
  }
  if (authorization_info.has_auth_code() &&
      !authorization_info.auth_code().empty()) {
    SYNC_VLOG(1) << "setting authorization_info";
    LOG(INFO) << authorization_info.DebugString();
    const OAuth2::Error error =
        oauth2_util_->RequestAccessToken(authorization_info.auth_code());
    if (error == OAuth2::kNone) {
      sync_status_manager_->NewSyncStatusSession();
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::INSYNC);
    } else {
      SYNC_VLOG(1) << "authorization failed. Error: " << error;
      syncer_->ClearLocal();
      sync_status_manager_->AddSyncError(
          commands::CloudSyncStatus::AUTHORIZATION_FAIL);
      sync_status_manager_->SetSyncGlobalStatus(
          commands::CloudSyncStatus::NOSYNC);
    }
  } else {
    SYNC_VLOG(1) << "clearing authorization_info";
    oauth2_util_->Clear();
    syncer_->ClearLocal();
    sync_status_manager_->SetSyncGlobalStatus(
        commands::CloudSyncStatus::NOSYNC);
  }
  return true;
}

const Scheduler::JobSetting &SyncHandler::GetSchedulerJobSetting() const {
  return cloud_sync_job_setting_;
}

void SyncHandler::SetSyncerForUnittest(SyncerInterface *syncer) {
  syncer_.reset(syncer);
}

void SyncHandler::SetOAuth2UtilForUnittest(OAuth2Util *oauth2_util) {
  oauth2_util_.reset(oauth2_util);
}

uint64 SyncHandler::GetReloadRequiredTimestamp() {
  scoped_lock lock(&status_mutex_);
  return reload_required_timestamp_;
}

}  // namespace sync
}  // namespace mozc
