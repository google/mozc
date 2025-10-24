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

#include "rewriter/correction_rewriter.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "config/config_handler.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "engine/supplemental_model_mock.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

using ::testing::_;

namespace mozc {
namespace {

Segment* AddSegment(const absl::string_view key, Segments* segments) {
  Segment* segment = segments->push_back_segment();
  segment->set_key(key);
  return segment;
}

converter::Candidate* AddCandidate(const absl::string_view key,
                                   const absl::string_view value,
                                   const absl::string_view content_key,
                                   const absl::string_view content_value,
                                   Segment* segment) {
  converter::Candidate* candidate = segment->add_candidate();
  candidate->key = key;
  candidate->value = value;
  candidate->content_key = content_key;
  candidate->content_value = content_value;
  return candidate;
}
}  // namespace

class CorrectionRewriterTest : public ::testing::Test {
 protected:
  static ConversionRequest ConvReq(const config::Config& config) {
    return ConversionRequestBuilder().SetConfig(config).Build();
  }

  static ConversionRequest ConvReq(const config::Config& config,
                                   const commands::Request& request) {
    return ConversionRequestBuilder()
        .SetConfig(config)
        .SetRequest(request)
        .Build();
  }

  std::unique_ptr<CorrectionRewriter> CreateRewriter(
      const engine::Modules& modules,
      absl::Span<const absl::string_view> values,
      absl::Span<const absl::string_view> errors,
      absl::Span<const absl::string_view> corrections) {
    return std::make_unique<CorrectionRewriter>(
        modules, SerializedStringArray::SerializeToBuffer(values, &values_buf_),
        SerializedStringArray::SerializeToBuffer(errors, &errors_buf_),
        SerializedStringArray::SerializeToBuffer(corrections,
                                                 &corrections_buf_));
  }

 private:
  std::unique_ptr<uint32_t[]> values_buf_;
  std::unique_ptr<uint32_t[]> errors_buf_;
  std::unique_ptr<uint32_t[]> corrections_buf_;
};

TEST_F(CorrectionRewriterTest, RewriteTest) {
  std::unique_ptr<engine::Modules> modules =
      engine::ModulesPresetBuilder()
          .Build(std::make_unique<testing::MockDataManager>())
          .value();

  std::unique_ptr<CorrectionRewriter> rewriter =
      CreateRewriter(*modules, {"TSUKIGIME"}, {"gekkyoku"}, {"tsukigime"});

  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config.set_use_spelling_correction(true);

  EXPECT_EQ(rewriter->capability(ConvReq(config)), RewriterInterface::ALL);

  Segments segments;
  Segment* segment = AddSegment("gekkyokuwo", &segments);
  converter::Candidate* candidate = AddCandidate(
      "gekkyokuwo", "TSUKIGIMEwo", "gekkyoku", "TSUKIGIME", segment);
  candidate->attributes |= converter::Attribute::RERANKED;

  AddCandidate("gekkyokuwo", "GEKKYOKUwo", "gekkyoku", "GEKKYOKU", segment);
  converter::Candidate* candidate2 =
      AddCandidate("かびばら", "かびばら", "カピバラ", "カピバラ", segment);
  candidate2->attributes |= converter::Attribute::SPELLING_CORRECTION;

  config.set_use_spelling_correction(false);

  const ConversionRequest convreq1 = ConvReq(config);
  EXPECT_FALSE(rewriter->Rewrite(convreq1, &segments));

  config.set_use_spelling_correction(true);
  const ConversionRequest convreq2 = ConvReq(config);
  EXPECT_TRUE(rewriter->Rewrite(convreq2, &segments));

  // candidate 0
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).attributes,
            (converter::Attribute::RERANKED |
             converter::Attribute::SPELLING_CORRECTION));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).description,
            "<もしかして: tsukigime>");

  // candidate 1
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).attributes,
            converter::Attribute::DEFAULT_ATTRIBUTE);
  EXPECT_TRUE(segments.conversion_segment(0).candidate(1).description.empty());

  // candidate 2
  EXPECT_EQ(segments.conversion_segment(0).candidate(2).attributes,
            converter::Attribute::SPELLING_CORRECTION);
  EXPECT_EQ(segments.conversion_segment(0).candidate(2).description,
            "<もしかして>");
}

TEST_F(CorrectionRewriterTest,
       RewriteWithDisplayValueForReadlingCorrectionTest) {
  auto mock = std::make_unique<engine::MockSupplementalModel>();
  // TODO(taku): Avoid sharing the pointer of std::unique_ptr.
  engine::MockSupplementalModel* mock_ptr = mock.get();

  // key -> aligner result
  const absl::flat_hash_map<
      absl::string_view,
      std::vector<std::pair<absl::string_view, absl::string_view>>>
      kReadingAlingerData = {
          {"すくつ", {{"巣", "す"}, {"窟", "くつ"}}},
          {"そうくつ", {{"巣", "そう"}, {"窟", "くつ"}}},
          {"ふいんき", {{"雰", "ふ"}, {"囲", "い"}, {"気", "んき"}}},
          {"ふんいき", {{"雰", "ふん"}, {"囲", "い"}, {"気", "き"}}},
          // incomplete alignment.
          {"ぼんれい", {{"凡例", "ぼんれい"}}},
          {"はんれい", {{"凡", "はん"}, {"例", "れい"}}},
      };

  // Emulates reading aligner.
  EXPECT_CALL(*mock_ptr, GetReadingAlignment(_, _))
      .WillRepeatedly(
          [&](absl::string_view surface, absl::string_view reading)
              -> std::vector<std::pair<absl::string_view, absl::string_view>> {
            if (const auto it = kReadingAlingerData.find(reading);
                it != kReadingAlingerData.end()) {
              return it->second;
            }
            return {};
          });

  std::unique_ptr<engine::Modules> modules =
      engine::ModulesPresetBuilder()
          .PresetSupplementalModel(std::move(mock))
          .Build(std::make_unique<testing::MockDataManager>())
          .value();

  std::unique_ptr<CorrectionRewriter> rewriter = CreateRewriter(
      *modules, {"巣窟", "雰囲気", "凡例"}, {"すくつ", "ふいんき", "ぼんれい"},
      {"そうくつ", "ふんいき", "はんれい"});

  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config.set_use_spelling_correction(true);

  auto rewrite = [&](absl::string_view key, absl::string_view value,
                     commands::Request::DisplayValueCapability capability) {
    Segments segments;
    Segment* segment = AddSegment(key, &segments);
    AddCandidate(key, value, key, value, segment);
    commands::Request request;
    request.set_display_value_capability(capability);
    rewriter->Rewrite(ConvReq(config, request), &segments);
    return segments.conversion_segment(0).candidate(0);
  };

  {
    const converter::Candidate candidate =
        rewrite("すくつ", "巣窟", commands::Request::NOT_SUPPORTED);
    EXPECT_EQ(candidate.description, "<もしかして: そうくつ>");
    EXPECT_TRUE(candidate.display_value.empty());
  }

  {
    const converter::Candidate candidate =
        rewrite("すくつ", "巣窟", commands::Request::PLAIN_TEXT);
    EXPECT_TRUE(candidate.description.empty());
    EXPECT_EQ(candidate.display_value, "巣(そう)窟");
  }

  {
    const converter::Candidate candidate =
        rewrite("ふいんき", "雰囲気", commands::Request::PLAIN_TEXT);
    EXPECT_TRUE(candidate.description.empty());
    EXPECT_EQ(candidate.display_value, "雰囲気(ふんいき)");
  }

  {
    // fallback result is returned when the alignment is broken.
    const converter::Candidate candidate =
        rewrite("ぼんれい", "凡例", commands::Request::PLAIN_TEXT);
    EXPECT_TRUE(candidate.description.empty());
    EXPECT_EQ(candidate.display_value, "凡例(はんれい)");
  }

  // reading aligner is not available. the same as the legacy description.
  EXPECT_CALL(*mock_ptr, GetReadingAlignment(_, _))
      .WillRepeatedly(
          [&](absl::string_view surface, absl::string_view reading)
              -> std::vector<std::pair<absl::string_view, absl::string_view>> {
            return {};
          });

  {
    const converter::Candidate candidate =
        rewrite("すくつ", "巣窟", commands::Request::PLAIN_TEXT);
    EXPECT_EQ(candidate.description, "<もしかして: そうくつ>");
    EXPECT_TRUE(candidate.display_value.empty());
  }

  {
    const converter::Candidate candidate =
        rewrite("ふいんき", "雰囲気", commands::Request::PLAIN_TEXT);
    EXPECT_EQ(candidate.description, "<もしかして: ふんいき>");
    EXPECT_TRUE(candidate.display_value.empty());
  }
}

TEST_F(CorrectionRewriterTest, RewriteWithDisplayValueForKatakanaTest) {
}
}  // namespace mozc
