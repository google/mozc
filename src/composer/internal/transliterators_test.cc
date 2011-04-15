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

#include "composer/internal/transliterators.h"

#include "testing/base/public/gunit.h"

namespace mozc {
namespace composer {

TEST(TransliteratorsTest, ConversionStringSelector) {
  const TransliteratorInterface *t12r =
      Transliterators::GetConversionStringSelector();
  // "ず", "ず"
  EXPECT_EQ("\xe3\x81\x9a", t12r->Transliterate("zu", "\xe3\x81\x9a"));
  // "っk", "っk"
  EXPECT_EQ("\xe3\x81\xa3\x6b", t12r->Transliterate("kk", "\xe3\x81\xa3\x6b"));

  string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  // "ず"
  EXPECT_TRUE(t12r->Split(1, "zu", "\xe3\x81\x9a",
                          &raw_lhs, &raw_rhs,
                          &converted_lhs, &converted_rhs));
  EXPECT_EQ("zu", raw_lhs);
  EXPECT_EQ("", raw_rhs);
  // "ず"
  EXPECT_EQ("\xe3\x81\x9a", converted_lhs);
  EXPECT_EQ("", converted_rhs);

  // "っk"
  EXPECT_TRUE(t12r->Split(1, "kk", "\xe3\x81\xa3\x6b",
                          &raw_lhs, &raw_rhs,
                          &converted_lhs, &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  // "っ"
  EXPECT_EQ("\xe3\x81\xa3", converted_lhs);
  EXPECT_EQ("k", converted_rhs);

  // Ideally "kkk" should be separated into "っ" and "っk", but it's
  // not implemented yet.
  // "っっk"
  EXPECT_FALSE(t12r->Split(1, "kkk", "\xe3\x81\xa3\xe3\x81\xa3\x6b",
                           &raw_lhs, &raw_rhs,
                           &converted_lhs, &converted_rhs));
  // "っ"
  EXPECT_EQ("\xe3\x81\xa3", raw_lhs);
  // "っk"
  EXPECT_EQ("\xe3\x81\xa3\x6b", raw_rhs);
  // "っ"
  EXPECT_EQ("\xe3\x81\xa3", converted_lhs);
  // "っk"
  EXPECT_EQ("\xe3\x81\xa3\x6b", converted_rhs);
}

TEST(TransliteratorsTest, RawStringSelector) {
  const TransliteratorInterface *t12r =
      Transliterators::GetRawStringSelector();
  // "ず"
  EXPECT_EQ("zu", t12r->Transliterate("zu", "\xe3\x81\x9a"));
  // "っk"
  EXPECT_EQ("kk", t12r->Transliterate("kk", "\xe3\x81\xa3\x6b"));

  string raw_lhs, raw_rhs, converted_lhs, converted_rhs;
  // "ず"
  EXPECT_FALSE(t12r->Split(1, "zu", "\xe3\x81\x9a",
                           &raw_lhs, &raw_rhs,
                           &converted_lhs, &converted_rhs));
  EXPECT_EQ("z", raw_lhs);
  EXPECT_EQ("u", raw_rhs);
  EXPECT_EQ("z", converted_lhs);
  EXPECT_EQ("u", converted_rhs);

  // "っk"
  EXPECT_TRUE(t12r->Split(1, "kk", "\xe3\x81\xa3\x6b",
                          &raw_lhs, &raw_rhs,
                          &converted_lhs, &converted_rhs));
  EXPECT_EQ("k", raw_lhs);
  EXPECT_EQ("k", raw_rhs);
  // "っ"
  EXPECT_EQ("\xe3\x81\xa3", converted_lhs);
  EXPECT_EQ("k", converted_rhs);
}

}  // namespace composer
}  // namespace mozc
