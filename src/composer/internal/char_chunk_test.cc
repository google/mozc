// Copyright 2010, Google Inc.
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
#include "composer/internal/transliterators_ja.h"
#include "composer/table.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace composer {

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
  EXPECT_EQ(1, chunk1.GetLength(
      TransliteratorsJa::GetConversionStringSelector()));
  EXPECT_EQ(2, chunk1.GetLength(
      TransliteratorsJa::GetRawStringSelector()));

  CharChunk chunk2;
  // "っと"
  chunk2.set_conversion("\xe3\x81\xa3\xe3\x81\xa8");
  chunk2.set_pending("");
  chunk2.set_raw("tto");
  EXPECT_EQ(2, chunk2.GetLength(
      TransliteratorsJa::GetConversionStringSelector()));
  EXPECT_EQ(3, chunk2.GetLength(
      TransliteratorsJa::GetRawStringSelector()));

  CharChunk chunk3;
  // "が"
  chunk3.set_conversion("\xE3\x81\x8C");
  chunk3.set_pending("");
  chunk3.set_raw("ga");
  EXPECT_EQ(1, chunk3.GetLength(
      TransliteratorsJa::GetConversionStringSelector()));
  EXPECT_EQ(2, chunk3.GetLength(
      TransliteratorsJa::GetRawStringSelector()));

  chunk3.SetTransliterator(TransliteratorsJa::GetHalfKatakanaTransliterator());
  EXPECT_EQ(2, chunk3.GetLength(
      TransliteratorsJa::GetHalfKatakanaTransliterator()));
  chunk3.SetTransliterator(TransliteratorsJa::GetHalfAsciiTransliterator());
  EXPECT_EQ(2, chunk3.GetLength(
      TransliteratorsJa::GetHalfAsciiTransliterator()));
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
  chunk.AppendResult(table, NULL, &result);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", result);

  chunk.SetTransliterator(TransliteratorsJa::GetFullKatakanaTransliterator());
  result.clear();
  chunk.AppendResult(table, NULL, &result);
  // "ア"
  EXPECT_EQ("\xE3\x82\xA2", result);

  chunk.SetTransliterator(TransliteratorsJa::GetHalfAsciiTransliterator());
  result.clear();
  chunk.AppendResult(table, NULL, &result);
  EXPECT_EQ("a", result);

  result.clear();
  chunk.AppendResult(table,
                     TransliteratorsJa::GetHalfKatakanaTransliterator(),
                     &result);
  // "ｱ"
  EXPECT_EQ("\xEF\xBD\xB1", result);
}

TEST(CharChunkTest, SplitChunk) {
  Table table;
  // "も"
  table.AddRule("mo", "\xE3\x82\x82", "");

  CharChunk chunk;
  chunk.SetTransliterator(TransliteratorsJa::GetHiraganaTransliterator());

  string input = "m";
  chunk.AddInputInternal(table, &input);
  EXPECT_TRUE(input.empty());

  string output;
  const TransliteratorInterface *kNullTransliterator = NULL;
  chunk.AppendResult(table, kNullTransliterator, &output);
  // "ｍ"
  EXPECT_EQ("\xEF\xBD\x8D", output);

  input = "o";
  chunk.AddInputInternal(table, &input);
  EXPECT_TRUE(input.empty());

  output.clear();
  chunk.AppendResult(table, kNullTransliterator, &output);
  // "も"
  EXPECT_EQ("\xE3\x82\x82", output);

  chunk.SetTransliterator(TransliteratorsJa::GetHalfAsciiTransliterator());
  output.clear();
  chunk.AppendResult(table, kNullTransliterator, &output);
  EXPECT_EQ("mo", output);

  // Split "mo" to "m" and "o".
  CharChunk left_chunk;
  chunk.SplitChunk(kNullTransliterator, 1, &left_chunk);

  // The output should be half width "m" rather than full width "ｍ".
  output.clear();
  left_chunk.AppendResult(table, kNullTransliterator, &output);
  EXPECT_EQ("m", output);
}

TEST(CharChunkTest, IsAppendable) {
  Table table;
  // "も"
  table.AddRule("mo", "\xE3\x82\x82", "");

  const TransliteratorInterface *kNullT12r = NULL;
  const TransliteratorInterface *kHiraganaT12r =
      TransliteratorsJa::GetHiraganaTransliterator();
  const TransliteratorInterface *kKatakanaT12r =
      TransliteratorsJa::GetFullKatakanaTransliterator();

  CharChunk chunk;
  chunk.SetTransliterator(kHiraganaT12r);

  string input = "m";
  chunk.AddInputInternal(table, &input);
  EXPECT_TRUE(input.empty());
  EXPECT_TRUE(chunk.IsAppendable(kNullT12r));
  EXPECT_TRUE(chunk.IsAppendable(kHiraganaT12r));
  EXPECT_FALSE(chunk.IsAppendable(kKatakanaT12r));

  input = "o";
  chunk.AddInputInternal(table, &input);
  EXPECT_TRUE(input.empty());
  EXPECT_FALSE(chunk.IsAppendable(kNullT12r));
  EXPECT_FALSE(chunk.IsAppendable(kHiraganaT12r));
  EXPECT_FALSE(chunk.IsAppendable(kKatakanaT12r));
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

TEST(CharChunkTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  Table table;
  // "ち゛", "ぢ"
  table.AddRule("\xE3\x81\xA1\xE3\x82\x9B", "\xE3\x81\xA2", "");

  const TransliteratorInterface *kNullT12r = NULL;
  const TransliteratorInterface *kFullAsciiT12r =
      TransliteratorsJa::GetFullAsciiTransliterator();

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

  const TransliteratorInterface *kNullT12r = NULL;
  const TransliteratorInterface *kHalfAsciiT12r =
      TransliteratorsJa::GetHalfAsciiTransliterator();

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

  const TransliteratorInterface *kHiraganaT12r =
      TransliteratorsJa::GetHiraganaTransliterator();

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

  const TransliteratorInterface *kHiraganaT12r =
      TransliteratorsJa::GetHiraganaTransliterator();

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

TEST(CharChunkTest, IsConvertible) {
  CharChunk chunk;
  const TransliteratorInterface *kHiraganaT12r =
      TransliteratorsJa::GetHiraganaTransliterator();
  const TransliteratorInterface *kFullAsciiT12r =
      TransliteratorsJa::GetFullAsciiTransliterator();
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

}  // namespace composer
}  // namespace mozc
