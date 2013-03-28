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

#include <string>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

using mozc::config::Config;
using mozc::config::ConfigHandler;

DECLARE_string(test_tmpdir);

namespace mozc {
namespace usage_stats {

class UsageStatsUpdaterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);

    UsageStats::ClearAllStatsForTest();
  }

  virtual void TearDown() {
    UsageStats::ClearAllStatsForTest();

    // just in case, reset the config in test_tmpdir
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);
  }

 private:
  scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(UsageStatsUpdaterTest, UpdaterTest) {
  const char *kStatsNames[] = {
    // Config stats
    "ConfigSessionKeymap",
    "ConfigPreeditMethod",
    "ConfigCustomRomanTable",
    "ConfigPunctuationMethod",
    "ConfigSymbolMethod",
    "ConfigHistoryLearningLevel",
    "ConfigUseDateConversion",
    "ConfigUseSingleKanjiConversion",
    "ConfigUseSymbolConversion",
    "ConfigUseNumberConversion",
    "ConfigUseEmoticonConversion",
    "ConfigUseCalculator",
    "ConfigUseT13nConversion",
    "ConfigUseZipCodeConversion",
    "ConfigUseSpellingCorrection",
    "ConfigUseEmojiConversion",
    "ConfigIncognito",
    "ConfigSelectionShortcut",
    "ConfigUseHistorySuggest",
    "ConfigUseDictionarySuggest",
    "ConfigUseRealtimeConversion",
    "ConfigSuggestionsSize",
    "ConfigUseAutoIMETurnOff",
    "ConfigUseCascadingWindow",
    "ConfigShiftKeyModeSwitch",
    "ConfigSpaceCharacterForm",
    "ConfigNumpadCharacterForm",
    "ConfigUseAutoConversion",
    "ConfigAutoConversionKey",
    "ConfigYenSignCharacter",
    "ConfigUseJapaneseLayout",
    "IMEActivationKeyCustomized",
    "ConfigUseConfigSync",
    "ConfigUseUserDictionarySync",
    "ConfigUseHistorySync",
    "ConfigUseLearningPreferenceSync",
    "ConfigUseContactListSync",
    "ConfigUseCloudSync",
    "ConfigAllowCloudHandwriting",
    "ConfigUseLocalUsageDictionary",
    "ConfigUseWebUsageDictionary",
    "WebServiceEntrySize",

    // Other stats
    "TotalPhysicalMemory",
#ifdef OS_WIN
    "WindowsX64",
    "CuasEnabled",
    "MsctfVerMajor",
    "MsctfVerMinor",
    "MsctfVerBuild",
    "MsctfVerRevision",
#endif  // OS_WIN
#ifdef OS_MACOSX
    "PrelauncherEnabled",
#endif  // OS_MACOSX
#ifdef OS_ANDROID
    "AndroidApiLevel",
#endif  // OS_ANDROID
  };

  for (size_t i = 0; i < arraysize(kStatsNames); ++i) {
    EXPECT_STATS_NOT_EXIST(kStatsNames[i]);
  }

  UsageStatsUpdater::UpdateStats();
  for (size_t i = 0; i < arraysize(kStatsNames); ++i) {
    EXPECT_STATS_EXIST(kStatsNames[i]);
  }

  // These stats should NOT cleared by ClearStats().
  UsageStats::ClearStats();
  for (size_t i = 0; i < arraysize(kStatsNames); ++i) {
    EXPECT_STATS_EXIST(kStatsNames[i]);
  }

  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  const bool current_mode = config.incognito_mode();
  EXPECT_BOOLEAN_STATS("ConfigIncognito", current_mode);

  config.set_incognito_mode(!current_mode);
  ConfigHandler::SetConfig(config);
  UsageStatsUpdater::UpdateStats();
  EXPECT_BOOLEAN_STATS("ConfigIncognito", !current_mode);
}

TEST_F(UsageStatsUpdaterTest, IMEActivationKeyCustomizedTest) {
  {  // Default keymap.
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);
    UsageStatsUpdater::UpdateStats();
    EXPECT_BOOLEAN_STATS("IMEActivationKeyCustomized", false);
  }

  {  // Customized keymap. (Activation key is customized)
    const char *kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "DirectInput\tCtrl j\tIMEOn\n"
        "DirectInput\tHenkan\tIMEOn\n"
        "DirectInput\tCtrl k\tIMEOff\n"
        "Precomposition\tCtrl l\tIMEOn\n";

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.set_session_keymap(Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);
    ConfigHandler::SetConfig(config);
    UsageStatsUpdater::UpdateStats();
    EXPECT_BOOLEAN_STATS("IMEActivationKeyCustomized", true);
  }

  {  // Customized keymap. (Activation key is not customized)
    const char *kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "DirectInput\tON\tIMEOn\n"
        "DirectInput\tHankaku/Zenkaku\tIMEOn\n"
        "Precomposition\tOFF\tIMEOff\n"
        "Precomposition\tHankaku/Zenkaku\tIMEOff\n";

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.set_session_keymap(config::Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);
    ConfigHandler::SetConfig(config);
    UsageStatsUpdater::UpdateStats();
    EXPECT_BOOLEAN_STATS("IMEActivationKeyCustomized", false);
  }
}

}  // namespace usage_stats
}  // namespace mozc
