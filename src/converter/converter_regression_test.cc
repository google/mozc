// Copyright 2010-2014, Google Inc.
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

#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

#ifdef OS_ANDROID
#include "base/mmap.h"
#include "base/singleton.h"
#include "data_manager/android/android_data_manager.h"
#endif

DECLARE_string(test_srcdir);
DECLARE_string(test_tmpdir);

namespace mozc {

using mozc::commands::Request;
using mozc::composer::Composer;
using mozc::composer::Table;
using mozc::config::Config;
using mozc::config::ConfigHandler;

namespace {
#ifdef OS_ANDROID
// In actual libmozc.so usage, the dictionary data will be given via JNI call
// because only Java side code knows where the data is.
// On native code unittest, we cannot do it, so instead we mmap the files
// and use it.
// Note that this technique works here because the no other test code doesn't
// link to this binary.
// TODO(hidehiko): Get rid of this hack by refactoring Engine/DataManager
// related code.
class AndroidInitializer {
 private:
  AndroidInitializer() {
    string dictionary_data_path = FileUtil::JoinPath(
        FLAGS_test_srcdir, "embedded_data/dictionary_data");
    CHECK(dictionary_mmap_.Open(dictionary_data_path.c_str(), "r"));
    mozc::android::AndroidDataManager::SetDictionaryData(
        dictionary_mmap_.begin(), dictionary_mmap_.size());

    string connection_data_path = FileUtil::JoinPath(
        FLAGS_test_srcdir, "embedded_data/connection_data");
    CHECK(connection_mmap_.Open(connection_data_path.c_str(), "r"));
    mozc::android::AndroidDataManager::SetConnectionData(
        connection_mmap_.begin(), connection_mmap_.size());
    LOG(ERROR) << "mmap data initialized.";
  }

  friend class Singleton<AndroidInitializer>;

  Mmap dictionary_mmap_;
  Mmap connection_mmap_;

  DISALLOW_COPY_AND_ASSIGN(AndroidInitializer);
};
#endif  // OS_ANDROID
}  // namespace

class ConverterRegressionTest : public ::testing::Test {
 protected:
  ConverterRegressionTest() {
  }
  virtual ~ConverterRegressionTest() {
  }

  virtual void SetUp() {
#ifdef OS_ANDROID
    Singleton<AndroidInitializer>::get();
#endif

    user_profile_directory_backup_ = SystemUtil::GetUserProfileDirectory();
    config_backup_.CopyFrom(ConfigHandler::GetConfig());

    // set default user profile directory
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    ConfigHandler::SetConfig(config_backup_);
    SystemUtil::SetUserProfileDirectory(user_profile_directory_backup_);
  }

 private:
  string user_profile_directory_backup_;
  Config config_backup_;
};

TEST_F(ConverterRegressionTest, QueryOfDeathTest) {
  scoped_ptr<EngineInterface> engine(EngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();

  CHECK(converter);
  Segments segments;
  // "りゅきゅけmぽ"
  EXPECT_TRUE(converter->StartConversion(
      &segments,
      "\xE3\x82\x8A\xE3\x82\x85"
      "\xE3\x81\x8D\xE3\x82\x85"
      "\xE3\x81\x91"
      "m"
      "\xE3\x81\xBD"));
}

TEST_F(ConverterRegressionTest, Regression3323108) {
  scoped_ptr<EngineInterface> engine(EngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  // "ここではきものをぬぐ"
  EXPECT_TRUE(converter->StartConversion(
      &segments,
      "\xE3\x81\x93\xE3\x81\x93\xE3\x81\xA7"
      "\xE3\x81\xAF\xE3\x81\x8D\xE3\x82\x82"
      "\xE3\x81\xAE\xE3\x82\x92\xE3\x81\xAC"
      "\xE3\x81\x90"));
  EXPECT_EQ(3, segments.conversion_segments_size());
  const ConversionRequest default_request;
  EXPECT_TRUE(converter->ResizeSegment(&segments, default_request, 1, 2));
  EXPECT_EQ(2, segments.conversion_segments_size());

  // "きものをぬぐ"
  EXPECT_EQ("\xE3\x81\x8D\xE3\x82\x82"
            "\xE3\x81\xAE\xE3\x82\x92"
            "\xE3\x81\xAC\xE3\x81\x90",
            segments.conversion_segment(1).key());
}

}  // namespace mozc
