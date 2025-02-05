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

#include <cstddef>
#include <string>
#include <utility>

#include "absl/container/btree_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators.h"
#include "composer/table.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace composer {

MATCHER(Loop, "") { return arg.first; }
MATCHER(NoLoop, "") { return !arg.first; }
MATCHER_P(RestIs, rest, "") { return arg.second == rest; }
MATCHER(RestIsEmpty, "") { return arg.second.empty(); }

TEST(CharChunkTest, AddInput_CharByChar) {
  // Test against http://b/1547858
  Table table;
  table.AddRule("i", "い", "");
  table.AddRule("tt", "っ", "t");
  table.AddRule("ta", "た", "");

  CharChunk chunk1(Transliterators::CONVERSION_STRING, table);
  std::pair<bool, absl::string_view> result = chunk1.AddInputInternal("i");
  EXPECT_THAT(result, NoLoop());
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ(chunk1.raw(), "i");
  EXPECT_EQ(chunk1.conversion(), "い");
  EXPECT_EQ(chunk1.pending(), "");
  EXPECT_THAT(result, RestIsEmpty());

  CharChunk chunk2(Transliterators::CONVERSION_STRING, table);
  result = chunk2.AddInputInternal("t");
  EXPECT_THAT(result, NoLoop());
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ(chunk2.raw(), "t");
  EXPECT_EQ(chunk2.conversion(), "");
  EXPECT_EQ(chunk2.pending(), "t");
  EXPECT_THAT(result, RestIsEmpty());

  result = chunk2.AddInputInternal("t");
  EXPECT_THAT(result, NoLoop());
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ(chunk2.raw(), "tt");
  EXPECT_EQ(chunk2.conversion(), "っ");
  EXPECT_EQ(chunk2.pending(), "t");
  EXPECT_THAT(result, RestIsEmpty());

  result = chunk2.AddInputInternal("a");
  EXPECT_THAT(result, NoLoop());
  EXPECT_TRUE(chunk2.IsFixed());
  EXPECT_EQ(chunk2.raw(), "tta");
  EXPECT_EQ(chunk2.conversion(), "った");
  EXPECT_EQ(chunk2.pending(), "");
  EXPECT_THAT(result, RestIsEmpty());
}

TEST(CharChunkTest, AddInput_NoEffectInput) {
  Table table;
  table.AddRule("2", "", "<*>2");
  table.AddRule("<*>1", "", "1");
  table.AddRule("*", "", "");

  CharChunk chunk1(Transliterators::CONVERSION_STRING, table);
  std::pair<bool, absl::string_view> result = chunk1.AddInputInternal("2");
  EXPECT_THAT(result, NoLoop());
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ(chunk1.raw(), "2");
  EXPECT_EQ(chunk1.conversion(), "");
  EXPECT_EQ(chunk1.pending(), "<*>2");
  EXPECT_THAT(result, RestIsEmpty());

  // "<*>2*" is used as a query but no such entry is in the table.
  // Thus AddInputInternal() should not consume the input.
  result = chunk1.AddInputInternal("*");
  EXPECT_THAT(result, NoLoop());
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ(chunk1.raw(), "2");
  EXPECT_EQ(chunk1.conversion(), "");
  EXPECT_EQ(chunk1.pending(), "<*>2");
  EXPECT_THAT(result, RestIs("*"));
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

  CharChunk chunk1(Transliterators::CONVERSION_STRING, table);
  std::pair<bool, absl::string_view> result = chunk1.AddInputInternal("n");
  EXPECT_THAT(result, NoLoop());
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ(chunk1.raw(), "n");
  EXPECT_EQ(chunk1.conversion(), "");
  EXPECT_EQ(chunk1.pending(), "n");
  EXPECT_THAT(result, RestIsEmpty());

  result = chunk1.AddInputInternal("y");
  EXPECT_THAT(result, NoLoop());
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ(chunk1.raw(), "ny");
  EXPECT_EQ(chunk1.conversion(), "");
  EXPECT_EQ(chunk1.pending(), "ny");
  EXPECT_THAT(result, RestIsEmpty());

  result = chunk1.AddInputInternal("n");
  EXPECT_THAT(result, NoLoop());
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ(chunk1.raw(), "ny");
  EXPECT_EQ(chunk1.conversion(), "");
  EXPECT_EQ(chunk1.pending(), "ny");
  EXPECT_THAT(result, RestIs("n"));

  result = chunk1.AddInputInternal("a");
  EXPECT_THAT(result, NoLoop());
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ(chunk1.raw(), "nya");
  EXPECT_EQ(chunk1.conversion(), "[NYA]");
  EXPECT_EQ(chunk1.pending(), "");
  EXPECT_THAT(result, RestIsEmpty());
}

TEST(CharChunkTest, AddInput_WithString) {
  // Test against http://b/1547858
  Table table;
  table.AddRule("i", "い", "");
  table.AddRule("tt", "っ", "t");
  table.AddRule("ta", "た", "");

  CharChunk chunk1(Transliterators::CONVERSION_STRING, table);
  std::pair<bool, absl::string_view> result = chunk1.AddInputInternal("itta");
  EXPECT_THAT(result, NoLoop());
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ(chunk1.raw(), "i");
  EXPECT_EQ(chunk1.conversion(), "い");
  EXPECT_EQ(chunk1.pending(), "");
  EXPECT_THAT(result, RestIs("tta"));

  CharChunk chunk2(Transliterators::CONVERSION_STRING, table);
  result = chunk2.AddInputInternal(result.second);
  EXPECT_THAT(result, Loop());
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ(chunk2.raw(), "tt");
  EXPECT_EQ(chunk2.conversion(), "っ");
  EXPECT_EQ(chunk2.pending(), "t");
  EXPECT_THAT(result, RestIs("a"));

  result = chunk2.AddInputInternal(result.second);
  EXPECT_THAT(result, NoLoop());
  EXPECT_TRUE(chunk2.IsFixed());
  EXPECT_EQ(chunk2.raw(), "tta");
  EXPECT_EQ(chunk2.conversion(), "った");
  EXPECT_EQ(chunk2.pending(), "");
  EXPECT_THAT(result, RestIsEmpty());
}

TEST(CharChunkTest, AddInput_EmptyOutput) {
  // Test against http://b/289217346
  Table table;
  table.AddRule("a", "", "");
  table.AddRuleWithAttributes("b", "", "", NO_TRANSLITERATION);
  table.AddRuleWithAttributes("c", "", "", NEW_CHUNK | NO_TRANSLITERATION);

  CharChunk chunk_a(Transliterators::CONVERSION_STRING, table);
  std::pair<bool, absl::string_view> result_a = chunk_a.AddInputInternal("a");
  EXPECT_TRUE(result_a.second.empty());
  EXPECT_EQ(chunk_a.raw(), "a");

  CharChunk chunk_b(Transliterators::CONVERSION_STRING, table);
  std::pair<bool, absl::string_view> result_b = chunk_b.AddInputInternal("b");
  EXPECT_TRUE(result_b.second.empty());
  EXPECT_TRUE(chunk_b.raw().empty());

  CharChunk chunk_c(Transliterators::CONVERSION_STRING, table);
  std::pair<bool, absl::string_view> result_c = chunk_c.AddInputInternal("c");
  EXPECT_TRUE(result_c.second.empty());
  EXPECT_TRUE(chunk_c.raw().empty());
}

TEST(CharChunkTest, GetLength) {
  CharChunk chunk1(Transliterators::CONVERSION_STRING);
  chunk1.set_conversion("ね");
  chunk1.set_pending("");
  chunk1.set_raw("ne");
  EXPECT_EQ(chunk1.GetLength(Transliterators::CONVERSION_STRING), 1);
  EXPECT_EQ(chunk1.GetLength(Transliterators::RAW_STRING), 2);

  CharChunk chunk2(Transliterators::CONVERSION_STRING);
  chunk2.set_conversion("っと");
  chunk2.set_pending("");
  chunk2.set_raw("tto");
  EXPECT_EQ(chunk2.GetLength(Transliterators::CONVERSION_STRING), 2);
  EXPECT_EQ(chunk2.GetLength(Transliterators::RAW_STRING), 3);

  CharChunk chunk3(Transliterators::CONVERSION_STRING);
  chunk3.set_conversion("が");
  chunk3.set_pending("");
  chunk3.set_raw("ga");
  EXPECT_EQ(chunk3.GetLength(Transliterators::CONVERSION_STRING), 1);
  EXPECT_EQ(chunk3.GetLength(Transliterators::RAW_STRING), 2);

  chunk3.SetTransliterator(Transliterators::HALF_KATAKANA);
  EXPECT_EQ(chunk3.GetLength(Transliterators::HALF_KATAKANA), 2);
  chunk3.SetTransliterator(Transliterators::HALF_ASCII);
  EXPECT_EQ(chunk3.GetLength(Transliterators::HALF_ASCII), 2);
}

TEST(CharChunkTest, AddCompositionInput) {
  Table table;
  table.AddRule("す゛", "ず", "");

  CharChunk chunk1(Transliterators::CONVERSION_STRING, table);
  CompositionInput input;
  input.InitFromRawAndConv("m", "も", false);
  chunk1.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ(chunk1.raw(), "m");
  EXPECT_EQ(chunk1.pending(), "も");
  EXPECT_TRUE(chunk1.conversion().empty());

  input.InitFromRawAndConv("r", "す", false);
  chunk1.AddCompositionInput(&input);
  // The input values are not used.
  EXPECT_EQ(input.raw(), "r");
  EXPECT_EQ(input.conversion(), "す");
  // The chunk remains the previous value.
  EXPECT_EQ(chunk1.raw(), "m");
  EXPECT_EQ(chunk1.pending(), "も");
  EXPECT_TRUE(chunk1.conversion().empty());

  CharChunk chunk2(Transliterators::CONVERSION_STRING, table);
  // raw == "r", conversion == "す";
  chunk2.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ(chunk2.raw(), "r");
  EXPECT_EQ(chunk2.pending(), "す");
  EXPECT_TRUE(chunk2.conversion().empty());

  input.InitFromRawAndConv("@", "゛", false);
  chunk2.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ(chunk2.raw(), "r@");
  EXPECT_TRUE(chunk2.pending().empty());
  EXPECT_EQ(chunk2.conversion(), "ず");

  input.InitFromRawAndConv("h", "く", false);
  chunk2.AddCompositionInput(&input);
  // The input values are not used.
  EXPECT_EQ(input.raw(), "h");
  EXPECT_EQ(input.conversion(), "く");
  // The chunk remains the previous value.
  EXPECT_EQ(chunk2.raw(), "r@");
  EXPECT_TRUE(chunk2.pending().empty());
  EXPECT_EQ(chunk2.conversion(), "ず");
}

TEST(CharChunkTest, AddCompositionInputWithHalfAscii) {
  Table table;
  table.AddRule("-", "ー", "");

  CharChunk chunk1(Transliterators::CONVERSION_STRING, table);
  CompositionInput input;
  input.InitFromRawAndConv("-", "-", false);
  chunk1.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ(chunk1.raw(), "-");
  EXPECT_EQ(chunk1.pending(), "-");
  EXPECT_TRUE(chunk1.conversion().empty());

  input.InitFromRawAndConv("-", "-", false);
  chunk1.AddCompositionInput(&input);
  // The input values are not used.
  EXPECT_EQ(input.raw(), "-");
  EXPECT_EQ(input.conversion(), "-");
  // The chunk remains the previous value.
  EXPECT_EQ(chunk1.raw(), "-");
  EXPECT_EQ(chunk1.pending(), "-");
  EXPECT_TRUE(chunk1.conversion().empty());

  CharChunk chunk2(Transliterators::CONVERSION_STRING, table);
  // key == "-", value == "-";
  chunk2.AddCompositionInput(&input);
  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  EXPECT_EQ(chunk2.raw(), "-");
  EXPECT_EQ(chunk2.pending(), "-");
  EXPECT_TRUE(chunk2.conversion().empty());
}

TEST(CharChunkTest, OutputMode) {
  Table table;
  table.AddRule("a", "あ", "");

  CharChunk chunk(Transliterators::CONVERSION_STRING, table);
  chunk.AddInputInternal("a");

  std::string result;
  chunk.AppendResult(Transliterators::LOCAL, &result);
  EXPECT_EQ(result, "あ");

  chunk.SetTransliterator(Transliterators::FULL_KATAKANA);
  result.clear();
  chunk.AppendResult(Transliterators::LOCAL, &result);
  EXPECT_EQ(result, "ア");

  chunk.SetTransliterator(Transliterators::HALF_ASCII);
  result.clear();
  chunk.AppendResult(Transliterators::LOCAL, &result);
  EXPECT_EQ(result, "a");

  result.clear();
  chunk.AppendResult(Transliterators::HALF_KATAKANA, &result);
  EXPECT_EQ(result, "ｱ");
}

TEST(CharChunkTest, SplitChunk) {
  Table table;
  table.AddRule("mo", "も", "");

  CharChunk chunk(Transliterators::HIRAGANA, table);

  EXPECT_THAT(chunk.AddInputInternal("m"), RestIsEmpty());

  std::string output;
  chunk.AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ(output, "ｍ");

  EXPECT_THAT(chunk.AddInputInternal("o"), RestIsEmpty());

  output.clear();
  chunk.AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ(output, "も");

  chunk.SetTransliterator(Transliterators::HALF_ASCII);
  output.clear();
  chunk.AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ(output, "mo");

  // Split "mo" to "m" and "o".
  absl::StatusOr<CharChunk> left_chunk =
      chunk.SplitChunk(Transliterators::LOCAL, 1);
  ASSERT_OK(left_chunk);

  // The output should be half width "m" rather than full width "ｍ".
  output.clear();
  left_chunk->AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ(output, "m");
}

TEST(CharChunkTest, IsAppendable) {
  Table table;
  table.AddRule("mo", "も", "");
  Table table_another;

  CharChunk chunk(Transliterators::HIRAGANA, table);

  EXPECT_THAT(chunk.AddInputInternal("m"), RestIsEmpty());
  EXPECT_TRUE(chunk.IsAppendable(Transliterators::LOCAL, table));
  EXPECT_TRUE(chunk.IsAppendable(Transliterators::HIRAGANA, table));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::FULL_KATAKANA, table));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::LOCAL, table_another));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::HIRAGANA, table_another));

  EXPECT_THAT(chunk.AddInputInternal("o"), RestIsEmpty());
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::LOCAL, table));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::HIRAGANA, table));
  EXPECT_FALSE(chunk.IsAppendable(Transliterators::FULL_KATAKANA, table));
}

TEST(CharChunkTest, AddInputInternal) {
  Table table;
  table.AddRule("tt", "っ", "t");

  CharChunk chunk(Transliterators::CONVERSION_STRING, table);
  {
    EXPECT_THAT(chunk.AddInputInternal("t"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "t");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "t");
  }
  {
    EXPECT_THAT(chunk.AddInputInternal("t"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "tt");
    EXPECT_EQ(chunk.conversion(), "っ");
    EXPECT_EQ(chunk.pending(), "t");
  }
  {
    EXPECT_THAT(chunk.AddInputInternal("t"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "ttt");
    EXPECT_EQ(chunk.conversion(), "っっ");
    EXPECT_EQ(chunk.pending(), "t");
  }
  {
    EXPECT_THAT(chunk.AddInputInternal("!"), RestIs("!"));
    EXPECT_EQ(chunk.raw(), "ttt");
    EXPECT_EQ(chunk.conversion(), "っっ");
    EXPECT_EQ(chunk.pending(), "t");
  }
}

TEST(CharChunkTest, AddInputInternalDifferentPending) {
  Table table;
  table.AddRule("1", "", "あ");
  table.AddRule("あ*", "", "ぁ");

  CharChunk chunk(Transliterators::CONVERSION_STRING, table);
  {
    EXPECT_THAT(chunk.AddInputInternal("1"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "1");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "あ");
  }
  {
    EXPECT_THAT(chunk.AddInputInternal("*"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "1*");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "ぁ");
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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    EXPECT_THAT(chunk.AddInputInternal("n"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "n");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_EQ(chunk.ambiguous(), "ん");

    EXPECT_THAT(chunk.AddInputInternal("a"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "na");
    EXPECT_EQ(chunk.conversion(), "な");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.ambiguous(), "");
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    EXPECT_THAT(chunk.AddInputInternal("n"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "n");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_EQ(chunk.ambiguous(), "ん");

    EXPECT_THAT(chunk.AddInputInternal("y"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "ny");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "ny");
    EXPECT_EQ(chunk.ambiguous(), "");

    EXPECT_THAT(chunk.AddInputInternal("a"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "nya");
    EXPECT_EQ(chunk.conversion(), "にゃ");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.ambiguous(), "");
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    EXPECT_THAT(chunk.AddInputInternal("nya"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "nya");
    EXPECT_EQ(chunk.conversion(), "にゃ");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.ambiguous(), "");
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    EXPECT_THAT(chunk.AddInputInternal("n"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "n");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_EQ(chunk.ambiguous(), "ん");

    EXPECT_THAT(chunk.AddInputInternal("k"), RestIs("k"));
    EXPECT_EQ(chunk.raw(), "n");
    EXPECT_EQ(chunk.conversion(), "ん");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.ambiguous(), "");
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    EXPECT_THAT(chunk.AddInputInternal("nk"), RestIs("k"));
    EXPECT_EQ(chunk.raw(), "n");
    EXPECT_EQ(chunk.conversion(), "ん");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.ambiguous(), "");
  }
}

TEST(CharChunkTest, AddInputInternalWithAttributes) {
  Table table;
  table.AddRuleWithAttributes("1", "", "あ", NO_TRANSLITERATION);
  table.AddRule("あ*", "", "ぁ");

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    EXPECT_THAT(chunk.AddInputInternal("1"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "1");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "あ");
    EXPECT_EQ(chunk.attributes(), NO_TRANSLITERATION);

    EXPECT_THAT(chunk.AddInputInternal("*"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "1*");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "ぁ");
    EXPECT_EQ(chunk.attributes(), NO_TRANSLITERATION);
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    EXPECT_THAT(chunk.AddInputInternal("1*"), RestIs("*"));
    EXPECT_EQ(chunk.raw(), "1");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "あ");
    EXPECT_EQ(chunk.attributes(), NO_TRANSLITERATION);

    EXPECT_THAT(chunk.AddInputInternal("*"), RestIsEmpty());
    EXPECT_EQ(chunk.raw(), "1*");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "ぁ");
    EXPECT_EQ(chunk.attributes(), NO_TRANSLITERATION);
  }

  Table table2;
  table2.AddRuleWithAttributes("n", "ん", "", NO_TRANSLITERATION);
  table2.AddRuleWithAttributes("na", "な", "", DIRECT_INPUT);

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table2);

    chunk.AddInputInternal("n");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_EQ(chunk.ambiguous(), "ん");
    EXPECT_EQ(chunk.attributes(), NO_TRANSLITERATION);

    chunk.AddInputInternal("a");
    EXPECT_EQ(chunk.conversion(), "な");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.ambiguous(), "");
    EXPECT_EQ(chunk.attributes(), DIRECT_INPUT);
  }
}

TEST(CharChunkTest, CaseSensitive) {
  Table table;
  table.AddRule("ka", "[ka]", "");
  CharChunk chunk(Transliterators::CONVERSION_STRING, table);

  EXPECT_THAT(chunk.AddInputInternal("Ka"), RestIsEmpty());
  EXPECT_EQ(chunk.raw(), "Ka");
  EXPECT_EQ(chunk.conversion(), "[ka]");
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
    CharChunk chunk(Transliterators::HIRAGANA, table);
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
    CharChunk chunk(Transliterators::HIRAGANA, table);
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
    CharChunk chunk(Transliterators::HIRAGANA, table);
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
    CharChunk chunk(Transliterators::HIRAGANA, table);
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

  CharChunk chunk(Transliterators::HIRAGANA, table);
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

  CharChunk chunk(Transliterators::HIRAGANA, table);
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

  CharChunk chunk(Transliterators::HIRAGANA, table);

  {
    std::string input("ssh");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "ssh");
    EXPECT_EQ(chunk.conversion(), "っ");
    EXPECT_EQ(chunk.pending(), "sh");
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ(chunk.ambiguous(), "");
  }
  {
    std::string result;
    chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
    EXPECT_EQ(result, "っｓｈ");
  }

  // Break down of the internal procedures
  chunk.Clear();
  chunk.SetTransliterator(Transliterators::HIRAGANA);
  {
    std::string input("s");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "s");
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "s");
    EXPECT_EQ(chunk.ambiguous(), "");
  }
  {
    std::string input("s");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "ss");
    EXPECT_EQ(chunk.conversion(), "っ");
    EXPECT_EQ(chunk.pending(), "s");
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ(chunk.ambiguous(), "");
  }
  {
    std::string input("h");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "ssh");
    EXPECT_EQ(chunk.conversion(), "っ");
    EXPECT_EQ(chunk.pending(), "sh");
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ(chunk.ambiguous(), "");
  }
}

TEST(CharChunkTest, ShouldCommit) {
  Table table;
  table.AddRuleWithAttributes("ka", "[KA]", "", DIRECT_INPUT);
  table.AddRuleWithAttributes("tt", "[X]", "t", DIRECT_INPUT);
  table.AddRuleWithAttributes("ta", "[TA]", "", NO_TABLE_ATTRIBUTE);

  {  // ka - DIRECT_INPUT
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    std::string input("k");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "k");
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "ka");
    EXPECT_EQ(chunk.conversion(), "[KA]");
    EXPECT_TRUE(chunk.ShouldCommit());
  }

  {  // ta - NO_TABLE_ATTRIBUTE
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    std::string input("t");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "t");
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "ta");
    EXPECT_EQ(chunk.conversion(), "[TA]");
    EXPECT_FALSE(chunk.ShouldCommit());
  }

  {  // tta - (tt: DIRECT_INPUT / ta: NO_TABLE_ATTRIBUTE)
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    std::string input("t");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "t");
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "t";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "tt");
    EXPECT_EQ(chunk.conversion(), "[X]");
    EXPECT_EQ(chunk.pending(), "t");
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "tta");
    EXPECT_EQ(chunk.conversion(), "[X][TA]");
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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    std::string input("2");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "2");

    input = "2";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "22");

    input = "2";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "222");
  }

  {  // flick#1
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    std::string input("2");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "2");

    input = "a";
    EXPECT_EQ(input, "a");
    EXPECT_EQ(chunk.raw(), "2");
  }

  {  // flick#2
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    std::string input("a");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "a");

    input = "b";
    EXPECT_EQ(input, "b");
    EXPECT_EQ(chunk.raw(), "a");
  }

  {  // flick and toggle
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    std::string input("a");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.raw(), "a");

    input = "2";
    EXPECT_EQ(input, "2");
    EXPECT_EQ(chunk.raw(), "a");
  }
}

TEST(CharChunkTest, ShouldInsertNewChunk) {
  Table table;
  table.AddRuleWithAttributes("na", "[NA]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("a", "[A]", "", NEW_CHUNK);
  table.AddRuleWithAttributes("ni", "[NI]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("i", "[I]", "", NO_TABLE_ATTRIBUTE);

  CompositionInput input;
  CharChunk chunk(Transliterators::CONVERSION_STRING, table);

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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ(chunk.raw(), "a");
    EXPECT_EQ(chunk.conversion(), "[A]");
    EXPECT_EQ(chunk.pending(), "");
  }

  {
    CompositionInput input;
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ(chunk.raw(), "a");
    EXPECT_EQ(chunk.conversion(), "[A]");
    EXPECT_EQ(chunk.pending(), "");
  }

  {
    CompositionInput input;
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    input.set_raw("n");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ(chunk.raw(), "n");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "n");

    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ(chunk.raw(), "na");
    EXPECT_EQ(chunk.conversion(), "[NA]");
    EXPECT_EQ(chunk.pending(), "");
  }

  {
    CompositionInput input;
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);

    input.set_raw("n");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ(chunk.raw(), "n");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "n");

    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_TRUE(chunk.ShouldInsertNewChunk(input));

    chunk.AddCompositionInput(&input);
    EXPECT_FALSE(input.Empty());
    EXPECT_EQ(chunk.raw(), "n");
    EXPECT_EQ(chunk.conversion(), "");
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_EQ(input.raw(), "a");
  }
}

TEST(CharChunkTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  Table table;
  table.AddRule("ち゛", "ぢ", "");

  CharChunk chunk(Transliterators::FULL_ASCII, table);
  CompositionInput input;
  input.InitFromRawAndConv("a", "ち", false);
  chunk.AddCompositionInput(&input);

  EXPECT_TRUE(input.raw().empty());
  EXPECT_TRUE(input.conversion().empty());
  // "ち" can be "ぢ", so it should be appendable.
  EXPECT_TRUE(chunk.IsAppendable(Transliterators::LOCAL, table));

  {  // The output should be "ａ".
    std::string output;
    chunk.AppendResult(Transliterators::LOCAL, &output);
    EXPECT_EQ(output, "ａ");
  }

  // Space input makes the internal state of chunk, but it is not consumed.
  std::string key = " ";
  chunk.AddInput(&key);
  EXPECT_EQ(key, " ");
  EXPECT_TRUE(chunk.IsAppendable(Transliterators::LOCAL, table));

  {  // The output should be still "ａ".
    std::string output;
    chunk.AppendResult(Transliterators::LOCAL, &output);
    EXPECT_EQ(output, "ａ");
  }
}

TEST(CharChunkTest, Issue2209634) {
  // This is a unittest against http://b/2209634
  // "q@" becomes "qた@".
  Table table;
  table.AddRule("q", "", "た");
  table.AddRule("た@", "だ", "");

  CharChunk chunk(Transliterators::HALF_ASCII, table);

  std::string key = "q@";
  chunk.AddInput(&key);
  EXPECT_TRUE(key.empty());

  std::string output;
  chunk.AppendResult(Transliterators::LOCAL, &output);
  EXPECT_EQ(output, "q@");
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
    CharChunk chunk(Transliterators::HIRAGANA, table);

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
      EXPECT_EQ(result, "ｎｙ");
    }

    {
      std::string input("a");
      chunk.AddInput(&input);
    }
    {
      std::string result;
      chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
      EXPECT_EQ(result, "にゃ");
    }
  }

  // Test for reported situation (ny) inputs with raw and conversion.
  {
    CharChunk chunk(Transliterators::HIRAGANA, table);

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
      EXPECT_EQ(result, "にゃ");
    }
  }

  // Test for reported situation ("pony").
  {
    CharChunk chunk(Transliterators::HIRAGANA, table);

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
      EXPECT_EQ(result, "ぽｎｙ");
    }

    {
      std::string input("a");
      chunk.AddInput(&input);
    }
    {
      std::string result;
      chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
      // "ぽにゃ"
      EXPECT_EQ(result, "ぽにゃ");
    }
  }

  // The first input is not contained in the table.
  {
    CharChunk chunk(Transliterators::HIRAGANA, table);

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
      EXPECT_EQ(result, "ｚｎｙ");
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

  CharChunk chunk(Transliterators::CONVERSION_STRING, table);

  {
    std::string input("n");
    chunk.AddInput(&input);
  }
  {
    std::string input("y");
    chunk.AddInput(&input);
  }

  absl::StatusOr<CharChunk> left_new_chunk =
      chunk.SplitChunk(Transliterators::HIRAGANA, 1);
  {
    std::string result;
    chunk.AppendFixedResult(Transliterators::HIRAGANA, &result);
    EXPECT_EQ(result, "ｙ");
  }
}

TEST(CharChunkTest, Combine) {
  {
    CharChunk lhs(Transliterators::CONVERSION_STRING);
    CharChunk rhs(Transliterators::CONVERSION_STRING);
    lhs.set_ambiguous("LA");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");

    rhs.set_ambiguous("RA");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");

    rhs.Combine(lhs);
    EXPECT_EQ(rhs.ambiguous(), "LARA");
    EXPECT_EQ(rhs.conversion(), "LCRC");
    EXPECT_EQ(rhs.pending(), "LPRP");
    EXPECT_EQ(rhs.raw(), "LRRR");
  }

  {  // lhs' ambiguous is empty.
    CharChunk lhs(Transliterators::CONVERSION_STRING);
    CharChunk rhs(Transliterators::CONVERSION_STRING);

    lhs.set_ambiguous("");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");

    rhs.set_ambiguous("RA");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");

    rhs.Combine(lhs);
    EXPECT_EQ(rhs.ambiguous(), "");
    EXPECT_EQ(rhs.conversion(), "LCRC");
    EXPECT_EQ(rhs.pending(), "LPRP");
    EXPECT_EQ(rhs.raw(), "LRRR");
  }

  {  // rhs' ambiguous is empty.
    CharChunk lhs(Transliterators::CONVERSION_STRING);
    CharChunk rhs(Transliterators::CONVERSION_STRING);

    lhs.set_ambiguous("LA");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");

    rhs.set_ambiguous("");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");

    rhs.Combine(lhs);
    EXPECT_EQ(rhs.ambiguous(), "LARP");
    EXPECT_EQ(rhs.conversion(), "LCRC");
    EXPECT_EQ(rhs.pending(), "LPRP");
    EXPECT_EQ(rhs.raw(), "LRRR");
  }
}

TEST(CharChunkTest, IsConvertible) {
  Table table;
  table.AddRule("n", "ん", "");
  table.AddRule("na", "な", "");

  CharChunk chunk(Transliterators::CONVERSION_STRING, table);
  {
    // If pending is empty, returns false.
    chunk.Clear();
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_FALSE(chunk.IsConvertible(Transliterators::HIRAGANA, table, "n"));
  }
  {
    // If t12r is inconsistent, returns false.
    chunk.Clear();
    chunk.SetTransliterator(Transliterators::HIRAGANA);
    std::string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_FALSE(chunk.IsConvertible(Transliterators::FULL_ASCII, table, "a"));
  }
  {
    // If no entries are found from the table, returns false.
    chunk.Clear();
    chunk.SetTransliterator(Transliterators::HIRAGANA);
    std::string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_FALSE(chunk.IsConvertible(Transliterators::HIRAGANA, table, "x"));
  }
  {
    // If found entry does not consume all of input, returns false.
    chunk.Clear();
    chunk.SetTransliterator(Transliterators::HIRAGANA);
    std::string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_FALSE(chunk.IsConvertible(Transliterators::HIRAGANA, table, "y"));
  }
  {
    // [pending='n'] + [input='a'] is convertible (single combination).
    chunk.Clear();
    chunk.SetTransliterator(Transliterators::HIRAGANA);
    std::string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ(chunk.pending(), "n");
    EXPECT_TRUE(chunk.IsConvertible(Transliterators::HIRAGANA, table, "a"));
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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.set_raw(table.ParseSpecialKey("[x]{#1}4"));
    chunk.set_conversion("");
    chunk.set_pending("[ta]");

    std::string result;
    chunk.AppendResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ(result, "[x]4");

    result.clear();
    chunk.AppendTrimedResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ(result, "[x]4");

    result.clear();
    chunk.AppendFixedResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ(result, "[x]4");

    EXPECT_EQ(chunk.GetLength(Transliterators::RAW_STRING), 4);

    result.clear();
    chunk.AppendResult(Transliterators::CONVERSION_STRING, &result);
    EXPECT_EQ(result, "[ta]");

    result.clear();
    chunk.AppendTrimedResult(Transliterators::CONVERSION_STRING, &result);
    // Trimed result does not take pending value.
    EXPECT_EQ(result, "");

    result.clear();
    chunk.AppendFixedResult(Transliterators::CONVERSION_STRING, &result);
    EXPECT_EQ(result, "[ta]");

    EXPECT_EQ(chunk.GetLength(Transliterators::CONVERSION_STRING), 4);
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.set_raw("[tu]*");
    chunk.set_conversion("");
    chunk.set_pending(table.ParseSpecialKey("[x]{#2}"));

    std::string result;
    chunk.AppendResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ(result, "[tu]*");

    result.clear();
    chunk.AppendTrimedResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ(result, "[tu]*");

    result.clear();
    chunk.AppendFixedResult(Transliterators::RAW_STRING, &result);
    EXPECT_EQ(result, "[tu]*");

    EXPECT_EQ(chunk.GetLength(Transliterators::RAW_STRING), 5);

    result.clear();
    chunk.AppendResult(Transliterators::CONVERSION_STRING, &result);
    EXPECT_EQ(result, "[x]");

    result.clear();
    chunk.AppendTrimedResult(Transliterators::CONVERSION_STRING, &result);
    // Trimed result does not take pending value.
    EXPECT_EQ(result, "");

    result.clear();
    chunk.AppendFixedResult(Transliterators::CONVERSION_STRING, &result);
    EXPECT_EQ(result, "[x]");

    EXPECT_EQ(chunk.GetLength(Transliterators::CONVERSION_STRING), 3);
  }
}

TEST(CharChunkTest, SplitChunkWithSpecialKeys) {
  Table table;
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.set_raw("a");
    chunk.set_conversion(table.ParseSpecialKey("ab{1}cd"));

    absl::StatusOr<CharChunk> left_chunk =
        chunk.SplitChunk(Transliterators::CONVERSION_STRING, 0);
    EXPECT_FALSE(left_chunk.ok());
    EXPECT_EQ(chunk.GetLength(Transliterators::CONVERSION_STRING), 4);

    left_chunk = chunk.SplitChunk(Transliterators::CONVERSION_STRING, 4);
    EXPECT_FALSE(left_chunk.ok());
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.set_raw("a");
    chunk.set_conversion(table.ParseSpecialKey("ab{1}cd"));

    absl::StatusOr<CharChunk> left_chunk =
        chunk.SplitChunk(Transliterators::CONVERSION_STRING, 1);
    ASSERT_OK(left_chunk);
    EXPECT_EQ(left_chunk->conversion(), "a");
    EXPECT_EQ(chunk.conversion(), "bcd");
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.set_raw("a");
    chunk.set_conversion(table.ParseSpecialKey("ab{1}cd"));

    absl::StatusOr<CharChunk> left_chunk =
        chunk.SplitChunk(Transliterators::CONVERSION_STRING, 2);
    ASSERT_OK(left_chunk);
    EXPECT_EQ(left_chunk->conversion(), "ab");
    EXPECT_EQ(chunk.conversion(), "cd");
  }

  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.set_raw("a");
    chunk.set_conversion(table.ParseSpecialKey("ab{1}cd"));

    absl::StatusOr<CharChunk> left_chunk =
        chunk.SplitChunk(Transliterators::CONVERSION_STRING, 3);
    ASSERT_OK(left_chunk);
    EXPECT_EQ(left_chunk->conversion(), "abc");
    EXPECT_EQ(chunk.conversion(), "d");
  }
}

TEST(CharChunkTest, NoTransliterationAttribute) {
  Table table;
  table.AddRule("ka", "KA", "");
  table.AddRuleWithAttributes("sa", "SA", "", NO_TRANSLITERATION);
  table.AddRuleWithAttributes("kk", "x", "k", NO_TRANSLITERATION);
  table.AddRule("ss", "x", "s");

  {  // "ka" - Default normal behavior.
    CharChunk chunk(Transliterators::RAW_STRING, table);
    ASSERT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::RAW_STRING);

    std::string input = "ka";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.conversion(), "KA");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::RAW_STRING);
  }

  {  // "sa" - kConvT12r is set if NO_TRANSLITERATION is specified
    CharChunk chunk(Transliterators::RAW_STRING, table);

    std::string input = "sa";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.conversion(), "SA");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::CONVERSION_STRING);
  }

  {  // "s" + "a" - Same with the above.
    CharChunk chunk(Transliterators::RAW_STRING, table);

    std::string input = "s";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(chunk.conversion().empty());
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::RAW_STRING);

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.conversion(), "SA");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::CONVERSION_STRING);
  }

  {  // "kka" - The first attribute (NO_TRANSLITERATION) is used.
    CharChunk chunk(Transliterators::RAW_STRING, table);

    std::string input = "kk";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.conversion(), "x");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::CONVERSION_STRING);

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.conversion(), "xKA");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::CONVERSION_STRING);
  }

  {  // "ssa" - The first attribute (default behavior) is used.
    CharChunk chunk(Transliterators::RAW_STRING, table);

    std::string input = "ss";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.conversion(), "x");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::RAW_STRING);

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ(chunk.conversion(), "xSA");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::RAW_STRING);
  }
}

TEST(CharChunkTest, NoTransliterationAttributeForInputAndConvertedChar) {
  Table table;
  table.AddRuleWithAttributes("[ka]@", "[ga]", "", NO_TRANSLITERATION);
  table.AddRuleWithAttributes("[sa]", "[sa]", "", NO_TRANSLITERATION);
  table.AddRule("[sa]@", "[za]", "");

  {  // "KA" - Default normal behavior.
    CharChunk chunk(Transliterators::RAW_STRING, table);
    ASSERT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::RAW_STRING);

    CompositionInput input;
    input.InitFromRawAndConv("t", "[ka]", false);
    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.raw().empty());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_EQ(chunk.raw(), "t");
    EXPECT_EQ(chunk.pending(), "[ka]");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::RAW_STRING);

    // "GA" - The first attribute (default behavior) is used.
    input.InitFromRawAndConv("!", "@", false);
    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.raw().empty());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_EQ(chunk.raw(), "t!");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.conversion(), "[ga]");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::RAW_STRING);
  }

  {  // "SA" - kConvT12r is set if NO_TRANSLITERATION is specified.
    CharChunk chunk(Transliterators::RAW_STRING, table);

    CompositionInput input;
    input.InitFromRawAndConv("x", "[sa]", false);
    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.raw().empty());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_EQ(chunk.raw(), "x");
    EXPECT_EQ(chunk.pending(), "[sa]");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::CONVERSION_STRING);

    // "ZA" - The first attribute (NO_TRANSLITERATION) is used.
    input.InitFromRawAndConv("!", "@", false);
    chunk.AddCompositionInput(&input);
    EXPECT_TRUE(input.raw().empty());
    EXPECT_TRUE(input.conversion().empty());
    EXPECT_EQ(chunk.raw(), "x!");
    EXPECT_EQ(chunk.pending(), "");
    EXPECT_EQ(chunk.conversion(), "[za]");
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::CONVERSION_STRING);
  }
}

namespace {
bool HasResult(const absl::btree_set<std::string> &results,
               const std::string &value) {
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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("ka");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "か");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 0);  // no ambiguity
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("k");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 12);
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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("ky");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 6);
    EXPECT_TRUE(HasResult(results, "ky"));
    EXPECT_TRUE(HasResult(results, "きゃ"));
    EXPECT_TRUE(HasResult(results, "きぃ"));
    EXPECT_TRUE(HasResult(results, "きゅ"));
    EXPECT_TRUE(HasResult(results, "きぇ"));
    EXPECT_TRUE(HasResult(results, "きょ"));
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("kk");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "っ");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 11);
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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("か");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(HasResult(results, "か"));
    EXPECT_TRUE(HasResult(results, "が"));
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("は");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 3);
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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("1");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(HasResult(results, "あ"));
    EXPECT_TRUE(HasResult(results, "ぁ"));
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("8");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(HasResult(results, "や"));
    EXPECT_TRUE(HasResult(results, "ゃ"));
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("や8");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(HasResult(results, "ゆ"));
    EXPECT_TRUE(HasResult(results, "ゅ"));
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("6");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 3);
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
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("1");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(HasResult(results, "あ"));
    EXPECT_TRUE(HasResult(results, "ぁ"));
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("8");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(HasResult(results, "や"));
    EXPECT_TRUE(HasResult(results, "ゃ"));
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("u");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 2);
    EXPECT_TRUE(HasResult(results, "ゆ"));
    EXPECT_TRUE(HasResult(results, "ゅ"));
  }
  {
    CharChunk chunk(Transliterators::CONVERSION_STRING, table);
    chunk.AddInputInternal("6");

    std::string base;
    chunk.AppendTrimedResult(Transliterators::LOCAL, &base);
    EXPECT_EQ(base, "");

    const absl::btree_set<std::string> results = chunk.GetExpandedResults();
    EXPECT_EQ(results.size(), 3);
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

  CharChunk chunk(Transliterators::HIRAGANA, table);

  std::string input = "2";
  chunk.AddInput(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_EQ(chunk.raw(), "2");
  EXPECT_EQ(chunk.conversion(), "");
  EXPECT_EQ(chunk.pending(), "a");
  EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
            Transliterators::CONVERSION_STRING);

  input = "2";
  chunk.AddInput(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_EQ(chunk.raw(), "22");
  EXPECT_EQ(chunk.conversion(), "");
  EXPECT_EQ(chunk.pending(), "b");
  EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
            Transliterators::CONVERSION_STRING);
}

TEST(CharChunkTest, Copy) {
  const Table table;
  CharChunk src(Transliterators::HIRAGANA, table);
  src.set_raw("raw");
  src.set_conversion("conversion");
  src.set_pending("pending");
  src.set_ambiguous("ambiguous");
  src.set_attributes(NEW_CHUNK);

  const CharChunk copy(src);
  EXPECT_EQ(copy.transliterator(), src.transliterator());
  EXPECT_TRUE(src.table() == copy.table());
  EXPECT_EQ(copy.raw(), src.raw());
  EXPECT_EQ(copy.conversion(), src.conversion());
  EXPECT_EQ(copy.pending(), src.pending());
  EXPECT_EQ(copy.ambiguous(), src.ambiguous());
  EXPECT_EQ(copy.attributes(), src.attributes());

  CharChunk assigned(Transliterators::HALF_ASCII);
  assigned = src;
  EXPECT_EQ(assigned.transliterator(), src.transliterator());
  EXPECT_TRUE(src.table() == assigned.table());
  EXPECT_EQ(assigned.raw(), src.raw());
  EXPECT_EQ(assigned.conversion(), src.conversion());
  EXPECT_EQ(assigned.pending(), src.pending());
  EXPECT_EQ(assigned.ambiguous(), src.ambiguous());
  EXPECT_EQ(assigned.attributes(), src.attributes());
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
    CharChunk chunk(t12r_1, table);
    for (size_t j = 0; j < Transliterators::NUM_OF_TRANSLITERATOR; ++j) {
      Transliterators::Transliterator t12r_2 =
          static_cast<Transliterators::Transliterator>(j);
      if (t12r_2 == Transliterators::LOCAL) {
        continue;
      }
      EXPECT_EQ(chunk.GetTransliterator(t12r_2), t12r_2);
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
    CharChunk chunk(t12r, table);
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL), t12r);
  }

  // Non-local (with NO_TRANSLITERATION attr) vs local.
  // If NO_TRANSLITERATION is set, returns CONVERSION_STRING always.
  for (size_t i = 0; i < Transliterators::NUM_OF_TRANSLITERATOR; ++i) {
    Transliterators::Transliterator t12r =
        static_cast<Transliterators::Transliterator>(i);
    if (t12r == Transliterators::LOCAL) {
      continue;
    }
    CharChunk chunk(t12r, table);
    chunk.set_attributes(NO_TRANSLITERATION);
    EXPECT_EQ(chunk.GetTransliterator(Transliterators::LOCAL),
              Transliterators::CONVERSION_STRING);
  }
}

}  // namespace composer
}  // namespace mozc
