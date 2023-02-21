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

#include "config/config_handler.h"

#include <atomic>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/clock_mock.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/util.h"
#include "protocol/config.pb.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

namespace mozc {
namespace config {
namespace {

class ConfigHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    default_config_filename_ = ConfigHandler::GetConfigFileName();
    Config default_config;
    ConfigHandler::GetDefaultConfig(&default_config);
    ConfigHandler::SetConfig(default_config);
  }

  void TearDown() override {
    ConfigHandler::SetConfigFileName(default_config_filename_);
    Config default_config;
    ConfigHandler::GetDefaultConfig(&default_config);
    ConfigHandler::SetConfig(default_config);
  }

 private:
  std::string default_config_filename_;
};

class ScopedSetConfigFileName {
 public:
  ScopedSetConfigFileName() = delete;
  ScopedSetConfigFileName(const ScopedSetConfigFileName &) = delete;
  ScopedSetConfigFileName &operator=(const ScopedSetConfigFileName &) = delete;
  explicit ScopedSetConfigFileName(const absl::string_view new_name)
      : default_config_filename_(ConfigHandler::GetConfigFileName()) {
    ConfigHandler::SetConfigFileName(new_name);
  }

  ~ScopedSetConfigFileName() {
    ConfigHandler::SetConfigFileName(default_config_filename_);
  }

 private:
  std::string default_config_filename_;
};

TEST_F(ConfigHandlerTest, SetConfig) {
  Config input;
  Config output;

  const std::string config_file = FileUtil::JoinPath(
      absl::GetFlag(FLAGS_test_tmpdir), "mozc_config_test_tmp");
  ASSERT_OK(FileUtil::UnlinkIfExists(config_file));
  ScopedSetConfigFileName scoped_config_file_name(config_file);
  EXPECT_EQ(ConfigHandler::GetConfigFileName(), config_file);
  ConfigHandler::Reload();

  ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(true);
#ifndef MOZC_NO_LOGGING
  input.set_verbose_level(2);
#endif  // MOZC_NO_LOGGING
  ConfigHandler::SetMetaData(&input);
  ConfigHandler::SetConfig(input);
  output.Clear();
  ConfigHandler::GetConfig(&output);
  std::unique_ptr<config::Config> output2 = ConfigHandler::GetConfig();
  input.mutable_general_config()->set_last_modified_time(0);
  output.mutable_general_config()->set_last_modified_time(0);
  output2->mutable_general_config()->set_last_modified_time(0);
  EXPECT_EQ(output.DebugString(), input.DebugString());
  EXPECT_EQ(output2->DebugString(), input.DebugString());

  ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(false);
#ifndef MOZC_NO_LOGGING
  input.set_verbose_level(0);
#endif  // MOZC_NO_LOGGING
  ConfigHandler::SetMetaData(&input);
  ConfigHandler::SetConfig(input);
  output.Clear();
  ConfigHandler::GetConfig(&output);
  output2 = ConfigHandler::GetConfig();

  input.mutable_general_config()->set_last_modified_time(0);
  output.mutable_general_config()->set_last_modified_time(0);
  output2->mutable_general_config()->set_last_modified_time(0);
  EXPECT_EQ(output.DebugString(), input.DebugString());
  EXPECT_EQ(output2->DebugString(), input.DebugString());

#if defined(__ANDROID__) && defined(CHANNEL_DEV)
  input.Clear();
  EXPECT_FALSE(input.general_config().has_upload_usage_stats());
  EXPECT_TRUE(ConfigHandler::SetConfig(input));
  output.Clear();
  ConfigHandler::GetConfig(&output);
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());

  input.Clear();
  input.mutable_general_config()->set_upload_usage_stats(false);
  EXPECT_TRUE(input.general_config().has_upload_usage_stats());
  EXPECT_FALSE(input.general_config().upload_usage_stats());
  EXPECT_TRUE(ConfigHandler::SetConfig(input));
  output.Clear();
  ConfigHandler::GetConfig(&output);
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());
#endif  // __ANDROID__ && CHANNEL_DEV
}

TEST_F(ConfigHandlerTest, SetMetadata) {
  ClockMock clock1(1000, 0);
  Clock::SetClockForUnitTest(&clock1);
  Config input1;
  ConfigHandler::SetMetaData(&input1);

  ClockMock clock2(1000, 0);
  Clock::SetClockForUnitTest(&clock2);
  Config input2;
  ConfigHandler::SetMetaData(&input2);

  ClockMock clock3(1001, 0);
  Clock::SetClockForUnitTest(&clock3);
  Config input3;
  ConfigHandler::SetMetaData(&input3);

  // input1 and input2 are created at the same time,
  // but input3 is not.
  EXPECT_EQ(input1.DebugString(), input2.DebugString());
  EXPECT_NE(input2.DebugString(), input3.DebugString());
  EXPECT_NE(input3.DebugString(), input1.DebugString());
  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(ConfigHandlerTest, SetConfig_IdentityCheck) {
  Config input;

  const std::string config_file = FileUtil::JoinPath(
      absl::GetFlag(FLAGS_test_tmpdir), "mozc_config_test_tmp");
  ASSERT_OK(FileUtil::UnlinkIfExists(config_file));
  ScopedSetConfigFileName scoped_config_file_name(config_file);
  EXPECT_EQ(ConfigHandler::GetConfigFileName(), config_file);
  ConfigHandler::Reload();

  ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(true);
#ifndef MOZC_NO_LOGGING
  input.set_verbose_level(2);
#endif  // MOZC_NO_LOGGING

  ClockMock clock1(1000, 0);
  Clock::SetClockForUnitTest(&clock1);
  ConfigHandler::SetConfig(input);
  std::unique_ptr<config::Config> output1 = ConfigHandler::GetConfig();

  ClockMock clock2(1001, 0);
  Clock::SetClockForUnitTest(&clock2);
  ConfigHandler::SetConfig(input);
  std::unique_ptr<config::Config> output2 = ConfigHandler::GetConfig();

  // As SetConfig() is called twice with the same config,
  // GetConfig() must return the identical (including metadata!) config.
  // This also means no actual storage write access happened.
  EXPECT_EQ(output1->DebugString(), output2->DebugString());
  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(ConfigHandlerTest, ConfigFileNameConfig) {
  const std::string config_file =
      std::string("config") + std::to_string(config::CONFIG_VERSION) + ".db";

  const std::string filename =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), config_file);
  ASSERT_OK(FileUtil::UnlinkIfExists(filename));
  Config input;
  ConfigHandler::SetConfig(input);
  EXPECT_OK(FileUtil::FileExists(filename));
}

TEST_F(ConfigHandlerTest, SetConfigFileName) {
  Config mozc_config;
  const bool default_incognito_mode = mozc_config.incognito_mode();
  mozc_config.set_incognito_mode(!default_incognito_mode);
  ConfigHandler::SetConfig(mozc_config);
  // ScopedSetConfigFileName internally calls SetConfigFileName.
  ScopedSetConfigFileName scoped_config_file_name(
      "memory://set_config_file_name_test.db");
  // After SetConfigFileName called, settings are set as default.
  Config updated_config;
  ConfigHandler::GetConfig(&updated_config);
  EXPECT_EQ(updated_config.incognito_mode(), default_incognito_mode);
}

#if !defined(__ANDROID__)
// Temporarily disable this test because FileUtil::CopyFile fails on
// Android for some reason.
TEST_F(ConfigHandlerTest, LoadTestConfig) {
  // TODO(yukawa): Generate test data automatically so that we can keep
  //     the compatibility among variety of config files.
  // TODO(yukawa): Enumerate test data in the directory automatically.
  const char *kDataFiles[] = {
      "linux_config1.db",
      "mac_config1.db",
      "win_config1.db",
  };

  for (size_t i = 0; i < std::size(kDataFiles); ++i) {
    const char *file_name = kDataFiles[i];
    const std::string src_path = mozc::testing::GetSourceFileOrDie(
        {"data", "test", "config", file_name});
    const std::string dest_path =
        FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), file_name);
    ASSERT_OK(FileUtil::CopyFile(src_path, dest_path))
        << "Copy failed: " << src_path << " to " << dest_path;

    ScopedSetConfigFileName scoped_config_file_name("user://" +
                                                    std::string(file_name));
    ConfigHandler::Reload();

    Config default_config;
    ConfigHandler::GetConfig(&default_config);

#ifdef _WIN32
    // Reset the file attributes since it may contain FILE_ATTRIBUTE_READONLY.
    std::wstring wdest_path;
    Util::Utf8ToWide(dest_path, &wdest_path);
    ::SetFileAttributesW(wdest_path.c_str(), FILE_ATTRIBUTE_NORMAL);
#endif  // _WIN32

    // Remove test file just in case.
    ASSERT_OK(FileUtil::Unlink(dest_path));
    EXPECT_FALSE(FileUtil::FileExists(dest_path).ok());
  }
}
#endif  // !__ANDROID__

TEST_F(ConfigHandlerTest, GetDefaultConfig) {
  Config output;

  output.Clear();
  ConfigHandler::GetDefaultConfig(&output);
#ifdef __APPLE__
  EXPECT_EQ(output.session_keymap(), Config::KOTOERI);
#elif defined(OS_CHROMEOS)  // __APPLE__
  EXPECT_EQ(output.session_keymap(), Config::CHROMEOS);
#else   // __APPLE__ || OS_CHROMEOS
  EXPECT_EQ(output.session_keymap(), Config::MSIME);
#endif  // __APPLE__ || OS_CHROMEOS
  EXPECT_EQ(output.character_form_rules_size(), 13);

  struct TestCase {
    const char *group;
    Config::CharacterForm preedit_character_form;
    Config::CharacterForm conversion_character_form;
  };
  const TestCase testcases[] = {
      // "ア"
      {"ア", Config::FULL_WIDTH, Config::FULL_WIDTH},
      {"A", Config::FULL_WIDTH, Config::LAST_FORM},
      {"0", Config::FULL_WIDTH, Config::LAST_FORM},
      {"(){}[]", Config::FULL_WIDTH, Config::LAST_FORM},
      {".,", Config::FULL_WIDTH, Config::LAST_FORM},
      // "。、",
      {"。、", Config::FULL_WIDTH, Config::FULL_WIDTH},
      // "・「」"
      {"・「」", Config::FULL_WIDTH, Config::FULL_WIDTH},
      {"\"'", Config::FULL_WIDTH, Config::LAST_FORM},
      {":;", Config::FULL_WIDTH, Config::LAST_FORM},
      {"#%&@$^_|`\\", Config::FULL_WIDTH, Config::LAST_FORM},
      {"~", Config::FULL_WIDTH, Config::LAST_FORM},
      {"<>=+-/*", Config::FULL_WIDTH, Config::LAST_FORM},
      {"?!", Config::FULL_WIDTH, Config::LAST_FORM},
  };
  EXPECT_EQ(output.character_form_rules_size(), std::size(testcases));
  for (size_t i = 0; i < std::size(testcases); ++i) {
    EXPECT_EQ(output.character_form_rules(i).group(), testcases[i].group);
    EXPECT_EQ(output.character_form_rules(i).preedit_character_form(),
              testcases[i].preedit_character_form);
    EXPECT_EQ(output.character_form_rules(i).conversion_character_form(),
              testcases[i].conversion_character_form);
  }

#if defined(__ANDROID__) && defined(CHANNEL_DEV)
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());
#endif  // __ANDROID__ && CHANNEL_DEV
}

TEST_F(ConfigHandlerTest, DefaultConfig) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  EXPECT_EQ(ConfigHandler::DefaultConfig().DebugString(), config.DebugString());
}

class SetConfigThread final : public Thread {
 public:
  explicit SetConfigThread(const std::vector<Config> &configs)
      : quitting_(false), configs_(configs) {}

  ~SetConfigThread() override {
    quitting_ = true;
    Join();
  }

 protected:
  void Run() override {
    absl::BitGen gen;
    while (!quitting_) {
      const size_t next_index = absl::Uniform(gen, 0u, configs_.size());
      ConfigHandler::SetConfig(configs_.at(next_index));
    }
  }

 private:
  std::atomic<bool> quitting_;
  std::vector<Config> configs_;
};

// Returns concatenated serialized data of |Config::character_form_rules|.
std::string ExtractCharacterFormRules(const Config &config) {
  std::string rules;
  for (size_t i = 0; i < config.character_form_rules_size(); ++i) {
    config.character_form_rules(i).AppendToString(&rules);
  }
  return rules;
}

class GetConfigThread final : public Thread {
 public:
  explicit GetConfigThread(
      const absl::flat_hash_set<std::string> &character_form_rules_set)
      : quitting_(false), character_form_rules_set_(character_form_rules_set) {}

  ~GetConfigThread() override {
    quitting_ = true;
    Join();
  }

 protected:
  void Run() override {
    while (!quitting_) {
      Config config;
      ConfigHandler::GetConfig(&config);
      const auto &rules = ExtractCharacterFormRules(config);
      EXPECT_NE(character_form_rules_set_.end(),
                character_form_rules_set_.find(rules));
    }
  }

 private:
  std::atomic<bool> quitting_;
  const absl::flat_hash_set<std::string> character_form_rules_set_;
};

TEST_F(ConfigHandlerTest, ConcurrentAccess) {
  std::vector<Config> configs;

  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    configs.push_back(config);
  }
  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.clear_character_form_rules();
    {
      auto *rule = config.add_character_form_rules();
      rule->set_group("0");
      rule->set_preedit_character_form(Config::HALF_WIDTH);
      rule->set_conversion_character_form(Config::HALF_WIDTH);
    }
    {
      auto *rule = config.add_character_form_rules();
      rule->set_group("A");
      rule->set_preedit_character_form(Config::LAST_FORM);
      rule->set_conversion_character_form(Config::LAST_FORM);
    }
    configs.push_back(config);
  }
  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    {
      auto *rule = config.add_character_form_rules();
      rule->set_group("0");
      rule->set_preedit_character_form(Config::HALF_WIDTH);
      rule->set_conversion_character_form(Config::HALF_WIDTH);
    }
    {
      auto *rule = config.add_character_form_rules();
      rule->set_group("A");
      rule->set_preedit_character_form(Config::LAST_FORM);
      rule->set_conversion_character_form(Config::LAST_FORM);
    }
    configs.push_back(config);
  }

  // Since |ConfigHandler::SetConfig()| actually updates some metadata in
  // |GeneralConfig|, the returned object from |ConfigHandler::GetConfig()|
  // is not predictable.  Hence we only make sure that
  // |Config::character_form_rules()| is one of expected values.
  absl::flat_hash_set<std::string> character_form_rules_set;
  for (const auto &config : configs) {
    character_form_rules_set.insert(ExtractCharacterFormRules(config));
  }

  // Before starting concurrent test, check to see if it works in single
  // thread.
  for (const auto &config : configs) {
    // Update the global config.
    ConfigHandler::SetConfig(config);

    // Check to see if the returned config contains one of expected
    // |Config::character_form_rules()|.
    Config returned_config;
    ConfigHandler::GetConfig(&returned_config);
    const auto &rules = ExtractCharacterFormRules(returned_config);
    ASSERT_NE(character_form_rules_set.end(),
              character_form_rules_set.find(rules));
  }

  // 250 msec is good enough to crash the code if it is not guarded by
  // the lock, but feel free to change the duration.  It is basically an
  // arbitrary number.
  constexpr auto kTestDuration = absl::Milliseconds(250);
  constexpr size_t kNumSetThread = 2;
  constexpr size_t kNumGetThread = 4;
  {
    // Set up background threads for concurrent access.
    std::vector<std::unique_ptr<SetConfigThread>> set_threads;
    for (size_t i = 0; i < kNumSetThread; ++i) {
      set_threads.emplace_back(std::make_unique<SetConfigThread>(configs));
    }
    std::vector<std::unique_ptr<GetConfigThread>> get_threads;
    for (size_t i = 0; i < kNumGetThread; ++i) {
      get_threads.emplace_back(
          std::make_unique<GetConfigThread>(character_form_rules_set));
    }
    // Let background threads start accessing ConfigHandler from multiple
    // background threads.
    for (size_t i = 0; i < set_threads.size(); ++i) {
      set_threads[i]->Start(
          absl::StrFormat("SetConfigThread%d", static_cast<int>(i)));
    }
    for (size_t i = 0; i < get_threads.size(); ++i) {
      get_threads[i]->Start(
          absl::StrFormat("GetConfigThread%d", static_cast<int>(i)));
    }
    // Wait for a while to see if everything goes well.
    absl::SleepFor(kTestDuration);
    // Destructors of |SetConfigThread| and |GetConfigThread| will take
    // care of their background threads (in a blocking way).
    set_threads.clear();
    get_threads.clear();
  }
}

}  // namespace
}  // namespace config
}  // namespace mozc
