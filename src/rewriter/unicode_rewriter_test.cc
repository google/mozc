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

#include <cstdlib>
#include <string>
#include "base/util.h"
#include "config/config.pb.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "rewriter/unicode_rewriter.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

void AddSegment(const string &key, const string &value, Segments *segments) {
  Segment *seg = segments->add_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  seg->set_key(key);
  candidate->content_key = key;
  candidate->value = value;
  candidate->content_value = value;
}

void InitSegments(const string &key, const string &value, Segments *segments) {
  segments->Clear();
  AddSegment(key, value, segments);
}

bool ContainCandidate(const Segments &segments, const string &candidate) {
  const Segment &segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (candidate == segment.candidate(i).value) {
      return true;
    }
  }
  return false;
}

}  // namespace

class UnicodeRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    converter_ = ConverterFactory::GetConverter();
  }

  const ConverterInterface *converter_;
};

TEST_F(UnicodeRewriterTest, UnicodeConvertionTest) {
  Segments segments;
  UnicodeRewriter rewriter(converter_);
  const ConversionRequest request;

  struct UCS4UTF8Data {
    const char *ucs4;
    const char *utf8;
  };

  const UCS4UTF8Data kUcs4Utf8Data[] = {
    // Hiragana
    { "U+3042", "\xE3\x81\x82" },  // "あ"
    { "U+3044", "\xE3\x81\x84" },  // "い"
    { "U+3046", "\xE3\x81\x86" },  // "う"
    { "U+3048", "\xE3\x81\x88" },  // "え"
    { "U+304A", "\xE3\x81\x8A" },  // "お"

    // Katakana
    { "U+30A2", "\xE3\x82\xA2" },  // "ア"
    { "U+30A4", "\xE3\x82\xA4" },  // "イ"
    { "U+30A6", "\xE3\x82\xA6" },  // "ウ"
    { "U+30A8", "\xE3\x82\xA8" },  // "エ"
    { "U+30AA", "\xE3\x82\xAA" },  // "オ"

    // half-Katakana
    { "U+FF71", "\xEF\xBD\xB1" },  // "ｱ"
    { "U+FF72", "\xEF\xBD\xB2" },  // "ｲ"
    { "U+FF73", "\xEF\xBD\xB3" },  // "ｳ"
    { "U+FF74", "\xEF\xBD\xB4" },  // "ｴ"
    { "U+FF75", "\xEF\xBD\xB5" },  // "ｵ"

    // CJK
    { "U+611B", "\xE6\x84\x9B" },  // "愛"
    { "U+690D", "\xE6\xA4\x8D" },  // "植"
    { "U+7537", "\xE7\x94\xB7" },  // "男"

    // Other types (Oriya script)
    { "U+0B00", "\xE0\xAC\x80" },  // "଀"
    { "U+0B01", "\xE0\xAC\x81" },  // "ଁ"
    { "U+0B02", "\xE0\xAC\x82" },  // "ଂ"

    // Other types (Arabic)
    { "U+0600", "\xD8\x80" },  // "؀"
    { "U+0601", "\xD8\x81" },  // "؁"
    { "U+0602", "\xD8\x82" },  // "؂"

    // Latin-1 support
    { "U+00A0", "\xC2\xA0" },  // " " (nbsp)
    { "U+00A1", "\xC2\xA1" },  // "¡"
  };

  const char* kMozcUnsupportedUtf8[] = {
    // Control characters
    "U+0000", "U+001F", "U+007F", "U+0080", "U+009F",
    // Out of Unicode
    "U+110000",
    // Bidirectional text
    "U+200E", "U+202D",
  };

  // All ascii code would be accepted.
  for (uint32 ascii = 0x20; ascii < 0x7F; ++ascii) {
    const string ucs4 = Util::StringPrintf("U+00%02X", ascii);
    InitSegments(ucs4, ucs4, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(ascii, segments.segment(0).candidate(0).value.at(0));
  }

  // Mozc accepts Japanese characters
  for (size_t i = 0; i < ARRAYSIZE(kUcs4Utf8Data); ++i) {
    InitSegments(kUcs4Utf8Data[i].ucs4, kUcs4Utf8Data[i].ucs4, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, kUcs4Utf8Data[i].utf8));
  }

  // Mozc does not accept other characters
  for (size_t i = 0; i < ARRAYSIZE(kMozcUnsupportedUtf8); ++i) {
    InitSegments(kMozcUnsupportedUtf8[i], kMozcUnsupportedUtf8[i], &segments);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  // invlaid style input
  InitSegments("U+1234567", "U+12345678", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("U+XYZ", "U+XYZ", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("12345", "12345", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("U12345", "U12345", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));
}

TEST_F(UnicodeRewriterTest, MultipleSegment) {
  Segments segments;
  UnicodeRewriter rewriter(converter_);
  const ConversionRequest request;

  // Multiple segments are combined.
  InitSegments("U+0", "U+0", &segments);
  AddSegment("02", "02", &segments);
  AddSegment("0", "0", &segments);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(' ', segments.conversion_segment(0).candidate(0).value.at(0));

  // If the segments is already resized, returns false.
  InitSegments("U+0020", "U+0020", &segments);
  AddSegment("U+0020", "U+0020", &segments);
  segments.set_resized(true);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  // History segment has to be ignored.
  // In this case 1st segment is HISTORY
  // so this rewriting returns true.
  InitSegments("U+0020", "U+0020", &segments);
  AddSegment("U+0020", "U+0020", &segments);
  segments.set_resized(true);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  EXPECT_EQ(' ', segments.conversion_segment(0).candidate(0).value.at(0));
}

TEST_F(UnicodeRewriterTest, RewriteToUnicodeCharFormat) {
  UnicodeRewriter rewriter(converter_);
  {  // Typical case
    composer::Composer composer;
    composer.set_source_text("A");
    ConversionRequest request(&composer);

    Segments segments;
    AddSegment("A", "A", &segments);

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "U+0041"));
  }

  {  // If source_text is not set, this rewrite is not triggered.
    composer::Composer composer;
    ConversionRequest request(&composer);

    Segments segments;
    AddSegment("A", "A", &segments);

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_FALSE(ContainCandidate(segments, "U+0041"));
  }

  {  // If source_text is not a single character, this rewrite is not
     // triggered.
    composer::Composer composer;
    composer.set_source_text("AB");
    ConversionRequest request(&composer);

    Segments segments;
    AddSegment("AB", "AB", &segments);

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  {  // Multibyte character is also supported.
    composer::Composer composer;
    composer.set_source_text("\xE6\x84\x9B");  // "愛"
    ConversionRequest request(&composer);

    Segments segments;
    AddSegment("\xE3\x81\x82\xE3\x81\x84", "\xE6\x84\x9B", &segments);

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "U+611B"));
  }
}
}  // namespace mozc
