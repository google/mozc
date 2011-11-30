// Copyright 2010-2011, Google Inc.
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

#include "sync/syncer.h"

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "storage/registry.h"
#include "sync/adapter_interface.h"
#include "sync/config_adapter.h"
#include "sync/mock_syncer.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_util.h"
#include "sync/user_dictionary_adapter.h"
#include "sync/service_interface.h"

DEFINE_string(sync_url,
              "",
              "sync url");

namespace mozc {
namespace sync {

using config::Config;
using config::ConfigHandler;
using config::SyncConfig;

namespace {
const int kSyncClientVersion = 1;
const char kLastDownloadTimeStampKey[] = "sync.last_download_timestamp";

bool CheckConfigToSync(const Config &config, const uint32 component_id) {
  if (!config.has_sync_config()) {
    LOG(WARNING) << "CheckConfigToSync() is called without sync_config set.";
    return false;
  }

  // Check config on each sync feature.
  const SyncConfig &sync_config = config.sync_config();
  switch(component_id) {
    case ime_sync::MOZC_SETTING:
      return sync_config.use_config_sync();
    case ime_sync::MOZC_USER_DICTIONARY:
      return sync_config.use_user_dictionary_sync();
    case ime_sync::MOZC_USER_HISTORY_PREDICTION:
      return sync_config.use_user_history_sync();
    case ime_sync::MOZC_LEARNING_PREFERENCE:
      return sync_config.use_learning_preference_sync();
  }

  // Do not sync unkown features.
  return false;
}
}  // anonymous namespace

Syncer::Syncer(ServiceInterface *service)
    : service_(service) {}

Syncer::~Syncer() {}

// This function is not thread safe.
bool Syncer::RegisterAdapter(AdapterInterface *adapter) {
  // Can't add more than one adapter for the same component id.
  if (adapters_.find(adapter->component_id()) != adapters_.end()) {
    LOG(WARNING) << "already regsiterd: " << adapter->component_id();
    return false;
  }
  adapters_.insert(make_pair(adapter->component_id(), adapter));
  return true;
}

bool Syncer::Sync(bool *reload_required) {
  if (!Download(reload_required)) {
    LOG(ERROR) << "Download failed";
    return false;
  }

  if (!Upload()) {
    LOG(ERROR) << "Upload failed";
    return false;
  }

  return true;
}

bool Syncer::Clear() {
  VLOG(1) << "Start Syncer::Clear";
  if (service_ == NULL) {
    LOG(ERROR) << "Service is NULL";
    return false;
  }

  ime_sync::ClearRequest request;
  ime_sync::ClearResponse response;

  request.set_version(kSyncClientVersion);
  request.set_client(ime_sync::MOZC);

  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end();
       ++iter) {
    request.add_components(iter->first);
  }

  if (!service_->Clear(&request, &response)) {
    LOG(ERROR) << "RPC error";
    return false;
  }

  VLOG(2) << "Request: " << request.DebugString();
  VLOG(2) << "Response: " << response.DebugString();

  if (!response.IsInitialized()) {
    LOG(ERROR) << "response is not initialized";
    return false;
  }

  if (response.error() != ime_sync::SYNC_OK) {
    LOG(ERROR) << "response header is not SYNC_OK";
    return false;
  }

  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end(); ++iter) {
    iter->second->Clear();
  }

  SetLastDownloadTimestamp(0);

  return true;
}

bool Syncer::Download(bool *reload_required) {
  VLOG(1) << "Start Syncer::Download()";
  DCHECK(reload_required);

  *reload_required = false;

  if (service_ == NULL) {
    LOG(ERROR) << "Service is NULL";
    return false;
  }

  ime_sync::DownloadRequest request;
  ime_sync::DownloadResponse response;

  request.set_version(kSyncClientVersion);
  request.set_client(ime_sync::MOZC);

  // if last_download_timestamp is void, use 0 as an initial value
  request.set_last_download_timestamp(GetLastDownloadTimestamp());

  Config config = ConfigHandler::GetConfig();

  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end();
       ++iter) {
    if (CheckConfigToSync(config, iter->first)) {
      request.add_components(iter->first);
    }
  }

  if (!service_->Download(&request, &response)) {
    LOG(ERROR) << "RPC error";
    return false;
  }

  VLOG(2) << "Request: " << request.DebugString();
  VLOG(2) << "Response: " << response.DebugString();

  if (!response.IsInitialized()) {
    LOG(ERROR) << "response is not initialized";
    return false;
  }

  if (response.error() != ime_sync::SYNC_OK) {
    LOG(ERROR) << "response header is not SYNC_OK";
    return false;
  }

  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end(); ++iter) {
    if (!CheckConfigToSync(config, iter->first)) {
      continue;
    }
    if (!iter->second->SetDownloadedItems(response.items())) {
      LOG(ERROR) << "Adapter component_id=" << iter->second->component_id()
                 << " failed in SetDownloadedItems()";
      return false;
    }
  }

  // when the size of donloaded items > 0, set *reload_required to be true.
  // TODO(taku): this might not be optimal, as items may contain
  // invalid data.
  *reload_required = response.items().size() > 0;

  SetLastDownloadTimestamp(response.download_timestamp());

  return true;
}

bool Syncer::Upload() {
  VLOG(1) << "Start Syncer::Upload()";

  if (service_ == NULL) {
    LOG(ERROR) << "Service is NULL";
    return false;
  }

  ime_sync::UploadRequest request;
  ime_sync::UploadResponse response;

  request.set_version(kSyncClientVersion);
  request.set_client(ime_sync::MOZC);

  Config config = ConfigHandler::GetConfig();

  bool result = true;
  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end(); ++iter) {
    if (!CheckConfigToSync(config, iter->first)) {
      continue;
    }
    if (!iter->second->GetItemsToUpload(request.mutable_items())) {
      LOG(ERROR) << "Adapter component_id=" << iter->second->component_id()
                 << " failed in GetItemsToUpload()";
      result = false;
    }
  }

  if (request.items_size() == 0) {
    LOG(WARNING) << "No items should be synced";
    return true;
  }

  if (result) {
    if (!service_->Upload(&request, &response)) {
      LOG(ERROR) << "RPC error";
      result = false;
    }

    VLOG(2) << "Request: " << request.DebugString();
    VLOG(2) << "Response: " << response.DebugString();

    if (response.error() != ime_sync::SYNC_OK) {
      LOG(ERROR) << "response header is not SYNC_OK";
      result = false;
    }
  }

  for (size_t i = 0; i < request.items_size(); ++i) {
    const ime_sync::SyncItem& item = request.items(i);
    AdapterMap::const_iterator iter = adapters_.find(item.component());
    if (iter == adapters_.end() ||
        !CheckConfigToSync(config, iter->first)) {
      continue;
    }
    // set result code.
    iter->second->MarkUploaded(item, result);
  }

  return result;
}

bool Syncer::Start() {
  VLOG(1) << "Start Syncer::Start()";

  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end();  ++iter) {
    if (!iter->second->Start()) {
      return false;
    }
  }

  return true;
}

uint64 Syncer::GetLastDownloadTimestamp() const {
  uint64 value = 0;
  if (!mozc::storage::Registry::Lookup(kLastDownloadTimeStampKey, &value)) {
    LOG(ERROR) << "cannot read: " << kLastDownloadTimeStampKey;
  }
  VLOG(1) << "GetLastDownloadTimestamp: " << value;
  return value;
}

void Syncer::SetLastDownloadTimestamp(uint64 value) {
  VLOG(1) << "SetLastDownloadTimestamp: " << value;
  if (!mozc::storage::Registry::Insert(kLastDownloadTimeStampKey, value)) {
    LOG(ERROR) << "cannot save: "<< kLastDownloadTimeStampKey;
  }
  mozc::storage::Registry::Sync();
}

namespace {
SyncerInterface *g_syncer = NULL;
OAuth2Util *g_oauth2 = NULL;

class DefaultSyncerCreator {
 public:
  DefaultSyncerCreator()
      : config_adapter_(new ConfigAdapter),
        user_dictionary_adapter_(new UserDictionaryAdapter) {
    if (g_oauth2 == NULL) {
      LOG(INFO) << "set oauth2 service";
      SyncerFactory::SetOAuth2(
          new OAuth2Util(OAuth2Client::GetDefaultClient()));
    }

    syncer_.reset(new Syncer(service_.get()));
    if (!syncer_->RegisterAdapter(config_adapter_.get())) {
      LOG(ERROR) << "cannot register ConfigAdapter";
    }

    if (!syncer_->RegisterAdapter(user_dictionary_adapter_.get())) {
      LOG(ERROR) << "cannot register UserDictionaryAdapter";
    }
  }

  SyncerInterface *Get() {
    return syncer_.get();
  }

 private:
  scoped_ptr<ServiceInterface>          service_;
  scoped_ptr<Syncer>                    syncer_;
  scoped_ptr<ConfigAdapter>             config_adapter_;
  scoped_ptr<UserDictionaryAdapter>     user_dictionary_adapter_;
};
}

SyncerInterface *SyncerFactory::GetSyncer() {
  if (g_syncer == NULL) {
    return Singleton<MockSyncer>::get();
  } else {
    return g_syncer;
  }
}

void SyncerFactory::SetSyncer(SyncerInterface *syncer) {
  g_syncer = syncer;
}

void SyncerFactory::SetOAuth2(OAuth2Util *oauth2) {
  g_oauth2 = oauth2;
}
}  // namespace sync
}  // namespace mozc
