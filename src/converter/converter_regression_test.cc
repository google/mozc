// Copyright 2010-2016, Google Inc.
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

#include <memory>
#include <string>

#include "base/file_util.h"
#include "base/system_util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"

#ifdef MOZC_USE_PACKED_DICTIONARY
#ifdef MOZC_BUILD
#include "data_manager/packed/packed_data_oss.h"
#endif  // MOZC_BUILD

#include "data_manager/packed/packed_data_manager.h"
#endif  // MOZC_USE_PACKED_DICTIONARY

DECLARE_string(test_srcdir);
DECLARE_string(test_tmpdir);

namespace mozc {

using mozc::commands::Request;
using mozc::composer::Composer;
using mozc::composer::Table;
using mozc::config::Config;
using mozc::config::ConfigHandler;

class ConverterRegressionTest : public ::testing::Test {
 protected:
  ConverterRegressionTest() {
  }
  virtual ~ConverterRegressionTest() {
  }

  virtual void SetUp() {
    user_profile_directory_backup_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
#ifdef MOZC_USE_PACKED_DICTIONARY
    // Use a full-size dictionary for regression tests.
    std::unique_ptr<mozc::packed::PackedDataManager>
        data_manager(new mozc::packed::PackedDataManager);
    CHECK(data_manager->Init(string(kPackedSystemDictionary_data,
                                    kPackedSystemDictionary_size)));
    mozc::packed::RegisterPackedDataManager(data_manager.release());
#endif  // MOZC_USE_PACKED_DICTIONARY
  }

  virtual void TearDown() {
#ifdef MOZC_USE_PACKED_DICTIONARY
    mozc::packed::RegisterPackedDataManager(nullptr);
#endif  // MOZC_USE_PACKED_DICTIONARY
    SystemUtil::SetUserProfileDirectory(user_profile_directory_backup_);
  }

 private:
  string user_profile_directory_backup_;
};

TEST_F(ConverterRegressionTest, QueryOfDeathTest) {
  std::unique_ptr<EngineInterface> engine(EngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();

  CHECK(converter);
  {
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
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "5.1,||t:1"));
  }
  {
    Segments segments;
    // Converter returns false, but not crash.
    EXPECT_FALSE(converter->StartConversion(&segments, ""));
  }
  {
    Segments segments;
    ConversionRequest conv_request;
    // Create an empty composer.
    const Table table;
    const commands::Request request;
    composer::Composer composer(&table, &request, nullptr);
    conv_request.set_composer(&composer);
    // Converter returns false, but not crash.
    EXPECT_FALSE(converter->StartConversionForRequest(conv_request,
                                                      &segments));
  }
}

TEST_F(ConverterRegressionTest, Regression3323108) {
  std::unique_ptr<EngineInterface> engine(EngineFactory::Create());
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
