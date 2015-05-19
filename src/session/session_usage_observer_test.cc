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

#include "session/session_usage_observer.h"

#include <fstream>
#include <set>
#include <string>

#include "base/base.h"
#include "base/clock_mock.h"
#include "base/logging.h"
#include "base/protobuf/protobuf.h"
#include "base/protobuf/text_format.h"
#include "base/protobuf/zero_copy_stream_impl.h"
#include "base/scheduler_stub.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "config/stats_config_util.h"
#include "session/commands.pb.h"
#include "session/internal/keymap.h"
#include "session/internal/keymap_factory.h"
#include "storage/registry.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.pb.h"

DECLARE_string(test_tmpdir);
DECLARE_string(test_srcdir);

namespace mozc {
namespace session {

namespace {
void ConfigSyncTest(const string &stats_key, const string &config_key) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  string reg_str;

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats." + stats_key, &reg_str));
  usage_stats::Stats stats;
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ(stats_key, stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_FALSE(stats.boolean_value());

  {
    mozc::config::SyncConfig* sync_config = config.mutable_sync_config();
    const mozc::protobuf::Descriptor* descriptor = sync_config->GetDescriptor();
    const mozc::protobuf::FieldDescriptor* field =
        descriptor->FindFieldByName(config_key);
    ASSERT_TRUE(field != NULL);
    ASSERT_TRUE(field->type() == mozc::protobuf::FieldDescriptor::TYPE_BOOL);
    const mozc::protobuf::Reflection* reflection = sync_config->GetReflection();
    ASSERT_TRUE(reflection != NULL);
    reflection->SetBool(sync_config, field, false);
    EXPECT_FALSE(reflection->GetBool(*sync_config, field));
    config::ConfigHandler::SetConfig(config);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats." + stats_key, &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ(stats_key, stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_FALSE(stats.boolean_value());

  {
    mozc::config::SyncConfig* sync_config = config.mutable_sync_config();
    const mozc::protobuf::Descriptor* descriptor = sync_config->GetDescriptor();
    const mozc::protobuf::FieldDescriptor* field =
        descriptor->FindFieldByName(config_key);
    ASSERT_TRUE(field != NULL);
    ASSERT_TRUE(field->type() == mozc::protobuf::FieldDescriptor::TYPE_BOOL);
    const mozc::protobuf::Reflection* reflection = sync_config->GetReflection();
    ASSERT_TRUE(reflection != NULL);
    reflection->SetBool(sync_config, field, true);
    EXPECT_TRUE(reflection->GetBool(*sync_config, field));
    config::ConfigHandler::SetConfig(config);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats." + stats_key, &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ(stats_key, stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_TRUE(stats.boolean_value());
}
}  // namespace

class StatsConfigStub : public config::StatsConfigUtilInterface {
 public:
  StatsConfigStub() {
    val_ = true;
  }

  virtual ~StatsConfigStub() {}

  virtual bool IsEnabled() const {
    return val_;
  }

  virtual bool SetEnabled(bool val) {
    val_ = val;
    return true;
  }

 private:
  bool val_;
};

class SessionUsageObserverTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    EXPECT_TRUE(storage::Registry::Clear());

    Util::SetClockHandler(NULL);

    scheduler_stub_.reset(new SchedulerStub);
    Scheduler::SetSchedulerHandler(scheduler_stub_.get());

    stats_config_stub_.reset(new StatsConfigStub);
    config::StatsConfigUtil::SetHandler(stats_config_stub_.get());
  }

  virtual void TearDown() {
    Util::SetClockHandler(NULL);
    Scheduler::SetSchedulerHandler(NULL);
    config::StatsConfigUtil::SetHandler(NULL);

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

  void EnsureSave() const {
    // Make sure to save stats.
    const uint32 kWaitngUsecForEnsureSave = 10 * 60 * 1000;
    scheduler_stub_->PutClockForward(kWaitngUsecForEnsureSave);
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

  void EnsureSaveAndExpectStatsCount(const string &name, uint64 val) const {
    EnsureSave();
    ExpectStatsCount(name, val);
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

  void EnsureSaveAndExpectStatsTiming(
      const string &name, uint64 num_val, uint64 avg_val,
      uint64 min_val, uint64 max_val) const {
    EnsureSave();
    ExpectStatsTiming(name, num_val, avg_val, min_val, max_val);
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

  scoped_ptr<SchedulerStub> scheduler_stub_;
  scoped_ptr<StatsConfigStub> stats_config_stub_;
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

TEST_F(SessionUsageObserverTest, DoNotSaveWhenDeleted) {
  stats_config_stub_->SetEnabled(false);

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
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.SessionAllEvent",
                                        &reg_str));
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

  EnsureSave();

  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.SessionAllEvent",
                                        &reg_str));
  usage_stats::Stats stats;
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("SessionAllEvent", stats.name());
  EXPECT_EQ(usage_stats::Stats::COUNT, stats.type());
  EXPECT_EQ(250, stats.count());
}

TEST_F(SessionUsageObserverTest, DoNotSavePeriodically) {
  stats_config_stub_->SetEnabled(false);

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

  EnsureSave();

  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.SessionAllEvent",
                                         &reg_str));
}

TEST_F(SessionUsageObserverTest, SaveSpecialKeys) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
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

  EnsureSave();
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
      EnsureSaveAndExpectStatsCount("Performed_Direct_" + *iter, 1);
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
      command.mutable_output()->
          set_performed_command("Precomposition_" + *iter);
      observer->EvalCommandHandler(command);
      EnsureSaveAndExpectStatsCount("Performed_Precomposition_" + *iter, 1);
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
      EnsureSaveAndExpectStatsCount("Performed_Composition_" + *iter, 1);
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
      EnsureSaveAndExpectStatsCount("Performed_Conversion_" + *iter, 1);
    }
  }
}

TEST_F(SessionUsageObserverTest, ConfigTest) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
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
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseConfigSync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseUserDictionarySync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseLearningPreferenceSync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseContactListSync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigAllowCloudHandwriting", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseLocalUsageDictionary", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseWebUsageDictionary", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.WebServiceEntrySize", &reg_str));

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
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseConfigSync", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseUserDictionarySync", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySync", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseLearningPreferenceSync", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseContactListSync", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigAllowCloudHandwriting", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseLocalUsageDictionary", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.ConfigUseWebUsageDictionary", &reg_str));
  EXPECT_FALSE(storage::Registry::Lookup(
      "usage_stats.WebServiceEntrySize", &reg_str));

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
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseConfigSync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseUserDictionarySync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseHistorySync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseLearningPreferenceSync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseContactListSync", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigAllowCloudHandwriting", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseLocalUsageDictionary", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseWebUsageDictionary", &reg_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.WebServiceEntrySize", &reg_str));
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

TEST_F(SessionUsageObserverTest, ConfigSyncTests) {
  ConfigSyncTest("ConfigUseCloudSync", "use_config_sync");
  ConfigSyncTest("ConfigUseCloudSync", "use_user_dictionary_sync");
  ConfigSyncTest("ConfigUseCloudSync", "use_user_history_sync");
  ConfigSyncTest("ConfigUseCloudSync", "use_learning_preference_sync");
  ConfigSyncTest("ConfigUseCloudSync", "use_contact_list_sync");

  ConfigSyncTest("ConfigUseConfigSync", "use_config_sync");
  ConfigSyncTest("ConfigUseUserDictionarySync", "use_user_dictionary_sync");
  ConfigSyncTest("ConfigUseHistorySync", "use_user_history_sync");
  ConfigSyncTest("ConfigUseLearningPreferenceSync",
                 "use_learning_preference_sync");
  ConfigSyncTest("ConfigUseContactListSync", "use_contact_list_sync");
}

TEST_F(SessionUsageObserverTest, ClientSideStatsInfolist) {
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  string reg_str;

  // create session
  {
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
    command.mutable_output()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  const uint64 kSeconds = 0;
  const uint32 kMicroSeconds = 0;
  ClockMock clock(kSeconds, kMicroSeconds);
  Util::SetClockHandler(&clock);

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

  // Wait a second.
  clock.PutClockForward(1, 0);

  // Add command (INFOLIST_WINDOW_HIDE)
  command.mutable_input()->mutable_command()->set_usage_stats_event(
      commands::SessionCommand::INFOLIST_WINDOW_HIDE);
  observer->EvalCommandHandler(command);

  EnsureSaveAndExpectStatsTiming("InfolistWindowDuration", 1, 1, 1, 1);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }

  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 1);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }

  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 6);
  EnsureSaveAndExpectStatsCount("ConsumedSendKey", 5);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 1);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 1);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 6, 6, 6);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 6, 6, 6);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 6);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);


  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 1);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 3, 3, 3);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 3, 3, 3);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 3);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  // It is prediction because user types 'tab' and expand them.
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 1);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 1);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 4, 4, 4);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 4, 4, 4);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 4);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 1);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 1);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 4, 4, 4);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 4, 4, 4);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 4);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 1);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 3, 3, 3);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 3, 3, 3);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 3);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 1);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 1);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 1);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 1);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 4, 4, 4);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 4, 4, 4);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 4);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 1);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 1);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 1, 1, 1);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 1);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 2);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 2, 5, 2, 8);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 2, 2, 2);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 10, 10, 10);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 10);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 1);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 2, 3, 2, 4);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 1, 2, 2, 2);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 1, 6, 6, 6);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 6);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 2);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 1);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 3, 2, 2, 4);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 2, 1, 1, 2);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 2, 4, 2, 6);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 8);
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

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 2);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 2);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 2);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 2);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 4, 3, 2, 4);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 2, 2, 2, 2);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 2, 6, 6, 6);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 12);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 9);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 3);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 3);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 2);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 2);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 3);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 1);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 0);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 9, 3, 3, 3);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 9, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 9, 3, 3, 3);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 27);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 2);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 1);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 0);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 1);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 2, 2, 1, 3);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 2, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 2, 2, 1, 3);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 4);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 2);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 0);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 1);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 1);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 1);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 2);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 2, 3, 3, 4);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 2, 1, 1, 1);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 2, 3, 3, 4);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 7);
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

  for (size_t i =0; i < command_list.commands_size(); ++i) {
    observer->EvalCommandHandler(command_list.commands(i));
  }
  int consumed_sendkey = 0;
  int unconsumed_sendkey = 0;
  CountSendKeyStats(command_list, &consumed_sendkey, &unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("ConsumedSendKey", consumed_sendkey);
  EnsureSaveAndExpectStatsCount("UnconsumedSendKey", unconsumed_sendkey);

  EnsureSaveAndExpectStatsCount("Commit", 3);
  EnsureSaveAndExpectStatsCount("CommitFromConversion", 2);
  EnsureSaveAndExpectStatsCount("CommitFromSuggestion", 1);
  EnsureSaveAndExpectStatsCount("CommitFromPrediction", 0);
  EnsureSaveAndExpectStatsCount("CommitFromComposition", 0);

  EnsureSaveAndExpectStatsCount("ConversionCandidates0", 3);
  EnsureSaveAndExpectStatsCount("ConversionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates2", 1);
  EnsureSaveAndExpectStatsCount("ConversionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("ConversionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("TransliterationCandidates0", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates1", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates2", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates3", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates4", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidates5", 0);
  EnsureSaveAndExpectStatsCount("TransliterationCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("PredictionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates1", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("PredictionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("SuggestionCandidates0", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates1", 1);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates2", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates3", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates4", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidates5", 0);
  EnsureSaveAndExpectStatsCount("SuggestionCandidatesGE10", 0);

  EnsureSaveAndExpectStatsCount("MouseSelect", 1);
  EnsureSaveAndExpectStatsCount("BackSpaceAfterCommit", 2);

  EnsureSaveAndExpectStatsTiming("SubmittedSegmentLength", 5, 4, 2, 8);
  EnsureSaveAndExpectStatsTiming("SubmittedSegmentNumber", 3, 1, 1, 2);
  EnsureSaveAndExpectStatsTiming("SubmittedLength", 3, 6, 4, 10);
  EnsureSaveAndExpectStatsCount("SubmittedTotalLength", 20);
}


TEST_F(SessionUsageObserverTest, ConfigInformationList) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  string reg_str;
  usage_stats::Stats stats;

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseLocalUsageDictionary", &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("ConfigUseLocalUsageDictionary", stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_FALSE(stats.boolean_value());

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseWebUsageDictionary", &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("ConfigUseWebUsageDictionary", stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_FALSE(stats.boolean_value());

  {
    config.mutable_information_list_config()->
        set_use_local_usage_dictionary(true);
    config::ConfigHandler::SetConfig(config);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseLocalUsageDictionary", &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("ConfigUseLocalUsageDictionary", stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_TRUE(stats.boolean_value());

  {
    config.mutable_information_list_config()->
        set_use_web_usage_dictionary(true);
    config::ConfigHandler::SetConfig(config);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigUseWebUsageDictionary", &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("ConfigUseWebUsageDictionary", stats.name());
  EXPECT_EQ(usage_stats::Stats::BOOLEAN, stats.type());
  EXPECT_TRUE(stats.boolean_value());
}

TEST_F(SessionUsageObserverTest, ConfigWebServiceEntrySize) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::ConfigHandler::SetConfig(config);
  scoped_ptr<SessionUsageObserver> observer(new SessionUsageObserver);
  string reg_str;
  usage_stats::Stats stats;

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.WebServiceEntrySize", &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("WebServiceEntrySize", stats.name());
  EXPECT_EQ(usage_stats::Stats::INTEGER, stats.type());
  EXPECT_EQ(stats.int_value(), 0);

  {
    config.mutable_information_list_config();
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.WebServiceEntrySize", &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("WebServiceEntrySize", stats.name());
  EXPECT_EQ(usage_stats::Stats::INTEGER, stats.type());
  EXPECT_EQ(stats.int_value(), 0);

  {
    config.mutable_information_list_config()->
        add_web_service_entries()->set_name("sample1");
    config::ConfigHandler::SetConfig(config);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.WebServiceEntrySize", &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("WebServiceEntrySize", stats.name());
  EXPECT_EQ(usage_stats::Stats::INTEGER, stats.type());
  EXPECT_EQ(stats.int_value(), 1);

  {
    config.mutable_information_list_config()->
        add_web_service_entries()->set_name("sample2");
    config::ConfigHandler::SetConfig(config);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->set_id(1);
    observer->EvalCommandHandler(command);
  }

  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.WebServiceEntrySize", &reg_str));
  EXPECT_TRUE(stats.ParseFromString(reg_str));
  EXPECT_EQ("WebServiceEntrySize", stats.name());
  EXPECT_EQ(usage_stats::Stats::INTEGER, stats.type());
  EXPECT_EQ(stats.int_value(), 2);
}

}  // namespace session
}  // namespace mozc
