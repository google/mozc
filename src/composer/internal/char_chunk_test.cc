// Copyright 2010-2012, Google Inc.
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

  CharChunk chunk1(kNullT12r, &table);
  input = "i";
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ("i", chunk1.raw());
  // "い"
  EXPECT_EQ("\xe3\x81\x84", chunk1.conversion());
  EXPECT_EQ("", chunk1.pending());
  EXPECT_EQ("", input);

  CharChunk chunk2(kNullT12r, &table);
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
  // "っ"
  EXPECT_EQ("\xe3\x81\xa3", chunk2.conversion());
  EXPECT_EQ("t", chunk2.pending());
  EXPECT_EQ("", input);

  input = "a";
  EXPECT_FALSE(chunk2.AddInputInternal(&input));
  EXPECT_TRUE(chunk2.IsFixed());
  EXPECT_EQ("tta", chunk2.raw());
  // "った"
  EXPECT_EQ("\xe3\x81\xa3\xe3\x81\x9f", chunk2.conversion());
  EXPECT_EQ("", chunk2.pending());
  EXPECT_EQ("", input);
}

TEST(CharChunkTest, AddInput_NoEffectInput) {
  Table table;
  table.AddRule("2", "", "<*>2");
  table.AddRule("<*>1", "", "1");
  table.AddRule("*", "", "");

  string input;

  CharChunk chunk1(kNullT12r, &table);
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

  string input;

  CharChunk chunk1(kNullT12r, &table);
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
  // "い"
  table.AddRule("i", "\xe3\x81\x84", "");
  // "っ"
  table.AddRule("tt", "\xe3\x81\xa3", "t");
  // "た"
  table.AddRule("ta", "\xe3\x81\x9f", "");

  string input = "itta";

  CharChunk chunk1(kNullT12r, &table);
  EXPECT_FALSE(chunk1.AddInputInternal(&input));
  EXPECT_TRUE(chunk1.IsFixed());
  EXPECT_EQ("i", chunk1.raw());
  // "い"
  EXPECT_EQ("\xe3\x81\x84", chunk1.conversion());
  EXPECT_EQ("", chunk1.pending());
  EXPECT_EQ("tta", input);

  CharChunk chunk2(kNullT12r, &table);
  EXPECT_TRUE(chunk2.AddInputInternal(&input));
  EXPECT_FALSE(chunk2.IsFixed());
  EXPECT_EQ("tt", chunk2.raw());
  // "っ"
  EXPECT_EQ("\xe3\x81\xa3", chunk2.conversion());
  EXPECT_EQ("t", chunk2.pending());
  EXPECT_EQ("a", input);

  EXPECT_FALSE(chunk2.AddInputInternal(&input));
  EXPECT_TRUE(chunk2.IsFixed());
  EXPECT_EQ("tta", chunk2.raw());
  // "った"
  EXPECT_EQ("\xe3\x81\xa3\xe3\x81\x9f", chunk2.conversion());
  EXPECT_EQ("", chunk2.pending());
  EXPECT_EQ("", input);
}

TEST(CharChunkTest, GetLength) {
  CharChunk chunk1(kNullT12r, NULL);
  // "ね"
  chunk1.set_conversion("\xe3\x81\xad");
  chunk1.set_pending("");
  chunk1.set_raw("ne");
  EXPECT_EQ(1, chunk1.GetLength(kConvT12r));
  EXPECT_EQ(2, chunk1.GetLength(kRawT12r));

  CharChunk chunk2(kNullT12r, NULL);
  // "っと"
  chunk2.set_conversion("\xe3\x81\xa3\xe3\x81\xa8");
  chunk2.set_pending("");
  chunk2.set_raw("tto");
  EXPECT_EQ(2, chunk2.GetLength(kConvT12r));
  EXPECT_EQ(3, chunk2.GetLength(kRawT12r));

  CharChunk chunk3(kNullT12r, NULL);
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

  CharChunk chunk1(kNullT12r, &table);
  string key = "m";
  // "も"
  string value = "\xe3\x82\x82";
  chunk1.AddInputAndConvertedChar(&key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("m", chunk1.raw());
  // "も"
  EXPECT_EQ("\xe3\x82\x82", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  key = "r";
  // "す"
  value = "\xe3\x81\x99";
  chunk1.AddInputAndConvertedChar(&key, &value);
  // The input values are not used.
  EXPECT_EQ("r", key);
  // "す"
  EXPECT_EQ("\xe3\x81\x99", value);
  // The chunk remains the previous value.
  EXPECT_EQ("m", chunk1.raw());
  // "も"
  EXPECT_EQ("\xe3\x82\x82", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  CharChunk chunk2(kNullT12r, &table);
  // key == "r", value == "す";
  chunk2.AddInputAndConvertedChar(&key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("r", chunk2.raw());
  // "す"
  EXPECT_EQ("\xe3\x81\x99", chunk2.pending());
  EXPECT_TRUE(chunk2.conversion().empty());

  key = "@";
  // "゛"
  value = "\xe3\x82\x9b";
  chunk2.AddInputAndConvertedChar(&key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("r@", chunk2.raw());
  EXPECT_TRUE(chunk2.pending().empty());
  // "ず"
  EXPECT_EQ("\xe3\x81\x9a", chunk2.conversion());

  key = "h";
  // "く"
  value = "\xe3\x81\x8f";
  chunk2.AddInputAndConvertedChar(&key, &value);
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

  CharChunk chunk1(kNullT12r, &table);
  string key = "-";
  string value = "-";
  chunk1.AddInputAndConvertedChar(&key, &value);
  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(value.empty());
  EXPECT_EQ("-", chunk1.raw());
  EXPECT_EQ("-", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  key = "-";
  value = "-";
  chunk1.AddInputAndConvertedChar(&key, &value);
  // The input values are not used.
  EXPECT_EQ("-", key);
  EXPECT_EQ("-", value);
  // The chunk remains the previous value.
  EXPECT_EQ("-", chunk1.raw());
  EXPECT_EQ("-", chunk1.pending());
  EXPECT_TRUE(chunk1.conversion().empty());

  CharChunk chunk2(kNullT12r, &table);
  // key == "-", value == "-";
  chunk2.AddInputAndConvertedChar(&key, &value);
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

  CharChunk chunk(kNullT12r, &table);
  string input = "a";
  chunk.AddInputInternal(&input);

  string result;
  chunk.AppendResult(kNullT12r, &result);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", result);

  chunk.SetTransliterator(kFullKatakanaT12r);
  result.clear();
  chunk.AppendResult(kNullT12r, &result);
  // "ア"
  EXPECT_EQ("\xE3\x82\xA2", result);

  chunk.SetTransliterator(kHalfAsciiT12r);
  result.clear();
  chunk.AppendResult(kNullT12r, &result);
  EXPECT_EQ("a", result);

  result.clear();
  chunk.AppendResult(kHalfKatakanaT12r, &result);
  // "ｱ"
  EXPECT_EQ("\xEF\xBD\xB1", result);
}

TEST(CharChunkTest, SplitChunk) {
  Table table;
  // "も"
  table.AddRule("mo", "\xE3\x82\x82", "");

  CharChunk chunk(kHiraganaT12r, &table);

  string input = "m";
  chunk.AddInputInternal(&input);
  EXPECT_TRUE(input.empty());

  string output;
  chunk.AppendResult(kNullT12r, &output);
  // "ｍ"
  EXPECT_EQ("\xEF\xBD\x8D", output);

  input = "o";
  chunk.AddInputInternal(&input);
  EXPECT_TRUE(input.empty());

  output.clear();
  chunk.AppendResult(kNullT12r, &output);
  // "も"
  EXPECT_EQ("\xE3\x82\x82", output);

  chunk.SetTransliterator(kHalfAsciiT12r);
  output.clear();
  chunk.AppendResult(kNullT12r, &output);
  EXPECT_EQ("mo", output);

  // Split "mo" to "m" and "o".
  CharChunk *left_chunk_ptr = NULL;
  chunk.SplitChunk(kNullT12r, 1, &left_chunk_ptr);
  scoped_ptr<CharChunk> left_chunk(left_chunk_ptr);

  // The output should be half width "m" rather than full width "ｍ".
  output.clear();
  left_chunk->AppendResult(kNullT12r, &output);
  EXPECT_EQ("m", output);
}

TEST(CharChunkTest, IsAppendable) {
  Table table;
  // "も"
  table.AddRule("mo", "\xE3\x82\x82", "");
  Table table_another;

  CharChunk chunk(kHiraganaT12r, &table);

  string input = "m";
  chunk.AddInputInternal(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_TRUE(chunk.IsAppendable(kNullT12r, &table));
  EXPECT_TRUE(chunk.IsAppendable(kHiraganaT12r, &table));
  EXPECT_FALSE(chunk.IsAppendable(kFullKatakanaT12r, &table));
  EXPECT_FALSE(chunk.IsAppendable(kNullT12r, &table_another));
  EXPECT_FALSE(chunk.IsAppendable(kHiraganaT12r, &table_another));

  input = "o";
  chunk.AddInputInternal(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_FALSE(chunk.IsAppendable(kNullT12r, &table));
  EXPECT_FALSE(chunk.IsAppendable(kHiraganaT12r, &table));
  EXPECT_FALSE(chunk.IsAppendable(kFullKatakanaT12r, &table));
}

TEST(CharChunkTest, AddInputInternal) {
  Table table;
  // "っ"
  table.AddRule("tt", "\xE3\x81\xA3", "t");

  CharChunk chunk(kNullT12r, &table);
  {
    string key = "t";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    string key = "t";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("tt", chunk.raw());
    // "っ"
    EXPECT_EQ("\xE3\x81\xA3", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    string key = "t";
    chunk.AddInputInternal(&key);
    EXPECT_TRUE(key.empty());
    EXPECT_EQ("ttt", chunk.raw());
    // "っっ"
    EXPECT_EQ("\xE3\x81\xA3\xE3\x81\xA3", chunk.conversion());
    EXPECT_EQ("t", chunk.pending());
  }
  {
    string key = "!";
    chunk.AddInputInternal(&key);
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
  CharChunk chunk(kNullT12r, &table);

  string key = "Ka";
  chunk.AddInputInternal(&key);
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

  CharChunk chunk(kHiraganaT12r, &table);

  {
    string input("ssh");
    chunk.AddInput(&input);
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
    chunk.AppendFixedResult(kHiraganaT12r, &result);
    // "っｓｈ"
    EXPECT_EQ("\xE3\x81\xA3\xEF\xBD\x93\xEF\xBD\x88", result);
  }

  // Break down of the internal procedures
  chunk.Clear();
  chunk.SetTransliterator(kHiraganaT12r);
  {
    string input("s");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("s", chunk.raw());
    // empty() is intentionaly not used due to check the actual value.
    EXPECT_EQ("", chunk.conversion());
    EXPECT_EQ("s", chunk.pending());
    EXPECT_EQ("", chunk.ambiguous());
  }
  {
    string input("s");
    chunk.AddInput(&input);
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
    chunk.AddInput(&input);
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
    CharChunk chunk(kNullT12r, &table);
    string input("k");
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
    CharChunk chunk(kNullT12r, &table);
    string input("t");
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
    CharChunk chunk(kNullT12r, &table);
    string input("t");
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
    CharChunk chunk(kNullT12r, &table);
    string input("2");
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
    CharChunk chunk(kNullT12r, &table);
    string input("2");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("2", chunk.raw());

    input = "a";
    EXPECT_EQ("a", input);
    EXPECT_EQ("2", chunk.raw());
  }

  {  // flick#2
    CharChunk chunk(kNullT12r, &table);
    string input("a");
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("a", chunk.raw());

    input = "b";
    EXPECT_EQ("b", input);
    EXPECT_EQ("a", chunk.raw());
  }

  {  // flick and toggle
    CharChunk chunk(kNullT12r, &table);
    string input("a");
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
  CharChunk chunk(kNullT12r, &table);

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

TEST(CharChunkTest, AddCompositionInput) {
  Table table;
  table.AddRuleWithAttributes("na", "[NA]", "", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("a", "[A]", "", NEW_CHUNK);

  {
    CompositionInput input;
    CharChunk chunk(kNullT12r, &table);

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
    CharChunk chunk(kNullT12r, &table);

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
    CharChunk chunk(kNullT12r, &table);

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
    CharChunk chunk(kNullT12r, &table);

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
    EXPECT_FALSE(input.has_conversion());
  }
}

TEST(CharChunkTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  Table table;
  // "ち゛", "ぢ"
  table.AddRule("\xE3\x81\xA1\xE3\x82\x9B", "\xE3\x81\xA2", "");

  CharChunk chunk(kFullAsciiT12r, &table);
  string key = "a";
  // "ち";
  string converted_char = "\xE3\x81\xA1";
  chunk.AddInputAndConvertedChar(&key, &converted_char);

  EXPECT_TRUE(key.empty());
  EXPECT_TRUE(converted_char.empty());
  // "ち" can be "ぢ", so it should be appendable.
  EXPECT_TRUE(chunk.IsAppendable(kNullT12r, &table));

  {  // The output should be "ａ".
    string output;
    chunk.AppendResult(kNullT12r, &output);
    // "ａ"
    EXPECT_EQ("\xEF\xBD\x81", output);
  }

  // Space input makes the internal state of chunk, but it is not consumed.
  key = " ";
  chunk.AddInput(&key);
  EXPECT_EQ(" ", key);
  EXPECT_TRUE(chunk.IsAppendable(kNullT12r, &table));

  {  // The output should be still "ａ".
    string output;
    chunk.AppendResult(kNullT12r, &output);
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

  CharChunk chunk(kHalfAsciiT12r, &table);

  string key = "q@";
  chunk.AddInput(&key);
  EXPECT_TRUE(key.empty());

  string output;
  chunk.AppendResult(kNullT12r, &output);
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
    CharChunk chunk(kHiraganaT12r, &table);

    {
      string input("n");
      chunk.AddInput(&input);
    }
    {
      string input("y");
      chunk.AddInput(&input);
    }
    {
      string result;
      chunk.AppendFixedResult(kHiraganaT12r, &result);
      // "んｙ"
      EXPECT_EQ("\xe3\x82\x93\xef\xbd\x99", result);
    }

    {
      string input("a");
      chunk.AddInput(&input);
    }
    {
      string result;
      chunk.AppendFixedResult(kHiraganaT12r, &result);
      // "にゃ"
      EXPECT_EQ("\xe3\x81\xab\xe3\x82\x83", result);
    }
  }

  // Test for reported situation (ny).
  // AddInputAndConvertedChar ver.
  {
    CharChunk chunk(kHiraganaT12r, &table);

    {
      string input("n");
      chunk.AddInput(&input);
    }
    {
      string input("y");
      chunk.AddInput(&input);
    }
    {
      string input("a");
      string converted("a");
      chunk.AddInputAndConvertedChar(&input, &converted);
    }
    {
      string result;
      chunk.AppendFixedResult(kHiraganaT12r, &result);
      // "にゃ"
      EXPECT_EQ("\xe3\x81\xab\xe3\x82\x83", result);
    }
  }

  // Test for reported situation ("pony").
  {
    CharChunk chunk(kHiraganaT12r, &table);

    {
      string input("p");
      chunk.AddInput(&input);
    }
    {
      string input("o");
      chunk.AddInput(&input);
    }
    {
      string input("n");
      chunk.AddInput(&input);
    }
    {
      string input("y");
      chunk.AddInput(&input);
    }
    {
      string result;
      chunk.AppendFixedResult(kHiraganaT12r, &result);
      // "ぽんｙ"
      EXPECT_EQ("\xe3\x81\xbd\xe3\x82\x93\xef\xbd\x99", result);
    }

    {
      string input("a");
      chunk.AddInput(&input);
    }
    {
      string result;
      chunk.AppendFixedResult(kHiraganaT12r, &result);
      // "ぽにゃ"
      EXPECT_EQ("\xe3\x81\xbd\xe3\x81\xab\xe3\x82\x83", result);
    }
  }

  // The first input is not contained in the table.
  {
    CharChunk chunk(kHiraganaT12r, &table);

    {
      string input("z");
      chunk.AddInput(&input);
    }
    {
      string input("n");
      chunk.AddInput(&input);
    }
    {
      string input("y");
      chunk.AddInput(&input);
    }
    {
      string result;
      chunk.AppendFixedResult(kHiraganaT12r, &result);
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

  CharChunk chunk(kNullT12r, &table);

  {
    string input("n");
    chunk.AddInput(&input);
  }
  {
    string input("y");
    chunk.AddInput(&input);
  }

  CharChunk *left_new_chunk_ptr = NULL;
  chunk.SplitChunk(kHiraganaT12r, size_t(1), &left_new_chunk_ptr);
  scoped_ptr<CharChunk> left_new_chunk(left_new_chunk_ptr);
  {
    string result;
    chunk.AppendFixedResult(kHiraganaT12r, &result);
    // "ｙ"
    EXPECT_EQ("\xef\xbd\x99", result);
  }
}

TEST(CharChunkTest, Combine) {
  {
    CharChunk lhs(kNullT12r, NULL), rhs(kNullT12r, NULL);
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
    CharChunk lhs(kNullT12r, NULL), rhs(kNullT12r, NULL);
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
    CharChunk lhs(kNullT12r, NULL), rhs(kNullT12r, NULL);
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
  Table table;
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");

  CharChunk chunk(kNullT12r, &table);
  {
    // If pending is empty, returns false.
    chunk.Clear();
    EXPECT_EQ("", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(kHiraganaT12r, &table, "n"));
  }
  {
    // If t12r is inconsistent, returns false.
    chunk.Clear();
    chunk.SetTransliterator(kHiraganaT12r);
    string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(kFullAsciiT12r, &table, "a"));
  }
  {
    // If no entries are found from the table, returns false.
    chunk.Clear();
    chunk.SetTransliterator(kHiraganaT12r);
    string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(kHiraganaT12r, &table, "x"));
  }
  {
    // If found entry does not consume all of input, returns false.
    chunk.Clear();
    chunk.SetTransliterator(kHiraganaT12r);
    string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_FALSE(chunk.IsConvertible(kHiraganaT12r, &table, "y"));
  }
  {
    // [pending='n'] + [input='a'] is convertible (single combination).
    chunk.Clear();
    chunk.SetTransliterator(kHiraganaT12r);
    string input("n");
    chunk.AddInput(&input);
    EXPECT_EQ("n", chunk.pending());
    EXPECT_TRUE(chunk.IsConvertible(kHiraganaT12r, &table, "a"));
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
    CharChunk chunk(kNullT12r, &table);
    chunk.set_raw(Table::ParseSpecialKey("[x]{#1}4"));
    chunk.set_conversion("");
    chunk.set_pending("[ta]");

    string result;
    chunk.AppendResult(kRawT12r, &result);
    EXPECT_EQ("[x]4", result);

    result.clear();
    chunk.AppendTrimedResult(kRawT12r, &result);
    EXPECT_EQ("[x]4", result);

    result.clear();
    chunk.AppendFixedResult(kRawT12r, &result);
    EXPECT_EQ("[x]4", result);

    EXPECT_EQ(4, chunk.GetLength(kRawT12r));

    result.clear();
    chunk.AppendResult(kConvT12r, &result);
    EXPECT_EQ("[ta]", result);

    result.clear();
    chunk.AppendTrimedResult(kConvT12r, &result);
    // Trimed result does not take pending value.
    EXPECT_EQ("", result);

    result.clear();
    chunk.AppendFixedResult(kConvT12r, &result);
    EXPECT_EQ("[ta]", result);

    EXPECT_EQ(4, chunk.GetLength(kConvT12r));
  }

  {
    CharChunk chunk(kNullT12r, &table);
    chunk.set_raw("[tu]*");
    chunk.set_conversion("");
    chunk.set_pending(Table::ParseSpecialKey("[x]{#2}"));

    string result;
    chunk.AppendResult(kRawT12r, &result);
    EXPECT_EQ("[tu]*", result);

    result.clear();
    chunk.AppendTrimedResult(kRawT12r, &result);
    EXPECT_EQ("[tu]*", result);

    result.clear();
    chunk.AppendFixedResult(kRawT12r, &result);
    EXPECT_EQ("[tu]*", result);

    EXPECT_EQ(5, chunk.GetLength(kRawT12r));

    result.clear();
    chunk.AppendResult(kConvT12r, &result);
    EXPECT_EQ("[x]", result);

    result.clear();
    chunk.AppendTrimedResult(kConvT12r, &result);
    // Trimed result does not take pending value.
    EXPECT_EQ("", result);

    result.clear();
    chunk.AppendFixedResult(kConvT12r, &result);
    EXPECT_EQ("[x]", result);

    EXPECT_EQ(3, chunk.GetLength(kConvT12r));
  }
}

TEST(CharChunkTest, SplitChunkWithSpecialKeys) {
  {
    CharChunk chunk(kNullT12r, NULL);
    chunk.set_raw("a");
    chunk.set_conversion(Table::ParseSpecialKey("ab{1}cd"));

    CharChunk *left_chunk_ptr = NULL;
    EXPECT_FALSE(chunk.SplitChunk(kConvT12r, 0, &left_chunk_ptr));
    scoped_ptr<CharChunk> left_chunk(left_chunk_ptr);
    EXPECT_EQ(4, chunk.GetLength(kConvT12r));
    EXPECT_FALSE(chunk.SplitChunk(kConvT12r, 4, &left_chunk_ptr));
    left_chunk.reset(left_chunk_ptr);
  }

  {
    CharChunk chunk(kNullT12r, NULL);
    chunk.set_raw("a");
    chunk.set_conversion(Table::ParseSpecialKey("ab{1}cd"));

    CharChunk *left_chunk_ptr = NULL;
    EXPECT_TRUE(chunk.SplitChunk(kConvT12r, 1, &left_chunk_ptr));
    scoped_ptr<CharChunk> left_chunk(left_chunk_ptr);
    EXPECT_EQ("a", left_chunk->conversion());
    EXPECT_EQ("bcd", chunk.conversion());
  }

  {
    CharChunk chunk(kNullT12r, NULL);
    chunk.set_raw("a");
    chunk.set_conversion(Table::ParseSpecialKey("ab{1}cd"));

    CharChunk *left_chunk_ptr = NULL;
    EXPECT_TRUE(chunk.SplitChunk(kConvT12r, 2, &left_chunk_ptr));
    scoped_ptr<CharChunk> left_chunk(left_chunk_ptr);
    EXPECT_EQ("ab", left_chunk->conversion());
    EXPECT_EQ("cd", chunk.conversion());
  }

  {
    CharChunk chunk(kNullT12r, NULL);
    chunk.set_raw("a");
    chunk.set_conversion(Table::ParseSpecialKey("ab{1}cd"));

    CharChunk *left_chunk_ptr = NULL;
    EXPECT_TRUE(chunk.SplitChunk(kConvT12r, 3, &left_chunk_ptr));
    scoped_ptr<CharChunk> left_chunk(left_chunk_ptr);
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
    CharChunk chunk(kRawT12r, &table);
    ASSERT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    string input = "ka";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("KA", chunk.conversion());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "sa" - kConvT12r is set if NO_TRANSLITERATION is specified
    CharChunk chunk(kRawT12r, &table);

    string input = "sa";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("SA", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "s" + "a" - Same with the above.
    CharChunk chunk(kRawT12r, &table);

    string input = "s";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(chunk.conversion().empty());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("SA", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "kka" - The first attribute (NO_TRANSLITERATION) is used.
    CharChunk chunk(kRawT12r, &table);

    string input = "kk";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("x", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));

    input = "a";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("xKA", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "ssa" - The first attribute (default behavior) is used.
    CharChunk chunk(kRawT12r, &table);

    string input = "ss";
    chunk.AddInput(&input);
    EXPECT_TRUE(input.empty());
    EXPECT_EQ("x", chunk.conversion());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    input = "a";
    chunk.AddInput(&input);
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
    CharChunk chunk(kRawT12r, &table);
    ASSERT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    string input = "t";
    string conv = "[ka]";
    chunk.AddInputAndConvertedChar(&input, &conv);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(conv.empty());
    EXPECT_EQ("t", chunk.raw());
    EXPECT_EQ("[ka]", chunk.pending());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));

    // "GA" - The first attribute (default behavior) is used.
    input = "!";
    conv = "@";
    chunk.AddInputAndConvertedChar(&input, &conv);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(conv.empty());
    EXPECT_EQ("t!", chunk.raw());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("[ga]", chunk.conversion());
    EXPECT_EQ(kRawT12r, chunk.GetTransliterator(kNullT12r));
  }

  {  // "SA" - kConvT12r is set if NO_TRANSLITERATION is specified.
    CharChunk chunk(kRawT12r, &table);

    string input = "x";
    string conv = "[sa]";
    chunk.AddInputAndConvertedChar(&input, &conv);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(conv.empty());
    EXPECT_EQ("x", chunk.raw());
    EXPECT_EQ("[sa]", chunk.pending());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));

    // "ZA" - The first attribute (NO_TRANSLITERATION) is used.
    input = "!";
    conv = "@";
    chunk.AddInputAndConvertedChar(&input, &conv);
    EXPECT_TRUE(input.empty());
    EXPECT_TRUE(conv.empty());
    EXPECT_EQ("x!", chunk.raw());
    EXPECT_EQ("", chunk.pending());
    EXPECT_EQ("[za]", chunk.conversion());
    EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
  }
}

namespace {
bool HasResult(const set<string> &results, const string &value) {
  return (results.find(value) != results.end());
}
}  // namespace

TEST(CharChunkTest, RomanGetExpandedResults) {
  Table table;
  // "きゃ"
  table.AddRule("kya", "\xe3\x81\x8d\xe3\x82\x83", "");
  // "きぃ"
  table.AddRule("kyi", "\xe3\x81\x8d\xe3\x81\x83", "");
  // "きゅ"
  table.AddRule("kyu", "\xe3\x81\x8d\xe3\x82\x85", "");
  // "きぇ"
  table.AddRule("kye", "\xe3\x81\x8d\xe3\x81\x87", "");
  // "きょ"
  table.AddRule("kyo", "\xe3\x81\x8d\xe3\x82\x87", "");
  // "っ"
  table.AddRule("kk", "\xe3\x81\xa3", "k");
  // "か"
  table.AddRule("ka", "\xe3\x81\x8b", "");
  // "き"
  table.AddRule("ki", "\xe3\x81\x8d", "");
  // "く"
  table.AddRule("ku", "\xe3\x81\x8f", "");
  // "け"
  table.AddRule("ke", "\xe3\x81\x91", "");
  // "こ"
  table.AddRule("ko", "\xe3\x81\x93", "");

  {
    string input = "ka";  // AddInputInternal requires non-const string
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    // "か"
    EXPECT_EQ("\xe3\x81\x8b", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(0, results.size());  // no ambiguity
  }
  {
    string input = "k";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(12, results.size());
    EXPECT_TRUE(HasResult(results, "k"));
    // "か"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8b"));  // ka
    // "き"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d"));  // ki
    // "きゃ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x83"));  // kya
    // "きぃ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x81\x83"));  // kyi
    // "きゅ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x85"));  // kyu
    // "きぇ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x81\x87"));  // kye
    // "きょ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x87"));  // kyo
    // "く"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8f"));  // ku
    // "け"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x91"));  // ke
    // "こ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x93"));  // ko
    // "っ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xa3"));  // kk
  }
  {
    string input = "ky";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(6, results.size());
    EXPECT_TRUE(HasResult(results, "ky"));
    // "きゃ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x83"));
    // "きぃ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x81\x83"));
    // "きゅ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x85"));
    // "きぇ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x81\x87"));
    // "きょ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x87"));
  }
  {
    string input = "kk";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    // "っ"
    EXPECT_EQ("\xe3\x81\xa3", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(11, results.size());
    // "か"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8b"));  // ka
    // "き"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d"));  // ki
    // "きゃ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x83"));  // kya
    // "きぃ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x81\x83"));  // kyi
    // "きゅ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x85"));  // kyu
    // "きぇ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x81\x87"));  // kye
    // "きょ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8d\xe3\x82\x87"));  // kyo
    // "く"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8f"));  // ku
    // "け"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x91"));  // ke
    // "こ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x93"));  // ko
    // "っ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xa3"));  // kk
  }
}

TEST(CharChunkTest, KanaGetExpandedResults) {
  Table table;
  // "か゛", "が"
  table.AddRule("\xe3\x81\x8b\xe3\x82\x9b", "\xe3\x81\x8c", "");
  // "は゛", "ば"
  table.AddRule("\xe3\x81\xaf\xe3\x82\x9b", "\xe3\x81\xb0", "");
  // "は゜", "ぱ"
  table.AddRule("\xe3\x81\xaf\xe3\x82\x9c", "\xe3\x81\xb1", "");

  {
    // AddInputInternal requires non-const string
    // "か"
    string input = "\xe3\x81\x8b";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(2, results.size());
    // "か"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8b"));
    // "が"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x8c"));
  }
  {
    // "は"
    string input = "\xe3\x81\xaf";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(3, results.size());
    // "は"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xaf"));
    // "ば"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xb0"));
    // "ぱ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xb1"));
  }
}

TEST(CharChunkTest, 12KeyGetExpandedResults) {
  Table table;
  // It's not the test for the table, but use the real table file
  // for checking it's functionality.
  table.LoadFromFile("system://12keys-hiragana.tsv");

  {
    string input = "1";  // AddInputInternal requires non-const string
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(2, results.size());
    // "あ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x82"));
    // "ぁ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x81"));
  }
  {
    string input = "8";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(2, results.size());
    // "や"
    EXPECT_TRUE(HasResult(results, "\xe3\x82\x84"));
    // "ゃ"
    EXPECT_TRUE(HasResult(results, "\xe3\x82\x83"));
  }
  {
    // "や8"
    string input = "\xe3\x82\x84\x38";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(2, results.size());
    // "ゆ"
    EXPECT_TRUE(HasResult(results, "\xe3\x82\x86"));
    // "ゅ"
    EXPECT_TRUE(HasResult(results, "\xe3\x82\x85"));
  }
  {
    string input = "6";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(3, results.size());
    // "は"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xaf"));
    // "ば"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xb0"));
    // "ぱ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xb1"));
  }
}

TEST(CharChunkTest, FlickGetExpandedResults) {
  Table table;
  // It's not the test for the table, but use the real table file
  // for checking it's functionality.
  table.LoadFromFile("system://flick-hiragana.tsv");

  {
    string input = "1";  // AddInputInternal requires non-const string
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(2, results.size());
    // "あ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x82"));
    // "ぁ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\x81"));
  }
  {
    string input = "8";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(2, results.size());
    // "や"
    EXPECT_TRUE(HasResult(results, "\xe3\x82\x84"));
    // "ゃ"
    EXPECT_TRUE(HasResult(results, "\xe3\x82\x83"));
  }
  {
    string input = "u";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(2, results.size());
    // "ゆ"
    EXPECT_TRUE(HasResult(results, "\xe3\x82\x86"));
    // "ゅ"
    EXPECT_TRUE(HasResult(results, "\xe3\x82\x85"));
  }
  {
    string input = "6";
    CharChunk chunk(kNullT12r, &table);
    chunk.AddInputInternal(&input);

    string base;
    chunk.AppendTrimedResult(kNullT12r, &base);
    EXPECT_EQ("", base);

    set<string> results;
    chunk.GetExpandedResults(kNullT12r, &results);
    EXPECT_EQ(3, results.size());
    // "は"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xaf"));
    // "ば"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xb0"));
    // "ぱ"
    EXPECT_TRUE(HasResult(results, "\xe3\x81\xb1"));
  }
}

TEST(CharChunkTest, NoTransliteration_Issue3497962) {
  Table table;
  table.AddRuleWithAttributes("2", "", "a", NEW_CHUNK | NO_TRANSLITERATION);
  table.AddRuleWithAttributes("a2", "", "b", NO_TABLE_ATTRIBUTE);
  table.AddRuleWithAttributes("b2", "", "c", NO_TABLE_ATTRIBUTE);

  CharChunk chunk(kHiraganaT12r, &table);

  string input = "2";
  chunk.AddInput(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_EQ("2", chunk.raw());
  EXPECT_EQ("", chunk.conversion());
  EXPECT_EQ("a", chunk.pending());
  EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));

  input = "2";
  chunk.AddInput(&input);
  EXPECT_TRUE(input.empty());
  EXPECT_EQ("22", chunk.raw());
  EXPECT_EQ("", chunk.conversion());
  EXPECT_EQ("b", chunk.pending());
  EXPECT_EQ(kConvT12r, chunk.GetTransliterator(kNullT12r));
}

TEST(CharChunkTest, Clone) {
  Table table;
  CharChunk src(kHiraganaT12r, &table);
  src.raw_ = "raw";
  src.conversion_ = "conversion";
  src.pending_ = "pending";
  src.ambiguous_ = "ambiguous";
  src.status_mask_ = CharChunk::NO_RAW;
  src.attributes_ = NEW_CHUNK;

  scoped_ptr<CharChunk> dest(new CharChunk(kNullT12r, NULL));
  EXPECT_FALSE(src.transliterator_ == dest->transliterator_);
  EXPECT_FALSE(src.table_ == dest->table_);
  EXPECT_NE(src.raw_, dest->raw_);
  EXPECT_NE(src.conversion_, dest->conversion_);
  EXPECT_NE(src.pending_, dest->pending_);
  EXPECT_NE(src.ambiguous_, dest->ambiguous_);
  EXPECT_NE(src.status_mask_, dest->status_mask_);
  EXPECT_NE(src.attributes_, dest->attributes_);

  dest.reset(src.Clone());
  EXPECT_TRUE(src.transliterator_ == dest->transliterator_);
  EXPECT_TRUE(src.table_ == dest->table_);
  EXPECT_EQ(src.raw_, dest->raw_);
  EXPECT_EQ(src.conversion_, dest->conversion_);
  EXPECT_EQ(src.pending_, dest->pending_);
  EXPECT_EQ(src.ambiguous_, dest->ambiguous_);
  EXPECT_EQ(src.status_mask_, dest->status_mask_);
  EXPECT_EQ(src.attributes_, dest->attributes_);
}

}  // namespace composer
}  // namespace mozc
