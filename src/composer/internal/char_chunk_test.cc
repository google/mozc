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

#include "composer/internal/char_chunk.h"

#include <memory>
#include <set>
#include <string>

#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"
#include "testing/gunit.h"

namespace mozc {
namespace composer {

TEST(CharChunkTest, AddInput_CharByChar) {
  // Test against http://b/1547858
  Table table;
  table.AddRule("i", "い", "");
  table.AddRule("tt", "っ", "t");
  table.AddRule("ta", "た", "");

  std::string input;

  CharChunk chunk1(Transliterators::CONVERSION_STRING, &table);
  input = "i";
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ("i", chunk1.raw());
  EXPECT_EQ("い", chunk1.conversion());
  EXPECT_EQ("", chunk1.pending());
  EXPECT_EQ("", input);

  CharChunk chunk2(Transliterators::CONVERSION_STRING, &table);
  input = "t";
  EXPECT_FALSE(chunk2.AddInputInternal(&input));
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ("t", chunk2.raw());
  EXPECT_EQ("", chunk2.conversion());
  EXPECT_EQ("t", chunk2.pending());
  EXPECT_EQ("", input);

  input = "t";
  EXPECT_FALSE(chunk2.AddInputInternal(&input));
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ("tt", chunk2.raw());
  EXPECT_EQ("っ", chunk2.conversion());
  EXPECT_EQ("t", chunk2.pending());
  EXPECT_EQ("", input);

  input = "a";
  EXPECT_FALSE(chunk2.AddInputInternal(&input));
  EXPECT_TRUE(chunk2.IsFixed());
  EXPECT_EQ("tta", chunk2.raw());
  EXPECT_EQ("った", chunk2.conversion());
  EXPECT_EQ("", chunk2.pending());
  EXPECT_EQ("", input);
}

TEST(CharChunkTest, AddInput_NoEffectInput) {
  Table table;
  table.AddRule("2", "", "<*>2");
  table.AddRule("<*>1", "", "1");
  table.AddRule("*", "", "");

  std::string input;

  CharChunk chunk1(Transliterators::CONVERSION_STRING, &table);
  input = "2";
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ("2", chunk1.raw());
  EXPECT_EQ("", chunk1.conversion());
  EXPECT_EQ("<*>2", chunk1.pending());
  EXPECT_EQ("", input);

  input = "*";
  // "<*>2*" is used as a query but no such entry is in the table.
  // Thus AddInputInternal() should not consume the input.
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ("2", chunk1.raw());
  EXPECT_EQ("", chunk1.conversion());
  EXPECT_EQ("<*>2", chunk1.pending());
  EXPECT_EQ("*", input);
}

TEST(CharChunkTest, AddInput_ForN) {
  Table table;
  table.AddRule("a", "[A]", "");
  table.AddRule("n", "[N]", "");
  table.AddRule("nn", "[N]", "");
  table.AddRule("na", "[NA]", "");
  table.AddRule("nya", "[NYA]", "");
  table.AddRule("ya", "[YA]", "");
  table.AddRule("ka", "[KA]", "");

  std::string input;

  CharChunk chunk1(Transliterators::CONVERSION_STRING, &table);
  input = "n";
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ("n", chunk1.raw());
  EXPECT_EQ("", chunk1.conversion());
  EXPECT_EQ("n", chunk1.pending());
  EXPECT_EQ("", input);

  input = "y";
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ("ny", chunk1.raw());
  EXPECT_EQ("", chunk1.conversion());
  EXPECT_EQ("ny", chunk1.pending());
  EXPECT_EQ("", input);

  input = "n";
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ("ny", chunk1.raw());
  EXPECT_EQ("", chunk1.conversion());
  EXPECT_EQ("ny", chunk1.pending());
  EXPECT_EQ("n", input);

  input = "a";
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ("nya", chunk1.raw());
  EXPECT_EQ("[NYA]", chunk1.conversion());
  EXPECT_EQ("", chunk1.pending());
  EXPECT_EQ("", input);
}

TEST(CharChunkTest, AddInput_WithString) {
  // Test against http://b/1547858
  Table table;
  table.AddRule("i", "い", "");
  table.AddRule("tt", "っ", "t");
  table.AddRule("ta", "た", "");

  std::string input = "itta";

  CharChunk chunk1(Transliterators::CONVERSION_STRING, &table);
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ("i", chunk1.raw());
  EXPECT_EQ("い", chunk1.conversion());
  EXPECT_EQ("", chunk1.pending());
  EXPECT_EQ("tta", input);

  CharChunk chunk2(Transliterators::CONVERSION_STRING, &table);
  EXPECT_TRUE(chunk2.AddInputInternal(&input));
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ("tt", chunk2.raw());
  EXPECT_EQ("っ", chunk2.conversion());
  EXPECT_EQ("t", chunk2.pending());
  EXPECT_EQ("a", input);

  EXPECT_FALSE(chunk2.AddInputInternal(&input));
  EXPECT_TRUE(chunk2.IsFixed());
  EXPECT_EQ("tta", chunk2.raw());
  EXPECT_EQ("った", chunk2.conversion());
  EXPECT_EQ("", chunk2.pending());
  EXPECT_EQ("", input);
}

TEST(CharChunkTest, GetLength) {
  CharChunk chunk1(Transliterators::CONVERSION_STRING, nullptr);
  chunk1.set_conversion("ね");
  chunk1.set_pending("");
  chunk1.set_raw("ne");
  EXPECT_EQ(1, chunk1.GetLength(Transliterators::CONVERSION_STRING));
  EXPECT_EQ(2, chunk1.GetLength(Transliterators::RAW_STRING));

  CharChunk chunk2(Transliterators::CONVERSION_STRING, nullptr);
  chunk2.set_conversion("っと");
  chunk2.set_pending("");
  chunk2.set_raw("tto");
  EXPECT_EQ(2, chunk2.GetLength(Transliterators::CONVERSION_STRING));
  EXPECT_EQ(3, chunk2.GetLength(Transliterators::RAW_STRING));

  CharChunk chunk3(Transliterators::CONVERSION_STRING, nullptr);
  chunk3.set_conversion("が");
  chunk3.set_pending("");
  chunk3.set_raw("ga");
  EXPECT_EQ(1, chunk3.GetLength(Transliterators::CONVERSION_STRING));
  EXPECT_EQ(2, chunk3.GetLength(Transliterators::RAW_STRING));

  chunk3.SetTransliterator(Transliterators::HALF_KATAKANA);
  EXPECT_EQ(2, chunk3.GetLength(Transliterators::HALF_KATAKANA));
  chunk3.SetTransliterator(Transliterators::HALF_ASCII);
  EXPECT_EQ(2, chunk3.GetLength(Transliterators::HALF_ASCII));
}

TEST(CharChunkTest, AddCompositionInput) {
  Table table;
  table.AddRule("す゛", "ず", "");

  CharChunk chunk1(Transliterators::CONVERSION_STRING, &table);
  CompositionInput input;
  input.InitFromRawAndConv("m", "も", false);
  chunk1.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ("m", chunk1.raw());
  EXPECT_EQ("も", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  input.InitFromRawAndConv("r", "す", false);
  chunk1.AddCompositionInput(&input);
  // The input values are not used.
  EXPECT_EQ("r", input.raw());
  EXPECT_EQ("す", input.conversion());
  // The chunk remains the previous value.
  EXPECT_EQ("m", chunk1.raw());
  EXPECT_EQ("も", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  CharChunk chunk2(Transliterators::CONVERSION_STRING, &table);
  // raw == "r", conversion == "す";
  chunk2.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ("r", chunk2.raw());
  EXPECT_EQ("す", chunk2.pending());
  EXPECT_TRUE(chunk2.conversion().empty());

  input.InitFromRawAndConv("@", "゛", false);
  chunk2.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ("r@", chunk2.raw());
  EXPECT_TRUE(chunk2.pending().empty());
  EXPECT_EQ("ず", chunk2.conversion());

  input.InitFromRawAndConv("h", "く", false);
  chunk2.AddCompositionInput(&input);
  // The input values are not used.
  EXPECT_EQ("h", input.raw());
  EXPECT_EQ("く", input.conversion());
  // The chunk remains the previous value.
  EXPECT_EQ("r@", chunk2.raw());
  EXPECT_TRUE(chunk2.pending().empty());
  EXPECT_EQ("ず", chunk2.conversion());
}

TEST(CharChunkTest, AddCompositionInputWithHalfAscii) {
  Table table;
  table.AddRule("-", "ー", "");

  CharChunk chunk1(Transliterators::CONVERSION_STRING, &table);
  CompositionInput input;
  input.InitFromRawAndConv("-", "-", false);
  chunk1.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ("-", chunk1.raw());
  EXPECT_EQ("-", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  input.InitFromRawAndConv("-", "-", false);
  chunk1.AddCompositionInput(&input);
  // The input values are not used.
  EXPECT_EQ("-", input.raw());
  EXPECT_EQ("-", input.conversion());
  // The chunk remains the previous value.
  EXPECT_EQ("-", chunk1.raw());
  EXPECT_EQ("-", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  CharChunk chunk2(Transliterators::CONVERSION_STRING, &table);
  // key == "-", value == "-";
  chunk2.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ("-", chunk2.raw());
  EXPECT_EQ("-", chunk2.pending());
  EXPECT_TRUE(chunk2.conversion().empty());
}

TEST(CharChunkTest, OutputMode) {
  Table table;
  table.AddRule("a", "あ", "");

  CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
  std::string input = "a";
  chunk.AddInputInternal(&input);

  std::string result;
  chunk.AppendResult(Transliterators::LOCAL, &result);
  EXPECT_EQ("あ", result);

  chunk.SetTransliterator(Transliterators::FULL_KATAKANA);
  result.clear();
  chunk.AppendResult(Transliterators::LOCAL, &result);
  EXPECT_EQ("ア", result);

  chunk.SetTransliterator(Transliterators::HALF_ASCII);
  result.clear();
  chunk.AppendResult(Transliterators::LOCAL, &result);
  EXPECT_EQ("a", result);

  result.clear();
  chunk.AppendResult(Transliterators::HALF_KATAKANA, &result);
  EXPECT_EQ("ｱ", result);
}

TEST(CharChunkTest, SplitChunk) {
  Table table;
  table.AddRule("mo", "も", "");

  CharChunk chunk(Transliterators::HIRAGANA, &table);

  std::string input = "m";
  chunk.AddInputInternal(&input);
  EXPECT_TRUE(input.empty());

  std::string output;
  chunk.AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ("ｍ", output);

  input = "o";
  chunk.AddInputInternal(&input);
  EXPECT_TRUE(input.empty());

  output.clear();
  chunk.AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ("も", output);

  chunk.SetTransliterator(Transliterators::HALF_ASCII);
  output.clear();
  chunk.AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ("mo", output);

  // Split "mo" to "m" and "o".
  std::unique_ptr<CharChunk> left_chunk =
      chunk.SplitChunk(Transliterators::LOCAL, 1);
  ASSERT_TRUE(left_chunk != nullptr);

  // The output should be half width "m" rather than full width "ｍ".
  output.clear();
  left_chunk->AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ("m", output);
}

TEST(CharChunkTest, IsAppendable) {
  Table table;
  table.AddRule("mo", "も", "");
  Table table_another;

  CharChunk chunk(Transliterators::HIRAGANA, &table);

  std::string input = "m";
  chunk.AddInputInternal(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_TRUE(chunk.IsAppendable(Transliterators::LOCAL, &table));
  EXPECT_TRUE(chunk.IsAppendable(Transliterators::HIRAGANA, &table));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::FULL_KATAKANA, &table));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::LOCAL, &table_another));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::HIRAGANA, &table_another));

  input = "o";
  chunk.AddInputInternal(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::LOCAL, &table));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::HIRAGANA, &table));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::FULL_KATAKANA, &table));
}

TEST(CharChunkTest, AddInputInternal) {
  Table table;
  table.AddRule("tt", "っ", "t");

  CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
  {
    std::string key = "t";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    std::string key = "t";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("tt", chunk.raw());
    EXPECT_EQ("っ", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    std::string key = "t";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("ttt", chunk.raw());
    EXPECT_EQ("っっ", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    std::string key = "!";
    chunk.AddInputInternal(&key);
    EXPECT_EQ("!", key);
    EXPECT_EQ("ttt", chunk.raw());
    EXPECT_EQ("っっ", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
}

TEST(CharChunkTest, AddInputInternalDifferentPending) {
  Table table;
  table.AddRule("1", "", "あ");
  table.AddRule("あ*", "", "ぁ");

  CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
  {
    std::string key = "1";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("1", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("あ", chunk.pending());
  }
  {
    std::string key = "*";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("1*", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("ぁ", chunk.pending());
  }
}

TEST(CharChunkTest, AddInputInternalAmbiguousConversion) {
  Table table;
  table.AddRule("a", "あ", "");
  table.AddRule("n", "ん", "");
  table.AddRule("nn", "ん", "");
  table.AddRule("na", "な", "");
  table.AddRule("ya", "や", "");
  table.AddRule("nya", "にゃ", "");

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    std::string key = "n";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());
    EXPECT_EQ("ん", chunk.ambiguous());

    key = "a";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("na", chunk.raw());
    EXPECT_EQ("な", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    std::string key = "n";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());
    EXPECT_EQ("ん", chunk.ambiguous());

    key = "y";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("ny", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("ny", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());

    key = "a";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("nya", chunk.raw());
    EXPECT_EQ("にゃ", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    std::string key = "nya";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("nya", chunk.raw());
    EXPECT_EQ("にゃ", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    std::string key = "n";
    chunk.AddInputInternal(&key);
    EXPECT_EQ("", key);
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());
    EXPECT_EQ("ん", chunk.ambiguous());

    key = "k";
    chunk.AddInputInternal(&key);
    EXPECT_EQ("k", key);
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("ん", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    std::string key = "nk";
    chunk.AddInputInternal(&key);
    EXPECT_EQ("k", key);
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("ん", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
  }
}

TEST(CharChunkTest, AddInputInternalWithAttributes) {
  Table table;
  table.AddRuleWithAttributes("1", "", "あ", NO_TRANSLITERATION);
  table.AddRule("あ*", "", "ぁ");

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    std::string key = "1";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("1", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("あ", chunk.pending());
    EXPECT_EQ(NO_TRANSLITERATION, chunk.attributes());

    key = "*";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("1*", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("ぁ", chunk.pending());
    EXPECT_EQ(NO_TRANSLITERATION, chunk.attributes());
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    std::string key = "1*";
    chunk.AddInputInternal(&key);
    EXPECT_EQ("*", key);
    EXPECT_EQ("1", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("あ", chunk.pending());
    EXPECT_EQ(NO_TRANSLITERATION, chunk.attributes());

    chunk.AddInputInternal(&key);
    EXPECT_EQ("", key);
    EXPECT_EQ("1*", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("ぁ", chunk.pending());
    EXPECT_EQ(NO_TRANSLITERATION, chunk.attributes());
  }

  Table table2;
  table2.AddRuleWithAttributes("n", "ん", "", NO_TRANSLITERATION);
  table2.AddRuleWithAttributes("na", "な", "", DIRECT_INPUT);

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table2);

    std::string key = "n";
    chunk.AddInputInternal(&key);
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());
    EXPECT_EQ("ん", chunk.ambiguous());
    EXPECT_EQ(NO_TRANSLITERATION, chunk.attributes());

    key = "a";
    chunk.AddInputInternal(&key);
    EXPECT_EQ("な", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
    EXPECT_EQ(DIRECT_INPUT, chunk.attributes());
  }
}

TEST(CharChunkTest, CaseSensitive) {
  Table table;
  table.AddRule("ka", "[ka]", "");
  CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

  std::string key = "Ka";
  chunk.AddInputInternal(&key);
  EXPECT_TRUE(key.empty());
  EXPECT_EQ("Ka", chunk.raw());
  EXPECT_EQ("[ka]", chunk.conversion());
  EXPECT_TRUE(chunk.pending().empty());
}

TEST(CharChunkTest, TrimLeadingSpecialKey) {
  Table table;
  table.AddRule("ああ", "", "い");
  table.AddRule("いあ", "", "う");
  table.AddRule("あ{!}", "あ", "");
  table.AddRule("い{!}", "い", "");
  table.AddRule("う{!}", "う", "");
  table.AddRule("{#}え", "え", "");

  std::string input(table.ParseSpecialKey("ああ{!}{!}あ{!}"));
  {
    // Check a normal behavior. "ああ{!}" is converted to "い".
    CharChunk chunk(Transliterators::HIRAGANA, &table);
    chunk.AddInput(&input);
    EXPECT_EQ(input, table.ParseSpecialKey("{!}あ{!}"));
    EXPECT_EQ(chunk.raw(), table.ParseSpecialKey("ああ{!}"));
    EXPECT_EQ(chunk.conversion(), "い");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.ambiguous(), "");
  }
  {
    // The first "{!}" is erased because:
    // 1. it is a special key.
    // 2. there is no conversion rule starting from "{!}"".
    CharChunk chunk(Transliterators::HIRAGANA, &table);
    chunk.AddInput(&input);
    EXPECT_EQ(input, "");
    EXPECT_EQ(chunk.raw(), table.ParseSpecialKey("あ{!}"));
    EXPECT_EQ(chunk.conversion(), "あ");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.ambiguous(), "");
  }

  // {?} is an unused special key.
  input = table.ParseSpecialKey("い{?}あ");
  {
    CharChunk chunk(Transliterators::HIRAGANA, &table);
    chunk.AddInput(&input);
    EXPECT_EQ(input, table.ParseSpecialKey("{?}あ"));
    EXPECT_EQ(chunk.raw(), "い");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "い");
    EXPECT_EQ(chunk.ambiguous(), "");

    // {?} is trimed because it is not used by any rules.
    chunk.AddInput(&input);
    EXPECT_EQ(input, "");
    EXPECT_EQ(chunk.raw(), "いあ");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "う");
    EXPECT_EQ(chunk.ambiguous(), "");
  }

  // {#} is a used special key for "{#}え".
  input = table.ParseSpecialKey("い{#}え");
  {
    CharChunk chunk(Transliterators::HIRAGANA, &table);
    chunk.AddInput(&input);
    EXPECT_EQ(input, table.ParseSpecialKey("{#}え"));
    EXPECT_EQ(chunk.raw(), "い");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "い");
    EXPECT_EQ(chunk.ambiguous(), "");

    // No input is used for this already filled chunk.
    chunk.AddInput(&input);
    EXPECT_EQ(input, table.ParseSpecialKey("{#}え"));
    EXPECT_EQ(chunk.raw(), "い");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "い");
    EXPECT_EQ(chunk.ambiguous(), "");
  }
}

TEST(CharChunkTest, LeadingSpecialKey) {
  Table table;
  table.AddRule("{!}あ", "い", "");

  std::string input(table.ParseSpecialKey("{!}"));

  CharChunk chunk(Transliterators::HIRAGANA, &table);
  chunk.AddInput(&input);
  EXPECT_EQ(input, "");
  EXPECT_EQ(chunk.raw(), table.ParseSpecialKey("{!}"));
  EXPECT_EQ(chunk.conversion(), "");
  EXPECT_EQ(chunk.pending(), table.ParseSpecialKey("{!}"));
  EXPECT_EQ(chunk.ambiguous(), "");

  input = "あ";
  chunk.AddInput(&input);
  EXPECT_EQ(input, "");
  EXPECT_EQ(chunk.raw(), table.ParseSpecialKey("{!}あ"));
  EXPECT_EQ(chunk.conversion(), "い");
  EXPECT_EQ(chunk.pending(), "");
  EXPECT_EQ(chunk.ambiguous(), "");
}

TEST(CharChunkTest, LeadingSpecialKey2) {
  Table table;
  table.AddRule("{henkan}", "", "{r}");
  table.AddRule("{r}j", "お", "");

  size_t used_length = 0;
  bool fixed = false;
  table.LookUpPrefix(table.ParseSpecialKey("{r}j"), &used_length, &fixed);
  EXPECT_EQ(used_length, 4);
  EXPECT_TRUE(fixed);

  std::string input(table.ParseSpecialKey("{henkan}"));

  CharChunk chunk(Transliterators::HIRAGANA, &table);
  chunk.AddInput(&input);
  EXPECT_EQ(input, "");
  EXPECT_EQ(chunk.raw(), table.ParseSpecialKey("{henkan}"));
  EXPECT_EQ(chunk.conversion(), "");
  EXPECT_EQ(chunk.pending(), table.ParseSpecialKey("{r}"));
  EXPECT_EQ(chunk.ambiguous(), "");

  input = "j";
  chunk.AddInput(&input);
  EXPECT_EQ(input, "");
  EXPECT_EQ(chunk.raw(), table.ParseSpecialKey("{henkan}j"));
  EXPECT_EQ(chunk.conversion(), "お");
  EXPECT_EQ(chunk.pending(), "");
  EXPECT_EQ(chunk.ambiguous(), "");
}

TEST(CharChunkTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  Table table;
  table.AddRule("ss", "っ", "s");
  table.AddRule("shi", "し", "");

  CharChunk chunk(Transliterators::HIRAGANA, &table);

  {
    std::string input("ssh");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ssh", chunk.raw());
    EXPECT_EQ("っ", chunk.conversion());
    EXPECT_EQ("sh", chunk.pending());
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ("", chunk.ambiguous());
  }
  {
    std::string result;
    chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
    EXPECT_EQ("っｓｈ", result);
  }

  // Break down of the internal procedures
  chunk.Clear();
  chunk.SetTransliterator(Transliterators::HIRAGANA);
  {
    std::string input("s");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("s", chunk.raw());
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("s", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
  }
  {
    std::string input("s");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ss", chunk.raw());
    EXPECT_EQ("っ", chunk.conversion());
    EXPECT_EQ("s", chunk.pending());
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ("", chunk.ambiguous());
  }
  {
    std::string input("h");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ssh", chunk.raw());
    EXPECT_EQ("っ", chunk.conversion());
    EXPECT_EQ("sh", chunk.pending());
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ("", chunk.ambiguous());
  }
}

TEST(CharChunkTest, ShouldCommit) {
  Table table;
  table.AddRuleWithAttributes("ka", "[KA]", "", DIRECT_INPUT);
  table.AddRuleWithAttributes("tt", "[X]", "t", DIRECT_INPUT);
  table.AddRuleWithAttributes("ta", "[TA]", "", NO_TABLE_ATTRIBUTE);

  {  // ka - DIRECT_INPUT
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    std::string input("k");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("k", chunk.raw());
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ka", chunk.raw());
    EXPECT_EQ("[KA]", chunk.conversion());
    EXPECT_TRUE(chunk.ShouldCommit());
  }

  {  // ta - NO_TABLE_ATTRIBUTE
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    std::string input("t");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ta", chunk.raw());
    EXPECT_EQ("[TA]", chunk.conversion());
    EXPECT_FALSE(chunk.ShouldCommit());
  }

  {  // tta - (tt: DIRECT_INPUT / ta: NO_TABLE_ATTRIBUTE)
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    std::string input("t");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "t";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("tt", chunk.raw());
    EXPECT_EQ("[X]", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("tta", chunk.raw());
    EXPECT_EQ("[X][TA]", chunk.conversion());
    EXPECT_TRUE(chunk.pending().empty());
    EXPECT_TRUE(chunk.ShouldCommit());
  }
}

TEST(CharChunkTest, FlickAndToggle) {
  Table table;
  // Rule for both toggle and flick
  table.AddRuleWithAttributes("2", "", "[KA]", NEW_CHUNK);
  // Rules for toggle
  table.AddRuleWithAttributes("[KA]2", "", "[KI]", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("[KI]2", "", "[KU]", NO_TABLE_ATTRIBUTE);
  // Rules for flick
  table.AddRuleWithAttributes("a", "", "[KI]", END_CHUNK);
  table.AddRuleWithAttributes("b", "", "[KU]", END_CHUNK);

  {  // toggle
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    std::string input("2");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("2", chunk.raw());

    input = "2";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("22", chunk.raw());

    input = "2";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("222", chunk.raw());
  }

  {  // flick#1
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    std::string input("2");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("2", chunk.raw());

    input = "a";
    EXPECT_EQ("a", input);
    EXPECT_EQ("2", chunk.raw());
  }

  {  // flick#2
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    std::string input("a");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("a", chunk.raw());

    input = "b";
    EXPECT_EQ("b", input);
    EXPECT_EQ("a", chunk.raw());
  }

  {  // flick and toggle
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    std::string input("a");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("a", chunk.raw());

    input = "2";
    EXPECT_EQ("2", input);
    EXPECT_EQ("a", chunk.raw());
  }
}

TEST(CharChunkTest, ShouldInsertNewChunk) {
  Table table;
  table.AddRuleWithAttributes("na", "[NA]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("a", "[A]", "", NEW_CHUNK);
  table.AddRuleWithAttributes("ni", "[NI]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("i", "[I]", "", NO_TABLE_ATTRIBUTE);

  CompositionInput input;
  CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

  {
    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));
  }

  {
    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));
  }

  {
    input.set_raw("n");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));
  }

  chunk.AddCompositionInput(&input);
  EXPECT_TRUE(input.Empty());

  {
    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));
  }

  {
    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_TRUE(chunk.ShouldInsertNewChunk(input));
  }

  {
    input.set_raw("i");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));
  }

  {
    input.set_raw("i");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));
  }

  {
    input.set_raw("z");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));
  }

  {
    input.set_raw("z");
    input.set_is_new_input(true);
    EXPECT_TRUE(chunk.ShouldInsertNewChunk(input));
  }
}

TEST(CharChunkTest, AddInputCompositionWithConvertedChar) {
  Table table;
  table.AddRuleWithAttributes("na", "[NA]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("a", "[A]", "", NEW_CHUNK);

  {
    CompositionInput input;
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("a", chunk.raw());
    EXPECT_EQ("[A]", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
  }

  {
    CompositionInput input;
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("a", chunk.raw());
    EXPECT_EQ("[A]", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
  }

  {
    CompositionInput input;
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    input.set_raw("n");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());

    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("na", chunk.raw());
    EXPECT_EQ("[NA]", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
  }

  {
    CompositionInput input;
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

    input.set_raw("n");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());

    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_TRUE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_FALSE(input.Empty());
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());
    EXPECT_EQ("a", input.raw());
  }
}

TEST(CharChunkTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  Table table;
  table.AddRule("ち゛", "ぢ", "");

  CharChunk chunk(Transliterators::FULL_ASCII, &table);
  CompositionInput input;
  input.InitFromRawAndConv("a", "ち", false);
  chunk.AddCompositionInput(&input);

  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  // "ち" can be "ぢ", so it should be appendable.
  EXPECT_TRUE(chunk.IsAppendable(Transliterators::LOCAL, &table));

  {  // The output should be "ａ".
    std::string output;
    chunk.AppendResult(Transliterators::LOCAL, &output);
    EXPECT_EQ("ａ", output);
  }

  // Space input makes the internal state of chunk, but it is not consumed.
  std::string key = " ";
  chunk.AddInput(&key);
  EXPECT_EQ(" ", key);
  EXPECT_TRUE(chunk.IsAppendable(Transliterators::LOCAL, &table));

  {  // The output should be still "ａ".
    std::string output;
    chunk.AppendResult(Transliterators::LOCAL, &output);
    EXPECT_EQ("ａ", output);
  }
}

TEST(CharChunkTest, Issue2209634) {
  // This is a unittest against http://b/2209634
  // "q@" becomes "qた@".
  Table table;
  table.AddRule("q", "", "た");
  table.AddRule("た@", "だ", "");

  CharChunk chunk(Transliterators::HALF_ASCII, &table);

  std::string key = "q@";
  chunk.AddInput(&key);
  EXPECT_TRUE(key.empty());

  std::string output;
  chunk.AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ("q@", output);
}

TEST(CharChunkTest, Issue2819580) {
  // This is an unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  Table table;
  table.AddRule("po", "ぽ", "");
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");
  table.AddRule("ya", "や", "");
  table.AddRule("nya", "にゃ", "");

  // Test for reported situation ("ny").
  // AddInput ver.
  {
    CharChunk chunk(Transliterators::HIRAGANA, &table);

    {
      std::string input("n");
      chunk.AddInput(&input);
    }
    {
      std::string input("y");
      chunk.AddInput(&input);
    }
    {
      std::string result;
      chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
      EXPECT_EQ("ｎｙ", result);
    }

    {
      std::string input("a");
      chunk.AddInput(&input);
    }
    {
      std::string result;
      chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
      EXPECT_EQ("にゃ", result);
    }
  }

  // Test for reported situation (ny) inputs with raw and conversion.
  {
    CharChunk chunk(Transliterators::HIRAGANA, &table);

    {
      std::string input("n");
      chunk.AddInput(&input);
    }
    {
      std::string input("y");
      chunk.AddInput(&input);
    }
    {
      CompositionInput input;
      input.InitFromRawAndConv("a", "a", false);
      chunk.AddCompositionInput(&input);
    }
    {
      std::string result;
      chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
      EXPECT_EQ("にゃ", result);
    }
  }

  // Test for reported situation ("pony").
  {
    CharChunk chunk(Transliterators::HIRAGANA, &table);

    {
      std::string input("p");
      chunk.AddInput(&input);
    }
    {
      std::string input("o");
      chunk.AddInput(&input);
    }
    {
      std::string input("n");
      chunk.AddInput(&input);
    }
    {
      std::string input("y");
      chunk.AddInput(&input);
    }
    {
      std::string result;
      chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
      EXPECT_EQ("ぽｎｙ", result);
    }

    {
      std::string input("a");
      chunk.AddInput(&input);
    }
    {
      std::string result;
      chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
      // "ぽにゃ"
      EXPECT_EQ("ぽにゃ", result);
    }
  }

  // The first input is not contained in the table.
  {
    CharChunk chunk(Transliterators::HIRAGANA, &table);

    {
      std::string input("z");
      chunk.AddInput(&input);
    }
    {
      std::string input("n");
      chunk.AddInput(&input);
    }
    {
      std::string input("y");
      chunk.AddInput(&input);
    }
    {
      std::string result;
      chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
      EXPECT_EQ("ｚｎｙ", result);
    }
  }
}

TEST(CharChunkTest, Issue2990253) {
  // http://b/2990253
  // SplitChunk fails.
  // Ambiguous text is left in rhs CharChunk invalidly.
  Table table;
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");
  table.AddRule("ya", "や", "");
  table.AddRule("nya", "にゃ", "");

  CharChunk chunk(Transliterators::CONVERSION_STRING, &table);

  {
    std::string input("n");
    chunk.AddInput(&input);
  }
  {
    std::string input("y");
    chunk.AddInput(&input);
  }

  std::unique_ptr<CharChunk> left_new_chunk =
      chunk.SplitChunk(Transliterators::HIRAGANA, 1);
  {
    std::string result;
    chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
    EXPECT_EQ("ｙ", result);
  }
}

TEST(CharChunkTest, Combine) {
  {
    CharChunk lhs(Transliterators::CONVERSION_STRING, nullptr);
    CharChunk rhs(Transliterators::CONVERSION_STRING, nullptr);
    lhs.set_ambiguous("LA");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");

    rhs.set_ambiguous("RA");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");

    rhs.Combine(lhs);
    EXPECT_EQ("LARA", rhs.ambiguous());
    EXPECT_EQ("LCRC", rhs.conversion());
    EXPECT_EQ("LPRP", rhs.pending());
    EXPECT_EQ("LRRR", rhs.raw());
  }

  {  // lhs' ambiguous is empty.
    CharChunk lhs(Transliterators::CONVERSION_STRING, nullptr);
    CharChunk rhs(Transliterators::CONVERSION_STRING, nullptr);

    lhs.set_ambiguous("");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");

    rhs.set_ambiguous("RA");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");

    rhs.Combine(lhs);
    EXPECT_EQ("", rhs.ambiguous());
    EXPECT_EQ("LCRC", rhs.conversion());
    EXPECT_EQ("LPRP", rhs.pending());
    EXPECT_EQ("LRRR", rhs.raw());
  }

  {  // rhs' ambiguous is empty.
    CharChunk lhs(Transliterators::CONVERSION_STRING, nullptr);
    CharChunk rhs(Transliterators::CONVERSION_STRING, nullptr);

    lhs.set_ambiguous("LA");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");

    rhs.set_ambiguous("");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");

    rhs.Combine(lhs);
    EXPECT_EQ("LARP", rhs.ambiguous());
    EXPECT_EQ("LCRC", rhs.conversion());
    EXPECT_EQ("LPRP", rhs.pending());
    EXPECT_EQ("LRRR", rhs.raw());
  }
}

TEST(CharChunkTest, IsConvertible) {
  Table table;
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");

  CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
  {
    // If pending is empty, returns false.
    chunk.Clear();
    EXPECT_EQ("", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(Transliterators::HIRAGANA, &table, "n"));
  }
  {
    // If t12r is inconsistent, returns false.
    chunk.Clear();
    chunk.SetTransliterator(Transliterators::HIRAGANA);
    std::string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(Transliterators::FULL_ASCII, &table, "a"));
  }
  {
    // If no entries are found from the table, returns false.
    chunk.Clear();
    chunk.SetTransliterator(Transliterators::HIRAGANA);
    std::string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(Transliterators::HIRAGANA, &table, "x"));
  }
  {
    // If found entry does not consume all of input, returns false.
    chunk.Clear();
    chunk.SetTransliterator(Transliterators::HIRAGANA);
    std::string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(Transliterators::HIRAGANA, &table, "y"));
  }
  {
    // [pending='n'] + [input='a'] is convertible (single combination).
    chunk.Clear();
    chunk.SetTransliterator(Transliterators::HIRAGANA);
    std::string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_TRUE(chunk.IsConvertible(Transliterators::HIRAGANA, &table, "a"));
  }
}

TEST(CharChunkTest, SpecialKeys) {
  Table table;
  table.AddRule("4", "", "[ta]");
  table.AddRule("[to]4", "", "[x]{#1}");
  table.AddRule("[x]{#1}4", "", "[ta]");

  table.AddRule("*", "", "");
  table.AddRule("[tu]*", "", "[x]{#2}");
  table.AddRule("[x]{#2}*", "", "[tu]");

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.set_raw(table.ParseSpecialKey("[x]{#1}4"));
    chunk.set_conversion("");
    chunk.set_pending("[ta]");

    std::string result;
    chunk.AppendResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ("[x]4", result);

    result.clear();
    chunk.AppendTrimedResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ("[x]4", result);

    result.clear();
    chunk.AppendFixedResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ("[x]4", result);

    EXPECT_EQ(4, chunk.GetLength(Transliterators::RAW_STRING));

    result.clear();
    chunk.AppendResult(Transliterators::CONVERSION_STRING, &result);
    EXPECT_EQ("[ta]", result);

    result.clear();
    chunk.AppendTrimedResult(Transliterators::CONVERSION_STRING, &result);
    // Trimed result does not take pending value.
    EXPECT_EQ("", result);

    result.clear();
    chunk.AppendFixedResult(Transliterators::CONVERSION_STRING, &result);
    EXPECT_EQ("[ta]", result);

    EXPECT_EQ(4, chunk.GetLength(Transliterators::CONVERSION_STRING));
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.set_raw("[tu]*");
    chunk.set_conversion("");
    chunk.set_pending(table.ParseSpecialKey("[x]{#2}"));

    std::string result;
    chunk.AppendResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ("[tu]*", result);

    result.clear();
    chunk.AppendTrimedResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ("[tu]*", result);

    result.clear();
    chunk.AppendFixedResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ("[tu]*", result);

    EXPECT_EQ(5, chunk.GetLength(Transliterators::RAW_STRING));

    result.clear();
    chunk.AppendResult(Transliterators::CONVERSION_STRING, &result);
    EXPECT_EQ("[x]", result);

    result.clear();
    chunk.AppendTrimedResult(Transliterators::CONVERSION_STRING, &result);
    // Trimed result does not take pending value.
    EXPECT_EQ("", result);

    result.clear();
    chunk.AppendFixedResult(Transliterators::CONVERSION_STRING, &result);
    EXPECT_EQ("[x]", result);

    EXPECT_EQ(3, chunk.GetLength(Transliterators::CONVERSION_STRING));
  }
}

TEST(CharChunkTest, SplitChunkWithSpecialKeys) {
  Table table;
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, nullptr);
    chunk.set_raw("a");
    chunk.set_conversion(table.ParseSpecialKey("ab{1}cd"));

    std::unique_ptr<CharChunk> left_chunk =
        chunk.SplitChunk(Transliterators::CONVERSION_STRING, 0);
    EXPECT_FALSE(left_chunk);
    EXPECT_EQ(4, chunk.GetLength(Transliterators::CONVERSION_STRING));

    left_chunk = chunk.SplitChunk(Transliterators::CONVERSION_STRING, 4);
    EXPECT_FALSE(left_chunk);
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, nullptr);
    chunk.set_raw("a");
    chunk.set_conversion(table.ParseSpecialKey("ab{1}cd"));

    std::unique_ptr<CharChunk> left_chunk =
        chunk.SplitChunk(Transliterators::CONVERSION_STRING, 1);
    ASSERT_TRUE(left_chunk);
    EXPECT_EQ("a", left_chunk->conversion());
    EXPECT_EQ("bcd", chunk.conversion());
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, nullptr);
    chunk.set_raw("a");
    chunk.set_conversion(table.ParseSpecialKey("ab{1}cd"));

    std::unique_ptr<CharChunk> left_chunk =
        chunk.SplitChunk(Transliterators::CONVERSION_STRING, 2);
    ASSERT_TRUE(left_chunk);
    EXPECT_EQ("ab", left_chunk->conversion());
    EXPECT_EQ("cd", chunk.conversion());
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, nullptr);
    chunk.set_raw("a");
    chunk.set_conversion(table.ParseSpecialKey("ab{1}cd"));

    std::unique_ptr<CharChunk> left_chunk =
        chunk.SplitChunk(Transliterators::CONVERSION_STRING, 3);
    ASSERT_TRUE(left_chunk);
    EXPECT_EQ("abc", left_chunk->conversion());
    EXPECT_EQ("d", chunk.conversion());
  }
}

TEST(CharChunkTest, NoTransliterationAttribute) {
  Table table;
  table.AddRule("ka", "KA", "");
  table.AddRuleWithAttributes("sa", "SA", "", NO_TRANSLITERATION);
  table.AddRuleWithAttributes("kk", "x", "k", NO_TRANSLITERATION);
  table.AddRule("ss", "x", "s");

  {  // "ka" - Default normal behavior.
    CharChunk chunk(Transliterators::RAW_STRING, &table);
    ASSERT_EQ(Transliterators::RAW_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));

    std::string input = "ka";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("KA", chunk.conversion());
    EXPECT_EQ(Transliterators::RAW_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));
  }

  {  // "sa" - kConvT12r is set if NO_TRANSLITERATION is specified
    CharChunk chunk(Transliterators::RAW_STRING, &table);

    std::string input = "sa";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("SA", chunk.conversion());
    EXPECT_EQ(Transliterators::CONVERSION_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));
  }

  {  // "s" + "a" - Same with the above.
    CharChunk chunk(Transliterators::RAW_STRING, &table);

    std::string input = "s";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(chunk.conversion().empty());
    EXPECT_EQ(Transliterators::RAW_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("SA", chunk.conversion());
    EXPECT_EQ(Transliterators::CONVERSION_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));
  }

  {  // "kka" - The first attribute (NO_TRANSLITERATION) is used.
    CharChunk chunk(Transliterators::RAW_STRING, &table);

    std::string input = "kk";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("x", chunk.conversion());
    EXPECT_EQ(Transliterators::CONVERSION_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("xKA", chunk.conversion());
    EXPECT_EQ(Transliterators::CONVERSION_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));
  }

  {  // "ssa" - The first attribute (default behavior) is used.
    CharChunk chunk(Transliterators::RAW_STRING, &table);

    std::string input = "ss";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("x", chunk.conversion());
    EXPECT_EQ(Transliterators::RAW_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("xSA", chunk.conversion());
    EXPECT_EQ(Transliterators::RAW_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));
  }
}

TEST(CharChunkTest, NoTransliterationAttributeForInputAndConvertedChar) {
  Table table;
  table.AddRuleWithAttributes("[ka]@", "[ga]", "", NO_TRANSLITERATION);
  table.AddRuleWithAttributes("[sa]", "[sa]", "", NO_TRANSLITERATION);
  table.AddRule("[sa]@", "[za]", "");

  {  // "KA" - Default normal behavior.
    CharChunk chunk(Transliterators::RAW_STRING, &table);
    ASSERT_EQ(Transliterators::RAW_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));

    CompositionInput input;
    input.InitFromRawAndConv("t", "[ka]", false);
    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.raw().empty());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_EQ("[ka]", chunk.pending());
    EXPECT_EQ(Transliterators::RAW_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));

    // "GA" - The first attribute (default behavior) is used.
    input.InitFromRawAndConv("!", "@", false);
    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.raw().empty());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_EQ("t!", chunk.raw());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("[ga]", chunk.conversion());
    EXPECT_EQ(Transliterators::RAW_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));
  }

  {  // "SA" - kConvT12r is set if NO_TRANSLITERATION is specified.
    CharChunk chunk(Transliterators::RAW_STRING, &table);

    CompositionInput input;
    input.InitFromRawAndConv("x", "[sa]", false);
    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.raw().empty());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_EQ("x", chunk.raw());
    EXPECT_EQ("[sa]", chunk.pending());
    EXPECT_EQ(Transliterators::CONVERSION_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));

    // "ZA" - The first attribute (NO_TRANSLITERATION) is used.
    input.InitFromRawAndConv("!", "@", false);
    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.raw().empty());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_EQ("x!", chunk.raw());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("[za]", chunk.conversion());
    EXPECT_EQ(Transliterators::CONVERSION_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));
  }
}

namespace {
bool HasResult(const std::set<std::string> &results, const std::string &value) {
  return (results.find(value) != results.end());
}
}  // namespace

TEST(CharChunkTest, RomanGetExpandedResults) {
  Table table;
  table.AddRule("kya", "きゃ", "");
  table.AddRule("kyi", "きぃ", "");
  table.AddRule("kyu", "きゅ", "");
  table.AddRule("kye", "きぇ", "");
  table.AddRule("kyo", "きょ", "");
  table.AddRule("kk", "っ", "k");
  table.AddRule("ka", "か", "");
  table.AddRule("ki", "き", "");
  table.AddRule("ku", "く", "");
  table.AddRule("ke", "け", "");
  table.AddRule("ko", "こ", "");

  {
    std::string input = "ka";  // AddInputInternal requires non-const string
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("か", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(0, results.size());  // no ambiguity
  }
  {
    std::string input = "k";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(12, results.size());
    EXPECT_TRUE(HasResult(results, "k"));
    EXPECT_TRUE(HasResult(results, "か"));    // ka
    EXPECT_TRUE(HasResult(results, "き"));    // ki
    EXPECT_TRUE(HasResult(results, "きゃ"));  // kya
    EXPECT_TRUE(HasResult(results, "きぃ"));  // kyi
    EXPECT_TRUE(HasResult(results, "きゅ"));  // kyu
    EXPECT_TRUE(HasResult(results, "きぇ"));  // kye
    EXPECT_TRUE(HasResult(results, "きょ"));  // kyo
    EXPECT_TRUE(HasResult(results, "く"));    // ku
    EXPECT_TRUE(HasResult(results, "け"));    // ke
    EXPECT_TRUE(HasResult(results, "こ"));    // ko
    EXPECT_TRUE(HasResult(results, "っ"));    // kk
  }
  {
    std::string input = "ky";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(6, results.size());
    EXPECT_TRUE(HasResult(results, "ky"));
    EXPECT_TRUE(HasResult(results, "きゃ"));
    EXPECT_TRUE(HasResult(results, "きぃ"));
    EXPECT_TRUE(HasResult(results, "きゅ"));
    EXPECT_TRUE(HasResult(results, "きぇ"));
    EXPECT_TRUE(HasResult(results, "きょ"));
  }
  {
    std::string input = "kk";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("っ", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(11, results.size());
    EXPECT_TRUE(HasResult(results, "か"));    // ka
    EXPECT_TRUE(HasResult(results, "き"));    // ki
    EXPECT_TRUE(HasResult(results, "きゃ"));  // kya
    EXPECT_TRUE(HasResult(results, "きぃ"));  // kyi
    EXPECT_TRUE(HasResult(results, "きゅ"));  // kyu
    EXPECT_TRUE(HasResult(results, "きぇ"));  // kye
    EXPECT_TRUE(HasResult(results, "きょ"));  // kyo
    EXPECT_TRUE(HasResult(results, "く"));    // ku
    EXPECT_TRUE(HasResult(results, "け"));    // ke
    EXPECT_TRUE(HasResult(results, "こ"));    // ko
    EXPECT_TRUE(HasResult(results, "っ"));    // kk
  }
}

TEST(CharChunkTest, KanaGetExpandedResults) {
  Table table;
  table.AddRule("か゛", "が", "");
  table.AddRule("は゛", "ば", "");
  table.AddRule("は゜", "ぱ", "");

  {
    // AddInputInternal requires non-const string
    std::string input = "か";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(HasResult(results, "か"));
    EXPECT_TRUE(HasResult(results, "が"));
  }
  {
    std::string input = "は";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(3, results.size());
    EXPECT_TRUE(HasResult(results, "は"));
    EXPECT_TRUE(HasResult(results, "ば"));
    EXPECT_TRUE(HasResult(results, "ぱ"));
  }
}

TEST(CharChunkTest, 12KeyGetExpandedResults) {
  Table table;
  // It's not the test for the table, but use the real table file
  // for checking it's functionality.
  table.LoadFromFile("system://12keys-hiragana.tsv");

  {
    std::string input = "1";  // AddInputInternal requires non-const string
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(HasResult(results, "あ"));
    EXPECT_TRUE(HasResult(results, "ぁ"));
  }
  {
    std::string input = "8";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(HasResult(results, "や"));
    EXPECT_TRUE(HasResult(results, "ゃ"));
  }
  {
    std::string input = "や8";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(HasResult(results, "ゆ"));
    EXPECT_TRUE(HasResult(results, "ゅ"));
  }
  {
    std::string input = "6";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(3, results.size());
    EXPECT_TRUE(HasResult(results, "は"));
    EXPECT_TRUE(HasResult(results, "ば"));
    EXPECT_TRUE(HasResult(results, "ぱ"));
  }
}

TEST(CharChunkTest, FlickGetExpandedResults) {
  Table table;
  // It's not the test for the table, but use the real table file
  // for checking it's functionality.
  table.LoadFromFile("system://flick-hiragana.tsv");

  {
    std::string input = "1";  // AddInputInternal requires non-const string
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(HasResult(results, "あ"));
    EXPECT_TRUE(HasResult(results, "ぁ"));
  }
  {
    std::string input = "8";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(HasResult(results, "や"));
    EXPECT_TRUE(HasResult(results, "ゃ"));
  }
  {
    std::string input = "u";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(2, results.size());
    EXPECT_TRUE(HasResult(results, "ゆ"));
    EXPECT_TRUE(HasResult(results, "ゅ"));
  }
  {
    std::string input = "6";
    CharChunk chunk(Transliterators::CONVERSION_STRING, &table);
    chunk.AddInputInternal(&input);

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ("", base);

    std::set<std::string> results;
    chunk.GetExpandedResults(&results);
    EXPECT_EQ(3, results.size());
    EXPECT_TRUE(HasResult(results, "は"));
    EXPECT_TRUE(HasResult(results, "ば"));
    EXPECT_TRUE(HasResult(results, "ぱ"));
  }
}

TEST(CharChunkTest, NoTransliteration_Issue3497962) {
  Table table;
  table.AddRuleWithAttributes("2", "", "a", NEW_CHUNK | NO_TRANSLITERATION);
  table.AddRuleWithAttributes("a2", "", "b", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("b2", "", "c", NO_TABLE_ATTRIBUTE);

  CharChunk chunk(Transliterators::HIRAGANA, &table);

  std::string input = "2";
  chunk.AddInput(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_EQ("2", chunk.raw());
  EXPECT_EQ("", chunk.conversion());
  EXPECT_EQ("a", chunk.pending());
  EXPECT_EQ(Transliterators::CONVERSION_STRING,
            chunk.GetTransliterator(Transliterators::LOCAL));

  input = "2";
  chunk.AddInput(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_EQ("22", chunk.raw());
  EXPECT_EQ("", chunk.conversion());
  EXPECT_EQ("b", chunk.pending());
  EXPECT_EQ(Transliterators::CONVERSION_STRING,
            chunk.GetTransliterator(Transliterators::LOCAL));
}

TEST(CharChunkTest, Copy) {
  const Table table;
  CharChunk src(Transliterators::HIRAGANA, &table);
  src.set_raw("raw");
  src.set_conversion("conversion");
  src.set_pending("pending");
  src.set_ambiguous("ambiguous");
  src.set_attributes(NEW_CHUNK);

  const CharChunk copy(src);
  EXPECT_EQ(src.transliterator(), copy.transliterator());
  EXPECT_TRUE(src.table() == copy.table());
  EXPECT_EQ(src.raw(), copy.raw());
  EXPECT_EQ(src.conversion(), copy.conversion());
  EXPECT_EQ(src.pending(), copy.pending());
  EXPECT_EQ(src.ambiguous(), copy.ambiguous());
  EXPECT_EQ(src.attributes(), copy.attributes());

  CharChunk assigned(Transliterators::HALF_ASCII, nullptr);
  assigned = src;
  EXPECT_EQ(src.transliterator(), assigned.transliterator());
  EXPECT_TRUE(src.table() == assigned.table());
  EXPECT_EQ(src.raw(), assigned.raw());
  EXPECT_EQ(src.conversion(), assigned.conversion());
  EXPECT_EQ(src.pending(), assigned.pending());
  EXPECT_EQ(src.ambiguous(), assigned.ambiguous());
  EXPECT_EQ(src.attributes(), assigned.attributes());
}

TEST(CharChunkTest, GetTransliterator) {
  Table table;

  // Non-local vs non-local.
  // Given parametes should be returned.
  for (size_t i = 0; i < Transliterators::NUM_OF_TRANSLITERATOR; ++i) {
    Transliterators::Transliterator t12r_1 =
        static_cast<Transliterators::Transliterator>(i);
    if (t12r_1 == Transliterators::LOCAL) {
      continue;
    }
    CharChunk chunk(t12r_1, &table);
    for (size_t j = 0; j < Transliterators::NUM_OF_TRANSLITERATOR; ++j) {
      Transliterators::Transliterator t12r_2 =
          static_cast<Transliterators::Transliterator>(j);
      if (t12r_2 == Transliterators::LOCAL) {
        continue;
      }
      EXPECT_EQ(t12r_2, chunk.GetTransliterator(t12r_2));
    }
  }

  // Non-local vs local.
  // Constructor parameter should be returned.
  for (size_t i = 0; i < Transliterators::NUM_OF_TRANSLITERATOR; ++i) {
    Transliterators::Transliterator t12r =
        static_cast<Transliterators::Transliterator>(i);
    if (t12r == Transliterators::LOCAL) {
      continue;
    }
    CharChunk chunk(t12r, &table);
    EXPECT_EQ(t12r, chunk.GetTransliterator(Transliterators::LOCAL));
  }

  // Non-local (with NO_TRANSLITERATION attr) vs local.
  // If NO_TRANSLITERATION is set, returns CONVERSION_STRING always.
  for (size_t i = 0; i < Transliterators::NUM_OF_TRANSLITERATOR; ++i) {
    Transliterators::Transliterator t12r =
        static_cast<Transliterators::Transliterator>(i);
    if (t12r == Transliterators::LOCAL) {
      continue;
    }
    CharChunk chunk(t12r, &table);
    chunk.set_attributes(NO_TRANSLITERATION);
    EXPECT_EQ(Transliterators::CONVERSION_STRING,
              chunk.GetTransliterator(Transliterators::LOCAL));
  }
}

}  // namespace composer
}  // namespace mozc
