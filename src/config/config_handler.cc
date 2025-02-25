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

#include <atomic>
#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/config_file_stream.h"
#include "base/hash.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/strings/assign.h"
#include "base/system_util.h"
#include "base/version.h"
#include "base/vlog.h"
#include "protocol/config.pb.h"

#ifdef _WIN32
#include "base/win32/wide_char.h"
#include "base/win32/win_sandbox.h"
#endif  // _WIN32

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

Config CreateDefaultConfig() {
  Config config;
  config.set_session_keymap(ConfigHandler::GetDefaultKeyMap());

  constexpr Config::CharacterForm kFullWidth = Config::FULL_WIDTH;
  constexpr Config::CharacterForm kLastForm = Config::LAST_FORM;
  // "ア"
  AddCharacterFormRule("ア", kFullWidth, kFullWidth, &config);
  AddCharacterFormRule("A", kFullWidth, kLastForm, &config);
  AddCharacterFormRule("0", kFullWidth, kLastForm, &config);
  AddCharacterFormRule("(){}[]", kFullWidth, kLastForm, &config);
  AddCharacterFormRule(".,", kFullWidth, kLastForm, &config);
  // "。、",
  AddCharacterFormRule("。、", kFullWidth, kFullWidth, &config);
  // "・「」"
  AddCharacterFormRule("・「」", kFullWidth, kFullWidth, &config);
  AddCharacterFormRule("\"'", kFullWidth, kLastForm, &config);
  AddCharacterFormRule(":;", kFullWidth, kLastForm, &config);
  AddCharacterFormRule("#%&@$^_|`\\", kFullWidth, kLastForm, &config);
  AddCharacterFormRule("~", kFullWidth, kLastForm, &config);
  AddCharacterFormRule("<>=+-/*", kFullWidth, kLastForm, &config);
  AddCharacterFormRule("?!", kFullWidth, kLastForm, &config);

#if defined(__ANDROID__) && defined(CHANNEL_DEV)
  config.mutable_general_config().set_upload_usage_stats(true);
#endif  // __ANDROID__ && CHANNEL_DEV

  if (GetPlatformSpecificDefaultEmojiSetting()) {
    config.set_use_emoji_conversion(true);
  }

  return config;
}

void SetMetaData(Config *config) {
  GeneralConfig *general_config = config->mutable_general_config();
  general_config->set_config_version(kConfigVersion);
  general_config->set_last_modified_time(
      absl::ToUnixSeconds(Clock::GetAbslTime()));
  general_config->set_last_modified_product_version(Version::GetMozcVersion());
  general_config->set_platform(SystemUtil::GetOSVersionString());
}

void NormalizeConfig(Config *config) {
#ifdef NDEBUG
  // Delete the optional field from the config.
  config->clear_verbose_level();
  // Fall back if the default value is not the expected value.
  if (config->verbose_level() != 0) {
    config->set_verbose_level(0);
  }
#endif  // NDEBUG

  mozc::internal::SetConfigVLogLevel(config->verbose_level());

  // Initialize platform specific configuration.
  if (config->session_keymap() == Config::NONE) {
    config->set_session_keymap(ConfigHandler::GetDefaultKeyMap());
  }

#if defined(__ANDROID__) && defined(CHANNEL_DEV)
  config->mutable_general_config()->set_upload_usage_stats(true);
#endif  // CHANNEL_DEV && __ANDROID__

  if (GetPlatformSpecificDefaultEmojiSetting() &&
      !config->has_use_emoji_conversion()) {
    config->set_use_emoji_conversion(true);
  }
}

class ConfigHandlerImpl final {
 public:
  ConfigHandlerImpl()
      :  // <user_profile>/config1.db
        filename_(absl::StrFormat("%s%d.db", kFileNamePrefix, kConfigVersion)),
        default_config_(std::make_shared<Config>(CreateDefaultConfig())) {
    Reload();
  }

  std::shared_ptr<const Config> GetSharedConfig() const;
  std::shared_ptr<const Config> GetSharedDefaultConfig() const;

  void SetConfig(const Config &config);
  void Reload();

  void SetConfigFileName(absl::string_view filename)
      ABSL_LOCKS_EXCLUDED(mutex_);

  std::string GetConfigFileName() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  void SetConfigInternal(std::shared_ptr<Config> config);

  std::string filename_ ABSL_GUARDED_BY(mutex_);
  std::atomic<uint64_t> config_hash_ = 0;   // hash of final config.
  std::atomic<uint64_t> content_hash_ = 0;  // hash w/o metadata.
  std::shared_ptr<Config> config_;
  std::shared_ptr<const Config> default_config_;
  mutable absl::Mutex mutex_;
};

ConfigHandlerImpl *GetConfigHandlerImpl() {
  return Singleton<ConfigHandlerImpl>::get();
}

std::shared_ptr<const Config> ConfigHandlerImpl::GetSharedConfig() const {
  // TODO(all): use std::atomic<std::shared_ptr<T>> once it is fully supported.
  return std::atomic_load(&config_);
}

std::shared_ptr<const Config> ConfigHandlerImpl::GetSharedDefaultConfig()
    const {
  return default_config_;
}

// update internal data
void ConfigHandlerImpl::SetConfigInternal(std::shared_ptr<Config> config) {
  std::atomic_store(&config_, std::move(config));
}

void ConfigHandlerImpl::SetConfig(const Config &config) {
  const uint64_t config_hash = Fingerprint(config.SerializeAsString());

  // If the wire format of config is identical to the one of the previously
  // stored config, skip updating.
  if (config_hash_ == config_hash) {
    return;
  }

  auto output_config = std::make_shared<Config>(config);

  // Fix config because `config` may be broken.
  NormalizeConfig(output_config.get());

  // If the wire format of the config w/o metadata is identical to the one of
  // the previous config, skip updating.
  output_config->mutable_general_config()->clear_last_modified_time();
  const uint64_t content_hash = Fingerprint(output_config->SerializeAsString());
  if (content_hash_ == content_hash) {
    return;
  }
  content_hash_ = content_hash;

  // Set metadata and update `config_hash_`.
  SetMetaData(output_config.get());
  config_hash_ = Fingerprint(output_config->SerializeAsString());

  const std::string filename = GetConfigFileName();

  MOZC_VLOG(1) << "Setting new config: " << filename;
  ConfigFileStream::AtomicUpdate(filename, output_config->SerializeAsString());

#ifdef _WIN32
  ConfigFileStream::FixupFilePermission(filename);
#endif  // _WIN32

#ifdef DEBUG
  const std::string debug_content = absl::StrCat(
      "# This is a text-based config file for debugging.\n"
      "# Nothing happens when you edit this file manually.\n",
      *output_config);
  ConfigFileStream::AtomicUpdate(absl::StrCat(filename, ".txt"), debug_content);
#endif  // DEBUG

  SetConfigInternal(output_config);
}

// Reload from file
void ConfigHandlerImpl::Reload() {
  const std::string filename = GetConfigFileName();

  MOZC_VLOG(1) << "Reloading config file: " << filename;
  std::unique_ptr<std::istream> is(ConfigFileStream::OpenReadBinary(filename));
  auto input_config = std::make_shared<Config>();

  if (is == nullptr) {
    LOG(ERROR) << filename << " is not found";
  } else if (!input_config->ParseFromIstream(is.get())) {
    LOG(ERROR) << filename << " is broken";
    input_config->Clear();  // revert to default setting
  }

  // we set default config when file is broken
  NormalizeConfig(input_config.get());

  SetConfigInternal(input_config);
}

void ConfigHandlerImpl::SetConfigFileName(const absl::string_view filename) {
  {
    absl::WriterMutexLock lock(&mutex_);
    MOZC_VLOG(1) << "set new config file name: " << filename;
    strings::Assign(filename_, filename);
  }
  Reload();
}

std::string ConfigHandlerImpl::GetConfigFileName() const {
  absl::ReaderMutexLock lock(&mutex_);
  return filename_;
}
}  // namespace

Config ConfigHandler::GetCopiedConfig() { return *GetSharedConfig(); }

std::shared_ptr<const Config> ConfigHandler::GetSharedConfig() {
  return GetConfigHandlerImpl()->GetSharedConfig();
}

void ConfigHandler::SetConfig(const Config &config) {
  GetConfigHandlerImpl()->SetConfig(config);
}

// static
void ConfigHandler::GetDefaultConfig(Config *config) {
  *config = DefaultConfig();
}

std::shared_ptr<const Config> ConfigHandler::GetSharedDefaultConfig() {
  return GetConfigHandlerImpl()->GetSharedDefaultConfig();
}

// static
const Config &ConfigHandler::DefaultConfig() {
  return *GetSharedDefaultConfig();
}

// Reload from file
void ConfigHandler::Reload() { GetConfigHandlerImpl()->Reload(); }

void ConfigHandler::SetConfigFileNameForTesting(
    const absl::string_view filename) {
  GetConfigHandlerImpl()->SetConfigFileName(filename);
}

std::string ConfigHandler::GetConfigFileNameForTesting() {
  return GetConfigHandlerImpl()->GetConfigFileName();
}

Config::SessionKeymap ConfigHandler::GetDefaultKeyMap() {
  if constexpr (TargetIsOSX()) {
    return Config::KOTOERI;
  } else if constexpr (TargetIsChromeOS()) {
    return Config::CHROMEOS;
  } else {
    return Config::MSIME;
  }
}

#ifdef _WIN32
// static
void ConfigHandler::FixupFilePermission() {
  ConfigFileStream::FixupFilePermission(GetConfigFileNameForTesting());
}
#endif  // _WIN32

}  // namespace config
}  // namespace mozc
