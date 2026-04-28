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

#include "rewriter/emoticon_rewriter.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "config/config_handler.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

void InitSegment(const absl::string_view key, const absl::string_view value,
                 Segments* segments) {
  segments->Clear();
  Segment* seg = segments->push_back_segment();
  seg->set_key(key);
  converter::Candidate* candidate = seg->add_candidate();
  candidate->value = key;
  candidate->content_key = key;
  candidate->content_value = value;

  // Fills other default candidates.
  for (int i = 0; i < 100; ++i) {
    converter::Candidate* candidate = seg->add_candidate();
    candidate->value = absl::StrCat("value", i);
    candidate->key = absl::StrCat("key", i);
  }
}

int GetEmoticonIndex(const Segments& segments) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const converter::Candidate& candidate = segments.segment(0).candidate(i);
    if (candidate.description.starts_with("顔文字")) {
      return i;
    }
  }
  return -1;
}

bool HasEmoticon(const Segments& segments) {
  return GetEmoticonIndex(segments) >= 0;
}

class EmoticonRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  testing::MockDataManager mock_data_manager_;
};

TEST_F(EmoticonRewriterTest, BasicTest) {
  const auto emoticon_rewriter = std::make_from_tuple<EmoticonRewriter>(
      mock_data_manager_.GetEmoticonRewriterData());

  config::Config config = config::ConfigHandler::DefaultConfig();
  {
    config.set_use_emoticon_conversion(true);
    const ConversionRequest request =
        ConversionRequestBuilder().SetConfig(config).Build();

    Segments segments;
    InitSegment("test", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    InitSegment("かお", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));
    EXPECT_GT(GetEmoticonIndex(segments), 6);

    InitSegment("かおもじ", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));
    EXPECT_GT(GetEmoticonIndex(segments), 6);

    InitSegment("にこにこ", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));
    EXPECT_LE(GetEmoticonIndex(segments), 6);

    InitSegment("ふくわらい", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));
    EXPECT_LE(GetEmoticonIndex(segments), 6);
  }

  {
    config.set_use_emoticon_conversion(false);
    const ConversionRequest request =
        ConversionRequestBuilder().SetConfig(config).Build();

    Segments segments;
    InitSegment("test", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    InitSegment("かお", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    InitSegment("かおもじ", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    InitSegment("にこにこ", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    InitSegment("ふくわらい", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));
  }
}

TEST_F(EmoticonRewriterTest, RandomTest) {
  const auto emoticon_rewriter = std::make_from_tuple<EmoticonRewriter>(
      mock_data_manager_.GetEmoticonRewriterData());

  config::Config config = config::ConfigHandler::DefaultConfig();
  config.set_use_emoticon_conversion(true);
  const ConversionRequest request =
      ConversionRequestBuilder().SetConfig(config).Build();

  std::set<std::string> variants;
  for (int i = 0; i < 100; ++i) {
    Segments segments;
    InitSegment("ふくわらい", "test", &segments);
    emoticon_rewriter.Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));
    // random candidate is inserted at 4-th position.
    variants.emplace(segments.segment(0).candidate(4).value);
  }
  EXPECT_GT(variants.size(), 1);
}

TEST_F(EmoticonRewriterTest, MobileEnvironmentTest) {
  const auto emoticon_rewriter = std::make_from_tuple<EmoticonRewriter>(
      mock_data_manager_.GetEmoticonRewriterData());

  commands::Request request;

  {
    request.set_mixed_conversion(true);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(emoticon_rewriter.capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(emoticon_rewriter.capability(convreq),
              RewriterInterface::CONVERSION);
  }
}

}  // namespace
}  // namespace mozc
