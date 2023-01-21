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

#include "base/text_normalizer.h"

#include <string>

#include "base/util.h"
#include "testing/gunit.h"

namespace mozc {

TEST(TextNormalizerTest, NormalizeText) {
  std::string output;

  output = TextNormalizer::NormalizeText("めかぶ");
  EXPECT_EQ("めかぶ", output);

  output = TextNormalizer::NormalizeText("ゔぁいおりん");
  EXPECT_EQ("ゔぁいおりん", output);

  // "〜" is U+301C
  output = TextNormalizer::NormalizeText("ぐ〜ぐる");
#ifdef OS_WIN
  EXPECT_EQ("ぐ～ぐる", output);  // "～" is U+FF5E
#else                             // OS_WIN
  EXPECT_EQ("ぐ〜ぐる", output);  // "〜" is U+301C
#endif                            // OS_WIN

  // "〜" is U+301C
  output =
      TextNormalizer::NormalizeTextWithFlag("ぐ〜ぐる", TextNormalizer::kAll);
  EXPECT_EQ("ぐ～ぐる", output);  // "～" is U+FF5E

  output =
      TextNormalizer::NormalizeTextWithFlag("ぐ〜ぐる", TextNormalizer::kNone);
  EXPECT_EQ("ぐ〜ぐる", output);  // "～" is U+301C

  // "−" is U+2212
  output = TextNormalizer::NormalizeText("１−２−３");
#ifdef OS_WIN
  EXPECT_EQ("１－２－３", output);  // "－" is U+FF0D
#else                               // OS_WIN
  EXPECT_EQ("１−２−３", output);  // "−" is U+2212
#endif                              // OS_WIN

  // "−" is U+2212
  output =
      TextNormalizer::NormalizeTextWithFlag("１−２−３", TextNormalizer::kAll);
  EXPECT_EQ("１－２－３", output);  // "－" is U+FF0D

  output =
      TextNormalizer::NormalizeTextWithFlag("１−２−３", TextNormalizer::kNone);
  EXPECT_EQ("１−２−３", output);  // "−" is U+2212

  // "¥" is U+00A5
  output = TextNormalizer::NormalizeText("¥298");
  // U+00A5 is no longer normalized.
  EXPECT_EQ("¥298", output);
}

TEST(TextNormalizerTest, NormalizeTextToSvs) {
  std::string output;
  EXPECT_FALSE(TextNormalizer::NormalizeTextToSvs("", &output));
  EXPECT_TRUE(output.empty());
  EXPECT_TRUE(TextNormalizer::NormalizeTextToSvs("").empty());

  std::string input = "abcあいう";
  EXPECT_FALSE(TextNormalizer::NormalizeTextToSvs(input, &output));
  EXPECT_TRUE(output.empty());
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs(input), input);

  input = "\uFA10";  // 塚 U+FA10, CJK compatibility character.
  std::string expected = "\u585A\uFE00";  // 塚︀ U+585A,U+FE00 SVS character.
  EXPECT_TRUE(TextNormalizer::NormalizeTextToSvs(input, &output));
  EXPECT_EQ(output, expected);
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs(input), expected);

  input = "abc\uFA10あいう";
  expected = "abc\u585A\uFE00あいう";
  EXPECT_TRUE(TextNormalizer::NormalizeTextToSvs(input, &output));
  EXPECT_EQ(output, expected);
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs(input), expected);

  // 欄 in KS X 1001
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs("\uF91D"), "\u6B04\uFE00");
  // 𤋮 in ARIB. The output is more than 16bits.
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs("\uFA6C"), "\U000242EE\uFE00");
  // 艹 uses FE01 for SVS extends
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs("\uFA5E"), "\u8279\uFE01");
  // 﨑 is a CJK compat ideograph, but not normalized.
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs("\uFA11"), "\uFA11");
  // 舘 has the largetst codepoint.
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs("\uFA6D"), "\u8218\uFE00");
  // Next codeponit of 舘.
  EXPECT_EQ(TextNormalizer::NormalizeTextToSvs("\uFA6E"), "\uFA6E");
}

}  // namespace mozc
