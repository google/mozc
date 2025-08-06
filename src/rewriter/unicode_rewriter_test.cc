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

#include "rewriter/unicode_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "engine/engine.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

void AddSegment(const absl::string_view key, const absl::string_view value,
                Segments *segments) {
  Segment *seg = segments->add_segment();
  converter::Candidate *candidate = seg->add_candidate();
  seg->set_key(key);
  candidate->content_key = std::string(key);
  candidate->value = std::string(value);
  candidate->content_value = std::string(value);
}

void InitSegments(const absl::string_view key, const absl::string_view value,
                  Segments *segments) {
  segments->Clear();
  AddSegment(key, value, segments);
}

bool ContainCandidate(const Segments &segments,
                      const absl::string_view candidate) {
  const Segment &segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (candidate == segment.candidate(i).value) {
      return true;
    }
  }
  return false;
}

class UnicodeRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override { engine_ = MockDataEngineFactory::Create().value(); }

  std::unique_ptr<Engine> engine_;
  const commands::Request &default_request() const { return default_request_; }
  const config::Config &default_config() const { return default_config_; }
  const commands::Context &default_context() const { return default_context_; }

 private:
  const commands::Request default_request_;
  const config::Config default_config_;
  const commands::Context default_context_;
};

TEST_F(UnicodeRewriterTest, UnicodeConversionTest) {
  Segments segments;
  UnicodeRewriter rewriter;

  struct UCS4UTF8Data {
    absl::string_view codepoint;
    absl::string_view utf8;
  };

  constexpr UCS4UTF8Data kCodepointUtf8Data[] = {
      // Hiragana
      {"U+3042", "あ"},
      {"U+3044", "い"},
      {"U+3046", "う"},
      {"U+3048", "え"},
      {"U+304A", "お"},

      // Katakana
      {"U+30A2", "ア"},
      {"U+30A4", "イ"},
      {"U+30A6", "ウ"},
      {"U+30A8", "エ"},
      {"U+30AA", "オ"},

      // half-Katakana
      {"U+FF71", "ｱ"},
      {"U+FF72", "ｲ"},
      {"U+FF73", "ｳ"},
      {"U+FF74", "ｴ"},
      {"U+FF75", "ｵ"},

      // CJK
      {"U+611B", "愛"},
      {"U+690D", "植"},
      {"U+7537", "男"},

      // Other types (Oriya script)
      {"U+0B00", "\xE0\xAC\x80"},  // "଀"
      {"U+0B01", "ଁ"},              // "ଁ"
      {"U+0B02", "ଂ"},             // "ଂ"

      // Other types (Arabic)
      {"U+0600", "؀"},
      {"U+0601", "؁"},
      {"U+0602", "؂"},

      // Latin-1 support
      {"U+00A0", "\xC2\xA0"},  // " " (nbsp)
      {"U+00A1", "¡"},
  };

  constexpr absl::string_view kMozcUnsupportedUtf8[] = {
      // Control characters
      "U+0000",
      "U+001F",
      "U+007F",
      "U+0080",
      "U+009F",
      // Out of Unicode
      "U+110000",
      // Bidirectional text
      "U+200E",
      "U+202D",
  };

  // All ascii code would be accepted.
  for (uint32_t ascii = 0x20; ascii < 0x7F; ++ascii) {
    const std::string codepoint = absl::StrFormat("U+00%02X", ascii);
    InitSegments(codepoint, codepoint, &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey(codepoint).Build();
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(segments.segment(0).candidate(0).value.at(0), ascii);
    EXPECT_TRUE(segments.segment(0).candidate(0).attributes &
                converter::Attribute::NO_MODIFICATION);
  }

  // Mozc accepts Japanese characters
  for (size_t i = 0; i < std::size(kCodepointUtf8Data); ++i) {
    absl::string_view codepoint = kCodepointUtf8Data[i].codepoint;
    InitSegments(codepoint, codepoint, &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey(codepoint).Build();
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, kCodepointUtf8Data[i].utf8));
    EXPECT_TRUE(segments.segment(0).candidate(0).attributes &
                converter::Attribute::NO_MODIFICATION);
  }

  // Mozc does not accept other characters
  for (size_t i = 0; i < std::size(kMozcUnsupportedUtf8); ++i) {
    InitSegments(kMozcUnsupportedUtf8[i], kMozcUnsupportedUtf8[i], &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey(kMozcUnsupportedUtf8[i]).Build();
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  // Invalid style input
  absl::string_view invalid_key1 = "U+123456789ABCDEF0";
  InitSegments(invalid_key1, invalid_key1, &segments);
  const ConversionRequest request1 =
      ConversionRequestBuilder().SetKey(invalid_key1).Build();
  EXPECT_FALSE(rewriter.Rewrite(request1, &segments));

  absl::string_view invalid_key2 = "U+12345678";
  InitSegments(invalid_key2, invalid_key2, &segments);
  const ConversionRequest request2 =
      ConversionRequestBuilder().SetKey(invalid_key2).Build();
  EXPECT_FALSE(rewriter.Rewrite(request2, &segments));

  absl::string_view invalid_key3 = "U+XYZ";
  InitSegments(invalid_key3, invalid_key3, &segments);
  const ConversionRequest request3 =
      ConversionRequestBuilder().SetKey(invalid_key3).Build();
  EXPECT_FALSE(rewriter.Rewrite(request3, &segments));

  absl::string_view invalid_key4 = "12345";
  InitSegments(invalid_key4, invalid_key4, &segments);
  const ConversionRequest request4 =
      ConversionRequestBuilder().SetKey(invalid_key4).Build();
  EXPECT_FALSE(rewriter.Rewrite(request4, &segments));

  absl::string_view invalid_key5 = "U12345";
  InitSegments(invalid_key5, invalid_key5, &segments);
  const ConversionRequest request5 =
      ConversionRequestBuilder().SetKey(invalid_key5).Build();
  EXPECT_FALSE(rewriter.Rewrite(request5, &segments));
}

TEST_F(UnicodeRewriterTest, MultipleSegment) {
  UnicodeRewriter rewriter;

  {
    // Multiple segments to be combined.
    Segments segments;
    InitSegments("U+0", "U+0", &segments);
    AddSegment("02", "02", &segments);
    AddSegment("0", "0", &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey("U+0020").Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_TRUE(resize_request.has_value());
    EXPECT_EQ(resize_request->segment_index, 0);
    EXPECT_EQ(resize_request->segment_sizes[0], 6);
    EXPECT_EQ(resize_request->segment_sizes[1], 0);
  }

  {
    // The segments is already resized.
    Segments segments;
    InitSegments("U+0", "U+0", &segments);
    AddSegment("02", "02", &segments);
    AddSegment("0", "0", &segments);
    segments.set_resized(true);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey("U+0020").Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
  }

  {
    // The size of segments is one.
    Segments segments;
    InitSegments("U+0020", "U+0020", &segments);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey("U+0020").Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value.at(0), ' ');
  }

  {
    // History segment has to be ignored.
    Segments segments;
    InitSegments("U+0", "U+0", &segments);
    AddSegment("02", "02", &segments);
    AddSegment("0", "0", &segments);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey("020").Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  {
    // History segment has to be ignored.
    // In this case 1st segment is HISTORY
    // so this rewriting returns true.
    Segments segments;
    InitSegments("U+0020", "U+0020", &segments);
    AddSegment("U+0020", "U+0020", &segments);
    segments.set_resized(true);
    segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
    const ConversionRequest request =
        ConversionRequestBuilder().SetKey("U+0020").Build();
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        rewriter.CheckResizeSegmentsRequest(request, segments);
    EXPECT_FALSE(resize_request.has_value());
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value.at(0), ' ');
  }
}

TEST_F(UnicodeRewriterTest, RewriteToUnicodeCharFormat) {
  UnicodeRewriter rewriter;
  {  // Typical case
    composer::Composer composer(default_request(), default_config());
    composer.set_source_text("A");
    const ConversionRequest request =
        ConversionRequestBuilder().SetComposer(composer).Build();

    Segments segments;
    AddSegment("A", "A", &segments);

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "U+0041"));
  }

  {  // If source_text is not set, this rewrite is not triggered.
    composer::Composer composer(default_request(), default_config());
    const ConversionRequest request =
        ConversionRequestBuilder().SetComposer(composer).Build();

    Segments segments;
    AddSegment("A", "A", &segments);

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_FALSE(ContainCandidate(segments, "U+0041"));
  }

  {  // If source_text is not a single character, this rewrite is not
     // triggered.
    composer::Composer composer(default_request(), default_config());
    composer.set_source_text("AB");
    const ConversionRequest request =
        ConversionRequestBuilder().SetComposer(composer).Build();

    Segments segments;
    AddSegment("AB", "AB", &segments);

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  {  // Multibyte character is also supported.
    composer::Composer composer(default_request(), default_config());
    composer.set_source_text("愛");
    const ConversionRequest request =
        ConversionRequestBuilder().SetComposer(composer).Build();

    Segments segments;
    AddSegment("あい", "愛", &segments);

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "U+611B"));
  }
}

}  // namespace
}  // namespace mozc
