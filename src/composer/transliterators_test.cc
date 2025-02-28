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

#include "composer/transliterators.h"

#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace composer {
namespace {

TEST(TransliteratorsTest, ConversionStringSelector) {
  const internal::TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::CONVERSION_STRING);
  EXPECT_EQ(t12r->Transliterate("zu", "ず"), "ず");
  EXPECT_EQ(t12r->Transliterate("kk", "っk"), "っk");

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_TRUE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "zu");
  EXPECT_EQ(raw_rhs, "");
  EXPECT_EQ(converted_lhs, "ず");
  EXPECT_EQ(converted_rhs, "");

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "k");
  EXPECT_EQ(raw_rhs, "k");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "k");

  // Ideally "kkk" should be separated into "っ" and "っk", but it's
  // not implemented yet.
  EXPECT_FALSE(t12r->Split(1, "kkk", "っっk", &raw_lhs, &raw_rhs,
                           &converted_lhs, &converted_rhs));
  EXPECT_EQ(raw_lhs, "っ");
  EXPECT_EQ(raw_rhs, "っk");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "っk");
}

TEST(TransliteratorsTest, RawStringSelector) {
  const internal::TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::RAW_STRING);
  EXPECT_EQ(t12r->Transliterate("zu", "ず"), "zu");
  EXPECT_EQ(t12r->Transliterate("kk", "っk"), "kk");

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_FALSE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                           &converted_rhs));
  EXPECT_EQ(raw_lhs, "z");
  EXPECT_EQ(raw_rhs, "u");
  EXPECT_EQ(converted_lhs, "z");
  EXPECT_EQ(converted_rhs, "u");

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "k");
  EXPECT_EQ(raw_rhs, "k");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "k");
}

TEST(TransliteratorsTest, HiraganaTransliterator) {
  const internal::TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::HIRAGANA);
  EXPECT_EQ(t12r->Transliterate("zu", "ず"), "ず");
  // Half width "k" is transliterated into full width "ｋ".
  EXPECT_EQ(t12r->Transliterate("kk", "っk"), "っｋ");

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_TRUE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "zu");
  EXPECT_EQ(raw_rhs, "");
  EXPECT_EQ(converted_lhs, "ず");
  EXPECT_EQ(converted_rhs, "");

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "k");
  EXPECT_EQ(raw_rhs, "k");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "k");

  // Ideally "kkk" should be separated into "っ" and "っk", but it's
  // not implemented yet.
  EXPECT_FALSE(t12r->Split(1, "kkk", "っっk", &raw_lhs, &raw_rhs,
                           &converted_lhs, &converted_rhs));
  EXPECT_EQ(raw_lhs, "っ");
  EXPECT_EQ(raw_rhs, "っk");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "っk");

  // "　"(full-width space), "　"(full-width space)
  EXPECT_EQ(t12r->Transliterate(" ", "　"), "　");
  // " "(half-width space), "　"(full-width space)
  EXPECT_EQ(t12r->Transliterate(" ", " "), "　");
}

TEST(TransliteratorsTest, FullKatakanaTransliterator) {
  const internal::TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::FULL_KATAKANA);
  EXPECT_EQ(t12r->Transliterate("zu", "ず"), "ズ");
  // Half width "k" is transliterated into full width "ｋ".
  EXPECT_EQ(t12r->Transliterate("kk", "っk"), "ッｋ");

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_TRUE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "zu");
  EXPECT_EQ(raw_rhs, "");
  EXPECT_EQ(converted_lhs, "ず");
  EXPECT_EQ(converted_rhs, "");

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "k");
  EXPECT_EQ(raw_rhs, "k");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "k");

  // "　"(full-width space), "　"(full-width space)
  EXPECT_EQ(t12r->Transliterate(" ", "　"), "　");
  // " "(half-width space), "　"(full-width space)
  EXPECT_EQ(t12r->Transliterate(" ", " "), "　");
}

TEST(TransliteratorsTest, HalfKatakanaTransliterator) {
  const internal::TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::HALF_KATAKANA);
  EXPECT_EQ(t12r->Transliterate("zu", "ず"), "ｽﾞ");
  // Half width "k" remains in the current implementation (can be changed).
  EXPECT_EQ(t12r->Transliterate("kk", "っk"), "ｯk");

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  // "ｽﾞ" is split to "ｽ" and "ﾞ".
  EXPECT_FALSE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                           &converted_rhs));
  EXPECT_EQ(raw_lhs, "す");
  EXPECT_EQ(raw_rhs, "゛");
  EXPECT_EQ(converted_lhs, "す");
  EXPECT_EQ(converted_rhs, "゛");

  EXPECT_TRUE(t12r->Split(2, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "zu");
  EXPECT_EQ(raw_rhs, "");
  EXPECT_EQ(converted_lhs, "ず");
  EXPECT_EQ(converted_rhs, "");

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "k");
  EXPECT_EQ(raw_rhs, "k");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "k");

  // "　"(full-width space), "　"(half-width space)
  EXPECT_EQ(t12r->Transliterate(" ", "　"), " ");
  // " "(half-width space), "　"(half-width space)
  EXPECT_EQ(t12r->Transliterate(" ", " "), " ");
}

TEST(TransliteratorsTest, HalfAsciiTransliterator) {
  const internal::TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::HALF_ASCII);
  EXPECT_EQ(t12r->Transliterate("zu", "ず"), "zu");
  EXPECT_EQ(t12r->Transliterate("kk", "っk"), "kk");

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_FALSE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                           &converted_rhs));
  EXPECT_EQ(raw_lhs, "z");
  EXPECT_EQ(raw_rhs, "u");
  EXPECT_EQ(converted_lhs, "z");
  EXPECT_EQ(converted_rhs, "u");

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "k");
  EXPECT_EQ(raw_rhs, "k");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "k");

  // "　"(full-width space), "　"(half-width space)
  EXPECT_EQ(t12r->Transliterate(" ", "　"), " ");
  // " "(half-width space), "　"(half-width space)
  EXPECT_EQ(t12r->Transliterate(" ", " "), " ");
}

TEST(TransliteratorsTest, FullAsciiTransliterator) {
  const internal::TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::FULL_ASCII);
  EXPECT_EQ(t12r->Transliterate("zu", "ず"), "ｚｕ");
  EXPECT_EQ(t12r->Transliterate("kk", "っk"), "ｋｋ");

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_FALSE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                           &converted_rhs));
  EXPECT_EQ(raw_lhs, "z");
  EXPECT_EQ(raw_rhs, "u");
  EXPECT_EQ(converted_lhs, "z");
  EXPECT_EQ(converted_rhs, "u");

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ(raw_lhs, "k");
  EXPECT_EQ(raw_rhs, "k");
  EXPECT_EQ(converted_lhs, "っ");
  EXPECT_EQ(converted_rhs, "k");

  // "　"(full-width space), "　"(full-width space)
  EXPECT_EQ(t12r->Transliterate(" ", "　"), "　");
  // " "(half-width space), "　"(full-width space)
  EXPECT_EQ(t12r->Transliterate(" ", " "), "　");
}

}  // namespace
}  // namespace composer
}  // namespace mozc
