// Copyright 2010-2014, Google Inc.
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

#include "win32/base/config_snapshot.h"

#include "base/win_util.h"
#include "client/client_interface.h"
#include "config/config.pb.h"
#include "config/config_handler.h"

namespace mozc {
namespace win32 {
namespace {

using ::mozc::client::ClientInterface;
using ::mozc::config::Config;

const size_t kMaxDirectModeKeys = 128;

struct StaticConfigSnapshot {
  bool use_kana_input;
  bool use_keyboard_to_change_preedit_method;
  bool use_mode_indicator;
  size_t num_direct_mode_keys;
  KeyInformation direct_mode_keys[kMaxDirectModeKeys];
};

bool IsProcessSandboxedImpl() {
  bool is_restricted = false;
  if (!WinUtil::IsProcessRestricted(::GetCurrentProcess(), &is_restricted)) {
    return true;
  }
  if (is_restricted) {
    return true;
  }

  bool in_appcontainer = false;
  if (!WinUtil::IsProcessInAppContainer(::GetCurrentProcess(),
                                        &in_appcontainer)) {
    return true;
  }

  return in_appcontainer;
}

bool IsProcessSandboxed() {
  // Thread safety is not required.
  static bool sandboxed = IsProcessSandboxedImpl();
  return sandboxed;
}

StaticConfigSnapshot GetConfigSnapshotImpl(ClientInterface *client) {
  StaticConfigSnapshot snapshot = {};

  Config config;
  if (IsProcessSandboxed()) {
    // config1.db is likely to be inaccessible from sandboxed processes.
    // So retrieve the config data from the server process.
    // CAVEATS: ConfigHandler::GetConfig always returns true even when it fails
    // to load the config file due to the sandbox. b/10449414
    if (!client->CheckVersionOrRestartServer()) {
      return snapshot;
    }
    if (!client->GetConfig(&config)) {
      return snapshot;
    }
  } else {
    // config1.db should be readable in this case.
    config.CopyFrom(config::ConfigHandler::GetConfig());
  }

  snapshot.use_kana_input = (config.preedit_method() == Config::KANA);
  snapshot.use_keyboard_to_change_preedit_method =
      config.use_keyboard_to_change_preedit_method();
  snapshot.use_mode_indicator = config.use_mode_indicator();

  const auto &direct_mode_keys =
      KeyInfoUtil::ExtractSortedDirectModeKeys(config);
  const size_t size_to_be_copied =
      min(direct_mode_keys.size(), kMaxDirectModeKeys);
  snapshot.num_direct_mode_keys = size_to_be_copied;
  for (size_t i = 0; i < size_to_be_copied; ++i) {
    snapshot.direct_mode_keys[i] = direct_mode_keys[i];
  }

  return snapshot;
}

}  // namespace

ConfigSnapshot::Info::Info()
    : use_kana_input(false),
      use_keyboard_to_change_preedit_method(false),
      use_mode_indicator(false) {}

// static
ConfigSnapshot::Info ConfigSnapshot::Get(client::ClientInterface *client) {
  if (client == nullptr) {
    return ConfigSnapshot::Info();
  }

  // Note: Thread-safety is not required.
  static auto static_snapshot = GetConfigSnapshotImpl(client);

  ConfigSnapshot::Info snapshot;
  snapshot.use_kana_input = static_snapshot.use_kana_input;
  snapshot.use_keyboard_to_change_preedit_method =
      static_snapshot.use_keyboard_to_change_preedit_method;
  snapshot.use_mode_indicator = static_snapshot.use_mode_indicator;
  snapshot.direct_mode_keys.resize(static_snapshot.num_direct_mode_keys);
  for (size_t i = 0; i < static_snapshot.num_direct_mode_keys; ++i) {
    snapshot.direct_mode_keys[i] = static_snapshot.direct_mode_keys[i];
  }
  return snapshot;
}

}  // namespace win32
}  // namespace mozc
