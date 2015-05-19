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

#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/logging.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "sync/sync.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

using mozc::config::Config;
using mozc::config::ConfigHandler;

namespace mozc {
namespace sync {

class ConfigAdapterTest : public testing::Test {
 public:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

    ConfigHandler::SetConfigFileName("memory://config");
    adapter_.Clear();
  }

  virtual void TearDown() {
    ConfigFileStream::ClearOnMemoryFiles();
    adapter_.Clear();
  }

  bool StoreConfigToFile(const string &filename, const Config &config) {
    return ConfigFileStream::AtomicUpdate(filename, config.SerializeAsString());
  }

 protected:
  ConfigAdapter adapter_;
};

TEST_F(ConfigAdapterTest, IsSameConfig) {
  Config config1;
  Config config2;
  ConfigHandler::GetDefaultConfig(&config1);
  ConfigHandler::GetDefaultConfig(&config2);
  EXPECT_TRUE(adapter_.IsSameConfig(config1, config2));

  // Edit makes a different config
  config1.set_incognito_mode(!config1.incognito_mode());
  EXPECT_FALSE(adapter_.IsSameConfig(config1, config2));

  // When a new config has the same value, they are same.
  config2.set_incognito_mode(config1.incognito_mode());
  EXPECT_TRUE(adapter_.IsSameConfig(config1, config2));

  // When a new value is set to config2, it should be detected.
  config2.set_preedit_method(Config::KANA);
  EXPECT_FALSE(adapter_.IsSameConfig(config1, config2));

  // Set the value again
  config1.set_preedit_method(config2.preedit_method());
  EXPECT_TRUE(adapter_.IsSameConfig(config1, config2));

  // If the edit is not sync-aware, the edit will be ignored.
  config1.mutable_general_config()->set_platform("Linux");
  config2.mutable_general_config()->clear_platform();
  EXPECT_TRUE(adapter_.IsSameConfig(config1, config2));

  // and vise versa
  config1.mutable_general_config()->clear_ui_locale();
  config2.mutable_general_config()->set_ui_locale("ja");
  EXPECT_TRUE(adapter_.IsSameConfig(config1, config2));

  // Verify for repeated field
  config1.clear_character_form_rules();
  config2.clear_character_form_rules();
  Config::CharacterFormRule *new_rule = config1.add_character_form_rules();
  new_rule->set_group("group1");
  new_rule->set_preedit_character_form(Config::HALF_WIDTH);
  new_rule->set_conversion_character_form(Config::HALF_WIDTH);
  new_rule = config1.add_character_form_rules();
  new_rule->set_group("gruop2");
  new_rule->set_preedit_character_form(Config::FULL_WIDTH);
  new_rule->set_conversion_character_form(Config::FULL_WIDTH);
  EXPECT_FALSE(adapter_.IsSameConfig(config1, config2));

  new_rule = config2.add_character_form_rules();
  new_rule->set_group("group1");
  new_rule->set_preedit_character_form(Config::HALF_WIDTH);
  new_rule->set_conversion_character_form(Config::HALF_WIDTH);
  new_rule = config2.add_character_form_rules();
  new_rule->set_group("gruop2");
  new_rule->set_preedit_character_form(Config::FULL_WIDTH);
  new_rule->set_conversion_character_form(Config::FULL_WIDTH);
  EXPECT_TRUE(adapter_.IsSameConfig(config1, config2));

  // If the repeated order is different, it's not same
  config2.clear_character_form_rules();
  new_rule = config2.add_character_form_rules();
  new_rule->set_group("gruop2");
  new_rule->set_preedit_character_form(Config::FULL_WIDTH);
  new_rule->set_conversion_character_form(Config::FULL_WIDTH);
  new_rule = config2.add_character_form_rules();
  new_rule->set_group("group1");
  new_rule->set_preedit_character_form(Config::HALF_WIDTH);
  new_rule->set_conversion_character_form(Config::HALF_WIDTH);
  EXPECT_FALSE(adapter_.IsSameConfig(config1, config2));
}

TEST_F(ConfigAdapterTest, IsSameConfigWithConfigHandler) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);
  EXPECT_TRUE(adapter_.IsSameConfig(config, ConfigHandler::GetConfig()));
}

TEST_F(ConfigAdapterTest, FileNames) {
  // every file names should not be same.
  string last_downloaded = adapter_.GetLastDownloadedConfigFileName();
  string last_uploaded = adapter_.GetLastUploadedConfigFileName();
  EXPECT_NE(last_downloaded, last_uploaded);
}

TEST_F(ConfigAdapterTest, SetDownloadedItems) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);
  EXPECT_TRUE(StoreConfigToFile(
      adapter_.GetLastDownloadedConfigFileName(), config));

  config.set_incognito_mode(!config.incognito_mode());
  ime_sync::SyncItems items;
  ime_sync::SyncItem *item = items.Add();
  CHECK(item);
  item->set_component(adapter_.component_id());
  sync::ConfigKey *key =
      item->mutable_key()->MutableExtension(sync::ConfigKey::ext);
  sync::ConfigValue *value =
      item->mutable_value()->MutableExtension(sync::ConfigValue::ext);
  CHECK(key);
  CHECK(value);
  value->mutable_config()->CopyFrom(config);

  // Do Sync
  EXPECT_TRUE(adapter_.SetDownloadedItems(items));

  // stored config file should be updated.
  Config loaded_config;
  EXPECT_TRUE(ConfigHandler::GetConfig(&loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(config, loaded_config));

  // last downloaded file should be updated.
  loaded_config.Clear();
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastDownloadedConfigFileName(), &loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(config, loaded_config));

  // last uploaded file should also be updated to prevent further
  // unnecessary upload.
  loaded_config.Clear();
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastUploadedConfigFileName(), &loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(config, loaded_config));
}

TEST_F(ConfigAdapterTest, SetDownloadedItemsEmpty) {
  ime_sync::SyncItems items;
  EXPECT_TRUE(adapter_.SetDownloadedItems(items));
}

TEST_F(ConfigAdapterTest, SetDownloadedItemsConflict) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);
  EXPECT_TRUE(StoreConfigToFile(
      adapter_.GetLastDownloadedConfigFileName(), config));
  Config remote_config;
  remote_config.CopyFrom(config);

  // edit the current config from the last downloaded.
  config.set_incognito_mode(!config.incognito_mode());
  EXPECT_TRUE(ConfigHandler::SetConfig(config));

  // another edit config is synced.
  remote_config.set_preedit_method(Config::KANA);
  ime_sync::SyncItems items;
  ime_sync::SyncItem *item = items.Add();
  CHECK(item);
  item->set_component(adapter_.component_id());
  sync::ConfigKey *key =
      item->mutable_key()->MutableExtension(sync::ConfigKey::ext);
  sync::ConfigValue *value =
      item->mutable_value()->MutableExtension(sync::ConfigValue::ext);
  CHECK(key);
  CHECK(value);
  value->mutable_config()->CopyFrom(remote_config);

  // Do Sync.  Succeeds even though it's conflict.
  EXPECT_TRUE(adapter_.SetDownloadedItems(items));

  // The current config is updated by the downloaded one.
  Config loaded_config;
  EXPECT_TRUE(ConfigHandler::GetConfig(&loaded_config));
  EXPECT_FALSE(adapter_.IsSameConfig(config, loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(remote_config, loaded_config));

  // last downloaded file should be updated.
  loaded_config.Clear();
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastDownloadedConfigFileName(), &loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(remote_config, loaded_config));

  // last uploaded file should be updated.
  loaded_config.Clear();
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastUploadedConfigFileName(), &loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(remote_config, loaded_config));
}

TEST_F(ConfigAdapterTest, InitialImports) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);

  ime_sync::SyncItems items;
  ime_sync::SyncItem *item = items.Add();
  CHECK(item);
  item->set_component(adapter_.component_id());
  sync::ConfigKey *key =
      item->mutable_key()->MutableExtension(sync::ConfigKey::ext);
  sync::ConfigValue *value =
      item->mutable_value()->MutableExtension(sync::ConfigValue::ext);
  CHECK(key);
  CHECK(value);
  value->mutable_config()->CopyFrom(config);

  // Do Sync.  It succeeds because the downloaded config is same as config.
  EXPECT_TRUE(adapter_.SetDownloadedItems(items));
}

TEST_F(ConfigAdapterTest, InitialImportsConflict) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);

  config.set_incognito_mode(!config.incognito_mode());
  ime_sync::SyncItems items;
  ime_sync::SyncItem *item = items.Add();
  CHECK(item);
  item->set_component(adapter_.component_id());
  sync::ConfigKey *key =
      item->mutable_key()->MutableExtension(sync::ConfigKey::ext);
  sync::ConfigValue *value =
      item->mutable_value()->MutableExtension(sync::ConfigValue::ext);
  CHECK(key);
  CHECK(value);
  value->mutable_config()->CopyFrom(config);

  // Do Sync.  true because force overwriting the existing config by
  // the downloaded one for the initial sync.
  EXPECT_TRUE(adapter_.SetDownloadedItems(items));

  // stored config file is updated.
  Config loaded_config;
  EXPECT_TRUE(ConfigHandler::GetConfig(&loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(config, loaded_config));

  // last downloaded file is also updated too.
  loaded_config.Clear();
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastDownloadedConfigFileName(), &loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(config, loaded_config));

  // last uploaded file is also updated to prevent further unnecessary
  // upload later.
  loaded_config.Clear();
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastUploadedConfigFileName(), &loaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(config, loaded_config));
}

TEST_F(ConfigAdapterTest, GetItemsToUpload) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  StoreConfigToFile(adapter_.GetLastUploadedConfigFileName(), config);

  config.set_incognito_mode(!config.incognito_mode());
  ConfigHandler::SetConfig(config);

  ime_sync::SyncItems items;
  // No uploaded items
  EXPECT_EQ(0, items.size());

  EXPECT_TRUE(adapter_.GetItemsToUpload(&items));
  EXPECT_EQ(1, items.size());

  const ime_sync::SyncItem &item = items.Get(0);
  EXPECT_TRUE(item.key().HasExtension(sync::ConfigKey::ext));
  EXPECT_TRUE(item.value().HasExtension(sync::ConfigValue::ext));
  const sync::ConfigValue &value =
      item.value().GetExtension(sync::ConfigValue::ext);
  EXPECT_TRUE(value.has_config());
  EXPECT_TRUE(adapter_.IsSameConfig(config, value.config()));

  // upload success
  EXPECT_TRUE(adapter_.MarkUploaded(item, true));

  Config last_uploaded_config;
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastUploadedConfigFileName(), &last_uploaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(config, last_uploaded_config));
}

TEST_F(ConfigAdapterTest, GetItemsToUploadIgnorableFields) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  StoreConfigToFile(adapter_.GetLastUploadedConfigFileName(), config);

  // Edit an ignorable field.
  config.mutable_general_config()->set_platform("Windows");
  ConfigHandler::SetConfig(config);

  ime_sync::SyncItems items;
  // No uploaded items
  EXPECT_EQ(0, items.size());

  EXPECT_TRUE(adapter_.GetItemsToUpload(&items));
  // Because the local edit is done into ignorable fields only, it
  // does not set an item to upload.
  EXPECT_EQ(0, items.size());
}

TEST_F(ConfigAdapterTest, GetItemsToUploadFailure) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  StoreConfigToFile(adapter_.GetLastUploadedConfigFileName(), config);

  Config uploading_config;
  uploading_config.CopyFrom(config);
  uploading_config.set_incognito_mode(!uploading_config.incognito_mode());
  ConfigHandler::SetConfig(uploading_config);

  ime_sync::SyncItems items;
  // No uploaded items before the sync.
  EXPECT_EQ(0, items.size());

  EXPECT_TRUE(adapter_.GetItemsToUpload(&items));
  EXPECT_EQ(1, items.size());

  const ime_sync::SyncItem &item = items.Get(0);
  EXPECT_TRUE(item.key().HasExtension(sync::ConfigKey::ext));
  EXPECT_TRUE(item.value().HasExtension(sync::ConfigValue::ext));
  const sync::ConfigValue &value =
      item.value().GetExtension(sync::ConfigValue::ext);
  EXPECT_TRUE(value.has_config());
  EXPECT_TRUE(adapter_.IsSameConfig(uploading_config, value.config()));

  // upload failure
  EXPECT_TRUE(adapter_.MarkUploaded(item, false));

  Config last_uploaded_config;
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastUploadedConfigFileName(), &last_uploaded_config));
  // The last uploaded config is not changed
  EXPECT_TRUE(adapter_.IsSameConfig(config, last_uploaded_config));
  EXPECT_FALSE(adapter_.IsSameConfig(uploading_config, last_uploaded_config));
}

TEST_F(ConfigAdapterTest, InitialUpload) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);

  ime_sync::SyncItems items;
  // No uploaded items
  EXPECT_EQ(0, items.size());

  EXPECT_TRUE(adapter_.GetItemsToUpload(&items));
  EXPECT_EQ(1, items.size());

  const ime_sync::SyncItem &item = items.Get(0);
  EXPECT_TRUE(item.key().HasExtension(sync::ConfigKey::ext));
  EXPECT_TRUE(item.value().HasExtension(sync::ConfigValue::ext));
  const sync::ConfigValue &value =
      item.value().GetExtension(sync::ConfigValue::ext);
  EXPECT_TRUE(value.has_config());
  EXPECT_TRUE(adapter_.IsSameConfig(config, value.config()));

  // upload success
  EXPECT_TRUE(adapter_.MarkUploaded(item, true));

  Config last_uploaded_config;
  EXPECT_TRUE(adapter_.LoadConfigFromFile(
      adapter_.GetLastUploadedConfigFileName(), &last_uploaded_config));
  EXPECT_TRUE(adapter_.IsSameConfig(config, last_uploaded_config));
}

TEST_F(ConfigAdapterTest, ComponentId) {
  EXPECT_EQ(ime_sync::MOZC_SETTING, adapter_.component_id());
}

}  // namespace mozc::sync
}  // namespace mozc
