// Copyright 2010-2013, Google Inc.
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

#include "languages/pinyin/unix/ibus/config_updater.h"

#include <cctype>
#include <map>
#include <string>

#include "base/singleton.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "languages/pinyin/session.h"
#include "unix/ibus/config_util.h"

// TODO(hsumita): Add test code.

namespace mozc {
namespace pinyin {
#ifdef OS_CHROMEOS
namespace {
const char kPinyinSectionName[] = "engine/Pinyin";
}  // namespace

ConfigUpdater::ConfigUpdater() {
  // bool values
  name_to_field_["CorrectPinyin"] = "correct_pinyin";
  name_to_field_["FuzzyPinyin"] = "fuzzy_pinyin";
  name_to_field_["ShiftSelectCandidate"] = "select_with_shift";
  name_to_field_["MinusEqualPage"] = "paging_with_minus_equal";
  name_to_field_["CommaPeriodPage"] = "paging_with_comma_period";
  name_to_field_["AutoCommit"] = "auto_commit";
  name_to_field_["DoublePinyin"] = "double_pinyin";
  name_to_field_["InitChinese"] = "initial_mode_chinese";
  name_to_field_["InitFull"] = "initial_mode_full_width_word";
  name_to_field_["InitFullPunct"] = "initial_mode_full_width_punctuation";
  name_to_field_["InitSimplifiedChinese"] = "initial_mode_simplified_chinese";

  // int values
  name_to_field_["DoublePinyinSchema"]  = "double_pinyin_schema";
}

// static
void ConfigUpdater::ConfigValueChanged(IBusConfig *config,
                                       const gchar *section,
                                       const gchar *name,
                                       GVariant *value,
                                       gpointer user_data) {
  Singleton<ConfigUpdater>::get()->UpdateConfig(section, name, value);
}

void ConfigUpdater::UpdateConfig(const gchar *section,
                                 const gchar *name,
                                 GVariant *value) {
  if (!section || !name || !value) {
    return;
  }

  if (g_strcmp0(section, kPinyinSectionName) != 0) {
    return;
  }

  config::PinyinConfig pinyin_config = GET_CONFIG(pinyin_config);

  if (!ibus::ConfigUtil::SetFieldForName(
          name_to_field_[name], value, &pinyin_config)) {
    return;
  }

  Session::UpdateConfig(pinyin_config);
}

const map<string, const char*>& ConfigUpdater::name_to_field() {
  return name_to_field_;
}

// static
void ConfigUpdater::InitConfig(IBusConfig *config) {
  // Initialize the mozc config with the config loaded from ibus-memconf, which
  // is the primary config storage on Chrome OS.
  ibus::ConfigUtil::InitConfig(
      config, kPinyinSectionName,
      Singleton<ConfigUpdater>::get()->name_to_field());
}

#endif  // OS_CHROMEOS

}  // namespace pinyin
}  // namespace mozc
