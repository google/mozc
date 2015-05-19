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

#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_srcdir);
DECLARE_string(test_tmpdir);

namespace mozc {

class ConfigHandlerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    default_config_filename_ = config::ConfigHandler::GetConfigFileName();
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfigFileName(default_config_filename_);
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
  }
 private:
  string default_config_filename_;
};

class ScopedSetConfigFileName {
 public:
  explicit ScopedSetConfigFileName(const string &new_name)
      : default_config_filename_(config::ConfigHandler::GetConfigFileName()) {
    config::ConfigHandler::SetConfigFileName(new_name);
  }

  ~ScopedSetConfigFileName() {
    config::ConfigHandler::SetConfigFileName(default_config_filename_);
  }

 private:
  string default_config_filename_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedSetConfigFileName);
};

TEST_F(ConfigHandlerTest, SetConfig) {
  config::Config input;
  config::Config output;

  const string config_file = Util::JoinPath(FLAGS_test_tmpdir,
                                            "mozc_config_test_tmp");
  Util::Unlink(config_file);
  ScopedSetConfigFileName scoped_config_file_name(config_file);
  EXPECT_EQ(config_file, config::ConfigHandler::GetConfigFileName());
  ASSERT_TRUE(config::ConfigHandler::Reload())
      << "failed to reload: " << config::ConfigHandler::GetConfigFileName();

  config::ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(true);
#ifndef NO_LOGGING
  input.set_verbose_level(2);
#endif  // NO_LOGGING
  config::ConfigHandler::SetMetaData(&input);
  EXPECT_TRUE(config::ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));
  input.mutable_general_config()->set_last_modified_time(0);
  output.mutable_general_config()->set_last_modified_time(0);
  EXPECT_EQ(input.DebugString(), output.DebugString());

  config::ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(false);
#ifndef NO_LOGGING
  input.set_verbose_level(0);
#endif  // NO_LOGGING
  config::ConfigHandler::SetMetaData(&input);
  EXPECT_TRUE(config::ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));

  input.mutable_general_config()->set_last_modified_time(0);
  output.mutable_general_config()->set_last_modified_time(0);
  EXPECT_EQ(input.DebugString(), output.DebugString());

#ifdef OS_ANDROID
#ifdef CHANNEL_DEV
  input.Clear();
  EXPECT_FALSE(input.general_config().has_upload_usage_stats());
  EXPECT_TRUE(config::ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());

  input.Clear();
  input.mutable_general_config()->set_upload_usage_stats(false);
  EXPECT_TRUE(input.general_config().has_upload_usage_stats());
  EXPECT_FALSE(input.general_config().upload_usage_stats());
  EXPECT_TRUE(config::ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());
#endif  // CHANNEL_DEV
#endif  // OS_ANDROID
}

TEST_F(ConfigHandlerTest, SetImposedConfig) {
  config::Config input;
  config::Config output;

  const string config_file = Util::JoinPath(FLAGS_test_tmpdir,
                                            "mozc_config_test_tmp");
  Util::Unlink(config_file);
  ScopedSetConfigFileName scoped_config_file_name(config_file);
  ASSERT_TRUE(config::ConfigHandler::Reload())
      << "failed to reload: " << config::ConfigHandler::GetConfigFileName();

  struct Testcase {
    bool stored_config_value;
    enum {DO_NOT_IMPOSE, IMPOSE_TRUE, IMPOSE_FALSE} imposed_config_value;
    bool expected_value;
  };
  const Testcase kTestcases[] = {
    {true, Testcase::IMPOSE_TRUE, true},
    {true, Testcase::IMPOSE_FALSE, false},
    {false, Testcase::IMPOSE_TRUE, true},
    {false, Testcase::IMPOSE_FALSE, false},
    {true, Testcase::DO_NOT_IMPOSE, true},
    {false, Testcase::DO_NOT_IMPOSE, false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestcases); ++i) {
    const bool stored_config_value = kTestcases[i].stored_config_value;
    const bool expected = kTestcases[i].expected_value;

    // Set current config.
    config::ConfigHandler::GetDefaultConfig(&input);
    input.set_incognito_mode(stored_config_value);
    config::ConfigHandler::SetMetaData(&input);
    EXPECT_TRUE(config::ConfigHandler::SetConfig(input));
    // Set imposed config.
    input.Clear();
    if (kTestcases[i].imposed_config_value != Testcase::DO_NOT_IMPOSE) {
      input.set_incognito_mode(
          kTestcases[i].imposed_config_value == Testcase::IMPOSE_TRUE);
    }
    config::ConfigHandler::SetImposedConfig(input);
    // Check post-condition.
    output.Clear();
    EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));
    EXPECT_EQ(expected, output.incognito_mode());
    output = config::ConfigHandler::GetConfig();
    EXPECT_EQ(expected, output.incognito_mode());
    output = config::ConfigHandler::GetStoredConfig();
    EXPECT_EQ(stored_config_value, output.incognito_mode());

    // Reload and check.
    ASSERT_TRUE(config::ConfigHandler::Reload())
        << "failed to reload: " << config::ConfigHandler::GetConfigFileName();
    output.Clear();
    EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));
    EXPECT_EQ(expected, output.incognito_mode());
    output = config::ConfigHandler::GetConfig();
    EXPECT_EQ(expected, output.incognito_mode());
    output = config::ConfigHandler::GetStoredConfig();
    EXPECT_EQ(stored_config_value, output.incognito_mode());

    // Unset imposed config.
    input.Clear();
    config::ConfigHandler::SetImposedConfig(input);
    // Check post-condition.
    output.Clear();
    EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));
    EXPECT_EQ(stored_config_value, output.incognito_mode());
    output = config::ConfigHandler::GetConfig();
    EXPECT_EQ(stored_config_value, output.incognito_mode());
    output = config::ConfigHandler::GetStoredConfig();
    EXPECT_EQ(stored_config_value, output.incognito_mode());
  }
}

TEST_F(ConfigHandlerTest, ConfigFileNameConfig) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  const string config_file = string("config")
      + NumberUtil::SimpleItoa(config::CONFIG_VERSION);

  const string filename = Util::JoinPath(FLAGS_test_tmpdir,
                                         config_file);
  Util::Unlink(filename);
  config::Config input;
  config::ConfigHandler::SetConfig(input);
  Util::FileExists(filename);
}

TEST_F(ConfigHandlerTest, SetConfigFileName) {
  config::Config mozc_config;
  mozc_config.mutable_hangul_config()->set_keyboard_type(
      config::HangulConfig::KEYBOARD_SebeolsikFinal);
  config::ConfigHandler::SetConfig(mozc_config);
  // ScopedSetConfigFileName internally calls SetConfigFileName.
  ScopedSetConfigFileName scoped_config_file_name(
      "memory://hangul_config.test.db");
  // After SetConfigFileName called, settings are set as default.
  EXPECT_EQ(config::HangulConfig::KEYBOARD_Dubeolsik,
            GET_CONFIG(hangul_config).keyboard_type());
}

#ifndef OS_ANDROID
// Temporarily disable this test because Util::CopyFile fails on
// Android for some reason.
// TODO(yukawa): Enable this test on Android.
TEST_F(ConfigHandlerTest, LoadTestConfig) {
  const char kPathPrefix[] = "";
  const char KDataDir[] = "data/test/config";
  // TODO(yukawa): Generate test data automatically so that we can keep
  //     the compatibility among variety of config files.
  // TODO(yukawa): Enumerate test data in the directory automatically.
  const char *kDataFiles[] = {
    "linux_config1.db",
    "mac_config1.db",
    "win_config1.db",
  };

  for (size_t i = 0; i < arraysize(kDataFiles); ++i) {
    const char *file_name = kDataFiles[i];
    const string &src_path = Util::JoinPath(
        Util::JoinPath(FLAGS_test_srcdir, kPathPrefix),
        Util::JoinPath(KDataDir, file_name));
    const string &dest_path = Util::JoinPath(
        Util::GetUserProfileDirectory(), file_name);
    ASSERT_TRUE(Util::CopyFile(src_path, dest_path));

    ScopedSetConfigFileName scoped_config_file_name(
        "user://" + string(file_name));
    ASSERT_TRUE(config::ConfigHandler::Reload())
        << "failed to reload: " << config::ConfigHandler::GetConfigFileName();

    config::Config default_config;
    EXPECT_TRUE(config::ConfigHandler::GetConfig(&default_config))
        << "failed to GetConfig from: " << file_name;

    // Remove test file just in case.
    ASSERT_TRUE(Util::Unlink(dest_path));
    EXPECT_FALSE(Util::FileExists(dest_path));
  }
}
#endif  // !OS_ANDROID

TEST_F(ConfigHandlerTest, GetDefaultConfig) {
  config::Config output;

  output.Clear();
  config::ConfigHandler::GetDefaultConfig(&output);
#ifdef OS_MACOSX
  EXPECT_EQ(output.session_keymap(), config::Config::KOTOERI);
#else  // OS_MACOSX
  EXPECT_EQ(output.session_keymap(), config::Config::MSIME);
#endif
  EXPECT_EQ(output.character_form_rules_size(), 13);

  struct TestCase {
    const char *group;
    config::Config::CharacterForm preedit_character_form;
    config::Config::CharacterForm conversion_character_form;
  };
  const TestCase testcases[] = {
     // "ア"
    {"\xE3\x82\xA2", config::Config::FULL_WIDTH, config::Config::FULL_WIDTH},
    {"A", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    {"0", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    {"(){}[]", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    {".,", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    // "。、",
    {"\xE3\x80\x82\xE3\x80\x81",
      config::Config::FULL_WIDTH, config::Config::FULL_WIDTH},
    // "・「」"
    {"\xE3\x83\xBB\xE3\x80\x8C\xE3\x80\x8D",
      config::Config::FULL_WIDTH, config::Config::FULL_WIDTH},
    {"\"'", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    {":;", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    {"#%&@$^_|`\\", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    {"~", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    {"<>=+-/*", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
    {"?!", config::Config::FULL_WIDTH, config::Config::LAST_FORM},
  };
  EXPECT_EQ(output.character_form_rules_size(), ARRAYSIZE_UNSAFE(testcases));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(testcases); ++i) {
    EXPECT_EQ(output.character_form_rules(i).group(),
              testcases[i].group);
    EXPECT_EQ(output.character_form_rules(i).preedit_character_form(),
              testcases[i].preedit_character_form);
    EXPECT_EQ(output.character_form_rules(i).conversion_character_form(),
              testcases[i].conversion_character_form);
  }

#ifdef OS_CHROMEOS
  EXPECT_FALSE(output.use_auto_conversion());
#endif

#ifdef OS_ANDROID
#ifdef CHANNEL_DEV
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());
#endif  // CHANNEL_DEV
#endif  // OS_ANDROID
}
}  // namespace mozc
