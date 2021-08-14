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

#include <cstdint>
#include <memory>
#include <string>

#include "base/clock.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_converter.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"

ABSL_FLAG(bool, test_deterministic, true,
          "if true, srand() is initialized by \"test_srand_seed\"."
          "if false, srand() is initialized by current time "
          "and \"test_srand_seed\" is ignored");

ABSL_FLAG(int32_t, test_srand_seed, 0,
          "seed number for srand(). "
          "used only when \"test_deterministic\" is true");

namespace mozc {

class ConverterInterface;

namespace session {

class SessionConverterStressTest : public ::testing::Test {
 public:
  SessionConverterStressTest() {
    if (!absl::GetFlag(FLAGS_test_deterministic)) {
      absl::SetFlag(&FLAGS_test_srand_seed,
                    static_cast<int32_t>(Clock::GetTime()));
    }
    Util::SetRandomSeed(
        static_cast<uint32_t>(absl::GetFlag(FLAGS_test_srand_seed)));
  }

  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }
};

namespace {
void GenerateRandomInput(size_t length, char min_code, char max_code,
                         std::string* output) {
  output->reserve(length);
  char tmp[2];
  tmp[1] = '\0';
  for (int i = 0; i < length; ++i) {
    tmp[0] = static_cast<unsigned char>(min_code +
                                        Util::Random(max_code - min_code + 1));
    output->append(tmp);
  }
}
}  // namespace

TEST_F(SessionConverterStressTest, ConvertToHalfWidthForRandomAsciiInput) {
  // ConvertToHalfWidth has to return the same string as the input.

  constexpr int kTestCaseSize = 2;
  struct TestCase {
    int min, max;
  } kTestCases[] = {
      {' ', '~'},  // All printable characters
      {'a', 'z'},  // Alphabets
  };

  const std::string kRomajiHiraganaTable = "system://romanji-hiragana.tsv";
  const commands::Request request;
  config::Config config;

  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface* converter = engine->GetConverter();
  SessionConverter sconverter(converter, &request, &config);
  composer::Table table;
  table.LoadFromFile(kRomajiHiraganaTable.c_str());
  composer::Composer composer(&table, &request, &config);
  commands::Output output;
  std::string input;

  for (int test = 0; test < kTestCaseSize; ++test) {
    constexpr int kLoopLimit = 100;
    for (int i = 0; i < kLoopLimit; ++i) {
      composer.Reset();
      sconverter.Reset();
      output.Clear();
      input.clear();

      // Limited by kMaxCharLength in immutable_converter.cc
      constexpr int kInputStringLength = 32;
      GenerateRandomInput(kInputStringLength, kTestCases[test].min,
                          kTestCases[test].max, &input);

      composer.InsertCharacterPreedit(input);
      sconverter.ConvertToTransliteration(composer,
                                          transliteration::HALF_ASCII);
      sconverter.FillOutput(composer, &output);

      const commands::Preedit& conversion = output.preedit();
      EXPECT_EQ(input, conversion.segment(0).value())
          << input << "\t" << conversion.segment(0).value();
    }
  }
}

}  // namespace session
}  // namespace mozc
