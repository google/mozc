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

#include "composer/internal/char_chunk.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/transliterators_ja.h"
#include "composer/table.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace composer {

namespace {
const TransliteratorInterface *kNullT12r = NULL;
const TransliteratorInterface *kRawT12r =
    TransliteratorsJa::GetRawStringSelector();
const TransliteratorInterface *kConvT12r =
    TransliteratorsJa::GetConversionStringSelector();
const TransliteratorInterface *kHalfKatakanaT12r =
    TransliteratorsJa::GetHalfKatakanaTransliterator();
const TransliteratorInterface *kFullKatakanaT12r =
    TransliteratorsJa::GetFullKatakanaTransliterator();
const TransliteratorInterface *kHalfAsciiT12r =
    TransliteratorsJa::GetHalfAsciiTransliterator();
const TransliteratorInterface *kFullAsciiT12r =
    TransliteratorsJa::GetFullAsciiTransliterator();
const TransliteratorInterface *kHiraganaT12r =
    TransliteratorsJa::GetHiraganaTransliterator();
const TransliteratorInterface *kDefaultT12r =
    Transliterators::GetConversionStringSelector();
}  // anonymous namespace

TEST(CharChunkTest, AddInput_CharByChar) {
  // Test against http://b/1547858
  Table table;
  // "い"
  table.AddRule("i", "\xe3\x81\x84", "");
  // "っ"
  table.AddRule("tt", "\xe3\x81\xa3", "t");
  // "た"
  table.AddRule("ta", "\xe3\x81\x9f", "");

  string input;

  CharChunk chunk1;
  input = "i";
  EXPECT_FALSE(chunk1.AddInputInternal(table, &input));
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ("i", chunk1.raw());
  // "い"
  EXPECT_EQ("\xe3\x81\x84", chunk1.conversion());
  EXPECT_EQ("", chunk1.pending());
  EXPECT_EQ("", input);

  CharChunk chunk2;
  input = "t";
  EXPECT_FALSE(chunk2.AddInputInternal(table, &input));
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ("t", chunk2.raw());
  EXPECT_EQ("", chunk2.conversion());
  EXPECT_EQ("t", chunk2.pending());
  EXPECT_EQ("", input);

  input = "t";
  EXPECT_FALSE(chunk2.AddInputInternal(table, &input));
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ("tt", chunk2.raw());
  // "っ"
  EXPECT_EQ("\xe3\x81\xa3", chunk2.conversion());
  EXPECT_EQ("t", chunk2.pending());
  EXPECT_EQ("", input);

  input = "a";
  EXPECT_FALSE(chunk2.AddInputInternal(table, &input));
  EXPECT_TRUE(chunk2.IsFixed());
  EXPECT_EQ("tta", chunk2.raw());
  // "った"
  EXPECT_EQ("\xe3\x81\xa3\xe3\x81\x9f", chunk2.conversion());
  EXPECT_EQ("", chunk2.pending());
  EXPECT_EQ("", input);
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

  string input;

  CharChunk chunk1;
  input = "n";
  EXPECT_FALSE(chunk1.AddInputInternal(table, &input));
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ("n", chunk1.raw());
  EXPECT_EQ("", chunk1.conversion());
  EXPECT_EQ("n", chunk1.pending());
  EXPECT_EQ("", input);

  input = "y";
  EXPECT_FALSE(chunk1.AddInputInternal(table, &input));
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ("ny", chunk1.raw());
  EXPECT_EQ("", chunk1.conversion());
  EXPECT_EQ("ny", chunk1.pending());
  EXPECT_EQ("", input);

  input = "n";
  EXPECT_FALSE(chunk1.AddInputInternal(table, &input));
  EXPECT_FALSE(chunk1.IsFixed());
  EXPECT_EQ("ny", chunk1.raw());
  EXPECT_EQ("", chunk1.conversion());
  EXPECT_EQ("ny", chunk1.pending());
  EXPECT_EQ("n", input);

  input = "a";
  EXPECT_FALSE(chunk1.AddInputInternal(table, &input));
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ("nya", chunk1.raw());
  EXPECT_EQ("[NYA]", chunk1.conversion());
  EXPECT_EQ("", chunk1.pending());
  EXPECT_EQ("", input);
}

TEST(CharChunkTest, AddInput_WithString) {
  // Test against http://b/1547858
  Table table;
  // "い"
  table.AddRule("i", "\xe3\x81\x84", "");
  // "っ"
  table.AddRule("tt", "\xe3\x81\xa3", "t");
  // "た"
  table.AddRule("ta", "\xe3\x81\x9f", "");

  string input = "itta";

  CharChunk chunk1;
  EXPECT_FALSE(chunk1.AddInputInternal(table, &input));
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ("i", chunk1.raw());
  // "い"
  EXPECT_EQ("\xe3\x81\x84", chunk1.conversion());
  EXPECT_EQ("", chunk1.pending());
  EXPECT_EQ("tta", input);

  CharChunk chunk2;
  EXPECT_TRUE(chunk2.AddInputInternal(table, &input));
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ("tt", chunk2.raw());
  // "っ"
  EXPECT_EQ("\xe3\x81\xa3", chunk2.conversion());
  EXPECT_EQ("t", chunk2.pending());
  EXPECT_EQ("a", input);

  EXPECT_FALSE(chunk2.AddInputInternal(table, &input));
  EXPECT_TRUE(chunk2.IsFixed());
  EXPECT_EQ("tta", chunk2.raw());
  // "った"
  EXPECT_EQ("\xe3\x81\xa3\xe3\x81\x9f", chunk2.conversion());
  EXPECT_EQ("", chunk2.pending());
  EXPECT_EQ("", input);
}

TEST(CharChunkTest, GetLength) {
  CharChunk chunk1;
  // "ね"
  chunk1.set_conversion("\xe3\x81\xad");
  chunk1.set_pending("");
  chunk1.set_raw("ne");
  EXPECT_EQ(1, chunk1.GetLength(kConvT12r));
  EXPECT_EQ(2, chunk1.GetLength(kRawT12r));

  CharChunk chunk2;
  // "っと"
  chunk2.set_conversion("\xe3\x81\xa3\xe3\x81\xa8");
  chunk2.set_pending("");
  chunk2.set_raw("tto");
  EXPECT_EQ(2, chunk2.GetLength(kConvT12r));
  EXPECT_EQ(3, chunk2.GetLength(kRawT12r));

  CharChunk chunk3;
  // "が"
  chunk3.set_conversion("\xE3\x81\x8C");
  chunk3.set_pending("");
  chunk3.set_raw("ga");
  EXPECT_EQ(1, chunk3.GetLength(kConvT12r));
  EXPECT_EQ(2, chunk3.GetLength(kRawT12r));

  chunk3.SetTransliterator(kHalfKatakanaT12r);
  EXPECT_EQ(2, chunk3.GetLength(kHalfKatakanaT12r));
  chunk3.SetTransliterator(kHalfAsciiT12r);
  EXPECT_EQ(2, chunk3.GetLength(kHalfAsciiT12r));
}

TEST(CharChunkTest, AddInputAndConvertedChar) {
  Table table;
  // "す゛, ず"
  table.AddRule("\xe3\x81\x99\xe3\x82\x9b", "\xe3\x81\x9a", "");

  CharChunk chunk1;
  string key = "m";
  // "も"
  string value = "\xe3\x82\x82";
  chunk1.AddInputAndConvertedChar(table, &key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("m", chunk1.raw());
  // "も"
  EXPECT_EQ("\xe3\x82\x82", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  key = "r";
  // "す"
  value = "\xe3\x81\x99";
  chunk1.AddInputAndConvertedChar(table, &key, &value);
  // The input values are not used.
  EXPECT_EQ("r", key);
  // "す"
  EXPECT_EQ("\xe3\x81\x99", value);
  // The chunk remains the previous value.
  EXPECT_EQ("m", chunk1.raw());
  // "も"
  EXPECT_EQ("\xe3\x82\x82", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  CharChunk chunk2;
  // key == "r", value == "す";
  chunk2.AddInputAndConvertedChar(table, &key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("r", chunk2.raw());
  // "す"
  EXPECT_EQ("\xe3\x81\x99", chunk2.pending());
  EXPECT_TRUE(chunk2.conversion().empty());

  key = "@";
  // "゛"
  value = "\xe3\x82\x9b";
  chunk2.AddInputAndConvertedChar(table, &key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("r@", chunk2.raw());
  EXPECT_TRUE(chunk2.pending().empty());
  // "ず"
  EXPECT_EQ("\xe3\x81\x9a", chunk2.conversion());

  key = "h";
  // "く"
  value = "\xe3\x81\x8f";
  chunk2.AddInputAndConvertedChar(table, &key, &value);
  // The input values are not used.
  EXPECT_EQ("h", key);
  // "く"
  EXPECT_EQ("\xe3\x81\x8f", value);
  // The chunk remains the previous value.
  EXPECT_EQ("r@", chunk2.raw());
  EXPECT_TRUE(chunk2.pending().empty());
  // "ず"
  EXPECT_EQ("\xe3\x81\x9a", chunk2.conversion());
}

TEST(CharChunkTest, AddInputAndConvertedCharWithHalfAscii) {
  Table table;
  // "ー"
  table.AddRule("-", "\xE3\x83\xBC", "");

  CharChunk chunk1;
  string key = "-";
  string value = "-";
  chunk1.AddInputAndConvertedChar(table, &key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("-", chunk1.raw());
  EXPECT_EQ("-", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  key = "-";
  value = "-";
  chunk1.AddInputAndConvertedChar(table, &key, &value);
  // The input values are not used.
  EXPECT_EQ("-", key);
  EXPECT_EQ("-", value);
  // The chunk remains the previous value.
  EXPECT_EQ("-", chunk1.raw());
  EXPECT_EQ("-", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  CharChunk chunk2;
  // key == "-", value == "-";
  chunk2.AddInputAndConvertedChar(table, &key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("-", chunk2.raw());
  EXPECT_EQ("-", chunk2.pending());
  EXPECT_TRUE(chunk2.conversion().empty());
}

TEST(CharChunkTest, OutputMode) {
  Table table;
  // "あ"
  table.AddRule("a", "\xE3\x81\x82", "");

  CharChunk chunk;
  string input = "a";
  chunk.AddInputInternal(table, &input);

  string result;
  chunk.AppendResult(table, kNullT12r, &result);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", result);

  chunk.SetTransliterator(kFullKatakanaT12r);
  result.clear();
  chunk.AppendResult(table, kNullT12r, &result);
  // "ア"
  EXPECT_EQ("\xE3\x82\xA2", result);

  chunk.SetTransliterator(kHalfAsciiT12r);
  result.clear();
  chunk.AppendResult(table, kNullT12r, &result);
  EXPECT_EQ("a", result);

  result.clear();
  chunk.AppendResult(table, kHalfKatakanaT12r, &result);
  // "ｱ"
  EXPECT_EQ("\xEF\xBD\xB1", result);
}

TEST(CharChunkTest, SplitChunk) {
  Table table;
  // "も"
  table.AddRule("mo", "\xE3\x82\x82", "");

  CharChunk chunk;
  chunk.SetTransliterator(kHiraganaT12r);

  string input = "m";
  chunk.AddInputInternal(table, &input);
  EXPECT_TRUE(input.empty());

  string output;
  chunk.AppendResult(table, kNullT12r, &output);
  // "ｍ"
  EXPECT_EQ("\xEF\xBD\x8D", output);

  input = "o";
  chunk.AddInputInternal(table, &input);
  EXPECT_TRUE(input.empty());

  output.clear();
  chunk.AppendResult(table, kNullT12r, &output);
  // "も"
  EXPECT_EQ("\xE3\x82\x82", output);

  chunk.SetTransliterator(kHalfAsciiT12r);
  output.clear();
  chunk.AppendResult(table, kNullT12r, &output);
  EXPECT_EQ("mo", output);

  // Split "mo" to "m" and "o".
  CharChunk left_chunk;
  chunk.SplitChunk(kNullT12r, 1, &left_chunk);

  // The output should be half width "m" rather than full width "ｍ".
  output.clear();
  left_chunk.AppendResult(table, kNullT12r, &output);
  EXPECT_EQ("m", output);
}

TEST(CharChunkTest, IsAppendable) {
  Table table;
  // "も"
  table.AddRule("mo", "\xE3\x82\x82", "");

  CharChunk chunk;
  chunk.SetTransliterator(kHiraganaT12r);

  string input = "m";
  chunk.AddInputInternal(table, &input);
  EXPECT_TRUE(input.empty());
  EXPECT_TRUE(chunk.IsAppendable(kNullT12r));
  EXPECT_TRUE(chunk.IsAppendable(kHiraganaT12r));
  EXPECT_FALSE(chunk.IsAppendable(kFullKatakanaT12r));

  input = "o";
  chunk.AddInputInternal(table, &input);
  EXPECT_TRUE(input.empty());
  EXPECT_FALSE(chunk.IsAppendable(kNullT12r));
  EXPECT_FALSE(chunk.IsAppendable(kHiraganaT12r));
  EXPECT_FALSE(chunk.IsAppendable(kFullKatakanaT12r));
}

TEST(CharChunkTest, AddInputInternal) {
  Table table;
  // "っ"
  table.AddRule("tt", "\xE3\x81\xA3", "t");

  CharChunk chunk;
  {
    string key = "t";
    chunk.AddInputInternal(table, &key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    string key = "t";
    chunk.AddInputInternal(table, &key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("tt", chunk.raw());
    // "っ"
    EXPECT_EQ("\xE3\x81\xA3", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    string key = "t";
    chunk.AddInputInternal(table, &key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("ttt", chunk.raw());
    // "っっ"
    EXPECT_EQ("\xE3\x81\xA3\xE3\x81\xA3", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    string key = "!";
    chunk.AddInputInternal(table, &key);
    EXPECT_EQ("!", key);
    EXPECT_EQ("ttt", chunk.raw());
    // "っっ"
    EXPECT_EQ("\xE3\x81\xA3\xE3\x81\xA3", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
}

TEST(CharChunkTest, CaseSensitive) {
  Table table;
  table.AddRule("ka", "[ka]", "");
  CharChunk chunk;

  string key = "Ka";
  chunk.AddInputInternal(table, &key);
  EXPECT_TRUE(key.empty());
  EXPECT_EQ("Ka", chunk.raw());
  EXPECT_EQ("[ka]", chunk.conversion());
  EXPECT_TRUE(chunk.pending().empty());
}

TEST(CharChunkTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  Table table;
  // "っ"
  table.AddRule("ss", "\xE3\x81\xA3", "s");
  // "し"
  table.AddRule("shi", "\xE3\x81\x97", "");

  CharChunk chunk;
  chunk.SetTransliterator(kHiraganaT12r);

  {
    string input("ssh");
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ssh", chunk.raw());
    // "っ"
    EXPECT_EQ("\xE3\x81\xA3", chunk.conversion());
    EXPECT_EQ("sh", chunk.pending());
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ("", chunk.ambiguous());
  }
  {
    string result;
    chunk.AppendFixedResult(table, kHiraganaT12r, &result);
    // "っｓｈ"
    EXPECT_EQ("\xE3\x81\xA3\xEF\xBD\x93\xEF\xBD\x88", result);
  }

  // Break down of the internal procedures
  chunk.Clear();
  chunk.SetTransliterator(kHiraganaT12r);
  {
    string input("s");
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("s", chunk.raw());
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("s", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
  }
  {
    string input("s");
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ss", chunk.raw());
    // "っ"
    EXPECT_EQ("\xE3\x81\xA3", chunk.conversion());
    EXPECT_EQ("s", chunk.pending());
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ("", chunk.ambiguous());
  }
  {
    string input("h");
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ssh", chunk.raw());
    // "っ"
    EXPECT_EQ("\xE3\x81\xA3", chunk.conversion());
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
    CharChunk chunk;
    string input("k");
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("k", chunk.raw());
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ka", chunk.raw());
    EXPECT_EQ("[KA]", chunk.conversion());
    EXPECT_TRUE(chunk.ShouldCommit());
  }

  {  // ta - NO_TABLE_ATTRIBUTE
    CharChunk chunk;
    string input("t");
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("ta", chunk.raw());
    EXPECT_EQ("[TA]", chunk.conversion());
    EXPECT_FALSE(chunk.ShouldCommit());
  }

  {  // tta - (tt: DIRECT_INPUT / ta: NO_TABLE_ATTRIBUTE)
    CharChunk chunk;
    string input("t");
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "t";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("tt", chunk.raw());
    EXPECT_EQ("[X]", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
    EXPECT_FALSE(chunk.ShouldCommit());

    input = "a";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("tta", chunk.raw());
    EXPECT_EQ("[X][TA]", chunk.conversion());
    EXPECT_TRUE(chunk.pending().empty());
    EXPECT_TRUE(chunk.ShouldCommit());
  }
}


TEST(CharChunkTest, ShouldInsertNewChunk) {
  Table table;
  table.AddRuleWithAttributes("na", "[NA]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("a", "[A]", "", NEW_CHUNK);
  table.AddRuleWithAttributes("ni", "[NI]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("i", "[I]", "", NO_TABLE_ATTRIBUTE);

  CompositionInput input;
  CharChunk chunk;

  {
    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));
  }

  {
    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));
  }

  {
    input.set_raw("n");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));
  }

  chunk.AddCompositionInput(table, &input);
  EXPECT_TRUE(input.Empty());

  {
    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));
  }

  {
    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_TRUE(chunk.ShouldInsertNewChunk(table, input));
  }

  {
    input.set_raw("i");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));
  }

  {
    input.set_raw("i");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));
  }

  {
    input.set_raw("z");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));
  }

  {
    input.set_raw("z");
    input.set_is_new_input(true);
    EXPECT_TRUE(chunk.ShouldInsertNewChunk(table, input));
  }
}

TEST(CharChunkTest, AddCompositionInput) {
  Table table;
  table.AddRuleWithAttributes("na", "[NA]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("a", "[A]", "", NEW_CHUNK);

  {
    CompositionInput input;
    CharChunk chunk;

    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));

    chunk.AddCompositionInput(table, &input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("a", chunk.raw());
    EXPECT_EQ("[A]", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
  }

  {
    CompositionInput input;
    CharChunk chunk;

    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));

    chunk.AddCompositionInput(table, &input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("a", chunk.raw());
    EXPECT_EQ("[A]", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
  }

  {
    CompositionInput input;
    CharChunk chunk;

    input.set_raw("n");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));

    chunk.AddCompositionInput(table, &input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());

    input.set_raw("a");
    input.set_is_new_input(false);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));

    chunk.AddCompositionInput(table, &input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("na", chunk.raw());
    EXPECT_EQ("[NA]", chunk.conversion());
    EXPECT_EQ("", chunk.pending());
  }

  {
    CompositionInput input;
    CharChunk chunk;

    input.set_raw("n");
    input.set_is_new_input(true);
    EXPECT_FALSE(chunk.ShouldInsertNewChunk(table, input));

    chunk.AddCompositionInput(table, &input);
    EXPECT_TRUE(input.Empty());
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());

    input.set_raw("a");
    input.set_is_new_input(true);
    EXPECT_TRUE(chunk.ShouldInsertNewChunk(table, input));

    chunk.AddCompositionInput(table, &input);
    EXPECT_FALSE(input.Empty());
    EXPECT_EQ("n", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("n", chunk.pending());
    EXPECT_EQ("a", input.raw());
    EXPECT_FALSE(input.has_conversion());
  }
}

TEST(CharChunkTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  Table table;
  // "ち゛", "ぢ"
  table.AddRule("\xE3\x81\xA1\xE3\x82\x9B", "\xE3\x81\xA2", "");

  CharChunk chunk;
  chunk.SetTransliterator(kFullAsciiT12r);
  string key = "a";
  // "ち";
  string converted_char = "\xE3\x81\xA1";
  chunk.AddInputAndConvertedChar(table, &key, &converted_char);

  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(converted_char.empty());
  // "ち" can be "ぢ", so it should be appendable.
  EXPECT_TRUE(chunk.IsAppendable(kNullT12r));

  {  // The output should be "ａ".
    string output;
    chunk.AppendResult(table, kNullT12r, &output);
    // "ａ"
    EXPECT_EQ("\xEF\xBD\x81", output);
  }

  // Space input makes the internal state of chunk, but it is not consumed.
  key = " ";
  chunk.AddInput(table, &key);
  EXPECT_EQ(" ", key);
  EXPECT_TRUE(chunk.IsAppendable(kNullT12r));

  {  // The output should be still "ａ".
    string output;
    chunk.AppendResult(table, kNullT12r, &output);
    // "ａ"
    EXPECT_EQ("\xEF\xBD\x81", output);
  }
}

TEST(CharChunkTest, Issue2209634) {
  // This is a unittest against http://b/2209634
  // "q@" becomes "qた@".
  Table table;
  // "た"
  table.AddRule("q", "", "\xE3\x81\x9F");
  // "た@", "だ"
  table.AddRule("\xE3\x81\x9F\x40", "\xE3\x81\xA0", "");

  CharChunk chunk;
  chunk.SetTransliterator(kHalfAsciiT12r);

  string key = "q@";
  chunk.AddInput(table, &key);
  EXPECT_TRUE(key.empty());

  string output;
  chunk.AppendResult(table, kNullT12r, &output);
  EXPECT_EQ("q@", output);
}

TEST(CharChunkTest, Issue2819580) {
  // This is an unittest against http://b/2819580.
  // 'y' after 'n' disappears.
  Table table;
  // "ぽ"
  table.AddRule("po", "\xe3\x81\xbd", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table.AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table.AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");

  // Test for reported situation ("ny").
  // AddInput ver.
  {
    CharChunk chunk;
    chunk.SetTransliterator(kHiraganaT12r);

    {
      string input("n");
      chunk.AddInput(table, &input);
    }
    {
      string input("y");
      chunk.AddInput(table, &input);
    }
    {
      string result;
      chunk.AppendFixedResult(table, kHiraganaT12r, &result);
      // "んｙ"
      EXPECT_EQ("\xe3\x82\x93\xef\xbd\x99", result);
    }

    {
      string input("a");
      chunk.AddInput(table, &input);
    }
    {
      string result;
      chunk.AppendFixedResult(table, kHiraganaT12r, &result);
      // "にゃ"
      EXPECT_EQ("\xe3\x81\xab\xe3\x82\x83", result);
    }
  }

  // Test for reported situation (ny).
  // AddInputAndConvertedChar ver.
  {
    CharChunk chunk;
    chunk.SetTransliterator(kHiraganaT12r);

    {
      string input("n");
      chunk.AddInput(table, &input);
    }
    {
      string input("y");
      chunk.AddInput(table, &input);
    }
    {
      string input("a");
      string converted("a");
      chunk.AddInputAndConvertedChar(table, &input, &converted);
    }
    {
      string result;
      chunk.AppendFixedResult(table, kHiraganaT12r, &result);
      // "にゃ"
      EXPECT_EQ("\xe3\x81\xab\xe3\x82\x83", result);
    }
  }

  // Test for reported situation ("pony").
  {
    CharChunk chunk;
    chunk.SetTransliterator(kHiraganaT12r);

    {
      string input("p");
      chunk.AddInput(table, &input);
    }
    {
      string input("o");
      chunk.AddInput(table, &input);
    }
    {
      string input("n");
      chunk.AddInput(table, &input);
    }
    {
      string input("y");
      chunk.AddInput(table, &input);
    }
    {
      string result;
      chunk.AppendFixedResult(table, kHiraganaT12r, &result);
      // "ぽんｙ"
      EXPECT_EQ("\xe3\x81\xbd\xe3\x82\x93\xef\xbd\x99", result);
    }

    {
      string input("a");
      chunk.AddInput(table, &input);
    }
    {
      string result;
      chunk.AppendFixedResult(table, kHiraganaT12r, &result);
      // "ぽにゃ"
      EXPECT_EQ("\xe3\x81\xbd\xe3\x81\xab\xe3\x82\x83", result);
    }
  }

  // The first input is not contained in the table.
  {
    CharChunk chunk;
    chunk.SetTransliterator(kHiraganaT12r);

    {
      string input("z");
      chunk.AddInput(table, &input);
    }
    {
      string input("n");
      chunk.AddInput(table, &input);
    }
    {
      string input("y");
      chunk.AddInput(table, &input);
    }
    {
      string result;
      chunk.AppendFixedResult(table, kHiraganaT12r, &result);
      // "ｚんｙ"
      EXPECT_EQ("\xef\xbd\x9a\xe3\x82\x93\xef\xbd\x99", result);
    }
  }
}

TEST(CharChunkTest, Issue2990253) {
  // http://b/2990253
  // SplitChunk fails.
  // Ambiguous text is left in rhs CharChunk invalidly.
  Table table;
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  // "や"
  table.AddRule("ya", "\xe3\x82\x84", "");
  // "にゃ"
  table.AddRule("nya", "\xe3\x81\xab\xe3\x82\x83", "");

  CharChunk chunk, left_new_chunk;

  {
    string input("n");
    chunk.AddInput(table, &input);
  }
  {
    string input("y");
    chunk.AddInput(table, &input);
  }
  chunk.SplitChunk(kHiraganaT12r, size_t(1), &left_new_chunk);
  {
    string result;
    chunk.AppendFixedResult(table, kHiraganaT12r, &result);
    // "ｙ"
    EXPECT_EQ("\xef\xbd\x99", result);
  }
}

TEST(CharChunkTest, Combine) {
  {
    CharChunk lhs, rhs;
    lhs.set_ambiguous("LA");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");
    rhs.set_status(CharChunk::NO_RAW);

    rhs.set_ambiguous("RA");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");
    rhs.set_status(CharChunk::NO_CONVERSION);

    rhs.Combine(lhs);
    EXPECT_EQ("LARA", rhs.ambiguous());
    EXPECT_EQ("LCRC", rhs.conversion());
    EXPECT_EQ("LPRP", rhs.pending());
    EXPECT_EQ("LRRR", rhs.raw());
    EXPECT_TRUE(rhs.has_status(CharChunk::NO_CONVERSION));
  }

  {  // lhs' ambigous is empty.
    CharChunk lhs, rhs;
    lhs.set_ambiguous("");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");
    rhs.set_status(CharChunk::NO_RAW);

    rhs.set_ambiguous("RA");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");
    rhs.set_status(CharChunk::NO_CONVERSION);

    rhs.Combine(lhs);
    EXPECT_EQ("", rhs.ambiguous());
    EXPECT_EQ("LCRC", rhs.conversion());
    EXPECT_EQ("LPRP", rhs.pending());
    EXPECT_EQ("LRRR", rhs.raw());
    EXPECT_TRUE(rhs.has_status(CharChunk::NO_CONVERSION));
  }

  {  // rhs' ambigous is empty.
    CharChunk lhs, rhs;
    lhs.set_ambiguous("LA");
    lhs.set_conversion("LC");
    lhs.set_pending("LP");
    lhs.set_raw("LR");
    rhs.set_status(CharChunk::NO_RAW);

    rhs.set_ambiguous("");
    rhs.set_conversion("RC");
    rhs.set_pending("RP");
    rhs.set_raw("RR");
    rhs.set_status(CharChunk::NO_CONVERSION);

    rhs.Combine(lhs);
    EXPECT_EQ("LARP", rhs.ambiguous());
    EXPECT_EQ("LCRC", rhs.conversion());
    EXPECT_EQ("LPRP", rhs.pending());
    EXPECT_EQ("LRRR", rhs.raw());
    EXPECT_TRUE(rhs.has_status(CharChunk::NO_CONVERSION));
  }
}

TEST(CharChunkTest, IsConvertible) {
  CharChunk chunk;
  Table table;
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  {
    // If pending is empty, returns false.
    chunk.Clear();
    EXPECT_EQ("", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(kHiraganaT12r, table, "n"));
  }
  {
    // If t12r is inconsistent, returns false.
    chunk.Clear();
    chunk.SetTransliterator(kHiraganaT12r);
    string input("n");
    chunk.AddInput(table, &input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(kFullAsciiT12r, table, "a"));
  }
  {
    // If no entries are found from the table, returns false.
    chunk.Clear();
    chunk.SetTransliterator(kHiraganaT12r);
    string input("n");
    chunk.AddInput(table, &input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(kHiraganaT12r, table, "x"));
  }
  {
    // If found entry does not consume all of input, returns false.
    chunk.Clear();
    chunk.SetTransliterator(kHiraganaT12r);
    string input("n");
    chunk.AddInput(table, &input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(kHiraganaT12r, table, "y"));
  }
  {
    // [pending='n'] + [input='a'] is convertible (single combination).
    chunk.Clear();
    chunk.SetTransliterator(kHiraganaT12r);
    string input("n");
    chunk.AddInput(table, &input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_TRUE(chunk.IsConvertible(kHiraganaT12r, table, "a"));
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
    CharChunk chunk;
    chunk.set_raw(Table::ParseSpecialKey("[x]{#1}4"));
    chunk.set_conversion("");
    chunk.set_pending("[ta]");

    string result;
    chunk.AppendResult(table, kRawT12r, &result);
    EXPECT_EQ("[x]4", result);

    result.clear();
    chunk.AppendTrimedResult(table, kRawT12r, &result);
    EXPECT_EQ("[x]4", result);

    result.clear();
    chunk.AppendFixedResult(table, kRawT12r, &result);
    EXPECT_EQ("[x]4", result);

    EXPECT_EQ(4, chunk.GetLength(kRawT12r));

    result.clear();
    chunk.AppendResult(table, kConvT12r, &result);
    EXPECT_EQ("[ta]", result);

    result.clear();
    chunk.AppendTrimedResult(table, kConvT12r, &result);
    // Trimed result does not take pending value.
    EXPECT_EQ("", result);

    result.clear();
    chunk.AppendFixedResult(table, kConvT12r, &result);
    EXPECT_EQ("[ta]", result);

    EXPECT_EQ(4, chunk.GetLength(kConvT12r));
  }

  {
    CharChunk chunk;
    chunk.set_raw("[tu]*");
    chunk.set_conversion("");
    chunk.set_pending(Table::ParseSpecialKey("[x]{#2}"));

    string result;
    chunk.AppendResult(table, kRawT12r, &result);
    EXPECT_EQ("[tu]*", result);

    result.clear();
    chunk.AppendTrimedResult(table, kRawT12r, &result);
    EXPECT_EQ("[tu]*", result);

    result.clear();
    chunk.AppendFixedResult(table, kRawT12r, &result);
    EXPECT_EQ("[tu]*", result);

    EXPECT_EQ(5, chunk.GetLength(kRawT12r));

    result.clear();
    chunk.AppendResult(table, kConvT12r, &result);
    EXPECT_EQ("[x]", result);

    result.clear();
    chunk.AppendTrimedResult(table, kConvT12r, &result);
    // Trimed result does not take pending value.
    EXPECT_EQ("", result);

    result.clear();
    chunk.AppendFixedResult(table, kConvT12r, &result);
    EXPECT_EQ("[x]", result);

    EXPECT_EQ(3, chunk.GetLength(kConvT12r));
  }
}

TEST(CharChunkTest, SplitChunkWithSpecialKeys) {
  {
    CharChunk chunk;
    chunk.set_raw("a");
    chunk.set_conversion(Table::ParseSpecialKey("ab{1}cd"));

    CharChunk left_chunk;
    EXPECT_FALSE(chunk.SplitChunk(kConvT12r, 0, &left_chunk));

    EXPECT_EQ(4, chunk.GetLength(kConvT12r));
    EXPECT_FALSE(chunk.SplitChunk(kConvT12r, 4, &left_chunk));
  }

  {
    CharChunk chunk;
    chunk.set_raw("a");
    chunk.set_conversion(Table::ParseSpecialKey("ab{1}cd"));

    CharChunk left_chunk;
    EXPECT_TRUE(chunk.SplitChunk(kConvT12r, 1, &left_chunk));
    EXPECT_EQ("a", left_chunk.conversion());
    EXPECT_EQ("bcd", chunk.conversion());
  }

  {
    CharChunk chunk;
    chunk.set_raw("a");
    chunk.set_conversion(Table::ParseSpecialKey("ab{1}cd"));

    CharChunk left_chunk;
    EXPECT_TRUE(chunk.SplitChunk(kConvT12r, 2, &left_chunk));
    EXPECT_EQ("ab", left_chunk.conversion());
    EXPECT_EQ("cd", chunk.conversion());
  }

  {
    CharChunk chunk;
    chunk.set_raw("a");
    chunk.set_conversion(Table::ParseSpecialKey("ab{1}cd"));

    CharChunk left_chunk;
    EXPECT_TRUE(chunk.SplitChunk(kConvT12r, 3, &left_chunk));
    EXPECT_EQ("abc", left_chunk.conversion());
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
    CharChunk chunk;
    chunk.SetTransliterator(kRawT12r);
    ASSERT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    string input = "ka";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("KA", chunk.conversion());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "sa" - kConvT12r is set if NO_TRANSLITERATION is specified.
    CharChunk chunk;
    chunk.SetTransliterator(kRawT12r);

    string input = "sa";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("SA", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "s" + "a" - Same with the above.
    CharChunk chunk;
    chunk.SetTransliterator(kRawT12r);

    string input = "s";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(chunk.conversion().empty());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    input = "a";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("SA", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "kka" - The first attribute (NO_TRANSLITERATION) is used.
    CharChunk chunk;
    chunk.SetTransliterator(kRawT12r);

    string input = "kk";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("x", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));

    input = "a";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("xKA", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "ssa" - The first attribute (default behavior) is used.
    CharChunk chunk;
    chunk.SetTransliterator(kRawT12r);

    string input = "ss";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("x", chunk.conversion());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    input = "a";
    chunk.AddInput(table, &input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("xSA", chunk.conversion());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));
  }
}

TEST(CharChunkTest, NoTransliterationAttributeForInputAndConvertedChar) {
  Table table;
  table.AddRuleWithAttributes("[ka]@", "[ga]", "", NO_TRANSLITERATION);
  table.AddRuleWithAttributes("[sa]", "[sa]", "", NO_TRANSLITERATION);
  table.AddRule("[sa]@", "[za]", "");

  {  // "KA" - Default normal behavior.
    CharChunk chunk;
    chunk.SetTransliterator(kRawT12r);
    ASSERT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    string input = "t";
    string conv = "[ka]";
    chunk.AddInputAndConvertedChar(table, &input, &conv);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(conv.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_EQ("[ka]", chunk.pending());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    // "GA" - The first attribute (default behavior) is used.
    input = "!";
    conv = "@";
    chunk.AddInputAndConvertedChar(table, &input, &conv);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(conv.empty());
    EXPECT_EQ("t!", chunk.raw());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("[ga]", chunk.conversion());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "SA" - kConvT12r is set if NO_TRANSLITERATION is specified.
    CharChunk chunk;
    chunk.SetTransliterator(kRawT12r);

    string input = "x";
    string conv = "[sa]";
    chunk.AddInputAndConvertedChar(table, &input, &conv);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(conv.empty());
    EXPECT_EQ("x", chunk.raw());
    EXPECT_EQ("[sa]", chunk.pending());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));

    // "ZA" - The first attribute (NO_TRANSLITERATION) is used.
    input = "!";
    conv = "@";
    chunk.AddInputAndConvertedChar(table, &input, &conv);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(conv.empty());
    EXPECT_EQ("x!", chunk.raw());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("[za]", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
  }
}


}  // namespace composer
}  // namespace mozc
