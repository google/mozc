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

#include "composer/internal/transliterators.h"

#include "testing/base/public/gunit.h"

namespace mozc {
namespace composer {
namespace {

TEST(TransliteratorsTest, ConversionStringSelector) {
  const TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::CONVERSION_STRING);
  EXPECT_EQ("ず", t12r->Transliterate("zu", "ず"));
  EXPECT_EQ("っk", t12r->Transliterate("kk", "っk"));

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_TRUE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("zu", raw_lhs);
  EXPECT_EQ("", raw_rhs);
  EXPECT_EQ("ず", converted_lhs);
  EXPECT_EQ("", converted_rhs);

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("k", converted_rhs);

  // Ideally "kkk" should be separated into "っ" and "っk", but it's
  // not implemented yet.
  EXPECT_FALSE(t12r->Split(1, "kkk", "っっk", &raw_lhs, &raw_rhs,
                           &converted_lhs, &converted_rhs));
  EXPECT_EQ("っ", raw_lhs);
  EXPECT_EQ("っk", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("っk", converted_rhs);
}

TEST(TransliteratorsTest, RawStringSelector) {
  const TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::RAW_STRING);
  EXPECT_EQ("zu", t12r->Transliterate("zu", "ず"));
  EXPECT_EQ("kk", t12r->Transliterate("kk", "っk"));

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_FALSE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                           &converted_rhs));
  EXPECT_EQ("z", raw_lhs);
  EXPECT_EQ("u", raw_rhs);
  EXPECT_EQ("z", converted_lhs);
  EXPECT_EQ("u", converted_rhs);

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("k", converted_rhs);
}

TEST(TransliteratorsTest, HiraganaTransliterator) {
  const TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::HIRAGANA);
  EXPECT_EQ("ず", t12r->Transliterate("zu", "ず"));
  // Half width "k" is transliterated into full width "ｋ".
  EXPECT_EQ("っｋ", t12r->Transliterate("kk", "っk"));

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_TRUE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("zu", raw_lhs);
  EXPECT_EQ("", raw_rhs);
  EXPECT_EQ("ず", converted_lhs);
  EXPECT_EQ("", converted_rhs);

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("k", converted_rhs);

  // Ideally "kkk" should be separated into "っ" and "っk", but it's
  // not implemented yet.
  EXPECT_FALSE(t12r->Split(1, "kkk", "っっk", &raw_lhs, &raw_rhs,
                           &converted_lhs, &converted_rhs));
  EXPECT_EQ("っ", raw_lhs);
  EXPECT_EQ("っk", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("っk", converted_rhs);

  // "　"(full-width space), "　"(full-width space)
  EXPECT_EQ("　", t12r->Transliterate(" ", "　"));
  // " "(half-width space), "　"(full-width space)
  EXPECT_EQ("　", t12r->Transliterate(" ", " "));
}

TEST(TransliteratorsTest, FullKatakanaTransliterator) {
  const TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::FULL_KATAKANA);
  EXPECT_EQ("ズ", t12r->Transliterate("zu", "ず"));
  // Half width "k" is transliterated into full width "ｋ".
  EXPECT_EQ("ッｋ", t12r->Transliterate("kk", "っk"));

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_TRUE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("zu", raw_lhs);
  EXPECT_EQ("", raw_rhs);
  EXPECT_EQ("ず", converted_lhs);
  EXPECT_EQ("", converted_rhs);

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("k", converted_rhs);

  // "　"(full-width space), "　"(full-width space)
  EXPECT_EQ("　", t12r->Transliterate(" ", "　"));
  // " "(half-width space), "　"(full-width space)
  EXPECT_EQ("　", t12r->Transliterate(" ", " "));
}

TEST(TransliteratorsTest, HalfKatakanaTransliterator) {
  const TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::HALF_KATAKANA);
  EXPECT_EQ("ｽﾞ", t12r->Transliterate("zu", "ず"));
  // Half width "k" remains in the current implementation (can be changed).
  EXPECT_EQ("ｯk", t12r->Transliterate("kk", "っk"));

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  // "ｽﾞ" is split to "ｽ" and "ﾞ".
  EXPECT_FALSE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                           &converted_rhs));
  EXPECT_EQ("す", raw_lhs);
  EXPECT_EQ("゛", raw_rhs);
  EXPECT_EQ("す", converted_lhs);
  EXPECT_EQ("゛", converted_rhs);

  EXPECT_TRUE(t12r->Split(2, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("zu", raw_lhs);
  EXPECT_EQ("", raw_rhs);
  EXPECT_EQ("ず", converted_lhs);
  EXPECT_EQ("", converted_rhs);

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("k", converted_rhs);

  // "　"(full-width space), "　"(half-width space)
  EXPECT_EQ(" ", t12r->Transliterate(" ", "　"));
  // " "(half-width space), "　"(half-width space)
  EXPECT_EQ(" ", t12r->Transliterate(" ", " "));
}

TEST(TransliteratorsTest, HalfAsciiTransliterator) {
  const TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::HALF_ASCII);
  EXPECT_EQ("zu", t12r->Transliterate("zu", "ず"));
  EXPECT_EQ("kk", t12r->Transliterate("kk", "っk"));

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_FALSE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                           &converted_rhs));
  EXPECT_EQ("z", raw_lhs);
  EXPECT_EQ("u", raw_rhs);
  EXPECT_EQ("z", converted_lhs);
  EXPECT_EQ("u", converted_rhs);

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("k", converted_rhs);

  // "　"(full-width space), "　"(half-width space)
  EXPECT_EQ(" ", t12r->Transliterate(" ", "　"));
  // " "(half-width space), "　"(half-width space)
  EXPECT_EQ(" ", t12r->Transliterate(" ", " "));
}

TEST(TransliteratorsTest, FullAsciiTransliterator) {
  const TransliteratorInterface *t12r =
      Transliterators::GetTransliterator(Transliterators::FULL_ASCII);
  EXPECT_EQ("ｚｕ", t12r->Transliterate("zu", "ず"));
  EXPECT_EQ("ｋｋ", t12r->Transliterate("kk", "っk"));

  std::string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  EXPECT_FALSE(t12r->Split(1, "zu", "ず", &raw_lhs, &raw_rhs, &converted_lhs,
                           &converted_rhs));
  EXPECT_EQ("z", raw_lhs);
  EXPECT_EQ("u", raw_rhs);
  EXPECT_EQ("z", converted_lhs);
  EXPECT_EQ("u", converted_rhs);

  EXPECT_TRUE(t12r->Split(1, "kk", "っk", &raw_lhs, &raw_rhs, &converted_lhs,
                          &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  EXPECT_EQ("っ", converted_lhs);
  EXPECT_EQ("k", converted_rhs);

  // "　"(full-width space), "　"(full-width space)
  EXPECT_EQ("　", t12r->Transliterate(" ", "　"));
  // " "(half-width space), "　"(full-width space)
  EXPECT_EQ("　", t12r->Transliterate(" ", " "));
}

}  // namespace
}  // namespace composer
}  // namespace mozc
