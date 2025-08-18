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

#include "absl/strings/string_view.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace config {

inline constexpr int kConfigVersion = 1;

// This is pure static class.  All public static methods are thread-safe.
class ConfigHandler {
 public:
  ConfigHandler() = delete;
  ConfigHandler(const ConfigHandler&) = delete;
  ConfigHandler& operator=(const ConfigHandler&) = delete;

  // Returns current config.
  // This method returns a *copied* Config instance
  // so use this with caution, especially when custom_keymap_table exists
  // the copy operation against typically 5KB string always happens.
  static config::Config GetCopiedConfig();

  // Returns current const Config as a shared_ptr.
  // The actual config is shared by ConfigHandler and caller unless config
  // is updated. This method is also thread safe, i.e., it is safe for caller
  // to use the Config while ConfigHandler is loading another config
  // asynchronously.
  // While std::shared_ptr is not recommended in the style guide, we use it to
  // avoid unintentionally copying Config, which has a large footprint and is
  // updated asynchronously.
  static std::shared_ptr<const config::Config> GetSharedConfig();

  // Sets config.
  static void SetConfig(Config config);

  // Gets default config value.
  //
  // Using these functions are safer than using an uninitialized config value.
  // These functions are also thread-safe.
  static void GetDefaultConfig(Config* config);

  static const Config& DefaultConfig();
  static std::shared_ptr<const config::Config> GetSharedDefaultConfig();

  // Reloads config from storage.
  //
  // This method does nothing on imposed config.
  static void Reload();

  // Sets config file. (for unit testing)
  static void SetConfigFileNameForTesting(absl::string_view filename);

  // Get config file name.
  static std::string GetConfigFileNameForTesting();

  // Get default keymap for each platform.
  static Config::SessionKeymap GetDefaultKeyMap();

#ifdef _WIN32
  static void FixupFilePermission();
#endif  // _WIN32
};

}  // namespace config
}  // namespace mozc

#endif  // MOZC_CONFIG_CONFIG_HANDLER_H_
