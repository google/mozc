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
    corrector.Clear();
    EXPECT_FALSE(corrector.IsAvailable());
  }
}

TEST(KeyCorrectorTest, KeyCorrectorKanaTest) {
  const std::string input = "みんあであそぼう";
  KeyCorrector corrector(input, KeyCorrector::KANA, 0);
  EXPECT_FALSE(corrector.IsAvailable());
  EXPECT_EQ(corrector.corrected_key(), "");
  EXPECT_EQ(corrector.original_key(), "");

  EXPECT_EQ(corrector.GetCorrectedPosition(0), KeyCorrector::InvalidPosition());

  EXPECT_EQ(corrector.GetOriginalPosition(0), KeyCorrector::InvalidPosition());
}

TEST(KeyCorrectorTest, KeyCorrectorRomanTest) {
  {
    const std::string input = "ん";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "ん");
  }

  {
    const std::string input = "かん";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "かん");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "かに";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "かに");
  }

  {
    const std::string input = "かｍ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "かｍ");
  }

  {
    const std::string input = "みんあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "みんなであそぼう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    // don't rewrite 1st "ん"
    const std::string input = "んあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "んあであそぼう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "こんかいのみんあはこんんでた";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんかいのみんなはこんでた");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "みんあみんいみんうみんえみんおみんんか";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(),
              "みんなみんにみんぬみんねみんのみんか");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちは");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "こんんいちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちは");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "しぜんんお";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "しぜんの");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "あんんんたい";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "あんんんたい");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "せにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "せんよう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "せにゃうせにゅうせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "せんやうせんゆうせんよう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "こんんにちはせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちはせんよう");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "おんあのここんいちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "おんなのここんにちは");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "きっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "きって");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "きっっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "きっっって");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "きっっっ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "きっっっ");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "っっ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "っっ");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "しｍばし";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "しんばし");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "しｍはししｍぱしー";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "しｍはししんぱしー");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "ちゅごく";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "ちゅうごく");
    EXPECT_EQ(corrector.original_key(), input);
  }

  {
    const std::string input = "きゅきゅしゃ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "きゅうきゅうしゃ");
    EXPECT_EQ(corrector.original_key(), input);
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanPositionTest) {
  {
    const std::string input = "みんあであそぼう";
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
    const std::string input = "こんんにちは";
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
    const std::string input = "こんんにちはせにょう";
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
    const std::string input = "てすと";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    size_t length = 0;

    // same as the original key
    EXPECT_TRUE(nullptr == corrector.GetCorrectedPrefix(0, &length));
    EXPECT_TRUE(nullptr == corrector.GetCorrectedPrefix(1, &length));
    EXPECT_TRUE(nullptr == corrector.GetCorrectedPrefix(2, &length));
    EXPECT_TRUE(nullptr == corrector.GetCorrectedPrefix(3, &length));
  }

  {
    const std::string input = "みんあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "みんなであそぼう");

    const char *output = nullptr;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    EXPECT_EQ(std::string(output, length), "みんなであそぼう");

    output = corrector.GetCorrectedPrefix(3, &length);
    EXPECT_EQ(std::string(output, length), "んなであそぼう");

    output = corrector.GetCorrectedPrefix(6, &length);
    EXPECT_EQ(std::string(output, length), "なであそぼう");

    output = corrector.GetCorrectedPrefix(9, &length);
    // same
    EXPECT_TRUE(nullptr == output);
  }

  {
    const std::string input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちは");

    const char *output = nullptr;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    EXPECT_EQ(std::string(output, length), "こんにちは");

    output = corrector.GetCorrectedPrefix(3, &length);
    EXPECT_TRUE(nullptr == output);

    output = corrector.GetCorrectedPrefix(6, &length);
    EXPECT_TRUE(nullptr == output);

    output = corrector.GetCorrectedPrefix(9, &length);
    EXPECT_TRUE(nullptr == output);
  }

  {
    const std::string input = "こんんにちはせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.corrected_key(), "こんにちはせんよう");

    const char *output = nullptr;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    EXPECT_EQ(std::string(output, length), "こんにちはせんよう");

    output = corrector.GetCorrectedPrefix(3, &length);
    EXPECT_TRUE(nullptr == output);

    output = corrector.GetCorrectedPrefix(6, &length);
    EXPECT_TRUE(nullptr == output);

    output = corrector.GetCorrectedPrefix(9, &length);
    EXPECT_EQ(std::string(output, length), "にちはせんよう");
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanGetOriginalOffsetTest) {
  {
    const std::string input = "てすと";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.GetOriginalOffset(0, 3), 3);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 6), 6);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 9), 9);

    EXPECT_EQ(corrector.GetOriginalOffset(3, 3), 3);
    EXPECT_EQ(corrector.GetOriginalOffset(3, 6), 6);
  }

  {
    const std::string input = "みんあ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.GetOriginalOffset(0, 3), 3);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 6), 6);
    EXPECT_EQ(corrector.GetOriginalOffset(0, 9), 9);
  }

  {
    const std::string input = "きっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(corrector.GetOriginalOffset(0, 9), 12);
  }

  {
    const std::string input = "こんんにちは";
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
    const std::string input = "𠮟";  // UCS4 char in UTF8
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
  }

  {
    const std::string input = "こ";  // UCS2 char in UTF8
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
  }
}

TEST(KeyCorrectorTest, UCS4Test) {
  {
    const std::string input = "😁みんあ";
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
    const std::string input = "かんあか";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 6);  // history_size = 6
    EXPECT_TRUE(corrector.IsAvailable());
    size_t length = 0;

    // same as the original key
    EXPECT_TRUE(nullptr == corrector.GetCorrectedPrefix(0, &length));
    EXPECT_TRUE(nullptr == corrector.GetCorrectedPrefix(1, &length));
    EXPECT_TRUE(nullptr == corrector.GetCorrectedPrefix(2, &length));
    EXPECT_TRUE(nullptr == corrector.GetCorrectedPrefix(3, &length));
  }
}

}  // namespace
}  // namespace mozc
