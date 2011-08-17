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

// Handler of mozc configuration.

#ifndef MOZC_CONFIG_CONFIG_HANDLER_H_
#define MOZC_CONFIG_CONFIG_HANDLER_H_

#include <string>

namespace mozc {
namespace config {
class Config;

enum {
  CONFIG_VERSION = 1,
};

// This is pure static class.
class ConfigHandler {
 public:
  // Returns current config.
  static const Config &GetConfig();

  // Returns current config.
  static bool GetConfig(Config *config);

  // Returns current config.
  // If imposed config is not set, the result is the same as GetConfig().
  static const Config &GetStoredConfig();

  // Returns stored config.
  // If imposed config is not set, the result is the same as GetConfig().
  static bool GetStoredConfig(Config *config);

  // Sets config.
  static bool SetConfig(const Config &config);

  // Sets imposed config.
  //
  // If imposed config is set, the result of GetConfig() method
  // turns to be a mixture of current confing and imposed config.
  // When merging, imposed config's fields are prioritized.
  // If you want to use raw Store config (which is not affected by
  // the imposed config), call this method with the "empty"
  // (no field values are set) config.
  //
  // Imposed config is never saved to storage.
  //
  // Reload() method does not affect the imposed config, and the same imposed
  // config is used after reloading.
  // In other words, the imposed config is neither reloaded nor reset.
  static void SetImposedConfig(const Config &config);

  // Gets default config value.
  //
  // Using this function is safer than
  // using an uninitialized config value.
  static void GetDefaultConfig(Config *config);

  // Reloads config from storage.
  //
  // This method does nothing on imposed config.
  static bool Reload();

  // Sets config file. (for unittesting)
  static void SetConfigFileName(const string &filename);

  // Get config file name.
  static string GetConfigFileName();

  // Utilitiy function to put config meta data
  static void SetMetaData(Config *config);

  // Do not allow instantiation
 private:
  ConfigHandler() {}
  virtual ~ConfigHandler() {}
};

// macro for config field
// if (GET_CONFIG(incognite_mode) == false) {
//  }
#define GET_CONFIG(field) \
  config::ConfigHandler::GetConfig().field()
}  // namespace config
}  // namespace mozc

#endif  // MOZC_CONFIG_CONFIG_HANDLER_H_
