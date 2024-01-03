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

#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/config_file_stream.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/strings/assign.h"
#include "base/system_util.h"
#include "base/version.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace config {
namespace {

constexpr absl::string_view kFileNamePrefix = "user://config";

void AddCharacterFormRule(const absl::string_view group,
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
  if constexpr (TargetIsAndroid()) {
    use_emoji_conversion_default = false;
  }
  return use_emoji_conversion_default;
}

class ConfigHandlerImpl final {
 public:
  ConfigHandlerImpl()
      :  // <user_profile>/config1.db
        filename_(absl::StrFormat("%s%d.db", kFileNamePrefix, kConfigVersion)) {
    Reload();
    ConfigHandler::GetDefaultConfig(&default_config_);
  }

  void GetConfig(Config *config) const ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<config::Config> GetConfig() const ABSL_LOCKS_EXCLUDED(mutex_);
  const Config &DefaultConfig() const;
  void SetConfig(const Config &config) ABSL_LOCKS_EXCLUDED(mutex_);
  void Reload() ABSL_LOCKS_EXCLUDED(mutex_);
  void SetConfigFileName(absl::string_view filename)
      ABSL_LOCKS_EXCLUDED(mutex_);
  const std::string &GetConfigFileName() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // copy config to config_ and do some
  // platform dependent hooks/rewrites
  void SetConfigInternal(Config config) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void ReloadUnlocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::string filename_ ABSL_GUARDED_BY(mutex_);
  Config config_ ABSL_GUARDED_BY(mutex_);
  Config default_config_;
  mutable absl::Mutex mutex_;
  uint64_t stored_config_hash_ ABSL_GUARDED_BY(mutex_) = 0;
};

ConfigHandlerImpl *GetConfigHandlerImpl() {
  return Singleton<ConfigHandlerImpl>::get();
}

// return current Config
void ConfigHandlerImpl::GetConfig(Config *config) const {
  absl::MutexLock lock(&mutex_);
  *config = config_;
}

// return current Config as a unique_ptr.
std::unique_ptr<config::Config> ConfigHandlerImpl::GetConfig() const {
  absl::MutexLock lock(&mutex_);
  return std::make_unique<config::Config>(config_);
}

const Config &ConfigHandlerImpl::DefaultConfig() const {
  return default_config_;
}

// set config and rewrite internal data
void ConfigHandlerImpl::SetConfigInternal(Config config) {
  config_ = std::move(config);

#ifdef MOZC_NO_LOGGING
  // Delete the optional field from the config.
  config_.clear_verbose_level();
  // Fall back if the default value is not the expected value.
  if (config_.verbose_level() != 0) {
    config_.set_verbose_level(0);
  }
#endif  // MOZC_NO_LOGGING

  Logging::SetConfigVerboseLevel(config_.verbose_level());

  // Initialize platform specific configuration.
  if (config_.session_keymap() == Config::NONE) {
    config_.set_session_keymap(ConfigHandler::GetDefaultKeyMap());
  }

#if defined(__ANDROID__) && defined(CHANNEL_DEV)
  stored_config_.mutable_general_config()->set_upload_usage_stats(true);
#endif  // CHANNEL_DEV && __ANDROID__

  if (GetPlatformSpecificDefaultEmojiSetting() &&
      !config_.has_use_emoji_conversion()) {
    config_.set_use_emoji_conversion(true);
  }
}

void ConfigHandlerImpl::SetConfig(const Config &config) {
  uint64_t hash = Fingerprint(config.SerializeAsString());

  absl::MutexLock lock(&mutex_);

  // If the wire format of config
  // (except for metadata, added by ConfigHandler::SetMetaData() soon later)
  // is identical to the one of the previously stored config, skip updating.
  if (stored_config_hash_ == hash) {
    return;
  }
  stored_config_hash_ = hash;

  Config output_config;
  output_config = config;

  ConfigHandler::SetMetaData(&output_config);

  VLOG(1) << "Setting new config: " << filename_;
  ConfigFileStream::AtomicUpdate(filename_, output_config.SerializeAsString());

#ifdef DEBUG
  std::string debug_content = absl::StrCat(
      "# This is a text-based config file for debugging.\n"
      "# Nothing happens when you edit this file manually.\n",
      output_config.DebugString());
  ConfigFileStream::AtomicUpdate(absl::StrCat(filename_, ".txt"),
                                 debug_content);
#endif  // DEBUG

  SetConfigInternal(std::move(output_config));
}

// Reload from file
void ConfigHandlerImpl::Reload() {
  absl::MutexLock lock(&mutex_);
  ReloadUnlocked();
}

void ConfigHandlerImpl::ReloadUnlocked() {
  VLOG(1) << "Reloading config file: " << filename_;
  std::unique_ptr<std::istream> is(ConfigFileStream::OpenReadBinary(filename_));
  Config input_proto;

  if (is == nullptr) {
    LOG(ERROR) << filename_ << " is not found";
  } else if (!input_proto.ParseFromIstream(is.get())) {
    LOG(ERROR) << filename_ << " is broken";
    input_proto.Clear();  // revert to default setting
  }

  // we set default config when file is broken
  SetConfigInternal(std::move(input_proto));
}

void ConfigHandlerImpl::SetConfigFileName(const absl::string_view filename) {
  absl::MutexLock lock(&mutex_);
  VLOG(1) << "set new config file name: " << filename;
  strings::Assign(filename_, filename);
  ReloadUnlocked();
}

const std::string &ConfigHandlerImpl::GetConfigFileName() {
  absl::MutexLock lock(&mutex_);
  return filename_;
}
}  // namespace

// Returns current Config
void ConfigHandler::GetConfig(Config *config) {
  GetConfigHandlerImpl()->GetConfig(config);
}

std::unique_ptr<config::Config> ConfigHandler::GetConfig() {
  return GetConfigHandlerImpl()->GetConfig();
}

void ConfigHandler::SetConfig(const Config &config) {
  GetConfigHandlerImpl()->SetConfig(config);
}

// static
void ConfigHandler::GetDefaultConfig(Config *config) {
  config->Clear();
  config->set_session_keymap(ConfigHandler::GetDefaultKeyMap());

  constexpr Config::CharacterForm kFullWidth = Config::FULL_WIDTH;
  constexpr Config::CharacterForm kLastForm = Config::LAST_FORM;
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

#if defined(__ANDROID__) && defined(CHANNEL_DEV)
  config->mutable_general_config()->set_upload_usage_stats(true);
#endif  // __ANDROID__ && CHANNEL_DEV

  if (GetPlatformSpecificDefaultEmojiSetting()) {
    config->set_use_emoji_conversion(true);
  }
}

// static
const Config &ConfigHandler::DefaultConfig() {
  return GetConfigHandlerImpl()->DefaultConfig();
}

// Reload from file
void ConfigHandler::Reload() { GetConfigHandlerImpl()->Reload(); }

void ConfigHandler::SetConfigFileName(const absl::string_view filename) {
  GetConfigHandlerImpl()->SetConfigFileName(filename);
}

const std::string &ConfigHandler::GetConfigFileName() {
  return GetConfigHandlerImpl()->GetConfigFileName();
}

// static
void ConfigHandler::SetMetaData(Config *config) {
  GeneralConfig *general_config = config->mutable_general_config();
  general_config->set_config_version(kConfigVersion);
  general_config->set_last_modified_time(
      absl::ToUnixSeconds(Clock::GetAbslTime()));
  general_config->set_last_modified_product_version(Version::GetMozcVersion());
  general_config->set_platform(SystemUtil::GetOSVersionString());
}

Config::SessionKeymap ConfigHandler::GetDefaultKeyMap() {
  if constexpr (TargetIsOSX()) {
    return config::Config::KOTOERI;
  } else if constexpr (TargetIsChromeOS()) {
    return config::Config::CHROMEOS;
  } else {
    return config::Config::MSIME;
  }
}

}  // namespace config
}  // namespace mozc
