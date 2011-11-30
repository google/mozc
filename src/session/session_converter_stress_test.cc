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

#include <ctime>
#include <string>
#include "base/base.h"
#include "base/util.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/converter_interface.h"
#include "session/session_converter.h"
#include "composer/table.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

DECLARE_string(test_tmpdir);

DEFINE_bool(test_deterministic, true,
             "if true, srand() is initialized by \"test_srand_seed\"."
             "if false, srand() is initialized by current time "
             "and \"test_srand_seed\" is ignored");

DEFINE_int32(test_srand_seed, 0,
             "seed number for srand(). "
             "used only when \"test_deterministic\" is true");

namespace mozc {
namespace session {

class SessionConverterStressTest : public testing::Test {
 public:
  SessionConverterStressTest() {
    if (!FLAGS_test_deterministic) {
      FLAGS_test_srand_seed = static_cast<int32>(::time(NULL));
    }
    ::srand(static_cast<uint32>(FLAGS_test_srand_seed));
  }

  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }
};

namespace {

int GetRandom(int size) {
  return static_cast<int> (1.0 * size * ::rand() / (RAND_MAX + 1.0));
}

void GenerateRandomInput(
    size_t length, char min_code, char max_code, string* output) {
  output->reserve(length);
  char tmp[2];
  tmp[1] = '\0';
  for (int i = 0; i < length; ++i) {
    tmp[0] = static_cast<unsigned char>(
        min_code + GetRandom(max_code - min_code + 1));
    output->append(tmp);
  }
}

}  // namespace

TEST_F(SessionConverterStressTest, ConvertToHalfWidthForRandomAsciiInput) {
  // ConvertToHalfWidth has to return the same string as the input.

  const int kTestCaseSize = 2;
  struct TestCase {
    int min, max;
  } kTestCases[] = {
      {' ', '~'},  // All printable characters
      {'a', 'z'},  // Alphabets
  };

  const string kRomajiHiraganaTable = "system://romanji-hiragana.tsv";

  ConverterInterface* converter = ConverterFactory::GetConverter();
  SessionConverter sconverter(converter);
  composer::Table table;
  table.LoadFromFile(kRomajiHiraganaTable.c_str());
  composer::Composer composer;
  composer.SetTable(&table);
  commands::Output output;
  string input;

  for (int test = 0; test < kTestCaseSize; ++test) {
    const int kLoopLimit = 100;
    for (int i = 0; i < kLoopLimit; ++i) {
      composer.Reset();
      sconverter.Reset();
      output.Clear();
      input.clear();

      // Limited by kMaxCharLength in immutable_converter.cc
      const int kInputStringLength = 32;
      GenerateRandomInput(
          kInputStringLength, kTestCases[test].min, kTestCases[test].max,
          &input);

      composer.InsertCharacterPreedit(input);
      sconverter.ConvertToHalfWidth(composer);
      sconverter.FillOutput(composer, &output);

      const commands::Preedit &conversion = output.preedit();
      EXPECT_EQ(input, conversion.segment(0).value());
    }
  }
}

}  // namespace session
}  // namespace mozc
