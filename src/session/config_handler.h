// Copyright 2010, Google Inc.
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

#ifndef MOZC_SESSION_CONFIG_HANDLER_H_
#define MOZC_SESSION_CONFIG_HANDLER_H_

#include <string>

namespace mozc {
namespace config {
class Config;

enum {
  CONFIG_VERSION = 1,
};

// this is pure static class
class ConfigHandler {
 public:
  // return current Config
  static const Config &GetConfig();

  // return current Config
  static bool GetConfig(Config *config);

  // set config
  static bool SetConfig(const Config &config);

  // get default config value.
  // Using this function is safer than
  // using an uninitialized config value.
  static void GetDefaultConfig(Config *config);

  // reload config
  static bool Reload();

  // Set config file. (for unittesting)
  static void SetConfigFileName(const string &filename);

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

#endif  // MOZC_SESSION_CONFIG_HANDLER_H_
