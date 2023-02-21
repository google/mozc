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

#ifndef MOZC_CONFIG_CONFIG_HANDLER_H_
#define MOZC_CONFIG_CONFIG_HANDLER_H_

#include <memory>
#include <string>

#include "protocol/config.pb.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace config {

enum {
  CONFIG_VERSION = 1,
};

// This is pure static class.  All public static methods are thread-safe.
class ConfigHandler {
 public:
  ConfigHandler() = delete;
  ConfigHandler(const ConfigHandler &) = delete;
  ConfigHandler &operator=(const ConfigHandler &) = delete;

  // Returns current config.
  // This method returns a *copied* Config instance
  // so use this with caution, especially when custom_keymap_table exists
  // the copy operation against typically 5KB string always happens.
  static void GetConfig(Config *config);

  // Returns current Config as a unique_ptr.
  // The same performance note as GetConfig(Config*) applies.
  static std::unique_ptr<config::Config> GetConfig();

  // Aliases of GetConfig.
  // TODO(b/267705984): Clean up these methods.
  static void GetStoredConfig(Config *config);
  static std::unique_ptr<config::Config> GetStoredConfig();

  // Sets config.
  static void SetConfig(const Config &config);

  // Gets default config value.
  //
  // Using these functions are safer than using an uninitialized config value.
  // These functions are also thread-safe.
  static void GetDefaultConfig(Config *config);
  static const Config &DefaultConfig();

  // Reloads config from storage.
  //
  // This method does nothing on imposed config.
  static void Reload();

  // Sets config file. (for unittesting)
  static void SetConfigFileName(absl::string_view filename);

  // Get config file name.
  static std::string GetConfigFileName();

  // Utilitiy function to put config meta data
  static void SetMetaData(Config *config);

  // Get default keymap for each platform.
  static Config::SessionKeymap GetDefaultKeyMap();
};

}  // namespace config
}  // namespace mozc

#endif  // MOZC_CONFIG_CONFIG_HANDLER_H_
