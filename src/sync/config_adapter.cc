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

#include "sync/config_adapter.h"

#include <set>
#include <string>
#include <vector>

#include "base/config_file_stream.h"
#include "base/protobuf/protobuf.h"
#include "base/protobuf/descriptor.h"
#include "base/singleton.h"
#include "config/config_handler.h"
#include "sync/sync.pb.h"
#include "sync/sync_util.h"

using mozc::protobuf::Reflection;
using mozc::protobuf::FieldDescriptor;
using mozc::config::Config;
using mozc::config::ConfigHandler;

namespace mozc {
namespace sync {
namespace {
const size_t kConfigFileSizeLimit = 128 * 1024;  // 128KiB

void StripUnnecessaryConfigFields(Config *config) {
  config->mutable_general_config()->Clear();
  config->mutable_sync_config()->Clear();
}

void MergeConfig(const Config &source, Config *target) {
  mozc::config::GeneralConfig target_general;
  target_general.CopyFrom(target->general_config());
  mozc::config::SyncConfig target_sync;
  target_sync.CopyFrom(target->sync_config());

  target->CopyFrom(source);
  target->mutable_general_config()->CopyFrom(target_general);
  target->mutable_sync_config()->CopyFrom(target_sync);
}
}  // anonymous namespace

ConfigAdapter::ConfigAdapter() {
}

ConfigAdapter::~ConfigAdapter() {
}

bool ConfigAdapter::SetDownloadedItems(const ime_sync::SyncItems &items) {
  VLOG(1) << "Start SetDownloadedItems: "
          << items.size() << " items for Config";

  const Config *remote_config = NULL;
  for (size_t i = 0; i < items.size(); ++i) {
    const ime_sync::SyncItem &item = items.Get(i);
    if (item.component() != component_id() ||
        !item.key().HasExtension(sync::ConfigKey::ext) ||
        !item.value().HasExtension(sync::ConfigValue::ext)) {
      continue;
    }
    remote_config = &item.value().GetExtension(sync::ConfigValue::ext).config();
  }

  if (remote_config == NULL) {
    VLOG(1) << "No new remote items downloaded";
    return true;
  }

  Config current_config;
  if (!ConfigHandler::GetConfig(&current_config)) {
    LOG(ERROR) << "Cannot obtain configs";
    return false;
  }

  Config last_downloaded_config;
  bool has_last_downloaded = LoadConfigFromFile(
      GetLastDownloadedConfigFileName(), &last_downloaded_config);

  // We do not care the conflict during the download because it is
  // really difficult for users to resolve conflicts manually.  So
  // here rule is very simple: override the config by the new one.
  if (has_last_downloaded &&
      IsSameConfig(last_downloaded_config, *remote_config)) {
    // No same config comes.
    return true;
  }

  // Anyway it stores the last downloaded file.
  ConfigFileStream::AtomicUpdate(
      GetLastDownloadedConfigFileName(), remote_config->SerializeAsString());

  MergeConfig(*remote_config, &current_config);
  ConfigHandler::SetConfig(current_config);
  // Also modify the last uploaded file to prevent unnecessary upload.
  ConfigFileStream::AtomicUpdate(
      GetLastUploadedConfigFileName(), remote_config->SerializeAsString());
  return true;
}

bool ConfigAdapter::GetItemsToUpload(ime_sync::SyncItems *items) {
  DCHECK(items);
  VLOG(1) << "Start GetItemsToUpload() for Config";

  Config current_config;
  if (!ConfigHandler::GetConfig(&current_config)) {
    LOG(ERROR) << "Cannot obtain configs";
    return false;
  }
  StripUnnecessaryConfigFields(&current_config);
  string serialized_data = current_config.SerializeAsString();
  if (serialized_data.size() > kConfigFileSizeLimit) {
    LOG(ERROR) << "Cannot upload such huge data";
    return false;
  }

  Config last_uploaded_config;
  bool has_last_uploaded = LoadConfigFromFile(
      GetLastUploadedConfigFileName(), &last_uploaded_config);

  if (has_last_uploaded && IsSameConfig(last_uploaded_config, current_config)) {
    VLOG(1) << "No need to upload config";
    return true;
  }

  ime_sync::SyncItem *item = items->Add();
  CHECK(item);

  item->set_component(component_id());
  sync::ConfigKey *key =
      item->mutable_key()->MutableExtension(sync::ConfigKey::ext);
  sync::ConfigValue *value =
      item->mutable_value()->MutableExtension(sync::ConfigValue::ext);
  CHECK(key);
  CHECK(value);
  value->mutable_config()->CopyFrom(current_config);

  return true;
}

bool ConfigAdapter::MarkUploaded(
    const ime_sync::SyncItem& item, bool uploaded) {
  VLOG(1) << "Start MarkUploaded() uploaded=" << uploaded;

  if (!item.key().HasExtension(sync::ConfigKey::ext) ||
      !item.value().HasExtension(sync::ConfigValue::ext)) {
    // This item is not for config
    return false;
  }

  const sync::ConfigValue &value =
      item.value().GetExtension(sync::ConfigValue::ext);
  if (!value.has_config()) {
    LOG(ERROR) << "invalid config item: " << value.DebugString();
    return false;
  }

  if (!uploaded) {
    VLOG(1) << "Upload failed during sync of Config";
    return true;
  }

  ConfigFileStream::AtomicUpdate(
      GetLastUploadedConfigFileName(), value.config().SerializeAsString());
  return true;
}

bool ConfigAdapter::Clear() {
  string last_downloaded_filename = ConfigFileStream::GetFileName(
      GetLastDownloadedConfigFileName());
  if (!last_downloaded_filename.empty()) {
    Util::Unlink(last_downloaded_filename);
  }
  string last_uploaded_filename = ConfigFileStream::GetFileName(
      GetLastUploadedConfigFileName());
  if (!last_uploaded_filename.empty()) {
    Util::Unlink(last_uploaded_filename);
  }
  return true;
}

ime_sync::Component ConfigAdapter::component_id() const {
  return ime_sync::MOZC_SETTING;
}

bool ConfigAdapter::LoadConfigFromFile(const string &filename, Config *config) {
  VLOG(1) << "Load config from file: " << filename;
  DCHECK(config);
  scoped_ptr<istream> ifs(ConfigFileStream::Open(filename));
  if (ifs.get() == NULL || ifs->fail()) {
    LOG(ERROR) << filename << " is not found";
    return false;
  }

  if (ifs->eof()) {
    LOG(ERROR) << filename << " is empty";
    return false;
  }

  config->Clear();
  bool success = true;
  if (!config->ParseFromIstream(ifs.get())) {
    LOG(ERROR) << filename << " is broken";
    config->Clear();
    success = false;
  }

  return success;
}

void ConfigAdapter::SetConfigFileNameBase(const string &filename) {
  config_filename_ = filename;
}

string ConfigAdapter::GetConfigFileNameBase() const {
  if (!config_filename_.empty()) {
    return config_filename_;
  }

  return ConfigHandler::GetConfigFileName();
}

string ConfigAdapter::GetLastDownloadedConfigFileName() const {
  const char kSuffix[] = ".last_downloaded";
  return GetConfigFileNameBase() + kSuffix;
}

string ConfigAdapter::GetLastUploadedConfigFileName() const {
  const char kSuffix[] = ".last_uploaded";
  return GetConfigFileNameBase() + kSuffix;
}

bool ConfigAdapter::IsSameConfig(const Config &config1, const Config &config2) {
  Config config1_copy, config2_copy;
  config1_copy.CopyFrom(config1);
  config2_copy.CopyFrom(config2);
  StripUnnecessaryConfigFields(&config1_copy);
  StripUnnecessaryConfigFields(&config2_copy);
  string config1_string, config2_string;
  config1_copy.SerializeToString(&config1_string);
  config2_copy.SerializeToString(&config2_string);
  return config1_string == config2_string;
}

}  // namespace mozc::sync
}  // namespace mozc
