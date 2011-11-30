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

#include <fstream>

#include "base/util.h"
#include "base/protobuf/protobuf.h"
#include "base/protobuf/text_format.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/internal/keymap.h"
#include "session/internal/keymap_factory.h"
#include "session/session_usage_observer.h"
#include "storage/registry.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"
#include "usage_stats/usage_stats.pb.h"

DECLARE_string(test_tmpdir);
DECLARE_string(test_srcdir);

namespace mozc {
namespace session {

class SessionUsageObserverTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    EXPECT_TRUE(storage::Registry::Clear());
  }

  virtual void TearDown() {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    EXPECT_TRUE(storage::Registry::Clear());
  }

  void ReadCommandListFromFile(const string &name,
                               commands::CommandList *command_list) const {
    const string filename = Util::JoinPath(
        FLAGS_test_srcdir,
        string("data/test/session/") + name);
    EXPECT_TRUE(Util::FileExists(filename)) << "Could not read: " << filename;
    fstream finput(filename.c_str(), ios::in);
    scoped_ptr<protobuf::io::IstreamInputStream>
        input(new protobuf::io::IstreamInputStream(&finput));
    EXPECT_TRUE(protobuf::TextFormat::Parse(input.get(), command_list));
  }

  void ExpectStatsCount(const string &name, uint64 val) const {
    string reg_str;
    if (val == 0) {
      EXPECT_FALSE(storage::Registry::Lookup(
          string("usage_stats.") + name, &reg_str)) << name;
      return;
    }
    EXPECT_TRUE(storage::Registry::Lookup(
        string("usage_stats.") + name, &reg_str)) << name;
    if (reg_str.empty()) {
      LOG(INFO) << "stats " << name << ": not found";
      return;
    }
    usage_stats::Stats stats;
    EXPECT_TRUE(stats.ParseFromString(reg_str)) << name;
    EXPECT_EQ(usage_stats::Stats::COUNT, stats.type()) << name;
    EXPECT_EQ(name, stats.name()) << name;
    EXPECT_EQ(val, stats.count()) << name;
  }

  void ExpectStatsTiming(const string &name, uint64 num_val, uint64 avg_val,
                         uint64 min_val, uint64 max_val) const {
    string reg_str;
    if (num_val == 0) {
      EXPECT_FALSE(storage::Registry::Lookup(
          string("usage_stats.") + name, &reg_str)) << name;
      return;
    }
    EXPECT_TRUE(storage::Registry::Lookup(
        string("usage_stats.") + name, &reg_str)) << name;
    if (reg_str.empty()) {
      LOG(ERROR) << "stats " << name << ": not found";
      return;
    }
    usage_stats::Stats stats;
    EXPECT_TRUE(stats.ParseFromString(reg_str)) << name;
    EXPECT_EQ(usage_stats::Stats::TIMING, stats.type()) << name;
    EXPECT_EQ(name, stats.name()) << name;
    EXPECT_EQ(num_val, stats.num_timings()) << name;
    EXPECT_EQ(avg_val, stats.avg_time()) << name;
    EXPECT_EQ(min_val, stats.min_time()) << name;
    EXPECT_EQ(max_val, stats.max_time()) << name;
  }

  void CountSendKeyStats(const commands::CommandList &command_list,
                         int *consumed_sendkey, int *unconsumed_sendkey) const {
    DCHECK(consumed_sendkey);
    DCHECK(unconsumed_sendkey);
    *consumed_sendkey = 0;
    *unconsumed_sendkey = 0;
    for (int i = 0; i < command_list.commands_size(); ++i) {
      const commands::Command command = command_list.commands(i);
      if (!command.has_input() || !command.has_output()) {
        continue;
      }
      const commands::Input &input = command.input();
      const commands::Output &output = command.output();
      if (input.type() != commands::Input::SEND_KEY) {
        continue;
      }
      if (output.has_consumed() && output.consumed()) {
        ++(*consumed_sendkey);
      } else {
        ++(*unconsumed_sendkey);
      }
    }
  }
};

TEST_F(SessionUsageObserverTest, SaveWhenDeleted) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  string reg_str;

  // Add command
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::NONE);
  command.mutable_input()->set_id(0);
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

TEST_F(SessionUsageObserverTest, SavePeriodically) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  string reg_str;

  // Add command
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::NONE);
  command.mutable_input()->set_id(0);
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

TEST_F(SessionUsageObserverTest, SetInterval) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
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

TEST_F(SessionUsageObserverTest, SaveSpecialKeys) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
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

TEST_F(SessionUsageObserverTest, AllSpecialKeysTest) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);
  string reg_str;

  // create session
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
    command.mutable_output()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  for (int i = 0; i < commands::KeyEvent::NUM_SPECIALKEYS; ++i) {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SEND_KEY);
    command.mutable_input()->set_id(1);
    command.mutable_output()->set_consumed(true);
    command.mutable_input()->mutable_key()->set_special_key(
        static_cast<commands::KeyEvent::SpecialKey>(i));
    EXPECT_EQ(commands::Input::SEND_KEY, command.input().type());
    EXPECT_TRUE(command.output().has_consumed());
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(command.input().has_id());
    EXPECT_TRUE(command.input().has_key());
    EXPECT_TRUE(command.input().key().has_special_key());
    observer->EvalCommandHandler(command);
  }
}

TEST_F(SessionUsageObserverTest, PerformedCommandTest) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);
  string reg_str;
  config::Config::SessionKeymap keymap = config::Config::MSIME;
  keymap::KeyMapManager *keymap_manager =
      keymap::KeyMapFactory::GetKeyMapManager(keymap);

  {
    set<string> command_names;
    keymap_manager->GetAvailableCommandNameDirect(&command_names);
    for (set<string>::const_iterator iter = command_names.begin();
         iter != command_names.end(); ++iter) {
      commands::Command command;
      command.mutable_input()->set_type(commands::Input::SEND_KEY);
      command.mutable_output()->set_id(1);
      command.mutable_output()->set_performed_command("Direct_" + *iter);
      observer->EvalCommandHandler(command);
      ExpectStatsCount("Performed_Direct_" + *iter, 1);
    }
  }
  {
    set<string> command_names;
    keymap_manager->GetAvailableCommandNamePrecomposition(&command_names);
    for (set<string>::const_iterator iter = command_names.begin();
         iter != command_names.end(); ++iter) {
      commands::Command command;
      command.mutable_input()->set_type(commands::Input::SEND_KEY);
      command.mutable_output()->set_id(1);
      command.mutable_output()->set_performed_command("Precomposition_" + *iter);
      observer->EvalCommandHandler(command);
      ExpectStatsCount("Performed_Precomposition_" + *iter, 1);
    }
  }
  {
    set<string> command_names;
    keymap_manager->GetAvailableCommandNameComposition(&command_names);
    for (set<string>::const_iterator iter = command_names.begin();
         iter != command_names.end(); ++iter) {
      commands::Command command;
      command.mutable_input()->set_type(commands::Input::SEND_KEY);
      command.mutable_output()->set_id(1);
      command.mutable_output()->set_performed_command("Composition_" + *iter);
      observer->EvalCommandHandler(command);
      ExpectStatsCount("Performed_Composition_" + *iter, 1);
    }
  }
  {
    set<string> command_names;
    keymap_manager->GetAvailableCommandNameConversion(&command_names);
    for (set<string>::const_iterator iter = command_names.begin();
         iter != command_names.end(); ++iter) {
      commands::Command command;
      command.mutable_input()->set_type(commands::Input::SEND_KEY);
      command.mutable_output()->set_id(1);
      command.mutable_output()->set_performed_command("Conversion_" + *iter);
      observer->EvalCommandHandler(command);
      ExpectStatsCount("Performed_Conversion_" + *iter, 1);
    }
  }
}

TEST_F(SessionUsageObserverTest, ConfigTest) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);
  string reg_str;

  // config stats are set when the observer instance is created.
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSessionKeymap", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigPreeditMethod", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigCustomRomanTable", &reg_str));
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
      "usage_stats.ConfigUseEmoticonConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseCalculator", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseT13nConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseZipCodeConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSpellingCorrection", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigIncognito", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSelectionShortcut", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySuggest", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDictionarySuggest", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseRealtimeConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSuggestionsSize", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoIMETurnOff", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseCascadingWindow", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigShiftKeyModeSwitch", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigAutoConversionKey", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigYenSignCharacter", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseJapaneseLayout", &reg_str));
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
      "usage_stats.ConfigCustomRomanTable", &reg_str));
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
      "usage_stats.ConfigUseEmoticonConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseCalculator", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseT13nConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseZipCodeConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSpellingCorrection", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigIncognito", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigSelectionShortcut", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySuggest", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDictionarySuggest", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseRealtimeConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigSuggestionsSize", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoIMETurnOff", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseCascadingWindow", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigShiftKeyModeSwitch", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoConversion", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigAutoConversionKey", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigYenSignCharacter", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseJapaneseLayout", &reg_str));
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
      "usage_stats.ConfigCustomRomanTable", &reg_str));
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
      "usage_stats.ConfigUseEmoticonConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseCalculator", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseT13nConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseZipCodeConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseSpellingCorrection", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigIncognito", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSelectionShortcut", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySuggest", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseDictionarySuggest", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseRealtimeConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSuggestionsSize", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoIMETurnOff", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseCascadingWindow", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigShiftKeyModeSwitch", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseAutoConversion", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigAutoConversionKey", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigYenSignCharacter", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseJapaneseLayout", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigSpaceCharacterForm", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.IMEActivationKeyCustomized", &reg_str));
}

TEST_F(SessionUsageObserverTest, IMEActivationKeyCustomizedTest) {
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

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
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

TEST_F(SessionUsageObserverTest, IMEActivationKeyDefaultTest) {
  // default
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
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

TEST_F(SessionUsageObserverTest, IMEActivationKeyNoCustomTest) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  const string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tON\tIMEOn\n"
      "DirectInput\tHankaku/Zenkaku\tIMEOn\n"
      "Precomposition\tOFF\tIMEOff\n"
      "Precomposition\tHankaku/Zenkaku\tIMEOff\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);

  config::ConfigHandler::SetConfig(config);
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
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


TEST_F(SessionUsageObserverTest, ClientSideStatsInfolist) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);
  string reg_str;

  // create session
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
    command.mutable_output()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  // Add command (INFOLIST_WINDOW_SHOW)
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command.mutable_input()->set_id(1);
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::USAGE_STATS_EVENT);
  command.mutable_input()->mutable_command()->set_usage_stats_event(
      commands::SessionCommand::INFOLIST_WINDOW_SHOW);
  command.mutable_output()->set_consumed(false);
  EXPECT_TRUE(command.output().has_consumed());
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(command.input().has_id());
  observer->EvalCommandHandler(command);
  EXPECT_FALSE(storage::Registry::Lookup(
      string("usage_stats.InfolistWindowDuration"), &reg_str));
  // Sleep 1 sec
  mozc::Util::Sleep(1000.0);
  // Add command (INFOLIST_WINDOW_HIDE)
  command.mutable_input()->mutable_command()->set_usage_stats_event(
      commands::SessionCommand::INFOLIST_WINDOW_HIDE);
  observer->EvalCommandHandler(command);

  ExpectStatsTiming("InfolistWindowDuration", 1, 1, 1, 1);
}

TEST_F(SessionUsageObserverTest, ConvertOneSegment) {
  // HANKAKU
  // a
  // SPACE
  // SPACE
  // ENTER
  // HANKAKU

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase1.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }

  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 1);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 1);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 1, 1, 1);
  ExpectStatsCount("SubmittedTotalLength", 1);
}

TEST_F(SessionUsageObserverTest, Prediction) {
  // HANKAKU
  // a
  // TAB
  // ENTER (submit "アイスランド")
  // HANKAKU

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase2.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }

  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("SubmittedTotalLength", 6);
  ExpectStatsCount("ConsumedSendKey", 5);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 0);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 1);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 1);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 6, 6, 6);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 6, 6, 6);
  ExpectStatsCount("SubmittedTotalLength", 6);
}

TEST_F(SessionUsageObserverTest, Suggestion) {
  // HANKAKU
  // mozuku
  // SHIFT + ENTER (submit "モズク")
  // HANKAKU

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase3.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);


  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 0);
  ExpectStatsCount("CommitFromSuggestion", 1);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 1);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 3, 3, 3);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 3, 3, 3);
  ExpectStatsCount("SubmittedTotalLength", 3);
}

TEST_F(SessionUsageObserverTest, SelectPrediction) {
  // HANKAKU
  // mozuku
  // TAB
  // TAB
  // ENTER (submit "もずく酢")
  // HANKAKU

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase4.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 0);
  ExpectStatsCount("CommitFromSuggestion", 0);
  // It is prediction because user types 'tab' and expand them.
  ExpectStatsCount("CommitFromPrediction", 1);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 1);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 4, 4, 4);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 4, 4, 4);
  ExpectStatsCount("SubmittedTotalLength", 4);
}

TEST_F(SessionUsageObserverTest, MouseSelectFromSuggestion) {
  // HANKAKU
  // mozuku
  // Select 2nd by mouse
  // ENTER (submit "もずく酢")
  // HANKAKU

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase5.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 0);
  ExpectStatsCount("CommitFromSuggestion", 1);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 1);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 1);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 4, 4, 4);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 4, 4, 4);
  ExpectStatsCount("SubmittedTotalLength", 4);
}

TEST_F(SessionUsageObserverTest, Composition) {
  // HANKAKU
  // mozuku
  // ENTER (submit "もずく")
  // HANKAKU

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase6.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 0);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 1);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 3, 3, 3);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 3, 3, 3);
  ExpectStatsCount("SubmittedTotalLength", 3);
}

TEST_F(SessionUsageObserverTest, SelectConversion) {
  // HANKAKU
  // a
  // SPACE, SPACE, ...
  // ENTER (submit "我")
  // HANKAKU

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase7.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 1);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 1);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 1, 1, 1);
  ExpectStatsCount("SubmittedTotalLength", 1);
}

TEST_F(SessionUsageObserverTest, SelectMinorPrediction) {
  // KANJI
  // a
  // TAB
  // TAB, SPACE, ...
  // ENTER (submit "アイドル")
  // KANJI

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase8.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 0);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 1);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 1);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 4, 4, 4);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 4, 4, 4);
  ExpectStatsCount("SubmittedTotalLength", 4);
}

TEST_F(SessionUsageObserverTest, SelectT13N) {
  // KANJI
  // a
  // SPACE, SPACE
  // UP x 4
  // ENTER (submit "ａ")
  // KANJI

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase9.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 1);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 1);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 1, 1, 1);
  ExpectStatsCount("SubmittedTotalLength", 1);
}

TEST_F(SessionUsageObserverTest, T13NbyKey) {
  // KANJI
  // a
  // F8
  // ENTER (submit "ｱ")
  // KANJI

  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase10.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 1);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 1);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 1, 1, 1, 1);
  ExpectStatsCount("SubmittedTotalLength", 1);
}

TEST_F(SessionUsageObserverTest, MultiSegments) {
  // KANJI
  // mataharuniaimasyou
  // SPACE
  // ENTER (submit "また|春に会いましょう")
  // KANJI
  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase11.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 1);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 2);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 2, 5, 2, 8);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 2, 2, 2);
  ExpectStatsTiming("SubmittedLength", 1, 10, 10, 10);
  ExpectStatsCount("SubmittedTotalLength", 10);
}

TEST_F(SessionUsageObserverTest, SelectCandidatesInMultiSegments) {
  // KANJI
  // nekowokaitai
  // SPACE
  // <select "猫を|飼いたい">
  // ENTER (submit "猫を|飼いたい")
  // KANJI
  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase12.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 1);
  ExpectStatsCount("CommitFromConversion", 1);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 1);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 1);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 2, 3, 2, 4);
  ExpectStatsTiming("SubmittedSegmentNumber", 1, 2, 2, 2);
  ExpectStatsTiming("SubmittedLength", 1, 6, 6, 6);
  ExpectStatsCount("SubmittedTotalLength", 6);
}

TEST_F(SessionUsageObserverTest, ContinueInput) {
  // KANJI
  // nekowokaitai
  // SPACE
  // <select "猫を|飼いたい">
  // yo- <submit "猫を|飼いたい">
  // ENTER "よー" <submit "よー">
  // KANJI
  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase13.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 2);
  ExpectStatsCount("CommitFromConversion", 1);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 1);

  ExpectStatsCount("ConversionCandidates0", 1);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 1);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 3, 2, 2, 4);
  ExpectStatsTiming("SubmittedSegmentNumber", 2, 1, 1, 2);
  ExpectStatsTiming("SubmittedLength", 2, 4, 2, 6);
  ExpectStatsCount("SubmittedTotalLength", 8);
}

TEST_F(SessionUsageObserverTest, MultiInputSession) {
  // KANJI
  // nekowokaitai
  // SPACE
  // <select "猫を|飼いたい">
  // ENTER (submit "猫を|飼いたい")
  // KANJI
  // <x2>
  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase12.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  // Count twice
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);
  consumed_sendkey *= 2;
  unconsumed_sendkey *= 2;

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 2);
  ExpectStatsCount("CommitFromConversion", 2);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 2);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 2);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 4, 3, 2, 4);
  ExpectStatsTiming("SubmittedSegmentNumber", 2, 2, 2, 2);
  ExpectStatsTiming("SubmittedLength", 2, 6, 6, 6);
  ExpectStatsCount("SubmittedTotalLength", 12);
}

TEST_F(SessionUsageObserverTest, ContinuousInput) {
  // KANJI
  // commit "もずく" from composition
  // commit "もずく" from first conversion (no window)
  // commit "モズク" from second conversion
  // commit "モズク" from suggestion
  // commit "モズク" from prediction
  // convert "もずく"  and continue next input
  // select "モズク" from prediction and continue next input
  // select "モズク" from prediction and continue next input
  // commit "もずく"
  // KANJI
  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase14.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 9);
  ExpectStatsCount("CommitFromConversion", 3);
  ExpectStatsCount("CommitFromSuggestion", 1);
  ExpectStatsCount("CommitFromPrediction", 3);
  ExpectStatsCount("CommitFromComposition", 2);

  ExpectStatsCount("ConversionCandidates0", 2);
  ExpectStatsCount("ConversionCandidates1", 1);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 3);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 1);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 0);

  ExpectStatsTiming("SubmittedSegmentLength", 9, 3, 3, 3);
  ExpectStatsTiming("SubmittedSegmentNumber", 9, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 9, 3, 3, 3);
  ExpectStatsCount("SubmittedTotalLength", 27);
}

TEST_F(SessionUsageObserverTest, BackSpaceAfterCommit) {
  // KANJI
  // commit "モズク" from second conversion
  // BACKSPACE
  // commit "あ" from conposition
  // KANJI
  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase15.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 2);
  ExpectStatsCount("CommitFromConversion", 1);
  ExpectStatsCount("CommitFromSuggestion", 0);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 1);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 1);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 0);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 0);
  ExpectStatsCount("BackSpaceAfterCommit", 1);

  ExpectStatsTiming("SubmittedSegmentLength", 2, 2, 1, 3);
  ExpectStatsTiming("SubmittedSegmentNumber", 2, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 2, 2, 1, 3);
  ExpectStatsCount("SubmittedTotalLength", 4);
}

TEST_F(SessionUsageObserverTest, MultipleBackSpaceAfterCommit) {
  //  KANJI
  //  commit "もずく" from composition
  //  BACKSPACE x 3
  //  select "もずく酢" from suggestion
  //  ENTER
  //  BACKSPACE x 4
  //  KANJI
  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase16.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 2);
  ExpectStatsCount("CommitFromConversion", 0);
  ExpectStatsCount("CommitFromSuggestion", 1);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 1);

  ExpectStatsCount("ConversionCandidates0", 0);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 0);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 1);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 1);
  ExpectStatsCount("BackSpaceAfterCommit", 2);

  ExpectStatsTiming("SubmittedSegmentLength", 2, 3, 3, 4);
  ExpectStatsTiming("SubmittedSegmentNumber", 2, 1, 1, 1);
  ExpectStatsTiming("SubmittedLength", 2, 3, 3, 4);
  ExpectStatsCount("SubmittedTotalLength", 7);
}

TEST_F(SessionUsageObserverTest, MultipleSessions) {
  // * session A
  // KANJI
  // convert and commit "また|春に会いましょう"
  // BACKSPACE x 10
  // convert and commit "猫を|飼いたい"
  // KANJI

  // * session B
  // KANJI
  // mouse select and commit "もずく酢" from suggestion
  // BACKSPACE x 4
  // KANJI

  // these sessions are mixed
  commands::CommandList command_list;
  ReadCommandListFromFile("session_usage_observer_testcase17.txt",
                          &command_list);

  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  observer->SetInterval(1);

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  ExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  ExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  ExpectStatsCount("Commit", 3);
  ExpectStatsCount("CommitFromConversion", 2);
  ExpectStatsCount("CommitFromSuggestion", 1);
  ExpectStatsCount("CommitFromPrediction", 0);
  ExpectStatsCount("CommitFromComposition", 0);

  ExpectStatsCount("ConversionCandidates0", 3);
  ExpectStatsCount("ConversionCandidates1", 0);
  ExpectStatsCount("ConversionCandidates2", 1);
  ExpectStatsCount("ConversionCandidates3", 0);
  ExpectStatsCount("ConversionCandidates4", 0);
  ExpectStatsCount("ConversionCandidates5", 0);
  ExpectStatsCount("ConversionCandidatesGE10", 0);

  ExpectStatsCount("TransliterationCandidates0", 0);
  ExpectStatsCount("TransliterationCandidates1", 0);
  ExpectStatsCount("TransliterationCandidates2", 0);
  ExpectStatsCount("TransliterationCandidates3", 0);
  ExpectStatsCount("TransliterationCandidates4", 0);
  ExpectStatsCount("TransliterationCandidates5", 0);
  ExpectStatsCount("TransliterationCandidatesGE10", 0);

  ExpectStatsCount("PredictionCandidates0", 0);
  ExpectStatsCount("PredictionCandidates1", 0);
  ExpectStatsCount("PredictionCandidates2", 0);
  ExpectStatsCount("PredictionCandidates3", 0);
  ExpectStatsCount("PredictionCandidates4", 0);
  ExpectStatsCount("PredictionCandidates5", 0);
  ExpectStatsCount("PredictionCandidatesGE10", 0);

  ExpectStatsCount("SuggestionCandidates0", 0);
  ExpectStatsCount("SuggestionCandidates1", 1);
  ExpectStatsCount("SuggestionCandidates2", 0);
  ExpectStatsCount("SuggestionCandidates3", 0);
  ExpectStatsCount("SuggestionCandidates4", 0);
  ExpectStatsCount("SuggestionCandidates5", 0);
  ExpectStatsCount("SuggestionCandidatesGE10", 0);

  ExpectStatsCount("MouseSelect", 1);
  ExpectStatsCount("BackSpaceAfterCommit", 2);

  ExpectStatsTiming("SubmittedSegmentLength", 5, 4, 2, 8);
  ExpectStatsTiming("SubmittedSegmentNumber", 3, 1, 1, 2);
  ExpectStatsTiming("SubmittedLength", 3, 6, 4, 10);
  ExpectStatsCount("SubmittedTotalLength", 20);
}


}  // namespace session
}  // namespace mozc
