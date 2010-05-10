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

#include "base/util.h"
#include "session/commands.pb.h"
#include "session/config_handler.h"
#include "session/session_usage_observer.h"
#include "storage/registry.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"
#include "usage_stats/usage_stats.pb.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace session {

TEST(SessionUsageObserverTest, SaveWhenDeleted) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);;
  string reg_str;

  // Add command
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::NONE);
  command.mutable_output()->set_consumed(true);
  for (int i = 0; i < 5; ++i) {
    observer->EvalCommandHandler(command);
    EXPECT_FALSE(storage::Registry::Lookup("usage_stats.SessionAllEvent",
                                           &reg_str));
  }

  observer.reset();
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.SessionAllEvent",
                                        &reg_str));
  usage_stats::Stats stats;
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("SessionAllEvent", stats.name());
  EXPECT_EQ(usage_stats::Stats::COUNT, stats.type());
  EXPECT_EQ(5, stats.count());
}

TEST(SessionUsageObserverTest, SavePeriodically) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);;
  string reg_str;

  // Add command
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::NONE);
  command.mutable_output()->set_consumed(true);
  for (int i = 0; i < (500 / 2) - 1; ++i) {
    // 2 stats are saved for every command (AllEvent, ElapsedTime)
    observer->EvalCommandHandler(command);
    EXPECT_FALSE(storage::Registry::Lookup("usage_stats.SessionAllEvent",
                                           &reg_str));
  }

  observer->EvalCommandHandler(command);
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.SessionAllEvent",
                                        &reg_str));
  usage_stats::Stats stats;
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("SessionAllEvent", stats.name());
  EXPECT_EQ(usage_stats::Stats::COUNT, stats.type());
  EXPECT_EQ(250, stats.count());
}

TEST(SessionUsageObserverTest, SetInterval) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);;
  observer->SetInterval(1);
  string reg_str;

  // Add command
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::NONE);
  command.mutable_output()->set_consumed(true);
  observer->EvalCommandHandler(command);
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.SessionAllEvent",
                                        &reg_str));
}

TEST(SessionUsageObserverTest, SaveSpecialKeys) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);;
  observer->SetInterval(1);
  string reg_str;

  // create session
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
    command.mutable_output()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  // Add command
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::SEND_KEY);
  command.mutable_input()->set_id(1);
  command.mutable_output()->set_consumed(true);
  commands::KeyEvent *key = command.mutable_input()->mutable_key();
  key->set_special_key(commands::KeyEvent::F1);
  EXPECT_EQ(commands::Input::SEND_KEY, command.input().type());
  EXPECT_TRUE(command.output().has_consumed());
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(command.input().has_id());
  EXPECT_TRUE(command.input().has_key());
  EXPECT_TRUE(command.input().key().has_special_key());
  observer->EvalCommandHandler(command);

  key->set_special_key(commands::KeyEvent::EISU);
  observer->EvalCommandHandler(command);

  key->set_special_key(commands::KeyEvent::F1);
  observer->EvalCommandHandler(command);

  {
    EXPECT_TRUE(storage::Registry::Lookup("usage_stats.NonASCIITyping",
                                          &reg_str));
    usage_stats::Stats stats;
    EXPECT_TRUE(stats.ParseFromString(reg_str));
    EXPECT_EQ("NonASCIITyping", stats.name());
    EXPECT_EQ(usage_stats::Stats::COUNT, stats.type());
    EXPECT_EQ(3, stats.count());
  }

  {
    EXPECT_TRUE(storage::Registry::Lookup("usage_stats.F1",
                                          &reg_str));
    usage_stats::Stats stats;
    EXPECT_TRUE(stats.ParseFromString(reg_str));
    EXPECT_EQ("F1", stats.name());
    EXPECT_EQ(usage_stats::Stats::COUNT, stats.type());
    EXPECT_EQ(2, stats.count());
  }

  {
    EXPECT_TRUE(storage::Registry::Lookup("usage_stats.EISU",
                                          &reg_str));
    usage_stats::Stats stats;
    EXPECT_TRUE(stats.ParseFromString(reg_str));
    EXPECT_EQ("EISU", stats.name());
    EXPECT_EQ(usage_stats::Stats::COUNT, stats.type());
    EXPECT_EQ(1, stats.count());
  }

  {
    EXPECT_FALSE(storage::Registry::Lookup("usage_stats.NUMPAD0",
                                           &reg_str));
  }
}

TEST(SessionUsageObserverTest, ConfigTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);;
  observer->SetInterval(1);
  string reg_str;

  // config stats are set when the observer instance is created.
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSessionKeymap", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigPreeditMethod", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigPunctuationMethod", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSymbolMethod", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDateConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSingleKanjiConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSymbolConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseNumberConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigIncognito", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSelectionShortcut", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySuggest", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDictionarySuggest", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSuggestionsSize", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoIMETurnOff", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigShiftKeyModeSwitch", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSpaceCharacterForm", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.IMEActivationKeyCustomized", &reg_str));

  // Clear Stats
  EXPECT_TRUE(storage::Registry::Clear());

  // No stats
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigSessionKeymap", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigPreeditMethod", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigPunctuationMethod", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigSymbolMethod", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDateConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSingleKanjiConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSymbolConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseNumberConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigIncognito", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigSelectionShortcut", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySuggest", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDictionarySuggest", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigSuggestionsSize", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoIMETurnOff", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigCapitalInputBehavior", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigSpaceCharacterForm", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.IMEActivationKeyCustomized", &reg_str));

  // Set config command
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  // Reset Stats
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSessionKeymap", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigPreeditMethod", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigPunctuationMethod", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSymbolMethod", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDateConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSingleKanjiConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSymbolConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseNumberConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigIncognito", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSelectionShortcut", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySuggest", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDictionarySuggest", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSuggestionsSize", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoIMETurnOff", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigShiftKeyModeSwitch", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSpaceCharacterForm", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.IMEActivationKeyCustomized", &reg_str));
}

TEST(SessionUsageObserverTest, IMEActivationKeyCustomizedTest) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  const string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tCtrl j\tIMEOn\n"
      "DirectInput\tHenkan\tIMEOn\n"
      "DirectInput\tCtrl k\tIMEOff\n"
      "Precomposition\tCtrl l\tIMEOn\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);
  config::ConfigHandler::SetConfig(config);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);;
  observer->SetInterval(1);
  string reg_str;

  // config stats are set when the observer instance is created.
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.IMEActivationKeyCustomized", &reg_str));
  usage_stats::Stats stats;
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("IMEActivationKeyCustomized", stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_TRUE(stats.boolean_value());
}

TEST(SessionUsageObserverTest, IMEActivationKeyDefaultTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  // default
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);
  EXPECT_TRUE(storage::Registry::Clear());
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);;
  observer->SetInterval(1);
  string reg_str;

  // config stats are set when the observer instance is created.
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.IMEActivationKeyCustomized", &reg_str));
  usage_stats::Stats stats;
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("IMEActivationKeyCustomized", stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_FALSE(stats.boolean_value());
}

TEST(SessionUsageObserverTest, IMEActivationKeyNoCustomTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  const string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tON\tIMEOn\n"
      "DirectInput\tHankaku/Zenkaku\tIMEOn\n"
      "Precomposition\tOFF\tIMEOff\n"
      "Precomposition\tHankaku/Zenkaku\tIMEOff\n";;
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);

  config::ConfigHandler::SetConfig(config);
  EXPECT_TRUE(storage::Registry::Clear());
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);;
  observer->SetInterval(1);
  string reg_str;

  // config stats are set when the observer instance is created.
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.IMEActivationKeyCustomized", &reg_str));
  usage_stats::Stats stats;
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("IMEActivationKeyCustomized", stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_FALSE(stats.boolean_value());
}

}  // namespace session
}  // namespace mozc
