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

#include "languages/chewing/unix/ibus/config_updater.h"

#include "base/base.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/singleton.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "languages/chewing/session.h"
#include "unix/ibus/config_util.h"

using mozc::config::ChewingConfig;

namespace mozc {
namespace chewing {
#ifdef OS_CHROMEOS
namespace {
const char kChewingSectionName[] = "engine/Chewing";

const char kKeyboardTypeName[] = "KBType";
const char kSelectionKeysName[] = "selKeys";
const char kHsuSelectionKeysTypeName[] = "hsuSelKeyType";
}  // anonymous namespace

ConfigUpdater::ConfigUpdater() {
  name_to_field_["autoShiftCur"] = "automatic_shift_cursor";
  name_to_field_["addPhraseDirection"] = "add_phrase_direction";
  name_to_field_["easySymbolInput"] = "easy_symbol_input";
  name_to_field_["escCleanAllBuf"] = "escape_cleans_all_buffer";
  name_to_field_["forceLowercaseEnglish"] = "force_lowercase_english";
  name_to_field_["plainZhuyin"] = "plain_zhuyin";
  name_to_field_["phraseChoiceRearward"] = "phrase_choice_rearward";
  name_to_field_["spaceAsSelection"] = "space_as_selection";
  name_to_field_["maxChiSymbolLen"] = "maximum_chinese_character_length";
  name_to_field_["candPerPage"] = "candidates_per_page";
  name_to_field_["KBType"] = "keyboard_type";
  name_to_field_["selKeys"] = "selection_keys";
  name_to_field_["hsuSelKeyType"] = "hsu_selection_keys";

  name_to_keyboard_type_["default"] = ChewingConfig::DEFAULT;
  name_to_keyboard_type_["hsu"] = ChewingConfig::HSU;
  name_to_keyboard_type_["ibm"] = ChewingConfig::IBM;
  name_to_keyboard_type_["gin_yieh"] = ChewingConfig::GIN_YIEH;
  name_to_keyboard_type_["eten"] = ChewingConfig::ETEN;
  name_to_keyboard_type_["eten26"] = ChewingConfig::ETEN26;
  name_to_keyboard_type_["dvorak"] = ChewingConfig::DVORAK;
  name_to_keyboard_type_["dvorak_hsu"] = ChewingConfig::DVORAK_HSU;
  name_to_keyboard_type_["dachen_26"] = ChewingConfig::DACHEN_26;
  name_to_keyboard_type_["hanyu"] = ChewingConfig::HANYU;

  name_to_selection_keys_["1234567890"] = ChewingConfig::SELECTION_1234567890;
  name_to_selection_keys_["asdfghjkl;"] = ChewingConfig::SELECTION_asdfghjkl;
  name_to_selection_keys_["asdfzxcv89"] = ChewingConfig::SELECTION_asdfzxcv89;
  name_to_selection_keys_["asdfjkl789"] = ChewingConfig::SELECTION_asdfjkl789;
  name_to_selection_keys_["aoeu;qjkix"] = ChewingConfig::SELECTION_aoeuqjkix;
  name_to_selection_keys_["aoeuhtnsid"] = ChewingConfig::SELECTION_aoeuhtnsid;
  name_to_selection_keys_["aoeuidhtns"] = ChewingConfig::SELECTION_aoeuidhtns;
  name_to_selection_keys_["1234qweras"] = ChewingConfig::SELECTION_1234qweras;

  name_to_hsu_keys_[1] = ChewingConfig::HSU_asdfjkl789;
  name_to_hsu_keys_[2] = ChewingConfig::HSU_asdfzxcv89;
}

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

  if (g_strcmp0(section, kChewingSectionName) != 0) {
    return;
  }

  config::Config mozc_config;
  config::ConfigHandler::GetConfig(&mozc_config);
  config::ChewingConfig config;
  config.CopyFrom(mozc_config.chewing_config());

  if (g_strcmp0(name, kKeyboardTypeName) == 0) {
    const gchar *string_value = NULL;
    if (!ibus::ConfigUtil::GetString(value, &string_value) ||
        string_value == NULL) {
      LOG(ERROR) << "Type mismatch: keyboard type is expected to be a string";
      return;
    }
    map<string, ChewingConfig::KeyboardType>::const_iterator it =
        name_to_keyboard_type_.find(string_value);
    if (it == name_to_keyboard_type_.end()) {
      LOG(ERROR) << "Cannot find a valid keyboard type for " << string_value;
      return;
    }
    config.set_keyboard_type(it->second);
  } else if (g_strcmp0(name, kSelectionKeysName) == 0) {
    const gchar *string_value = NULL;
    if (!ibus::ConfigUtil::GetString(value, &string_value) ||
        string_value == NULL) {
      LOG(ERROR) << "Type mismatch: selection keys is expected to be a string";
      return;
    }
    map<string, ChewingConfig::SelectionKeys>::const_iterator it =
        name_to_selection_keys_.find(string_value);
    if (it == name_to_selection_keys_.end()) {
      LOG(ERROR) << "Cannot find a valid selection keys for " << string_value;
      return;
    }
    config.set_selection_keys(it->second);
  } else if (g_strcmp0(name, kHsuSelectionKeysTypeName) == 0) {
    gint int_value = 0;
    if (!ibus::ConfigUtil::GetInteger(value, &int_value)) {
      LOG(ERROR) << "Type mismatch: hsu keys is expected to be an int";
      return;
    }
    map<int, ChewingConfig::HsuSelectionKeys>::const_iterator it =
        name_to_hsu_keys_.find(int_value);
    if (it == name_to_hsu_keys_.end()) {
      LOG(ERROR) << "Cannot find a valid hsu keys for " << int_value;
      return;
    }
    config.set_hsu_selection_keys(it->second);
  } else {
    ibus::ConfigUtil::SetFieldForName(name_to_field_[name], value, &config);
  }

  Session::UpdateConfig(config);
}

const map<string, const char*>& ConfigUpdater::name_to_field() {
  return name_to_field_;
}

void ConfigUpdater::InitConfig(IBusConfig *config) {
  // Initialize the mozc config with the config loaded from ibus-memconf, which
  // is the primary config storage on Chrome OS.
  ibus::ConfigUtil::InitConfig(
      config,
      kChewingSectionName,
      Singleton<ConfigUpdater>::get()->name_to_field());
}

#endif  // OS_CHROMEOS

}  // namespace chewing
}  // namespace mozc
