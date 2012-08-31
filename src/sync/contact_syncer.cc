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

#include "sync/contact_syncer.h"

#include "base/base.h"
#include "base/logging.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "storage/registry.h"
#include "sync/contact_list_util.h"
#include "sync/oauth2.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_util.h"
#include "sync/sync_util.h"
#include "sync/user_dictionary_sync_util.h"

namespace mozc {
namespace sync {

using config::Config;
using config::ConfigHandler;
using config::SyncConfig;

using user_dictionary::UserDictionary;

namespace {
const char kGdataLastDownloadTimeKey[] = "gdata.last_download_time";
const char kContactResourceUri[] = "https://www.google.com/m8/feeds/"
                                   "contacts/default/full";
const char kScope[] = "https://www.google.com/m8/feeds/";
const char kContactsDictionaryName[] = "UserContacts";

bool CheckConfigureToSyncContactList() {
  // Check configuration to sync contact list.
  Config config = ConfigHandler::GetConfig();
  if (!config.has_sync_config()) {
    LOG(WARNING) << "sync_config is not set in configuretion.";
    return false;
  }
  return config.sync_config().use_contact_list_sync();
}

}

ContactSyncer::ContactSyncer(OAuth2Util* oauth2_util)
    : oauth2_util_(oauth2_util) {}

bool ContactSyncer::Start() {
  return true;
}

bool ContactSyncer::Sync(bool *reload_required) {
  if (!CheckConfigureToSyncContactList()) {
    // If config says that no-sync in contact list, it means just
    // ignores, thus returns true.
    return true;
  }

  user_dictionary::UserDictionaryStorage remote_update;
  if (!Download(&remote_update, reload_required)) {
    VLOG(1) << "No contact list updates.";
    return true;
  }

  const string dict_file = GetUserDictionaryFileName();
  UserDictionaryStorage dict_storage(dict_file);
  dict_storage.Load();
  UserDictionarySyncUtil::MergeUpdate(remote_update, &dict_storage);
  if (!UserDictionarySyncUtil::VerifyLockAndSaveStorage(&dict_storage)) {
    return false;
  }

  return true;
}

bool ContactSyncer::Clear() {
  if (!CheckConfigureToSyncContactList()) {
    return false;
  }

  const string dict_file = GetUserDictionaryFileName();
  UserDictionaryStorage dict_storage(dict_file);
  if (!dict_storage.Load()) {
    DLOG(INFO) << "Cannot find the dictionary file.";
    return false;
  }

  uint64 dic_id;
  if (dict_storage.GetUserDictionaryId(dict_file, &dic_id)) {
    dict_storage.DeleteDictionary(dic_id);
  }

  return dict_storage.Save();
}

bool ContactSyncer::Download(user_dictionary::UserDictionaryStorage *storage,
                             bool *reload_required) {
  *reload_required = false;

  string timestamp;
  GetLastDownloadTimestamp(&timestamp);
  // Get information from Google server.
  string resource_uri = string(kContactResourceUri) + "?";
  vector<pair<string, string> > params;
  // Each parameter specifies the format, the version, a maximum number of
  // entries, and the oldest update timestamp, respectively.
  params.push_back(make_pair("alt", "json"));
  params.push_back(make_pair("v", "3.0"));
  params.push_back(make_pair("max-results", "999999"));
  params.push_back(make_pair("updated-min", timestamp));
  Util::AppendCGIParams(params, &resource_uri);

  string response;
  OAuth2::Error error;
  if (!oauth2_util_->RequestResource(resource_uri, &response) &&
      (!oauth2_util_->RefreshAccessToken(&error) ||
       !oauth2_util_->RequestResource(resource_uri, &response))) {
    return false;
  }

  UserDictionary *contact_dictionary = storage->add_dictionaries();
  string last_timestamp;
  if (!ContactListUtil::ParseContacts(response, contact_dictionary,
                                      &last_timestamp)) {
    return false;
  }
  contact_dictionary->set_name(kContactsDictionaryName);
  storage->set_storage_type(user_dictionary::UserDictionaryStorage::UPDATE);

  SetLastDownloadTimestamp(last_timestamp);
  return true;
}

bool ContactSyncer::Upload() {
  return true;
}

bool ContactSyncer::GetLastDownloadTimestamp(string *timestamp) const {
  if (!mozc::storage::Registry::Lookup(kGdataLastDownloadTimeKey, timestamp)) {
    LOG(ERROR) << "cannot read: " << kGdataLastDownloadTimeKey;
    *timestamp = "0000-00-00T00:00:00.000Z";
    return false;
  }
  LOG(INFO) << "GetLastDownloadTimestamp: " << *timestamp;
  return true;
}

void ContactSyncer::SetLastDownloadTimestamp(const string &timestamp) {
  VLOG(1) << "SetLastDownloadTimestamp: " << timestamp;
  if (!mozc::storage::Registry::Insert(kGdataLastDownloadTimeKey, timestamp)) {
    LOG(ERROR) << "cannot save: "<< kGdataLastDownloadTimeKey;
  }
  mozc::storage::Registry::Sync();
}

string ContactSyncer::GetUserDictionaryFileName() const {
  return UserDictionaryUtil::GetUserDictionaryFileName();
}
}  // namespace sync
}  // namespace mozc
