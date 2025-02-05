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
#include <random>
#include <string>

#include "absl/flags/flag.h"
#include "base/random.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "engine/engine.h"
#include "engine/engine_converter.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "transliteration/transliteration.h"

ABSL_FLAG(bool, test_deterministic, true,
          "if true, srand() is initialized by \"test_srand_seed\"."
          "if false, srand() is initialized by current time "
          "and \"test_srand_seed\" is ignored");

ABSL_FLAG(int32_t, test_srand_seed, 0,
          "seed number for srand(). "
          "used only when \"test_deterministic\" is true");

namespace mozc {
namespace engine {

class EngineConverterStressTest : public testing::TestWithTempUserProfile {
 public:
  EngineConverterStressTest() {
    if (absl::GetFlag(FLAGS_test_deterministic)) {
      random_ = Random(std::seed_seq{absl::GetFlag(FLAGS_test_srand_seed)});
    }
  }

  void SetUp() override {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

 protected:
  Random random_;
};

TEST_F(EngineConverterStressTest, ConvertToHalfWidthForRandomAsciiInput) {
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

  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  EngineConverter sconverter(*engine->GetConverter(), request, config);
  composer::Table table;
  table.LoadFromFile(kRomajiHiraganaTable.c_str());
  composer::Composer composer(table, request, config);
  commands::Output output;

  for (int test = 0; test < kTestCaseSize; ++test) {
    constexpr int kLoopLimit = 100;
    for (int i = 0; i < kLoopLimit; ++i) {
      composer.Reset();
      sconverter.Reset();
      output.Clear();

      // Limited by kMaxCharLength in immutable_converter.cc
      constexpr int kInputStringLength = 32;
      const std::string input = random_.Utf8String(
          kInputStringLength, kTestCases[test].min, kTestCases[test].max);

      composer.InsertCharacterPreedit(input);
      sconverter.ConvertToTransliteration(composer,
                                          transliteration::HALF_ASCII);
      sconverter.FillOutput(composer, &output);

      const commands::Preedit& conversion = output.preedit();
      EXPECT_EQ(conversion.segment(0).value(), input)
          << input << "\t" << conversion.segment(0).value();
    }
  }
}

}  // namespace engine
}  // namespace mozc
