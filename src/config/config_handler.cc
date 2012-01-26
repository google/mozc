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

// Handler of mozc configuration.
#include "config/config_handler.h"

#include "base/config_file_stream.h"
#include "base/singleton.h"
#include "base/version.h"
#include "config/config.pb.h"

#include "base/logging.h"

namespace mozc {
namespace config {

namespace {

static const char kFileNamePrefix[] = "user://config";

void AddCharacterFormRule(config::Config *config,
                          const char *group,
                          config::Config::CharacterForm preedit_form,
                          config::Config::CharacterForm conversion_form) {
  config::Config::CharacterFormRule *rule =
      config->add_character_form_rules();
  rule->set_group(group);
  rule->set_preedit_character_form(preedit_form);
  rule->set_conversion_character_form(conversion_form);
}

class ConfigHandlerImpl {
 public:
  ConfigHandlerImpl() {
    // <user_profile>/config1.db
    filename_ = kFileNamePrefix;
    filename_ += Util::SimpleItoa(CONFIG_VERSION);
    filename_ += ".db";
    Reload();
  }
  virtual ~ConfigHandlerImpl() {}
  const Config &GetConfig() const;
  bool GetConfig(Config *config) const;
  const Config &GetStoredConfig() const;
  bool GetStoredConfig(Config *config) const;
  bool SetConfig(const Config &config);
  void SetImposedConfig(const Config &config);
  bool Reload();
  void SetConfigFileName(const string &filename);
  string GetConfigFileName();

 private:
  // copy config to config_ and do some
  // platform dependent hooks/rewrites
  bool SetConfigInternal(const Config &config);
  void UpdateMergedConfig();

  string filename_;
  mozc::config::Config stored_config_;
  mozc::config::Config imposed_config_;
  // equals to config_.MergeFrom(imposed_config_)
  mozc::config::Config merged_config_;
};

ConfigHandlerImpl *GetConfigHandlerImpl() {
  return Singleton<ConfigHandlerImpl>::get();
}

const Config &ConfigHandlerImpl::GetConfig() const {
  return merged_config_;
}

// return current Config
bool ConfigHandlerImpl::GetConfig(Config *config) const {
  config->CopyFrom(merged_config_);
  return true;
}

const Config &ConfigHandlerImpl::GetStoredConfig() const {
  return stored_config_;
}

// return stored Config
bool ConfigHandlerImpl::GetStoredConfig(Config *config) const {
  config->CopyFrom(stored_config_);
  return true;
}

// set config and rewirte internal data
bool ConfigHandlerImpl::SetConfigInternal(const Config &config) {
  stored_config_.CopyFrom(config);

#ifdef NO_LOGGING
  // Delete the optional field from the config.
  stored_config_.clear_verbose_level();
  // Fall back if the default value is not the expected value.
  if (stored_config_.verbose_level() != 0) {
    stored_config_.set_verbose_level(0);
  }
#endif

  Logging::SetConfigVerboseLevel(stored_config_.verbose_level());

  // Initialize platform specific configuration.
  if (stored_config_.session_keymap() == Config::NONE) {
#ifdef OS_MACOSX
    stored_config_.set_session_keymap(Config::KOTOERI);
#else  // OS_MACOSX
    stored_config_.set_session_keymap(Config::MSIME);
#endif  // OS_MACOSX
  }

#ifdef CHANNEL_DEV
#endif  // CHANNEL_DEV

  UpdateMergedConfig();

  return true;
}

void ConfigHandlerImpl::UpdateMergedConfig() {
  merged_config_.CopyFrom(stored_config_);
  merged_config_.MergeFrom(imposed_config_);
}

bool ConfigHandlerImpl::SetConfig(const Config &config) {
  Config output_config;
  output_config.CopyFrom(config);

  ConfigHandler::SetMetaData(&output_config);

  VLOG(1) << "Setting new config: " << filename_;
  ConfigFileStream::AtomicUpdate(filename_, output_config.SerializeAsString());

#ifdef DEBUG
  string debug_content(
      "# This is a text-based config file for debugging.\n"
      "# Nothing happens when you edit this file manually.\n");
  debug_content += output_config.DebugString();
  ConfigFileStream::AtomicUpdate(filename_ + ".txt", debug_content);
#endif  // DEBUG

  return SetConfigInternal(output_config);
}

void ConfigHandlerImpl::SetImposedConfig(const Config &config) {
  VLOG(1) << "Setting new overriding config";
  imposed_config_.CopyFrom(config);

#ifdef DEBUG
  string debug_content(
      "# This is a text-based config file for debugging.\n"
      "# Nothing happens when you edit this file manually.\n");
  debug_content += config.DebugString();
  ConfigFileStream::AtomicUpdate(filename_ + ".overriding.txt", debug_content);
#endif  // DEBUG
  UpdateMergedConfig();
}

// Reload from file
bool ConfigHandlerImpl::Reload() {
  VLOG(1) << "Reloading config file: " << filename_;
  scoped_ptr<istream> is(ConfigFileStream::Open(
                             filename_,
                             ios::in | ios::binary));
  Config input_proto;
  bool ret_code = true;

  if (is.get() == NULL) {
    LOG(ERROR) << filename_ << " is not found";
    ret_code = false;
  } else if (!input_proto.ParseFromIstream(is.get())) {
    LOG(ERROR) << filename_ << " is broken";
    input_proto.Clear();   // revert to default setting
    ret_code = false;
  }

  // we set default config when file is broekn
  ret_code |= SetConfigInternal(input_proto);

  return ret_code;
}

void ConfigHandlerImpl::SetConfigFileName(const string &filename) {
  VLOG(1) << "set new config file name: " << filename;
  filename_ = filename;
  Reload();
}

string ConfigHandlerImpl::GetConfigFileName() {
  return filename_;
}
}  // namespace

const Config &ConfigHandler::GetConfig() {
  return GetConfigHandlerImpl()->GetConfig();
}

// return current Config
bool ConfigHandler::GetConfig(Config *config) {
  return GetConfigHandlerImpl()->GetConfig(config);
}

const Config &ConfigHandler::GetStoredConfig() {
  return GetConfigHandlerImpl()->GetStoredConfig();
}

// return Stored Config
bool ConfigHandler::GetStoredConfig(Config *config) {
  return GetConfigHandlerImpl()->GetStoredConfig(config);
}

// set config
bool ConfigHandler::SetConfig(const Config &config) {
  return GetConfigHandlerImpl()->SetConfig(config);
}

// set overriding config
void ConfigHandler::SetImposedConfig(const Config &config) {
  GetConfigHandlerImpl()->SetImposedConfig(config);
}

void ConfigHandler::GetDefaultConfig(Config *config) {
  config->Clear();
#ifdef OS_MACOSX
  config->set_session_keymap(Config::KOTOERI);
#else  // OS_MACOSX
  config->set_session_keymap(Config::MSIME);
#endif  // OS_MACOSX

  // "ア"
  AddCharacterFormRule(config, "\xE3\x82\xA2",
                       config::Config::FULL_WIDTH, config::Config::FULL_WIDTH);
  AddCharacterFormRule(config, "A",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  AddCharacterFormRule(config, "0",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  AddCharacterFormRule(config, "(){}[]",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  AddCharacterFormRule(config, ".,",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  // "。、",
  AddCharacterFormRule(config, "\xE3\x80\x82\xE3\x80\x81",
                       config::Config::FULL_WIDTH, config::Config::FULL_WIDTH);
  // "・「」"
  AddCharacterFormRule(config, "\xE3\x83\xBB\xE3\x80\x8C\xE3\x80\x8D",
                       config::Config::FULL_WIDTH, config::Config::FULL_WIDTH);
  AddCharacterFormRule(config, "\"'",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  AddCharacterFormRule(config, ":;",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  AddCharacterFormRule(config, "#%&@$^_|`\\",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  AddCharacterFormRule(config, "~",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  AddCharacterFormRule(config, "<>=+-/*",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);
  AddCharacterFormRule(config, "?!",
                       config::Config::FULL_WIDTH, config::Config::LAST_FORM);

#ifdef CHANNEL_DEV
#endif  // CHANNEL_DEV
}

// Reload from file
bool ConfigHandler::Reload() {
  return GetConfigHandlerImpl()->Reload();
}

void ConfigHandler::SetConfigFileName(const string &filename) {
  GetConfigHandlerImpl()->SetConfigFileName(filename);
}

string ConfigHandler::GetConfigFileName() {
  return GetConfigHandlerImpl()->GetConfigFileName();
}

// static
void ConfigHandler::SetMetaData(Config *config) {
  GeneralConfig *general_config = config->mutable_general_config();
  general_config->set_config_version(CONFIG_VERSION);
  general_config->set_last_modified_time(Util::GetTime());
  general_config->set_last_modified_product_version(Version::GetMozcVersion());
  general_config->set_platform(Util::GetOSVersionString());
}

}  // config
}  // mozc
