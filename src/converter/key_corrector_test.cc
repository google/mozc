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

#include <string>
#include "base/base.h"
#include "base/util.h"
#include "converter/key_corrector.h"
#include "testing/base/public/gunit.h"

namespace mozc {

TEST(KeyCorrectorTest, KeyCorrectorBasicTest) {
  EXPECT_FALSE(KeyCorrector::IsValidPosition(
      KeyCorrector::InvalidPosition()));

  EXPECT_TRUE(KeyCorrector::IsInvalidPosition(
      KeyCorrector::InvalidPosition()));

  {
    KeyCorrector corrector("", KeyCorrector::KANA, 0);
    EXPECT_EQ(KeyCorrector::KANA, corrector.mode());
    EXPECT_FALSE(corrector.IsAvailable());
  }

  {
    KeyCorrector corrector("", KeyCorrector::ROMAN, 0);
    EXPECT_EQ(KeyCorrector::ROMAN, corrector.mode());
    EXPECT_FALSE(corrector.IsAvailable());
  }

  {
    // "てすと"
    KeyCorrector corrector("\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8",
                           KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "てすと"
    EXPECT_EQ("\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8", corrector.original_key());
    corrector.Clear();
    EXPECT_FALSE(corrector.IsAvailable());
  }
}

TEST(KeyCorrectorTest, KeyCorrectorKanaTest) {
  // "みんあであそぼう"
  const string input = "\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x82\xe3\x81\xa7\xe3"
      "\x81\x82\xe3\x81\x9d\xe3\x81\xbc\xe3\x81\x86";
  KeyCorrector corrector(input, KeyCorrector::KANA, 0);
  EXPECT_FALSE(corrector.IsAvailable());
  EXPECT_EQ("", corrector.corrected_key());
  EXPECT_EQ("", corrector.original_key());

  EXPECT_EQ(KeyCorrector::InvalidPosition(),
            corrector.GetCorrectedPosition(0));

  EXPECT_EQ(KeyCorrector::InvalidPosition(),
            corrector.GetOriginalPosition(0));
}

TEST(KeyCorrectorTest, KeyCorrectorRomanTest) {
  {
    // "ん"
    const string input = "\xe3\x82\x93";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "ん"
    EXPECT_EQ("\xe3\x82\x93", corrector.corrected_key());
  }

  {
    // "かん"
    const string input = "\xe3\x81\x8b\xe3\x82\x93";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "かん"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x82\x93",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "かに"
    const string input = "\xe3\x81\x8b\xe3\x81\xab";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "かに"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xab", corrector.corrected_key());
  }

  {
    // "かｍ"
    const string input = "\xe3\x81\x8b\xef\xbd\x8d";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "かｍ"
    EXPECT_EQ("\xe3\x81\x8b\xef\xbd\x8d", corrector.corrected_key());
  }

  {
    // "みんあであそぼう"
    const string input = "\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x82\xe3\x81\xa7\xe3"
        "\x81\x82\xe3\x81\x9d\xe3\x81\xbc\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "みんなであそぼう"
    EXPECT_EQ("\xe3\x81\xbf\xe3\x82\x93\xe3\x81\xaa\xe3\x81\xa7\xe3\x81\x82\xe3"
              "\x81\x9d\xe3\x81\xbc\xe3\x81\x86",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // don't rewrite 1st "ん"
    // "んあであそぼう"
    const string input = "\xe3\x82\x93\xe3\x81\x82\xe3\x81\xa7\xe3\x81\x82\xe3"
        "\x81\x9d\xe3\x81\xbc\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "んあであそぼう"
    EXPECT_EQ("\xe3\x82\x93\xe3\x81\x82\xe3\x81\xa7\xe3\x81\x82\xe3\x81\x9d\xe3"
              "\x81\xbc\xe3\x81\x86",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "こんかいのみんあはこんんでた"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x81\x8b\xe3\x81\x84\xe3"
        "\x81\xae\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x82\xe3\x81"
        "\xaf\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\xa7"
        "\xe3\x81\x9f";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんかいのみんなはこんでた"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\x8b\xe3\x81\x84\xe3\x81\xae\xe3"
              "\x81\xbf\xe3\x82\x93\xe3\x81\xaa\xe3\x81\xaf\xe3\x81\x93\xe3\x82"
              "\x93\xe3\x81\xa7\xe3\x81\x9f",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "みんあみんいみんうみんえみんおみんんか"
    const string input = "\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x82\xe3\x81\xbf\xe3"
        "\x82\x93\xe3\x81\x84\xe3\x81\xbf\xe3\x82\x93\xe3\x81"
        "\x86\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x88\xe3\x81\xbf"
        "\xe3\x82\x93\xe3\x81\x8a\xe3\x81\xbf\xe3\x82\x93\xe3"
        "\x82\x93\xe3\x81\x8b";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "みんなみんにみんぬみんねみんのみんか"
    EXPECT_EQ("\xe3\x81\xbf\xe3\x82\x93\xe3\x81\xaa\xe3\x81\xbf\xe3\x82\x93\xe3"
              "\x81\xab\xe3\x81\xbf\xe3\x82\x93\xe3\x81\xac\xe3\x81\xbf\xe3\x82"
              "\x93\xe3\x81\xad\xe3\x81\xbf\xe3\x82\x93\xe3\x81\xae\xe3\x81\xbf"
              "\xe3\x82\x93\xe3\x81\x8b",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "こんんにちは"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\xab\xe3"
        "\x81\xa1\xe3\x81\xaf";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんにちは"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "こんんいちは"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\x84\xe3"
        "\x81\xa1\xe3\x81\xaf";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんにちは"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "しぜんんお"
    const string input = "\xe3\x81\x97\xe3\x81\x9c\xe3\x82\x93\xe3\x82\x93\xe3"
        "\x81\x8a";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "しぜんの"
    EXPECT_EQ("\xe3\x81\x97\xe3\x81\x9c\xe3\x82\x93\xe3\x81\xae",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "あんんんたい"
    const string input = "\xe3\x81\x82\xe3\x82\x93\xe3\x82\x93\xe3\x82\x93\xe3"
        "\x81\x9f\xe3\x81\x84";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "あんんんたい"
    EXPECT_EQ("\xe3\x81\x82\xe3\x82\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\x9f\xe3"
              "\x81\x84",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "せにょう"
    const string input = "\xe3\x81\x9b\xe3\x81\xab\xe3\x82\x87\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "せんよう"
    EXPECT_EQ("\xe3\x81\x9b\xe3\x82\x93\xe3\x82\x88\xe3\x81\x86",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "せにゃうせにゅうせにょう"
    const string input = "\xe3\x81\x9b\xe3\x81\xab\xe3\x82\x83\xe3\x81\x86\xe3"
        "\x81\x9b\xe3\x81\xab\xe3\x82\x85\xe3\x81\x86\xe3\x81"
        "\x9b\xe3\x81\xab\xe3\x82\x87\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "せんやうせんゆうせんよう"
    EXPECT_EQ("\xe3\x81\x9b\xe3\x82\x93\xe3\x82\x84\xe3\x81\x86\xe3\x81\x9b\xe3"
              "\x82\x93\xe3\x82\x86\xe3\x81\x86\xe3\x81\x9b\xe3\x82\x93\xe3\x82"
              "\x88\xe3\x81\x86",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }


  {
    // "こんんにちはせにょう"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\xab\xe3"
        "\x81\xa1\xe3\x81\xaf\xe3\x81\x9b\xe3\x81\xab\xe3\x82"
        "\x87\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんにちはせんよう"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\xe3"
              "\x81\x9b\xe3\x82\x93\xe3\x82\x88\xe3\x81\x86",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "おんあのここんいちは"
    const string input = "\xe3\x81\x8a\xe3\x82\x93\xe3\x81\x82\xe3\x81\xae\xe3"
        "\x81\x93\xe3\x81\x93\xe3\x82\x93\xe3\x81\x84\xe3\x81"
        "\xa1\xe3\x81\xaf";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "おんなのここんにちは"
    EXPECT_EQ("\xe3\x81\x8a\xe3\x82\x93\xe3\x81\xaa\xe3\x81\xae\xe3\x81\x93\xe3"
              "\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "きっって"
    const string input = "\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\xa3\xe3\x81\xa6";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "きって"
    EXPECT_EQ("\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\xa6",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "きっっって"
    const string input = "\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\xa3\xe3\x81\xa3\xe3"
        "\x81\xa6";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "きっっって"
    EXPECT_EQ("\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\xa3\xe3\x81\xa3\xe3\x81\xa6",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "きっっっ"
    const string input = "\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\xa3\xe3\x81\xa3";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "きっっっ"
    EXPECT_EQ("\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\xa3\xe3\x81\xa3",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "っっ"
    const string input = "\xe3\x81\xa3\xe3\x81\xa3";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "っっ"
    EXPECT_EQ("\xe3\x81\xa3\xe3\x81\xa3",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }


  {
    // "しｍばし"
    const string input = "\xe3\x81\x97\xef\xbd\x8d\xe3\x81\xb0\xe3\x81\x97";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "しんばし"
    EXPECT_EQ("\xe3\x81\x97\xe3\x82\x93\xe3\x81\xb0\xe3\x81\x97",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "しｍはししｍぱしー"
    const string input = "\xe3\x81\x97\xef\xbd\x8d\xe3\x81\xaf\xe3\x81\x97\xe3"
        "\x81\x97\xef\xbd\x8d\xe3\x81\xb1\xe3\x81\x97\xe3\x83"
        "\xbc";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "しｍはししんぱしー"
    EXPECT_EQ("\xe3\x81\x97\xef\xbd\x8d\xe3\x81\xaf\xe3\x81\x97\xe3\x81\x97"
              "\xe3\x82\x93\xe3\x81\xb1\xe3\x81\x97\xe3\x83\xbc",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "ちゅごく"
    const string input = "\xE3\x81\xA1\xE3\x82\x85\xE3\x81\x94\xE3\x81\x8F";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "ちゅうごく"
    EXPECT_EQ("\xE3\x81\xA1\xE3\x82\x85\xE3\x81\x86\xE3\x81\x94\xE3\x81\x8F",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }

  {
    // "きゅきゅしゃ"
    const string input = "\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x8D"
        "\xE3\x82\x85\xE3\x81\x97\xE3\x82\x83";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "きゅうきゅうしゃ"
    EXPECT_EQ("\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86\xE3\x81\x8D"
              "\xE3\x82\x85\xE3\x81\x86\xE3\x81\x97\xE3\x82\x83",
              corrector.corrected_key());
    EXPECT_EQ(input,
              corrector.original_key());
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanPositionTest) {
  {
    // "みんあであそぼう"
    const string input = "\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x82\xe3\x81\xa7\xe3"
        "\x81\x82\xe3\x81\x9d\xe3\x81\xbc\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "みんなであそぼう"
    EXPECT_EQ("\xe3\x81\xbf\xe3\x82\x93\xe3\x81\xaa\xe3\x81\xa7\xe3\x81\x82\xe3"
              "\x81\x9d\xe3\x81\xbc\xe3\x81\x86",
              corrector.corrected_key());

    EXPECT_EQ(0, corrector.GetCorrectedPosition(0));
    EXPECT_EQ(1, corrector.GetCorrectedPosition(1));

    EXPECT_EQ(3, corrector.GetCorrectedPosition(3));
    EXPECT_EQ(6, corrector.GetCorrectedPosition(6));

    EXPECT_EQ(9, corrector.GetCorrectedPosition(9));
    EXPECT_EQ(12, corrector.GetCorrectedPosition(12));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(30));

    EXPECT_EQ(0, corrector.GetOriginalPosition(0));
    EXPECT_EQ(1, corrector.GetOriginalPosition(1));

    EXPECT_EQ(3, corrector.GetOriginalPosition(3));
    EXPECT_EQ(6, corrector.GetOriginalPosition(6));

    EXPECT_EQ(9, corrector.GetOriginalPosition(9));
    EXPECT_EQ(12, corrector.GetOriginalPosition(12));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(30));
  }

  {
    // "こんんにちは"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\xab\xe3"
        "\x81\xa1\xe3\x81\xaf";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんにちは"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf",
              corrector.corrected_key());

    EXPECT_EQ(0, corrector.GetCorrectedPosition(0));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(1));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(3));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(6));

    EXPECT_EQ(6, corrector.GetCorrectedPosition(9));
    EXPECT_EQ(9, corrector.GetCorrectedPosition(12));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(30));

    EXPECT_EQ(0, corrector.GetOriginalPosition(0));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(1));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(3));
    EXPECT_EQ(9, corrector.GetOriginalPosition(6));

    EXPECT_EQ(12, corrector.GetOriginalPosition(9));
    EXPECT_EQ(15, corrector.GetOriginalPosition(12));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(30));
  }

  {
    // "こんんにちはせにょう"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\xab\xe3"
        "\x81\xa1\xe3\x81\xaf\xe3\x81\x9b\xe3\x81\xab\xe3\x82"
        "\x87\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんにちはせんよう"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\xe3"
              "\x81\x9b\xe3\x82\x93\xe3\x82\x88\xe3\x81\x86",
              corrector.corrected_key());

    EXPECT_EQ(0, corrector.GetCorrectedPosition(0));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(3));
    EXPECT_EQ(6, corrector.GetCorrectedPosition(9));
    EXPECT_EQ(9, corrector.GetCorrectedPosition(12));
    EXPECT_EQ(21, corrector.GetCorrectedPosition(24));
    EXPECT_EQ(24, corrector.GetCorrectedPosition(27));

    EXPECT_EQ(0, corrector.GetOriginalPosition(0));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(3));
    EXPECT_EQ(9, corrector.GetOriginalPosition(6));
    EXPECT_EQ(27, corrector.GetOriginalPosition(24));
    EXPECT_EQ(24, corrector.GetOriginalPosition(21));
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanCorrectedPrefixTest) {
  {
    // "てすと"
    const string input = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    size_t length = 0;

    // same as the original key
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(0, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(1, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(2, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(3, &length));
  }

  {
    // "みんあであそぼう"
    const string input = "\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x82\xe3\x81\xa7\xe3"
        "\x81\x82\xe3\x81\x9d\xe3\x81\xbc\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "みんなであそぼう"
    EXPECT_EQ("\xe3\x81\xbf\xe3\x82\x93\xe3\x81\xaa\xe3\x81\xa7\xe3\x81\x82\xe3"
              "\x81\x9d\xe3\x81\xbc\xe3\x81\x86",
              corrector.corrected_key());

    const char *output = NULL;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    // "みんなであそぼう"
    EXPECT_EQ("\xe3\x81\xbf\xe3\x82\x93\xe3\x81\xaa\xe3\x81\xa7\xe3\x81\x82\xe3"
              "\x81\x9d\xe3\x81\xbc\xe3\x81\x86", string(output, length));

    output = corrector.GetCorrectedPrefix(3, &length);
    // "んなであそぼう"
    EXPECT_EQ("\xe3\x82\x93\xe3\x81\xaa\xe3\x81\xa7\xe3\x81\x82\xe3\x81\x9d\xe3"
              "\x81\xbc\xe3\x81\x86", string(output, length));

    output = corrector.GetCorrectedPrefix(6, &length);
    // "なであそぼう"
    EXPECT_EQ("\xe3\x81\xaa\xe3\x81\xa7\xe3\x81\x82\xe3\x81\x9d\xe3\x81\xbc\xe3"
              "\x81\x86", string(output, length));

    output = corrector.GetCorrectedPrefix(9, &length);
    // same
    EXPECT_TRUE(NULL == output);
  }

  {
    // "こんんにちは"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\xab\xe3"
        "\x81\xa1\xe3\x81\xaf";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんにちは"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf",
              corrector.corrected_key());

    const char *output = NULL;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    // "こんにちは"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf",
              string(output, length));

    output = corrector.GetCorrectedPrefix(3, &length);
    EXPECT_TRUE(NULL == output);

    output = corrector.GetCorrectedPrefix(6, &length);
    EXPECT_TRUE(NULL == output);

    output = corrector.GetCorrectedPrefix(9, &length);
    EXPECT_TRUE(NULL == output);
  }

  {
    // "こんんにちはせにょう"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\xab\xe3"
        "\x81\xa1\xe3\x81\xaf\xe3\x81\x9b\xe3\x81\xab\xe3\x82"
        "\x87\xe3\x81\x86";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんにちはせんよう"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\xe3"
              "\x81\x9b\xe3\x82\x93\xe3\x82\x88\xe3\x81\x86",
              corrector.corrected_key());

    const char *output = NULL;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    // "こんにちはせんよう"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\xe3"
              "\x81\x9b\xe3\x82\x93\xe3\x82\x88\xe3\x81\x86",
              string(output, length));

    output = corrector.GetCorrectedPrefix(3, &length);
    EXPECT_TRUE(NULL == output);

    output = corrector.GetCorrectedPrefix(6, &length);
    EXPECT_TRUE(NULL == output);

    output = corrector.GetCorrectedPrefix(9, &length);
    // "にちはせんよう"
    EXPECT_EQ("\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\xe3\x81\x9b\xe3\x82\x93\xe3"
              "\x82\x88\xe3\x81\x86", string(output, length));
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanGetOriginalOffsetTest) {
  {
    // "てすと"
    const string input = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(3, corrector.GetOriginalOffset(0, 3));
    EXPECT_EQ(6, corrector.GetOriginalOffset(0, 6));
    EXPECT_EQ(9, corrector.GetOriginalOffset(0, 9));

    EXPECT_EQ(3, corrector.GetOriginalOffset(3, 3));
    EXPECT_EQ(6, corrector.GetOriginalOffset(3, 6));
  }

   {
    // "みんあ"
    const string input = "\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x82";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(3, corrector.GetOriginalOffset(0, 3));
    EXPECT_EQ(6, corrector.GetOriginalOffset(0, 6));
    EXPECT_EQ(9, corrector.GetOriginalOffset(0, 9));
  }

  {
    // "きっって"
    const string input = "\xe3\x81\x8d\xe3\x81\xa3\xe3\x81\xa3\xe3\x81\xa6";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(12, corrector.GetOriginalOffset(0, 9));
  }

  {
    // "こんんにちは"
    const string input = "\xe3\x81\x93\xe3\x82\x93\xe3\x82\x93\xe3\x81\xab\xe3"
        "\x81\xa1\xe3\x81\xaf";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    // "こんにちは"
    EXPECT_EQ("\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf",
              corrector.corrected_key());

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalOffset(0, 3));
    EXPECT_EQ(9, corrector.GetOriginalOffset(0, 6));
    EXPECT_EQ(12, corrector.GetOriginalOffset(0, 9));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalOffset(3, 3));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalOffset(3, 6));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalOffset(6, 3));

    EXPECT_EQ(3,
              corrector.GetOriginalOffset(9, 3));

    EXPECT_EQ(6,
              corrector.GetOriginalOffset(9, 6));
  }
}

// Check if UCS4 is supported. b/3386634
TEST(KeyCorrectorTest, UCS4IsAvailable) {
  {
    // "𠮟"
    const string input = "\xF0\xA0\xAE\x9F";  // UCS4 char in UTF8
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
  }

  {
    // "叱"
    const string input = "\xe3\x81\x93";      // UCS2 char in UTF8
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
  }
}

TEST(KeyCorrectorTest, UCS4Test) {
   {
    // "😁みんあ"
    const string input =
        "\xF0\x9F\x98\x81\xe3\x81\xbf\xe3\x82\x93\xe3\x81\x82";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(7, corrector.GetOriginalOffset(0, 7));
    EXPECT_EQ(10, corrector.GetOriginalOffset(0, 10));
    EXPECT_EQ(13, corrector.GetOriginalOffset(0, 13));
  }
}

// Should not rewrite the character which is at the beginning of current input
TEST(KeyCorrectorTest, Bug3046266Test) {
   {
    // "かんあか"
    const string input = "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x82\xE3\x81\x8B";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 6);  // history_size = 6
    EXPECT_TRUE(corrector.IsAvailable());
    size_t length = 0;

    // same as the original key
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(0, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(1, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(2, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(3, &length));
  }
}

}  // namespace mozc
