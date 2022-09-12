// Copyright 2010-2021, Google Inc.
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

// Handler of mozc configuration.

#include "config/config_handler.h"

#include <algorithm>
#include <istream>
#include <memory>
#include <string>

#include "base/clock.h"
#include "base/config_file_stream.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/version.h"
#include "protocol/config.pb.h"
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace config {
namespace {

constexpr char kFileNamePrefix[] = "user://config";

void AddCharacterFormRule(const char *group,
                          const Config::CharacterForm preedit_form,
                          const Config::CharacterForm conversion_form,
                          Config *config) {
  Config::CharacterFormRule *rule = config->add_character_form_rules();
  rule->set_group(group);
  rule->set_preedit_character_form(preedit_form);
  rule->set_conversion_character_form(conversion_form);
}

bool GetPlatformSpecificDefaultEmojiSetting() {
  // Disable Unicode emoji conversion by default on specific platforms.
  bool use_emoji_conversion_default = true;
#if defined(OS_WIN)
  if (!SystemUtil::IsWindows8OrLater()) {
    use_emoji_conversion_default = false;
  }
#elif defined(OS_ANDROID)
  use_emoji_conversion_default = false;
#endif  // defined(OS_WIN), defined(OS_ANDROID)
  return use_emoji_conversion_default;
}

class ConfigHandlerImpl {
 public:
  ConfigHandlerImpl() {
    // <user_profile>/config1.db
    filename_ = kFileNamePrefix;
    filename_ += std::to_string(CONFIG_VERSION);
    filename_ += ".db";
    Reload();
    ConfigHandler::GetDefaultConfig(&default_config_);
  }
  virtual ~ConfigHandlerImpl() {}
  bool GetConfig(Config *config) const;
  std::unique_ptr<config::Config> GetConfig() const;
  const Config &DefaultConfig() const;
  bool GetStoredConfig(Config *config) const;
  std::unique_ptr<config::Config> GetStoredConfig() const;
  bool SetConfig(const Config &config);
  void SetImposedConfig(const Config &config);
  bool Reload();
  void SetConfigFileName(const std::string &filename);
  std::string GetConfigFileName();

 private:
  // copy config to config_ and do some
  // platform dependent hooks/rewrites
  bool SetConfigInternal(const Config &config);
  void UpdateMergedConfig();
  bool ReloadUnlocked();

  std::string filename_;
  Config stored_config_;
  Config imposed_config_;
  // equals to config_.MergeFrom(imposed_config_)
  Config merged_config_;
  Config default_config_;
  mutable absl::Mutex mutex_;
};

ConfigHandlerImpl *GetConfigHandlerImpl() {
  return Singleton<ConfigHandlerImpl>::get();
}

// return current Config
bool ConfigHandlerImpl::GetConfig(Config *config) const {
  absl::MutexLock lock(&mutex_);
  *config = merged_config_;
  return true;
}

// return current Config as a unique_ptr.
std::unique_ptr<config::Config> ConfigHandlerImpl::GetConfig() const {
  absl::MutexLock lock(&mutex_);
  return std::make_unique<config::Config>(merged_config_);
}

const Config &ConfigHandlerImpl::DefaultConfig() const {
  return default_config_;
}

// return stored Config
bool ConfigHandlerImpl::GetStoredConfig(Config *config) const {
  absl::MutexLock lock(&mutex_);
  *config = stored_config_;
  return true;
}

// return stored Config as a unique_ptr.
std::unique_ptr<config::Config> ConfigHandlerImpl::GetStoredConfig() const {
  absl::MutexLock lock(&mutex_);
  return std::make_unique<config::Config>(stored_config_);
}

// set config and rewrite internal data
bool ConfigHandlerImpl::SetConfigInternal(const Config &config) {
  stored_config_ = config;

#ifdef MOZC_NO_LOGGING
  // Delete the optional field from the config.
  stored_config_.clear_verbose_level();
  // Fall back if the default value is not the expected value.
  if (stored_config_.verbose_level() != 0) {
    stored_config_.set_verbose_level(0);
  }
#endif  // MOZC_NO_LOGGING

  Logging::SetConfigVerboseLevel(stored_config_.verbose_level());

  // Initialize platform specific configuration.
  if (stored_config_.session_keymap() == Config::NONE) {
    stored_config_.set_session_keymap(ConfigHandler::GetDefaultKeyMap());
  }

#if defined(OS_ANDROID) && defined(CHANNEL_DEV)
  stored_config_.mutable_general_config()->set_upload_usage_stats(true);
#endif  // CHANNEL_DEV && OS_ANDROID

  if (GetPlatformSpecificDefaultEmojiSetting() &&
      !stored_config_.has_use_emoji_conversion()) {
    stored_config_.set_use_emoji_conversion(true);
  }

  UpdateMergedConfig();

  return true;
}

void ConfigHandlerImpl::UpdateMergedConfig() {
  merged_config_ = stored_config_;
  merged_config_.MergeFrom(imposed_config_);
}

bool ConfigHandlerImpl::SetConfig(const Config &config) {
  absl::MutexLock lock(&mutex_);
  Config output_config;
  output_config = config;

  ConfigHandler::SetMetaData(&output_config);

  VLOG(1) << "Setting new config: " << filename_;
  ConfigFileStream::AtomicUpdate(filename_, output_config.SerializeAsString());

#ifdef DEBUG
  std::string debug_content(
      "# This is a text-based config file for debugging.\n"
      "# Nothing happens when you edit this file manually.\n");
  debug_content += output_config.DebugString();
  ConfigFileStream::AtomicUpdate(filename_ + ".txt", debug_content);
#endif  // DEBUG

  return SetConfigInternal(output_config);
}

void ConfigHandlerImpl::SetImposedConfig(const Config &config) {
  absl::MutexLock lock(&mutex_);
  VLOG(1) << "Setting new overriding config";
  imposed_config_ = config;

#ifdef DEBUG
  std::string debug_content(
      "# This is a text-based config file for debugging.\n"
      "# Nothing happens when you edit this file manually.\n");
  debug_content += config.DebugString();
  ConfigFileStream::AtomicUpdate(filename_ + ".overriding.txt", debug_content);
#endif  // DEBUG
  UpdateMergedConfig();
}

// Reload from file
bool ConfigHandlerImpl::Reload() {
  absl::MutexLock lock(&mutex_);
  return ReloadUnlocked();
}

bool ConfigHandlerImpl::ReloadUnlocked() {
  VLOG(1) << "Reloading config file: " << filename_;
  std::unique_ptr<std::istream> is(ConfigFileStream::OpenReadBinary(filename_));
  Config input_proto;
  bool ret_code = true;

  if (is == nullptr) {
    LOG(ERROR) << filename_ << " is not found";
    ret_code = false;
  } else if (!input_proto.ParseFromIstream(is.get())) {
    LOG(ERROR) << filename_ << " is broken";
    input_proto.Clear();  // revert to default setting
    ret_code = false;
  }

  // we set default config when file is broken
  ret_code |= SetConfigInternal(input_proto);

  return ret_code;
}

void ConfigHandlerImpl::SetConfigFileName(const std::string &filename) {
  absl::MutexLock lock(&mutex_);
  VLOG(1) << "set new config file name: " << filename;
  filename_ = filename;
  ReloadUnlocked();
}

std::string ConfigHandlerImpl::GetConfigFileName() {
  absl::MutexLock lock(&mutex_);
  return filename_;
}
}  // namespace

// Returns current Config
bool ConfigHandler::GetConfig(Config *config) {
  return GetConfigHandlerImpl()->GetConfig(config);
}

std::unique_ptr<config::Config> ConfigHandler::GetConfig() {
  return GetConfigHandlerImpl()->GetConfig();
}

// Returns Stored Config
bool ConfigHandler::GetStoredConfig(Config *config) {
  return GetConfigHandlerImpl()->GetStoredConfig(config);
}

std::unique_ptr<config::Config> ConfigHandler::GetStoredConfig() {
  return GetConfigHandlerImpl()->GetStoredConfig();
}

bool ConfigHandler::SetConfig(const Config &config) {
  return GetConfigHandlerImpl()->SetConfig(config);
}

// Sets overriding config
void ConfigHandler::SetImposedConfig(const Config &config) {
  GetConfigHandlerImpl()->SetImposedConfig(config);
}

// static
void ConfigHandler::GetDefaultConfig(Config *config) {
  config->Clear();
  config->set_session_keymap(ConfigHandler::GetDefaultKeyMap());

  const Config::CharacterForm kFullWidth = Config::FULL_WIDTH;
  const Config::CharacterForm kLastForm = Config::LAST_FORM;
  // "ア"
  AddCharacterFormRule("ア", kFullWidth, kFullWidth, config);
  AddCharacterFormRule("A", kFullWidth, kLastForm, config);
  AddCharacterFormRule("0", kFullWidth, kLastForm, config);
  AddCharacterFormRule("(){}[]", kFullWidth, kLastForm, config);
  AddCharacterFormRule(".,", kFullWidth, kLastForm, config);
  // "。、",
  AddCharacterFormRule("。、", kFullWidth, kFullWidth, config);
  // "・「」"
  AddCharacterFormRule("・「」", kFullWidth, kFullWidth, config);
  AddCharacterFormRule("\"'", kFullWidth, kLastForm, config);
  AddCharacterFormRule(":;", kFullWidth, kLastForm, config);
  AddCharacterFormRule("#%&@$^_|`\\", kFullWidth, kLastForm, config);
  AddCharacterFormRule("~", kFullWidth, kLastForm, config);
  AddCharacterFormRule("<>=+-/*", kFullWidth, kLastForm, config);
  AddCharacterFormRule("?!", kFullWidth, kLastForm, config);

#if defined(OS_ANDROID) && defined(CHANNEL_DEV)
  config->mutable_general_config()->set_upload_usage_stats(true);
#endif  // OS_ANDROID && CHANNEL_DEV

  if (GetPlatformSpecificDefaultEmojiSetting()) {
    config->set_use_emoji_conversion(true);
  }
}

// static
const Config &ConfigHandler::DefaultConfig() {
  return GetConfigHandlerImpl()->DefaultConfig();
}

// Reload from file
bool ConfigHandler::Reload() { return GetConfigHandlerImpl()->Reload(); }

void ConfigHandler::SetConfigFileName(const std::string &filename) {
  GetConfigHandlerImpl()->SetConfigFileName(filename);
}

std::string ConfigHandler::GetConfigFileName() {
  return GetConfigHandlerImpl()->GetConfigFileName();
}

// static
void ConfigHandler::SetMetaData(Config *config) {
  GeneralConfig *general_config = config->mutable_general_config();
  general_config->set_config_version(CONFIG_VERSION);
  general_config->set_last_modified_time(Clock::GetTime());
  general_config->set_last_modified_product_version(Version::GetMozcVersion());
  general_config->set_platform(SystemUtil::GetOSVersionString());
}

Config::SessionKeymap ConfigHandler::GetDefaultKeyMap() {
#if defined(__APPLE__)
  return config::Config::KOTOERI;
#elif defined(OS_CHROMEOS)  // __APPLE__
  return config::Config::CHROMEOS;
#else   // __APPLE__ or OS_CHROMEOS
  return config::Config::MSIME;
#endif  // __APPLE__ or OS_CHROMEOS
}

}  // namespace config
}  // namespace mozc
