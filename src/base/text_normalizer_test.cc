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

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "base/port.h"
#include "testing/gunit.h"

namespace mozc {

TEST(TextNormalizerTest, NormalizeText) {
  std::string output;

  output = TextNormalizer::NormalizeText("めかぶ");
  EXPECT_EQ(output, "めかぶ");

  output = TextNormalizer::NormalizeText("ゔぁいおりん");
  EXPECT_EQ(output, "ゔぁいおりん");

  // "〜" is U+301C
  output = TextNormalizer::NormalizeText("ぐ〜ぐる");
  if constexpr (port::IsWindows()) {
    EXPECT_EQ(output, "ぐ～ぐる");  // "～" is U+FF5E
  } else {
    EXPECT_EQ(output, "ぐ〜ぐる");  // "〜" is U+301C
  }

  // "〜" is U+301C
  output =
      TextNormalizer::NormalizeTextWithFlag("ぐ〜ぐる", TextNormalizer::kAll);
  EXPECT_EQ(output, "ぐ～ぐる");  // "～" is U+FF5E

  output =
      TextNormalizer::NormalizeTextWithFlag("ぐ〜ぐる", TextNormalizer::kNone);
  EXPECT_EQ(output, "ぐ〜ぐる");  // "～" is U+301C

  // "−" is U+2212
  output = TextNormalizer::NormalizeText("１−２−３");
  if constexpr (port::IsWindows()) {
    EXPECT_EQ(output, "１－２－３");  // "－" is U+FF0D
  } else {
    EXPECT_EQ(output, "１−２−３");  // "−" is U+2212
  }

  // "−" is U+2212
  output =
      TextNormalizer::NormalizeTextWithFlag("１−２−３", TextNormalizer::kAll);
  EXPECT_EQ(output, "１－２－３");  // "－" is U+FF0D

  output =
      TextNormalizer::NormalizeTextWithFlag("１−２−３", TextNormalizer::kNone);
  EXPECT_EQ(output, "１−２−３");  // "−" is U+2212

  // "¥" is U+00A5
  output = TextNormalizer::NormalizeText("¥298");
  // U+00A5 is no longer normalized.
  EXPECT_EQ(output, "¥298");
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

TEST(TextNormalizerTest, SanitizeText) {
  // 1. No modifications
  EXPECT_EQ(TextNormalizer::SanitizeText("abcあいう", 15), std::nullopt);
  EXPECT_EQ(TextNormalizer::SanitizeText("", 10), std::nullopt);

  // 2. Truncation by max_bytes (safely preserving UTF-8 boundaries)
  // "あいうえお" is 15 bytes in UTF-8 (3 bytes per character).
  // Triggered at limit 6 bytes (should cut exactly to "あい" -> 6 bytes).
  EXPECT_EQ(*TextNormalizer::SanitizeText("あいうえお", 6), "あい");
  EXPECT_EQ(*TextNormalizer::SanitizeText("あいうえお", 7), "あい");
  EXPECT_EQ(*TextNormalizer::SanitizeText("あいうえお", 8), "あい");
  EXPECT_EQ(*TextNormalizer::SanitizeText("あいうえお", 9), "あいう");

  // 3. Skip control characters
  EXPECT_EQ(
      *TextNormalizer::SanitizeText(absl::string_view("abc\n\t\0def", 9), 10),
      "abcdef");

  // 4. Skip invalid UTF-8 sequences
  // (including partial characters of "あ" \xE3\x81\x82)
  EXPECT_EQ(*TextNormalizer::SanitizeText("abc\xFF""def", 10), "abcdef");
  // \xE3 only (missing 2nd and 3rd bytes)
  EXPECT_EQ(*TextNormalizer::SanitizeText("abc\xE3""def", 10), "abcdef");
  // \xE3\x81 only (missing 3rd byte)
  EXPECT_EQ(*TextNormalizer::SanitizeText("abc\xE3\x81""def", 10), "abcdef");
  // \x81 only (missing 1st and 2nd bytes)
  EXPECT_EQ(*TextNormalizer::SanitizeText("abc\x81""def", 10), "abcdef");
  // \x81\x82 only (missing 1st byte)
  EXPECT_EQ(*TextNormalizer::SanitizeText("abc\x81\x82""def", 10), "abcdef");

  // 5. Valid emoji or SVS cases (no modifications -> std::nullopt)
  // Emoji 😊 (4 bytes: \xF0\x9F\x98\x8A)
  EXPECT_EQ(TextNormalizer::SanitizeText("😊", 10), std::nullopt);
  // SVS 塚︀ (6 bytes: 塚\u585A [3] + FE00 [3])
  EXPECT_EQ(TextNormalizer::SanitizeText("\u585A\uFE00", 10), std::nullopt);
  // Combined Emoji and SVS (4 + 6 = 10 bytes)
  EXPECT_EQ(TextNormalizer::SanitizeText("😊\u585A\uFE00", 10), std::nullopt);

  // 6. Truncation of Emoji and SVS
  // 😊 (4 bytes) triggers at 3 bytes -> truncated, broken byte ignored -> ""
  EXPECT_EQ(*TextNormalizer::SanitizeText("😊", 3), "");
  EXPECT_EQ(*TextNormalizer::SanitizeText("ab😊", 5), "ab");
  // 塚︀ (6 bytes) truncated at 5 bytes -> U+FE00 SVS part is cut -> "\u585A"
  EXPECT_EQ(*TextNormalizer::SanitizeText("\u585A\uFE00", 5), "\u585A");
  EXPECT_EQ(*TextNormalizer::SanitizeText("\u585A\uFE00", 4), "\u585A");
  EXPECT_EQ(*TextNormalizer::SanitizeText("\u585A\uFE00", 2), "");

  // 7. Combined control skip and byte truncation
  // "あ\nい\tうえお" is truncated by 11 bytes after skips -> "あいう".
  EXPECT_EQ(*TextNormalizer::SanitizeText("あ\nい\tうえお", 11), "あいう");
}

}  // namespace mozc
