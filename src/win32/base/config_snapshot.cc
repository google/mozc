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

#include "win32/base/config_snapshot.h"

#include <algorithm>
#include <memory>

#include "config/config_handler.h"
#include "protocol/config.pb.h"
#include "session/key_info_util.h"

namespace mozc {
namespace win32 {
namespace {

using ::mozc::config::Config;

constexpr size_t kMaxDirectModeKeys = 128;

struct StaticConfigSnapshot {
  bool use_kana_input;
  bool use_keyboard_to_change_preedit_method;
  bool use_mode_indicator;
  size_t num_direct_mode_keys;
  KeyInformation direct_mode_keys[kMaxDirectModeKeys];
};

StaticConfigSnapshot GetConfigSnapshotImpl() {
  std::shared_ptr<const Config> config =
      config::ConfigHandler::GetSharedConfig();

  StaticConfigSnapshot snapshot = {};
  snapshot.use_kana_input = (config->preedit_method() == Config::KANA);
  snapshot.use_keyboard_to_change_preedit_method =
      config->use_keyboard_to_change_preedit_method();
  snapshot.use_mode_indicator = config->use_mode_indicator();

  const auto &direct_mode_keys =
      KeyInfoUtil::ExtractSortedDirectModeKeys(*config);
  const size_t size_to_be_copied =
      std::min(direct_mode_keys.size(), kMaxDirectModeKeys);
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
bool ConfigSnapshot::Get(Info *info) {
  // Note: Thread-safety is not required.
  static const StaticConfigSnapshot cached_snapshot = GetConfigSnapshotImpl();
  info->use_kana_input = cached_snapshot.use_kana_input;
  info->use_keyboard_to_change_preedit_method =
      cached_snapshot.use_keyboard_to_change_preedit_method;
  info->use_mode_indicator = cached_snapshot.use_mode_indicator;
  info->direct_mode_keys.resize(cached_snapshot.num_direct_mode_keys);
  for (size_t i = 0; i < cached_snapshot.num_direct_mode_keys; ++i) {
    info->direct_mode_keys[i] = cached_snapshot.direct_mode_keys[i];
  }
  return true;
}

}  // namespace win32
}  // namespace mozc
