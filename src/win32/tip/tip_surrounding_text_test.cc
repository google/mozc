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

#include "win32/tip/tip_surrounding_text.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

TEST(TipSurroundingTextUtilTest, MeasureCharactersBackward) {
  {
    constexpr char kSource[] = "abcde";
    std::wstring source;
    Util::Utf8ToWide(kSource, &source);
    size_t characters_in_utf16 = 0;
    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        source, 0, &characters_in_utf16));
    EXPECT_EQ(0, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        source, 1, &characters_in_utf16));
    EXPECT_EQ(1, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        source, 5, &characters_in_utf16));
    EXPECT_EQ(5, characters_in_utf16);

    EXPECT_FALSE(TipSurroundingTextUtil::MeasureCharactersBackward(
        source, 6, &characters_in_utf16));
  }
  {
    constexpr char kSource[] = "𠮟咤";
    std::wstring source;
    Util::Utf8ToWide(kSource, &source);
    size_t characters_in_utf16 = 0;
    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        source, 1, &characters_in_utf16));
    EXPECT_EQ(1, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        source, 2, &characters_in_utf16));
    EXPECT_EQ(3, characters_in_utf16);

    EXPECT_FALSE(TipSurroundingTextUtil::MeasureCharactersBackward(
        source, 3, &characters_in_utf16));
  }

  const wchar_t kHighSurrogateStart = 0xd800;
  const wchar_t kLowSurrogateStart = 0xdc00;

  // Only Low surrogate.
  {
    const wchar_t kSource[] = {kLowSurrogateStart, kLowSurrogateStart, L'\0'};
    size_t characters_in_utf16 = 0;

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 0, &characters_in_utf16));
    EXPECT_EQ(0, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 1, &characters_in_utf16));
    EXPECT_EQ(1, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 2, &characters_in_utf16));
    EXPECT_EQ(2, characters_in_utf16);

    EXPECT_FALSE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 3, &characters_in_utf16));
  }

  // Only High surrogate.
  {
    const wchar_t kSource[] = {kHighSurrogateStart, kHighSurrogateStart, L'\0'};
    size_t characters_in_utf16 = 0;
    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 0, &characters_in_utf16));
    EXPECT_EQ(0, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 1, &characters_in_utf16));
    EXPECT_EQ(1, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 2, &characters_in_utf16));
    EXPECT_EQ(2, characters_in_utf16);

    EXPECT_FALSE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 3, &characters_in_utf16));
  }

  // An isolated high surrogate and one surrogate pair.
  {
    const wchar_t kSource[] = {kHighSurrogateStart, kHighSurrogateStart,
                               kLowSurrogateStart, L'\0'};
    size_t characters_in_utf16 = 0;
    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 0, &characters_in_utf16));
    EXPECT_EQ(0, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 1, &characters_in_utf16));
    EXPECT_EQ(2, characters_in_utf16);

    EXPECT_TRUE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 2, &characters_in_utf16));
    EXPECT_EQ(3, characters_in_utf16);

    EXPECT_FALSE(TipSurroundingTextUtil::MeasureCharactersBackward(
        kSource, 3, &characters_in_utf16));
  }
}

}  // namespace
}  // namespace tsf
}  // namespace win32
}  // namespace mozc
