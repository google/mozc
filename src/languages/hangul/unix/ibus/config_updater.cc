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

#include "languages/hangul/unix/ibus/config_updater.h"

#include "base/base.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "languages/hangul/session.h"
#include "unix/ibus/config_util.h"

using mozc::config::HangulConfig;

namespace mozc {
namespace hangul {
#ifdef OS_CHROMEOS
namespace {
const char kHangulSectionName[] = "engine/Hangul";
const char kKeyboardLayout[] = "HangulKeyboard";
const char kHanjaKeyBinding[] = "HanjaKeyBindings";
}  // anonymous namespace

ConfigUpdater::ConfigUpdater() {
  name_to_field_[kKeyboardLayout] = "KeyboardLayout";
  name_to_field_[kHanjaKeyBinding] = "HanjaKeyBindings";

  name_to_keyboard_types_["2"] = HangulConfig::KEYBOARD_Dubeolsik;
  name_to_keyboard_types_["2y"] = HangulConfig::KEYBOARD_DubeolsikYetgeul;
  name_to_keyboard_types_["32"] = HangulConfig::KEYBOARD_SebeolsikDubeol;
  name_to_keyboard_types_["39"] = HangulConfig::KEYBOARD_Sebeolsik390;
  name_to_keyboard_types_["3f"] = HangulConfig::KEYBOARD_SebeolsikFinal;
  name_to_keyboard_types_["3s"] = HangulConfig::KEYBOARD_SebeolsikNoshift;
  name_to_keyboard_types_["3y"] = HangulConfig::KEYBOARD_SebeolsikYetgeul;
  name_to_keyboard_types_["ro"] = HangulConfig::KEYBOARD_Romaja;
  name_to_keyboard_types_["ahn"] = HangulConfig::KEYBOARD_Ahnmatae;
}

// static
void ConfigUpdater::ConfigValueChanged(IBusConfig *config,
                                       const gchar *section,
                                       const gchar *name,
#if IBUS_CHECK_VERSION(1, 3, 99)
                                       GVariant *value,
#else
                                       GValue *value,
#endif
                                       gpointer user_data) {
  Singleton<ConfigUpdater>::get()->UpdateConfig(section, name, value);
}

void ConfigUpdater::UpdateConfig(const gchar *section,
                                 const gchar *name,
#if IBUS_CHECK_VERSION(1, 3, 99)
                                 GVariant *value
#else
                                 GValue *value
#endif
                                 ) {
  if (!section || !name || !value) {
    return;
  }

  if (g_strcmp0(section, kHangulSectionName) != 0) {
    return;
  }

  config::Config mozc_config;
  config::ConfigHandler::GetConfig(&mozc_config);
  config::HangulConfig config;
  config.CopyFrom(mozc_config.hangul_config());

  if (g_strcmp0(name, kKeyboardLayout) == 0) {
    const gchar *string_value = NULL;
    if (!ibus::ConfigUtil::GetString(value, &string_value)) {
      LOG(ERROR) << "Type mismatch: keyboard type is expected to be a string";
      return;
    }
    map<string, HangulConfig::KeyboardTypes>::const_iterator it =
        name_to_keyboard_types_.find(string_value);
    if (it == name_to_keyboard_types_.end()) {
      LOG(ERROR) << "Cannot find a valid keyboard type for " << string_value;
      return;
    }
    config.set_keyboard_type(it->second);
  }

  if (g_strcmp0(name, kHanjaKeyBinding) == 0) {
    const gchar *string_value = NULL;
    if (!ibus::ConfigUtil::GetString(value, &string_value)) {
      LOG(ERROR) << "Type mismatch: hanja keys is expected to be a string";
    }

    vector<string> keys;
    Util::SplitStringUsing(string_value, ",", &keys);

    for (size_t i = 0; i < keys.size(); ++i) {
      config.add_hanja_keys(keys[i]);
    }
  }
  Session::UpdateConfig(config);
}

const map<string, const char*>& ConfigUpdater::name_to_field() {
  return name_to_field_;
}

// static
void ConfigUpdater::InitConfig(IBusConfig *config) {
  // Initialize the mozc config with the config loaded from ibus-memconf, which
  // is the primary config storage on Chrome OS.
  ibus::ConfigUtil::InitConfig(
      config,
      kHangulSectionName,
      Singleton<ConfigUpdater>::get()->name_to_field());
}

#endif  // OS_CHROMEOS

}  // namespace hangul
}  // namespace mozc
