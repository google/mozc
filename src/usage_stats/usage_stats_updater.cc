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

#include "usage_stats/usage_stats_updater.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <vector>

#ifdef OS_ANDROID
#include "base/android_util.h"
#endif  // OS_ANDROID
#include "base/config_file_stream.h"
#include "base/mac_util.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/win_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/internal/keymap.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace usage_stats {

namespace {
const char kIMEOnCommand[] = "IMEOn";
const char kIMEOffCommand[] = "IMEOff";
const config::Config::SessionKeymap kKeyMaps[] = {
  config::Config::ATOK,
  config::Config::MSIME,
  config::Config::KOTOERI,
};

void ExtractActivationKeys(istream *ifs, set<string> *keys) {
  DCHECK(keys);
  string line;
  getline(*ifs, line);  // first line is comment.
  while (getline(*ifs, line)) {
    Util::ChopReturns(&line);
    if (line.empty() || line[0] == '#') {
      // empty or comment
      continue;
    }
    vector<string> rules;
    Util::SplitStringUsing(line, "\t", &rules);
    if (rules.size() == 3 &&
        (rules[2] == kIMEOnCommand || rules[2] == kIMEOffCommand)) {
      keys->insert(line);
    }
  }
}

bool IMEActivationKeyCustomized() {
  const config::Config::SessionKeymap keymap = GET_CONFIG(session_keymap);
  if (keymap != config::Config::CUSTOM) {
    return false;
  }
  const string &custom_keymap_table = GET_CONFIG(custom_keymap_table);
  istringstream ifs_custom(custom_keymap_table);
  set<string> customized;
  ExtractActivationKeys(&ifs_custom, &customized);
  for (size_t i = 0; i < arraysize(kKeyMaps); ++i) {
    const char *keymap_file =
        keymap::KeyMapManager::GetKeyMapFileName(kKeyMaps[i]);
    scoped_ptr<istream> ifs(ConfigFileStream::LegacyOpen(keymap_file));
    if (ifs.get() == NULL) {
      LOG(ERROR) << "can not open default keymap table " << i;
      continue;
    }
    set<string> keymap_table;
    ExtractActivationKeys(ifs.get(), &keymap_table);
    if (includes(keymap_table.begin(), keymap_table.end(),
                 customized.begin(), customized.end())) {
      // customed keymap is subset of preset keymap
      return false;
    }
  }
  return true;
}

void UpdateConfigStats() {
  const mozc::config::Config config = mozc::config::ConfigHandler::GetConfig();

  UsageStats::SetInteger("ConfigSessionKeymap", config.session_keymap());
  const uint32 preedit_method = config.preedit_method();
  UsageStats::SetInteger("ConfigPreeditMethod", preedit_method);
  const bool custom_roman = (!config.custom_roman_table().empty() &&
                             preedit_method == config::Config::ROMAN);
  UsageStats::SetBoolean("ConfigCustomRomanTable", custom_roman);
  UsageStats::SetInteger("ConfigPunctuationMethod",
                         config.punctuation_method());
  UsageStats::SetInteger("ConfigSymbolMethod", config.symbol_method());
  UsageStats::SetInteger("ConfigHistoryLearningLevel",
                         config.history_learning_level());

  UsageStats::SetBoolean("ConfigUseDateConversion",
                         config.use_date_conversion());
  UsageStats::SetBoolean("ConfigUseSingleKanjiConversion",
                         config.use_single_kanji_conversion());
  UsageStats::SetBoolean("ConfigUseSymbolConversion",
                         config.use_symbol_conversion());
  UsageStats::SetBoolean("ConfigUseNumberConversion",
                         config.use_number_conversion());
  UsageStats::SetBoolean("ConfigUseEmoticonConversion",
                         config.use_emoticon_conversion());
  UsageStats::SetBoolean("ConfigUseCalculator", config.use_calculator());
  UsageStats::SetBoolean("ConfigUseT13nConversion",
                         config.use_t13n_conversion());
  UsageStats::SetBoolean("ConfigUseZipCodeConversion",
                         config.use_zip_code_conversion());
  UsageStats::SetBoolean("ConfigUseSpellingCorrection",
                         config.use_spelling_correction());
  UsageStats::SetBoolean("ConfigUseEmojiConversion",
                         config.use_emoji_conversion());
  UsageStats::SetBoolean("ConfigIncognito", config.incognito_mode());

  UsageStats::SetInteger("ConfigSelectionShortcut",
                         config.selection_shortcut());

  UsageStats::SetBoolean("ConfigUseHistorySuggest",
                         config.use_history_suggest());
  UsageStats::SetBoolean("ConfigUseDictionarySuggest",
                         config.use_dictionary_suggest());
  UsageStats::SetBoolean("ConfigUseRealtimeConversion",
                         config.use_realtime_conversion());

  UsageStats::SetInteger("ConfigSuggestionsSize", config.suggestions_size());

  UsageStats::SetBoolean("ConfigUseAutoIMETurnOff",
                         config.use_auto_ime_turn_off());
  UsageStats::SetBoolean("ConfigUseCascadingWindow",
                         config.use_cascading_window());

  UsageStats::SetInteger("ConfigShiftKeyModeSwitch",
                         config.shift_key_mode_switch());
  UsageStats::SetInteger("ConfigSpaceCharacterForm",
                         config.space_character_form());
  UsageStats::SetInteger("ConfigNumpadCharacterForm",
                         config.numpad_character_form());

  UsageStats::SetBoolean("ConfigUseAutoConversion",
                         config.use_auto_conversion());
  UsageStats::SetInteger("ConfigAutoConversionKey",
                         config.auto_conversion_key());

  UsageStats::SetInteger("ConfigYenSignCharacter", config.yen_sign_character());
  UsageStats::SetBoolean("ConfigUseJapaneseLayout",
                         config.use_japanese_layout());
  UsageStats::SetBoolean("IMEActivationKeyCustomized",
                         IMEActivationKeyCustomized());

  const bool has_sync_config = config.has_sync_config();
  const bool use_config_sync =
      has_sync_config && config.sync_config().use_config_sync();
  UsageStats::SetBoolean("ConfigUseConfigSync", use_config_sync);
  const bool use_user_dictionary_sync =
      has_sync_config && config.sync_config().use_user_dictionary_sync();
  UsageStats::SetBoolean("ConfigUseUserDictionarySync",
                         use_user_dictionary_sync);
  const bool use_user_history_sync =
      has_sync_config && config.sync_config().use_user_history_sync();
  UsageStats::SetBoolean("ConfigUseHistorySync", use_user_history_sync);
  const bool use_learning_preference_sync =
      has_sync_config && config.sync_config().use_learning_preference_sync();
  UsageStats::SetBoolean("ConfigUseLearningPreferenceSync",
                         use_learning_preference_sync);
  const bool use_contact_list_sync =
      has_sync_config && config.sync_config().use_contact_list_sync();
  UsageStats::SetBoolean("ConfigUseContactListSync", use_contact_list_sync);

  const bool use_cloud_sync =
      use_config_sync || use_user_dictionary_sync || use_user_history_sync ||
      use_learning_preference_sync || use_contact_list_sync;
  UsageStats::SetBoolean("ConfigUseCloudSync", use_cloud_sync);

  UsageStats::SetBoolean("ConfigAllowCloudHandwriting",
                         config.allow_cloud_handwriting());

  const bool has_information_list_config =
      config.has_information_list_config();
  const bool use_local_usage_dictionary =
      has_information_list_config &&
      config.information_list_config().use_local_usage_dictionary();
  UsageStats::SetBoolean("ConfigUseLocalUsageDictionary",
                         use_local_usage_dictionary);
  const bool use_web_usage_dictionary =
      has_information_list_config &&
      config.information_list_config().use_web_usage_dictionary();
  UsageStats::SetBoolean("ConfigUseWebUsageDictionary",
                         use_web_usage_dictionary);
  const uint32 web_service_entries_size =
      has_information_list_config ?
      config.information_list_config().web_service_entries_size() : 0;
  UsageStats::SetInteger("WebServiceEntrySize", web_service_entries_size);
}
}  // namespace

void UsageStatsUpdater::UpdateStats() {
  UpdateConfigStats();

  // Get total memory in MB.
  const uint32 memory_in_mb =
      SystemUtil::GetTotalPhysicalMemory() / (1024 * 1024);
  UsageStats::SetInteger("TotalPhysicalMemory", memory_in_mb);

#ifdef OS_WIN
  UsageStats::SetBoolean("WindowsX64", SystemUtil::IsWindowsX64());
  UsageStats::SetBoolean("CuasEnabled", WinUtil::IsCuasEnabled());
  {
    // get msctf version
    int major, minor, build, revision;
    const wchar_t kDllName[] = L"msctf.dll";
    wstring path = SystemUtil::GetSystemDir();
    path += L"\\";
    path += kDllName;
    if (SystemUtil::GetFileVersion(path, &major, &minor, &build, &revision)) {
      UsageStats::SetInteger("MsctfVerMajor", major);
      UsageStats::SetInteger("MsctfVerMinor", minor);
      UsageStats::SetInteger("MsctfVerBuild", build);
      UsageStats::SetInteger("MsctfVerRevision", revision);
    } else {
      LOG(ERROR) << "get file version for msctf.dll failed";
    }
  }
#endif  // OS_WIN

#ifdef OS_MACOSX
  UsageStats::SetBoolean("PrelauncherEnabled",
                         MacUtil::CheckPrelauncherLoginItemStatus());
#endif  // OS_MACOSX

#ifdef OS_ANDROID
  const int sdk_level = NumberUtil::SimpleAtoi(AndroidUtil::GetSystemProperty(
                            AndroidUtil::kSystemPropertySdkVersion, "0"));
  UsageStats::SetInteger("AndroidApiLevel", sdk_level);
#endif  // OS_ANDROID
}

}  // namespace usage_stats
}  // namespace mozc
