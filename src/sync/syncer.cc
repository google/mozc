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

#include "sync/syncer.h"

#include "base/base.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "storage/registry.h"
#include "sync/adapter_interface.h"
#include "sync/logging.h"
#include "sync/mock_syncer.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_util.h"
#include "sync/service_interface.h"

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
  switch (component_id) {
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

Syncer::Syncer(ServiceInterface *service) : service_(service) {}

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
  SYNC_VLOG(1) << "start Syncer::Sync";
  if (!Download(reload_required)) {
    SYNC_VLOG(1) << "Download failed";
    return false;
  }

  if (!Upload()) {
    SYNC_VLOG(1)  << "Upload failed";
    return false;
  }

  return true;
}

bool Syncer::Clear() {
  SYNC_VLOG(1) << "start Syncer::Clear";
  if (service_ == NULL) {
    SYNC_VLOG(1) << "service is NULL";
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

  SYNC_VLOG(1) << "sending clear RPC call";
  if (!service_->Clear(&request, &response)) {
    SYNC_VLOG(1) << "RPC error";
    return false;
  }

  VLOG(2) << "Request: " << request.DebugString();
  VLOG(2) << "Response: " << response.DebugString();

  if (!response.IsInitialized()) {
    SYNC_VLOG(1) << "response is not initialized";
    return false;
  }

  if (response.error() != ime_sync::SYNC_OK) {
    SYNC_VLOG(1) << "response header is not SYNC_OK";
    return false;
  }

  SYNC_VLOG(1) << "clear is called successfully";

  if (!ClearLocal()) {
    return false;
  }

  SYNC_VLOG(1) << "initializing LastDownloadTimestamp";
  SetLastDownloadTimestamp(0);

  return true;
}

bool Syncer::ClearLocal() {
  bool result = true;
  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end(); ++iter) {
    if (!iter->second->Clear()) {
      result = false;
    }
  }

  return result;
}

bool Syncer::Download(bool *reload_required) {
  SYNC_VLOG(1) << "start Syncer::Download";
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
  const uint64 last_download_timestamp = GetLastDownloadTimestamp();
  request.set_last_download_timestamp(last_download_timestamp);

  SYNC_VLOG(1) << "setting last_download_timestamp=" << last_download_timestamp;

  Config config = ConfigHandler::GetConfig();

  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end();
       ++iter) {
    if (CheckConfigToSync(config, iter->first)) {
      request.add_components(iter->first);
    }
  }

  SYNC_VLOG(1) << "downloading remote items...";
  if (!service_->Download(&request, &response)) {
    SYNC_VLOG(1) << "RPC error";
    return false;
  }

  VLOG(2) << "Request: " << request.DebugString();
  VLOG(2) << "Response: " << response.DebugString();

  if (!response.IsInitialized()) {
    SYNC_VLOG(1) << "response is not initialized";
    return false;
  }

  if (response.error() != ime_sync::SYNC_OK) {
    SYNC_VLOG(1) << "response header is not SYNC_OK";
    return false;
  }

  SYNC_VLOG(1) << "done. remote items are downloaded";

  SYNC_VLOG(1) << "updating local components...";
  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end(); ++iter) {
    if (!CheckConfigToSync(config, iter->first)) {
      continue;
    }
    SYNC_VLOG(1) << "calling SetDownloadedItems: "
                 << iter->second->component_id();
    if (!iter->second->SetDownloadedItems(response.items())) {
      SYNC_VLOG(1) << "SetDownloadedItems failed: "
                   << iter->second->component_id();
      return false;
    }
  }

  SYNC_VLOG(1) << "done. local components are updated";

  // when the size of donloaded items > 0, set *reload_required to be true.
  // TODO(taku): this might not be optimal, as items may contain
  // invalid data.
  *reload_required = response.items().size() > 0;

  SYNC_VLOG(1) << "reload_required=" << *reload_required;
  SYNC_VLOG(1) << "LastDownloadTimestamp is updated from "
               << last_download_timestamp << " to "
               << response.download_timestamp();

  SetLastDownloadTimestamp(response.download_timestamp());

  return true;
}

bool Syncer::Upload() {
  SYNC_VLOG(1) << "start Syncer::Upload";

  if (service_ == NULL) {
    LOG(ERROR) << "Service is NULL";
    return false;
  }

  ime_sync::UploadRequest request;
  ime_sync::UploadResponse response;

  request.set_version(kSyncClientVersion);
  request.set_client(ime_sync::MOZC);

  Config config = ConfigHandler::GetConfig();

  SYNC_VLOG(1) << "collecting local updates...";
  bool result = true;
  for (AdapterMap::const_iterator iter = adapters_.begin();
       iter != adapters_.end(); ++iter) {
    if (!CheckConfigToSync(config, iter->first)) {
      continue;
    }
    SYNC_VLOG(1) << "calling GetItemsToUpload: "
                 << iter->second->component_id();
    if (!iter->second->GetItemsToUpload(request.mutable_items())) {
      SYNC_VLOG(1) << "GetItemsToUpload failed: "
                   << iter->second->component_id();
      result = false;
    }
  }

  if (request.items_size() == 0) {
    SYNC_VLOG(1) << "no items should be uploaded";
    return true;
  }

  if (result) {
    SYNC_VLOG(1) << "uploading local items to the server...";
    if (!service_->Upload(&request, &response)) {
      SYNC_VLOG(1) << "RPC error";
      result = false;
    }

    VLOG(2) << "Request: " << request.DebugString();
    VLOG(2) << "Response: " << response.DebugString();

    if (response.error() != ime_sync::SYNC_OK) {
      SYNC_VLOG(1) << "response header is not SYNC_OK";
      result = false;
    }

    SYNC_VLOG(1) << "done. local items are uploaded";
  }

  SYNC_VLOG(1) << "marking uploaded flags...";
  for (size_t i = 0; i < request.items_size(); ++i) {
    const ime_sync::SyncItem& item = request.items(i);
    AdapterMap::const_iterator iter = adapters_.find(item.component());
    if (iter == adapters_.end() ||
        !CheckConfigToSync(config, iter->first)) {
      continue;
    }
    // set result code.
    SYNC_VLOG(1) << "calling MarkUploaded: " << iter->second->component_id()
                 << " result=" << result;
    iter->second->MarkUploaded(item, result);
  }

  SYNC_VLOG(1) << "done. uploaded flags";

  return result;
}

bool Syncer::Start() {
  VLOG(1) << "start Syncer::Start()";

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
    SYNC_VLOG(1) << "cannot read: " << kLastDownloadTimeStampKey;
  }
  VLOG(1) << "GetLastDownloadTimestamp: " << value;
  return value;
}

void Syncer::SetLastDownloadTimestamp(uint64 value) {
  VLOG(1) << "SetLastDownloadTimestamp: " << value;
  if (!mozc::storage::Registry::Insert(kLastDownloadTimeStampKey, value)) {
    SYNC_VLOG(1) << "cannot save: "<< kLastDownloadTimeStampKey;
  }
  mozc::storage::Registry::Sync();
}

}  // namespace sync
}  // namespace mozc
