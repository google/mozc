// Copyright 2010-2020, Google Inc.
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

#include "base/text_normalizer.h"

#include <string>

#include "base/util.h"
#include "testing/base/public/gunit.h"

namespace mozc {

TEST(TextNormalizerTest, NormalizeText) {
  std::string output;
  TextNormalizer::NormalizeText("めかぶ", &output);
  EXPECT_EQ("めかぶ", output);

  TextNormalizer::NormalizeText("ゔぁいおりん", &output);
  EXPECT_EQ("ゔぁいおりん", output);

  // "〜" is U+301C
  TextNormalizer::NormalizeText("ぐ〜ぐる", &output);
#ifdef OS_WIN
  EXPECT_EQ("ぐ～ぐる", output);  // "～" is U+FF5E
#else
  EXPECT_EQ("ぐ〜ぐる", output);  // "〜" is U+301C
#endif

  // "−" is U+2212
  TextNormalizer::NormalizeText("１−２−３", &output);
#ifdef OS_WIN
  EXPECT_EQ("１－２－３", output);  // "－" is U+FF0D
#else
  EXPECT_EQ("１−２−３", output);  // "−" is U+2212
#endif

  // "¥" is U+00A5
  TextNormalizer::NormalizeText("¥298", &output);
  // U+00A5 is no longer normalized.
  EXPECT_EQ("¥298", output);
}

}  // namespace mozc
