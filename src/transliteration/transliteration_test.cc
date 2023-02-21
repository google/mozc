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

#include "transliteration/transliteration.h"

#include "testing/gunit.h"

namespace mozc {
namespace transliteration {

TEST(T13nTest, IsInFullAsciiTypes) {
  EXPECT_TRUE(T13n::IsInFullAsciiTypes(FULL_ASCII));
  EXPECT_TRUE(T13n::IsInFullAsciiTypes(FULL_ASCII_UPPER));
  EXPECT_TRUE(T13n::IsInFullAsciiTypes(FULL_ASCII_LOWER));
  EXPECT_TRUE(T13n::IsInFullAsciiTypes(FULL_ASCII_CAPITALIZED));

  EXPECT_FALSE(T13n::IsInFullAsciiTypes(HIRAGANA));
  EXPECT_FALSE(T13n::IsInFullAsciiTypes(FULL_KATAKANA));
  EXPECT_FALSE(T13n::IsInFullAsciiTypes(HALF_KATAKANA));
  EXPECT_FALSE(T13n::IsInFullAsciiTypes(HALF_ASCII));
  EXPECT_FALSE(T13n::IsInFullAsciiTypes(HALF_ASCII_UPPER));
  EXPECT_FALSE(T13n::IsInFullAsciiTypes(HALF_ASCII_LOWER));
  EXPECT_FALSE(T13n::IsInFullAsciiTypes(HALF_ASCII_CAPITALIZED));
}

TEST(T13nTest, IsInHalfAsciiTypes) {
  EXPECT_TRUE(T13n::IsInHalfAsciiTypes(HALF_ASCII));
  EXPECT_TRUE(T13n::IsInHalfAsciiTypes(HALF_ASCII_UPPER));
  EXPECT_TRUE(T13n::IsInHalfAsciiTypes(HALF_ASCII_LOWER));
  EXPECT_TRUE(T13n::IsInHalfAsciiTypes(HALF_ASCII_CAPITALIZED));

  EXPECT_FALSE(T13n::IsInHalfAsciiTypes(HIRAGANA));
  EXPECT_FALSE(T13n::IsInHalfAsciiTypes(FULL_KATAKANA));
  EXPECT_FALSE(T13n::IsInHalfAsciiTypes(HALF_KATAKANA));
  EXPECT_FALSE(T13n::IsInHalfAsciiTypes(FULL_ASCII));
  EXPECT_FALSE(T13n::IsInHalfAsciiTypes(FULL_ASCII_UPPER));
  EXPECT_FALSE(T13n::IsInHalfAsciiTypes(FULL_ASCII_LOWER));
  EXPECT_FALSE(T13n::IsInHalfAsciiTypes(FULL_ASCII_CAPITALIZED));
}

TEST(T13nTest, IsInHiraganaTypes) {
  EXPECT_TRUE(T13n::IsInHiraganaTypes(HIRAGANA));

  EXPECT_FALSE(T13n::IsInHiraganaTypes(HALF_ASCII));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(HALF_ASCII_UPPER));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(HALF_ASCII_LOWER));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(HALF_ASCII_CAPITALIZED));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(FULL_KATAKANA));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(HALF_KATAKANA));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(FULL_ASCII));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(FULL_ASCII_UPPER));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(FULL_ASCII_LOWER));
  EXPECT_FALSE(T13n::IsInHiraganaTypes(FULL_ASCII_CAPITALIZED));
}

TEST(T13nTest, IsInHalfKatakanaTypes) {
  EXPECT_TRUE(T13n::IsInHalfKatakanaTypes(HALF_KATAKANA));

  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(HALF_ASCII));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(HALF_ASCII_UPPER));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(HALF_ASCII_LOWER));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(HALF_ASCII_CAPITALIZED));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(HIRAGANA));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(FULL_KATAKANA));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(FULL_ASCII));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(FULL_ASCII_UPPER));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(FULL_ASCII_LOWER));
  EXPECT_FALSE(T13n::IsInHalfKatakanaTypes(FULL_ASCII_CAPITALIZED));
}

TEST(T13nTest, IsInFullKatakanaTypes) {
  EXPECT_TRUE(T13n::IsInFullKatakanaTypes(FULL_KATAKANA));

  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(HALF_ASCII));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(HALF_ASCII_UPPER));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(HALF_ASCII_LOWER));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(HALF_ASCII_CAPITALIZED));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(HIRAGANA));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(HALF_KATAKANA));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(FULL_ASCII));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(FULL_ASCII_UPPER));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(FULL_ASCII_LOWER));
  EXPECT_FALSE(T13n::IsInFullKatakanaTypes(FULL_ASCII_CAPITALIZED));
}

TEST(T13nTest, ToggleFullAsciiTypes) {
  EXPECT_EQ(T13n::ToggleFullAsciiTypes(HIRAGANA), FULL_ASCII);
  EXPECT_EQ(T13n::ToggleFullAsciiTypes(FULL_ASCII), FULL_ASCII_UPPER);
  EXPECT_EQ(T13n::ToggleFullAsciiTypes(FULL_ASCII_UPPER), FULL_ASCII_LOWER);
  EXPECT_EQ(T13n::ToggleFullAsciiTypes(FULL_ASCII_LOWER),
            FULL_ASCII_CAPITALIZED);
  EXPECT_EQ(T13n::ToggleFullAsciiTypes(FULL_ASCII_CAPITALIZED), FULL_ASCII);
}

TEST(T13nTest, ToggleHalfAsciiTypes) {
  EXPECT_EQ(T13n::ToggleHalfAsciiTypes(HIRAGANA), HALF_ASCII);
  EXPECT_EQ(T13n::ToggleHalfAsciiTypes(HALF_ASCII), HALF_ASCII_UPPER);
  EXPECT_EQ(T13n::ToggleHalfAsciiTypes(HALF_ASCII_UPPER), HALF_ASCII_LOWER);
  EXPECT_EQ(T13n::ToggleHalfAsciiTypes(HALF_ASCII_LOWER),
            HALF_ASCII_CAPITALIZED);
  EXPECT_EQ(T13n::ToggleHalfAsciiTypes(HALF_ASCII_CAPITALIZED), HALF_ASCII);
}

}  // namespace transliteration
}  // namespace mozc
