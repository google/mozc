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

#include "converter/key_corrector.h"

#include <cstddef>
#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(KeyCorrectorTest, KeyCorrectorBasicTest) {
  EXPECT_FALSE(KeyCorrector::IsValidPosition(KeyCorrector::InvalidPosition()));

  EXPECT_TRUE(KeyCorrector::IsInvalidPosition(KeyCorrector::InvalidPosition()));

  {
    KeyCorrector corrector("", KeyCorrector::KANA, 0);
    EXPECT_EQ(corrector.mode(), KeyCorrector::KANA);
    EXPECT_FALSE(corrector.IsAvailable());
  }

  {
    KeyCorrector corrector("", KeyCorrector::ROMAN, 0);
    EXPECT_EQ(corrector.mode(), KeyCorrector::ROMAN);
    EXPECT_FALSE(corrector.IsAvailable());
  }

  {
    KeyCorrector corrector("てすと", KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.original_key(), "てすと");
  }
}

TEST(KeyCorrectorTest, KeyCorrectorKanaTest) {
  constexpr absl::string_view input = "みんあであそぼう";
  KeyCorrector corrector(input, KeyCorrector::KANA, 0);
  EXPECT_FALSE(corrector.IsAvailable());
  EXPECT_EQ(corrector.corrected_key(), "");
  EXPECT_EQ(corrector.original_key(), "");

  EXPECT_EQ(corrector.GetCorrectedPosition(0), KeyCorrector::InvalidPosition());

  EXPECT_EQ(corrector.GetOriginalPosition(0), KeyCorrector::InvalidPosition());
}

TEST(KeyCorrectorTest, KeyCorrectorRomanTest) {
  {
    constexpr absl::string_view input = "ん";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "ん");
  }

  {
    constexpr absl::string_view input = "かん";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "かん");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "かに";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "かに");
  }

  {
    constexpr absl::string_view input = "かｍ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "かｍ");
  }

  {
    constexpr absl::string_view input = "みんあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "みんなであそぼう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    // don't rewrite 1st "ん"
    constexpr absl::string_view input = "んあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "んあであそぼう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "こんかいのみんあはこんんでた";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんかいのみんなはこんでた");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input =
        "みんあみんいみんうみんえみんおみんんか";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(),
              "みんなみんにみんぬみんねみんのみんか");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちは");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "こんんいちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちは");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "しぜんんお";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "しぜんの");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "あんんんたい";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "あんんんたい");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "せにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "せんよう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "せにゃうせにゅうせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "せんやうせんゆうせんよう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "こんんにちはせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちはせんよう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "おんあのここんいちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "おんなのここんにちは");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "きっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "きって");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "きっっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "きっっって");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "きっっっ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "きっっっ");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "っっ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "っっ");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "しｍばし";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "しんばし");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "しｍはししｍぱしー";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "しｍはししんぱしー");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "ちゅごく";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "ちゅうごく");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    constexpr absl::string_view input = "きゅきゅしゃ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "きゅうきゅうしゃ");
    EXPECT_EQ(corrector.original_key(), input);
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanPositionTest) {
  {
    constexpr absl::string_view input = "みんあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "みんなであそぼう");

    EXPECT_EQ(corrector.GetCorrectedPosition(0), 0);
    EXPECT_EQ(corrector.GetCorrectedPosition(1), 1);

    EXPECT_EQ(corrector.GetCorrectedPosition(3), 3);
    EXPECT_EQ(corrector.GetCorrectedPosition(6), 6);

    EXPECT_EQ(corrector.GetCorrectedPosition(9), 9);
    EXPECT_EQ(corrector.GetCorrectedPosition(12), 12);
    EXPECT_EQ(corrector.GetCorrectedPosition(30),
              KeyCorrector::InvalidPosition());

    EXPECT_EQ(corrector.GetOriginalPosition(0), 0);
    EXPECT_EQ(corrector.GetOriginalPosition(1), 1);

    EXPECT_EQ(corrector.GetOriginalPosition(3), 3);
    EXPECT_EQ(corrector.GetOriginalPosition(6), 6);

    EXPECT_EQ(corrector.GetOriginalPosition(9), 9);
    EXPECT_EQ(corrector.GetOriginalPosition(12), 12);
    EXPECT_EQ(corrector.GetOriginalPosition(30),
              KeyCorrector::InvalidPosition());
  }

  {
    constexpr absl::string_view input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちは");

    EXPECT_EQ(corrector.GetCorrectedPosition(0), 0);
    EXPECT_EQ(corrector.GetCorrectedPosition(1),
              KeyCorrector::InvalidPosition());

    EXPECT_EQ(corrector.GetCorrectedPosition(3),
              KeyCorrector::InvalidPosition());
    EXPECT_EQ(corrector.GetCorrectedPosition(6),
              KeyCorrector::InvalidPosition());

    EXPECT_EQ(corrector.GetCorrectedPosition(9), 6);
    EXPECT_EQ(corrector.GetCorrectedPosition(12), 9);
    EXPECT_EQ(corrector.GetCorrectedPosition(30),
              KeyCorrector::InvalidPosition());

    EXPECT_EQ(corrector.GetOriginalPosition(0), 0);
    EXPECT_EQ(corrector.GetOriginalPosition(1),
              KeyCorrector::InvalidPosition());

    EXPECT_EQ(corrector.GetOriginalPosition(3),
              KeyCorrector::InvalidPosition());
    EXPECT_EQ(corrector.GetOriginalPosition(6), 9);

    EXPECT_EQ(corrector.GetOriginalPosition(9), 12);
    EXPECT_EQ(corrector.GetOriginalPosition(12), 15);
    EXPECT_EQ(corrector.GetOriginalPosition(30),
              KeyCorrector::InvalidPosition());
  }

  {
    constexpr absl::string_view input = "こんんにちはせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちはせんよう");

    EXPECT_EQ(corrector.GetCorrectedPosition(0), 0);
    EXPECT_EQ(corrector.GetCorrectedPosition(3),
              KeyCorrector::InvalidPosition());
    EXPECT_EQ(corrector.GetCorrectedPosition(9), 6);
    EXPECT_EQ(corrector.GetCorrectedPosition(12), 9);
    EXPECT_EQ(corrector.GetCorrectedPosition(24), 21);
    EXPECT_EQ(corrector.GetCorrectedPosition(27), 24);

    EXPECT_EQ(corrector.GetOriginalPosition(0), 0);
    EXPECT_EQ(corrector.GetOriginalPosition(3),
              KeyCorrector::InvalidPosition());
    EXPECT_EQ(corrector.GetOriginalPosition(6), 9);
    EXPECT_EQ(corrector.GetOriginalPosition(24), 27);
    EXPECT_EQ(corrector.GetOriginalPosition(21), 24);
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanCorrectedPrefixTest) {
  {
    constexpr absl::string_view input = "てすと";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());

    // same as the original key
    EXPECT_TRUE(corrector.GetCorrectedPrefix(0).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(1).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(2).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(3).empty());
  }

  {
    constexpr absl::string_view input = "みんあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "みんなであそぼう");

    EXPECT_EQ(corrector.GetCorrectedPrefix(0), "みんなであそぼう");
    EXPECT_EQ(corrector.GetCorrectedPrefix(3), "んなであそぼう");
    EXPECT_EQ(corrector.GetCorrectedPrefix(6), "なであそぼう");
    EXPECT_EQ(corrector.GetCorrectedPrefix(9), "");
  }

  {
    constexpr absl::string_view input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちは");

    EXPECT_EQ(corrector.GetCorrectedPrefix(0), "こんにちは");
    EXPECT_TRUE(corrector.GetCorrectedPrefix(3).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(6).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(9).empty());
  }

  {
    constexpr absl::string_view input = "こんんにちはせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちはせんよう");

    EXPECT_EQ(corrector.GetCorrectedPrefix(0), "こんにちはせんよう");
    EXPECT_TRUE(corrector.GetCorrectedPrefix(3).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(6).empty());
    EXPECT_EQ(corrector.GetCorrectedPrefix(9), "にちはせんよう");
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanGetOriginalOffsetTest) {
  {
    constexpr absl::string_view input = "てすと";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.GetOriginalOffset(0, 3), 3);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 6), 6);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 9), 9);

    EXPECT_EQ(corrector.GetOriginalOffset(3, 3), 3);
    EXPECT_EQ(corrector.GetOriginalOffset(3, 6), 6);
  }

  {
    constexpr absl::string_view input = "みんあ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.GetOriginalOffset(0, 3), 3);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 6), 6);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 9), 9);
  }

  {
    constexpr absl::string_view input = "きっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.GetOriginalOffset(0, 9), 12);
  }

  {
    constexpr absl::string_view input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちは");

    EXPECT_EQ(corrector.GetOriginalOffset(0, 3),
              KeyCorrector::InvalidPosition());
    EXPECT_EQ(corrector.GetOriginalOffset(0, 6), 9);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 9), 12);

    EXPECT_EQ(corrector.GetOriginalOffset(3, 3),
              KeyCorrector::InvalidPosition());

    EXPECT_EQ(corrector.GetOriginalOffset(3, 6),
              KeyCorrector::InvalidPosition());

    EXPECT_EQ(corrector.GetOriginalOffset(6, 3),
              KeyCorrector::InvalidPosition());

    EXPECT_EQ(corrector.GetOriginalOffset(9, 3), 3);

    EXPECT_EQ(corrector.GetOriginalOffset(9, 6), 6);
  }
}

// Check if UCS4 is supported. b/3386634
TEST(KeyCorrectorTest, UCS4IsAvailable) {
  {
    constexpr absl::string_view input = "𠮟";  // UCS4 char in UTF8
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
  }

  {
    constexpr absl::string_view input = "こ";  // UCS2 char in UTF8
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
  }
}

TEST(KeyCorrectorTest, UCS4Test) {
  {
    constexpr absl::string_view input = "😁みんあ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.GetOriginalOffset(0, 7), 7);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 10), 10);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 13), 13);
  }
}

// Should not rewrite the character which is at the beginning of current input
TEST(KeyCorrectorTest, Bug3046266Test) {
  {
    constexpr absl::string_view input = "かんあか";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 6);  // history_size = 6
    EXPECT_TRUE(corrector.IsAvailable());

    // same as the original key
    EXPECT_TRUE(corrector.GetCorrectedPrefix(0).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(1).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(2).empty());
    EXPECT_TRUE(corrector.GetCorrectedPrefix(3).empty());
  }
}

}  // namespace
}  // namespace mozc
