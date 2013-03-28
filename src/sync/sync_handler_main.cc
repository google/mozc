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

#include <string>
#include <iostream>

#include "base/logging.h"
#include "base/system_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "sync/oauth2.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_server.h"
#include "sync/oauth2_util.h"
#include "sync/sync_handler.h"

DEFINE_string(service, "goopy", "service");
DEFINE_string(source, "ime-goopy", "source");
DEFINE_bool(clear, false, "clear data");
DEFINE_string(work_dir, "", "working directory");
DEFINE_bool(oauth2_login, false, "try to oauth2 login");
DEFINE_bool(oauth2_token_refresh, false,
            "refresh the stored oauth2 access token before start sync");
DEFINE_bool(sync_config, false, "sync configurations");
DEFINE_bool(sync_user_dictionary, false, "sync user dictionary");
DEFINE_bool(sync_user_history, false, "sync user conversion history");
DEFINE_bool(sync_contact_list, false, "sync contact list with dictionary");
DEFINE_bool(sync_all, false, "sync all sync-features");

using mozc::config::Config;
using mozc::config::ConfigHandler;
using mozc::config::SyncConfig;

namespace mozc {
namespace sync {

namespace {

bool SetConfigures() {
  // Set which features to sync in user configures.
  ConfigHandler::SetConfigFileName("memory://config.1.db");

  Config config;
  ConfigHandler::GetConfig(&config);
  if (FLAGS_sync_config || FLAGS_sync_user_dictionary ||
      FLAGS_sync_user_history || FLAGS_sync_contact_list || FLAGS_sync_all) {
    if (FLAGS_sync_all) {
      FLAGS_sync_config = true;
      FLAGS_sync_user_dictionary = true;
      FLAGS_sync_user_history = true;
      FLAGS_sync_contact_list = true;
    }

    SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_config_sync(FLAGS_sync_config);
    sync_config->set_use_user_dictionary_sync(FLAGS_sync_user_dictionary);
    sync_config->set_use_user_history_sync(FLAGS_sync_user_history);
    sync_config->set_use_contact_list_sync(FLAGS_sync_contact_list);
    ConfigHandler::SetConfig(config);
  }

  if (!config.has_sync_config()) {
    return false;
  }
  SyncConfig *sync_config = config.mutable_sync_config();
  return sync_config->use_config_sync() ||
      sync_config->use_user_dictionary_sync() ||
      sync_config->use_user_history_sync() ||
      sync_config->use_contact_list_sync();
}

bool OAuth2Login(OAuth2Util *oauth2) {
  string auth_token;
  cout << "Access " << oauth2->GetAuthenticateUri() << endl
       << "and enter the auth token: " << flush;
  cin >> auth_token;
  if (!auth_token.empty() &&
      oauth2->RequestAccessToken(auth_token) != OAuth2::kNone) {
    return false;
  }

  return true;
}

bool Sync(SyncHandler *sync_handler, bool use_oauth2) {
  LOG(INFO) << "Start syncing...";

  scoped_ptr<OAuth2Util> oauth2_util(
      new OAuth2Util(OAuth2Client::GetDefaultInstance(),
                     OAuth2Server::GetDefaultInstance()));

  if (FLAGS_oauth2_login && use_oauth2 && OAuth2Login(oauth2_util.get())) {
    sync_handler->SetOAuth2UtilForUnittest(oauth2_util.release());
  } else if (FLAGS_oauth2_token_refresh && use_oauth2 &&
             oauth2_util->RefreshAccessToken() == OAuth2::kNone) {
    sync_handler->SetOAuth2UtilForUnittest(oauth2_util.release());
  } else if (use_oauth2) {
    LOG(ERROR) << "Something goes wrong with oauth2";
    return false;
  } else {
    LOG(ERROR) << "Cannot authenticate";
    return false;
  }

  if (!SetConfigures()) {
    LOG(ERROR) << "No features are set to sync";
    return false;
  }

  if (FLAGS_clear) {
    return sync_handler->Clear();
  } else {
    return sync_handler->Sync();
  }

  return true;
}
}   // namespace
}   // namespace sync
}   // namespace mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  if (!FLAGS_work_dir.empty()) {
    mozc::SystemUtil::SetUserProfileDirectory(FLAGS_work_dir);
  }

  mozc::sync::SyncHandler sync_handler;

  mozc::commands::CloudSyncStatus dummy_status;
  sync_handler.GetCloudSyncStatus(&dummy_status);
  const bool use_oauth2 = (FLAGS_oauth2_login || FLAGS_oauth2_token_refresh);
  if (!mozc::sync::Sync(&sync_handler, use_oauth2)) {
    LOG(ERROR) << "sync failed";
  }

  sync_handler.Wait();

  return 0;
}
