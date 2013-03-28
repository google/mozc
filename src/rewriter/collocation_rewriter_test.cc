// Copyright 2010-2013, Google Inc.
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

#include "rewriter/collocation_rewriter.h"

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "base/system_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

class CollocationRewriterTest : public ::testing::Test {
 protected:
  // Helper data structures to define test cases.
  // Used to generate Segment::Candidate.
  struct CandidateData {
    const char *key;
    const char *content_key;
    const char *value;
    const char *content_value;
    const uint16 lid;
    const uint16 rid;
  };

  // Used to generate Segment.
  struct SegmentData {
    const char *key;
    const CandidateData *candidates;
    const size_t candidates_size;
  };

  // Used to generate Segments.
  struct SegmentsData {
    const SegmentData *segments;
    const size_t segments_size;
  };

  CollocationRewriterTest() {}
  virtual ~CollocationRewriterTest() {}

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetConfig(&config_backup_);
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);

    const mozc::testing::MockDataManager data_manager;
    pos_matcher_ = data_manager.GetPOSMatcher();
    collocation_rewriter_.reset(new CollocationRewriter(&data_manager));
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(config_backup_);
  }

  // Makes a segment from SegmentData.
  static void MakeSegment(const SegmentData &data, Segment *segment) {
    segment->Clear();
    segment->set_key(data.key);
    for (size_t i = 0; i < data.candidates_size; ++i) {
      Segment::Candidate *cand = segment->add_candidate();
      const CandidateData &cand_data = data.candidates[i];
      cand->key = cand_data.key;
      cand->content_key = cand_data.content_key;
      cand->value = cand_data.value;
      cand->content_value = cand_data.content_value;
      cand->lid = cand_data.lid;
      cand->rid = cand_data.rid;
    }
  }

  // Makes a segments from SegmentsData.
  static void MakeSegments(const SegmentsData &data, Segments *segments) {
    segments->Clear();
    for (size_t i = 0; i < data.segments_size; ++i) {
      MakeSegment(data.segments[i], segments->add_segment());
    }
  }

  bool Rewrite(Segments *segments) const {
    const ConversionRequest request;
    return collocation_rewriter_->Rewrite(request, segments);
  }

  // Returns the concatenated string of top candidates.
  static string GetTopValue(const Segments &segments) {
    string result;
    for (size_t i = 0; i < segments.conversion_segments_size(); ++i) {
      const Segment::Candidate &candidate =
          segments.conversion_segment(i).candidate(0);
      result.append(candidate.value);
    }
    return result;
  }

  const POSMatcher *pos_matcher_;

 private:
  config::Config config_backup_;
  scoped_ptr<const CollocationRewriter> collocation_rewriter_;
  DISALLOW_COPY_AND_ASSIGN(CollocationRewriterTest);
};

TEST_F(CollocationRewriterTest, NekowoKaitai) {
  // Make the following Segments:
  // "ねこを" | "かいたい"
  // --------------------
  // "ネコを" | "買いたい"
  // "猫を"   | "解体"
  //         | "飼いたい"
  const char *kNekowo =
      "\xE3\x81\xAD\xE3\x81\x93\xE3\x82\x92";  // "ねこを"
  const char *kNeko = "\xE3\x81\xAD\xE3\x81\x93";  // "ねこ"
  const uint16 id = pos_matcher_->GetUnknownId();
  const CandidateData kNekowoCands[] = {
    {kNekowo, kNeko,
     "\xE3\x83\x8D\xE3\x82\xB3\xE3\x82\x92",  // "ネコを"
     "\xE3\x83\x8D\xE3\x82\xB3\xE3\x82\x92",  // "ネコ"
     id, id},
    {kNekowo, kNeko,
     // "猫を", "猫"
     "\xE7\x8C\xAB\xE3\x82\x92", "\xE7\x8C\xAB\xE3\x82\x92",
     id, id},
  };
  const char *kKaitaiHiragana =
      "\xE3\x81\x8B\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "かいたい"
  const char *kBuy =
      "\xE8\xB2\xB7\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "買いたい"
  const char *kCut = "\xE8\xA7\xA3\xE4\xBD\x93";  // "解体"
  const char *kFeed =
      "\xE9\xA3\xBC\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "飼いたい"
  const CandidateData kKaitaiCands[] = {
    {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, id, id},
    {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, id, id},
    {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, id, id},
  };
  const SegmentData kSegmentData[] = {
    {kNekowo, kNekowoCands, arraysize(kNekowoCands)},
    {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ(
      // "猫を飼いたい"
      "\xE7\x8C\xAB\xE3\x82\x92"
      "\xE9\xA3\xBC\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84",
      GetTopValue(segments)) << segments.DebugString();
}

TEST_F(CollocationRewriterTest, MagurowoKaitai) {
  // Make the following Segments:
  // "まぐろを" | "かいたい"
  // --------------------
  // "マグロを" | "買いたい"
  // "鮪を"    | "解体"
  //          | "飼いたい"
  const char *kMagurowo =
      "\xE3\x81\xBE\xE3\x81\x90\xE3\x82\x8D\xE3\x82\x92";  // "まぐろを"
  const char *kMaguro =
      "\xE3\x81\xBE\xE3\x81\x90\xE3\x82\x8D";  // "まぐろ"
  const uint16 id = pos_matcher_->GetUnknownId();
  const CandidateData kMagurowoCands[] = {
    {kMagurowo, kMaguro,
     "\xE3\x83\x9E\xE3\x82\xB0\xE3\x83\xAD\xE3\x82\x92",  // "マグロを"
     "\xE3\x83\x9E\xE3\x82\xB0\xE3\x83\xAD",  // "マグロ"
     id, id},
    // "鮪を", "鮪を"
    {kMagurowo, kMaguro, "\xE9\xAE\xAA\xE3\x82\x92", "\xE9\xAE\xAA", id, id},
  };
  const char *kKaitaiHiragana =
      "\xE3\x81\x8B\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "かいたい"
  const char *kBuy =
      "\xE8\xB2\xB7\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "買いたい"
  const char *kCut = "\xE8\xA7\xA3\xE4\xBD\x93";  // "解体"
  const char *kFeed =
      "\xE9\xA3\xBC\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "飼いたい"
  const CandidateData kKaitaiCands[] = {
    {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, id, id},
    {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, id, id},
    {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, id, id},
  };
  const SegmentData kSegmentData[] = {
    {kMagurowo, kMagurowoCands, arraysize(kMagurowoCands)},
    {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "マグロを解体" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ(
      // "マグロを解体"
     "\xE3\x83\x9E\xE3\x82\xB0\xE3\x83\xAD\xE3\x82\x92"
     "\xE8\xA7\xA3\xE4\xBD\x93",
     GetTopValue(segments)) << segments.DebugString();
}

TEST_F(CollocationRewriterTest, CrossOverAdverbSegment) {
  // "ねこを"    | "ネコを" "猫を"
  // "すごく"    | "すごく"
  // "かいたい"  | "買いたい" "解体" "飼いたい"
  const char *kNekowo =
      "\xE3\x81\xAD\xE3\x81\x93\xE3\x82\x92";  // "ねこを"
  const char *kNeko = "\xE3\x81\xAD\xE3\x81\x93";  // "ねこ"
  const uint16 id = pos_matcher_->GetUnknownId();
  const CandidateData kNekowoCands[] = {
    {kNekowo, kNeko,
     "\xE3\x83\x8D\xE3\x82\xB3\xE3\x82\x92",  // "ネコを"
     "\xE3\x83\x8D\xE3\x82\xB3\xE3\x82\x92",  // "ネコ"
     id, id},
    {kNekowo, kNeko,
     // "猫を", "猫"
     "\xE7\x8C\xAB\xE3\x82\x92", "\xE7\x8C\xAB\xE3\x82\x92",
     id, id},
  };

  // "すごく"
  const char *kSugoku = "\xe3\x81\x99\xe3\x81\x94\xe3\x81\x8f";
  const uint16 adverb_id = pos_matcher_->GetAdverbId();
  const CandidateData kSugokuCands[] = {
    {kSugoku, kSugoku, kSugoku, kSugoku, adverb_id, adverb_id},
  };

  const char *kKaitaiHiragana =
      "\xE3\x81\x8B\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "かいたい"
  const char *kBuy =
      "\xE8\xB2\xB7\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "買いたい"
  const char *kCut = "\xE8\xA7\xA3\xE4\xBD\x93";  // "解体"
  const char *kFeed =
      "\xE9\xA3\xBC\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "飼いたい"
  const CandidateData kKaitaiCands[] = {
    {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, id, id},
    {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, id, id},
    {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, id, id},
  };
  const SegmentData kSegmentData[] = {
    {kNekowo, kNekowoCands, arraysize(kNekowoCands)},
    {kSugoku, kSugokuCands, arraysize(kSugokuCands)},
    {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should be promoted.
  EXPECT_TRUE(Rewrite(&segments));
  EXPECT_EQ(
      // "猫をすごく飼いたい"
      "\xe7\x8c\xab\xe3\x82\x92\xe3\x81\x99\xe3\x81\x94\xe3\x81\x8f\xe9"
      "\xa3\xbc\xe3\x81\x84\xe3\x81\x9f\xe3\x81\x84",
      GetTopValue(segments)) << segments.DebugString();
}

TEST_F(CollocationRewriterTest, DoNotCrossOverNonAdverbSegment) {
  // "ねこを"    | "ネコを" "猫を"
  // "すごく"    | "すごく"
  // "かいたい"  | "買いたい" "解体" "飼いたい"
  const char *kNekowo =
      "\xE3\x81\xAD\xE3\x81\x93\xE3\x82\x92";  // "ねこを"
  const char *kNeko = "\xE3\x81\xAD\xE3\x81\x93";  // "ねこ"
  const uint16 id = pos_matcher_->GetUnknownId();
  const CandidateData kNekowoCands[] = {
    {kNekowo, kNeko,
     "\xE3\x83\x8D\xE3\x82\xB3\xE3\x82\x92",  // "ネコを"
     "\xE3\x83\x8D\xE3\x82\xB3\xE3\x82\x92",  // "ネコ"
     id, id},
    {kNekowo, kNeko,
     // "猫を", "猫"
     "\xE7\x8C\xAB\xE3\x82\x92", "\xE7\x8C\xAB\xE3\x82\x92",
     id, id},
  };

  // "すごく"
  const char *kSugoku = "\xe3\x81\x99\xe3\x81\x94\xe3\x81\x8f";
  const CandidateData kSugokuCands[] = {
    {kSugoku, kSugoku, kSugoku, kSugoku, id, id},
  };

  const char *kKaitaiHiragana =
      "\xE3\x81\x8B\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "かいたい"
  const char *kBuy =
      "\xE8\xB2\xB7\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "買いたい"
  const char *kCut = "\xE8\xA7\xA3\xE4\xBD\x93";  // "解体"
  const char *kFeed =
      "\xE9\xA3\xBC\xE3\x81\x84\xE3\x81\x9F\xE3\x81\x84";  // "飼いたい"
  const CandidateData kKaitaiCands[] = {
    {kKaitaiHiragana, kKaitaiHiragana, kBuy, kBuy, id, id},
    {kKaitaiHiragana, kKaitaiHiragana, kCut, kCut, id, id},
    {kKaitaiHiragana, kKaitaiHiragana, kFeed, kFeed, id, id},
  };
  const SegmentData kSegmentData[] = {
    {kNekowo, kNekowoCands, arraysize(kNekowoCands)},
    {kSugoku, kSugokuCands, arraysize(kSugokuCands)},
    {kKaitaiHiragana, kKaitaiCands, arraysize(kKaitaiCands)},
  };
  const SegmentsData kSegments = {kSegmentData, arraysize(kSegmentData)};

  Segments segments;
  MakeSegments(kSegments, &segments);

  // "猫を飼いたい" should be promoted.
  EXPECT_FALSE(Rewrite(&segments));
  EXPECT_NE(
      // "猫をすごく飼いたい"
      "\xe7\x8c\xab\xe3\x82\x92\xe3\x81\x99\xe3\x81\x94\xe3\x81\x8f\xe9"
      "\xa3\xbc\xe3\x81\x84\xe3\x81\x9f\xe3\x81\x84",
      GetTopValue(segments)) << segments.DebugString();
}
}  // namespace mozc
