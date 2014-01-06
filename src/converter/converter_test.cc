// Copyright 2010-2014, Google Inc.
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

#include "converter/converter.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/connector_base.h"
#include "converter/connector_interface.h"
#include "converter/conversion_request.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node.h"
#include "converter/segmenter_base.h"
#include "converter/segmenter_interface.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary_stub.h"
#include "engine/engine.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/suggestion_filter.h"
#include "prediction/user_history_predictor.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

DECLARE_string(test_tmpdir);

namespace mozc {

struct SuffixToken;
using mozc::dictionary::DictionaryImpl;
using mozc::dictionary::SystemDictionary;
using mozc::dictionary::ValueDictionary;
using mozc::usage_stats::UsageStats;

namespace {

class StubPredictor : public PredictorInterface {
 public:
  StubPredictor() : predictor_name_("StubPredictor") {}

  virtual bool PredictForRequest(const ConversionRequest &request,
                                 Segments *segments) const {
    return true;
  };

  virtual const string &GetPredictorName() const {
    return predictor_name_;
  };

 private:
  const string predictor_name_;
};

class StubRewriter : public RewriterInterface {
  bool Rewrite(const ConversionRequest &request, Segments *segments) const {
    return true;
  };
};

SuffixDictionary *CreateSuffixDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  const SuffixToken *tokens = NULL;
  size_t size = 0;
  data_manager.GetSuffixDictionaryData(&tokens, &size);
  return new SuffixDictionary(tokens, size);
}

class InsertDummyWordsRewriter : public RewriterInterface {
  bool Rewrite(const ConversionRequest &, Segments *segments) const {
    for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
      Segment *seg = segments->mutable_conversion_segment(i);
      {
        Segment::Candidate *cand = seg->add_candidate();
        cand->Init();
        cand->key = "tobefiltered";
        cand->value = "ToBeFiltered";
      }
      {
        Segment::Candidate *cand = seg->add_candidate();
        cand->Init();
        cand->key = "nottobefiltered";
        cand->value = "NotToBeFiltered";
      }
    }
    return true;
  }
};

}  // namespace

class ConverterTest : public ::testing::Test {
 protected:
  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  ConverterTest() {}
  virtual ~ConverterTest() {}

  virtual void SetUp() {
    // set default user profile directory
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    UsageStats::ClearAllStatsForTest();
  }

  virtual void TearDown() {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    UsageStats::ClearAllStatsForTest();
  }

  // This struct holds resources used by converter.
  struct ConverterAndData {
    scoped_ptr<DictionaryInterface> user_dictionary;
    scoped_ptr<SuppressionDictionary> suppression_dictionary;
    scoped_ptr<DictionaryInterface> suffix_dictionary;
    scoped_ptr<const ConnectorInterface> connector;
    scoped_ptr<const SegmenterInterface> segmenter;
    scoped_ptr<DictionaryInterface> dictionary;
    scoped_ptr<const PosGroup> pos_group;
    scoped_ptr<const SuggestionFilter> suggestion_filter;
    scoped_ptr<ImmutableConverterInterface> immutable_converter;
    scoped_ptr<ConverterImpl> converter;
  };

  ConverterAndData *CreateConverterAndData(RewriterInterface *rewriter) {
    ConverterAndData *ret = new ConverterAndData;

    testing::MockDataManager data_manager;

    const char *dictionary_data = NULL;
    int dictionary_size = 0;
    data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);

    ret->user_dictionary.reset(new UserDictionaryStub);
    ret->suppression_dictionary.reset(new SuppressionDictionary);
    ret->dictionary.reset(new DictionaryImpl(
        SystemDictionary::CreateSystemDictionaryFromImage(
            dictionary_data, dictionary_size),
        ValueDictionary::CreateValueDictionaryFromImage(
            *data_manager.GetPOSMatcher(), dictionary_data, dictionary_size),
        ret->user_dictionary.get(),
        ret->suppression_dictionary.get(),
        data_manager.GetPOSMatcher()));
    ret->pos_group.reset(new PosGroup(data_manager.GetPosGroupData()));
    ret->suggestion_filter.reset(CreateSuggestionFilter(data_manager));
    ret->suffix_dictionary.reset(
        CreateSuffixDictionaryFromDataManager(data_manager));
    ret->connector.reset(ConnectorBase::CreateFromDataManager(data_manager));
    ret->segmenter.reset(SegmenterBase::CreateFromDataManager(data_manager));
    ret->immutable_converter.reset(
        new ImmutableConverterImpl(ret->dictionary.get(),
                                   ret->suffix_dictionary.get(),
                                   ret->suppression_dictionary.get(),
                                   ret->connector.get(),
                                   ret->segmenter.get(),
                                   data_manager.GetPOSMatcher(),
                                   ret->pos_group.get(),
                                   ret->suggestion_filter.get()));
    ret->converter.reset(new ConverterImpl);
    ret->converter->Init(data_manager.GetPOSMatcher(),
                         ret->suppression_dictionary.get(),
                         new StubPredictor,
                         rewriter,
                         ret->immutable_converter.get());
    return ret;
  }

  ConverterAndData *CreateStubbedConverterAndData() {
    return CreateConverterAndData(new StubRewriter);
  }

  ConverterAndData *CreateConverterAndDataWithInsertDummyWordsRewriter() {
    return CreateConverterAndData(new InsertDummyWordsRewriter);
  }

  EngineInterface *CreateEngineWithMobilePredictor() {
    Engine *engine = new Engine;
    testing::MockDataManager data_manager;
    engine->Init(&data_manager, MobilePredictor::CreateMobilePredictor);
    return engine;
  }

  bool FindCandidateByValue(const string &value, const Segment &segment) const {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == value) {
        return true;
      }
    }
    return false;
  }

  const commands::Request &default_request() const {
    return default_request_;
  }

  static SuggestionFilter *CreateSuggestionFilter(
      const DataManagerInterface &data_manager) {
    const char *data = NULL;
    size_t size = 0;
    data_manager.GetSuggestionFilterData(&data, &size);
    return new SuggestionFilter(data, size);
  }

 private:
  const commands::Request default_request_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

// test for issue:2209644
// just checking whether this causes segmentation fault or not.
// TODO(toshiyuki): make dictionary mock and test strictly.
TEST_F(ConverterTest, CanConvertTest) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, string("-")));
  }
  {
    Segments segments;
    // "おきておきて"
    EXPECT_TRUE(converter->StartConversion(
        &segments,
        string("\xe3\x81\x8a\xe3\x81\x8d\xe3\x81\xa6\xe3\x81\x8a"
               "\xe3\x81\x8d\xe3\x81\xa6")));
  }
}

namespace {
string ContextAwareConvert(const string &first_key,
                           const string &first_value,
                           const string &second_key) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);

  Segments segments;
  EXPECT_TRUE(converter->StartConversion(&segments, first_key));

  string converted;
  int segment_num = 0;
  while (1) {
    int position = -1;
    for (size_t i = 0; i < segments.segment(segment_num).candidates_size();
         ++i) {
      const string &value = segments.segment(segment_num).candidate(i).value;
      if (first_value.substr(converted.size(), value.size()) == value) {
        position = static_cast<int>(i);
        converted += value;
        break;
      }
    }

    if (position < 0) {
      break;
    }

    EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, position))
        << first_value;

    ++segment_num;

    if (first_value == converted) {
      break;
    }
  }
  EXPECT_EQ(first_value, converted) << first_value;
  // TODO(team): Use StartConversionForRequest instead of StartConversion.
  const ConversionRequest default_request;
  EXPECT_TRUE(converter->FinishConversion(default_request, &segments));
  EXPECT_TRUE(converter->StartConversion(&segments, second_key));
  EXPECT_EQ(segment_num + 1, segments.segments_size());

  return segments.segment(segment_num).candidate(0).value;
}
}  // anonymous namespace

TEST_F(ConverterTest, ContextAwareConversionTest) {
  // Desirable context aware conversions
  // EXPECT_EQ("一髪", ContextAwareConvert("きき", "危機", "いっぱつ"));
  EXPECT_EQ("\xE4\xB8\x80\xE9\xAB\xAA",
            ContextAwareConvert(
                "\xE3\x81\x8D\xE3\x81\x8D",
                "\xE5\x8D\xB1\xE6\xA9\x9F",
                "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\xB1\xE3\x81\xA4"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 2000, 1, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 2000, 1, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 1000, 1, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 2);

  // EXPECT_EQ("大", ContextAwareConvert("きょうと", "京都", "だい"));
  EXPECT_EQ("\xE5\xA4\xA7",
            ContextAwareConvert(
                "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8",
                "\xE4\xBA\xAC\xE9\x83\xBD",
                "\xE3\x81\xA0\xE3\x81\x84"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 4000, 2, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 4000, 2, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 2000, 2, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 4);

  // EXPECT_EQ("点", ContextAwareConvert("もんだい", "問題", "てん"));
  EXPECT_EQ("\xE7\x82\xB9",
            ContextAwareConvert(
                "\xE3\x82\x82\xE3\x82\x93\xE3\x81\xA0\xE3\x81\x84",
                "\xE5\x95\x8F\xE9\xA1\x8C",
                "\xE3\x81\xA6\xE3\x82\x93"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 6000, 3, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 6000, 3, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 3000, 3, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 6);

  // EXPECT_EQ("陽水", ContextAwareConvert("いのうえ", "井上", "ようすい"));
  EXPECT_EQ("\xE9\x99\xBD\xE6\xB0\xB4",
            ContextAwareConvert(
                "\xE3\x81\x84\xE3\x81\xAE\xE3\x81\x86\xE3\x81\x88",
                "\xE4\xBA\x95\xE4\xB8\x8A",
                "\xE3\x82\x88\xE3\x81\x86\xE3\x81\x99\xE3\x81\x84"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 8000, 4, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 8000, 4, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 4000, 4, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 8);

  // Undesirable context aware conversions
  // EXPECT_NE("宗号", ContextAwareConvert("19じ", "19時", "しゅうごう"));
  EXPECT_NE("\xE5\xAE\x97\xE5\x8F\xB7", ContextAwareConvert(
      "19\xE3\x81\x98",
      "19\xE6\x99\x82",
      "\xE3\x81\x97\xE3\x82\x85\xE3\x81\x86\xE3\x81\x94\xE3\x81\x86"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 11000, 6, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 11000, 5, 2000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 6000, 5, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 11);

  // EXPECT_NE("な前", ContextAwareConvert("の", "の", "なまえ"));
  EXPECT_NE("\xE3\x81\xAA\xE5\x89\x8D", ContextAwareConvert(
      "\xE3\x81\xAE",
      "\xE3\x81\xAE",
      "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 12000, 7, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 12000, 6, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 7000, 6, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 12);

  // EXPECT_NE("し料", ContextAwareConvert("の", "の", "しりょう"));
  EXPECT_NE("\xE3\x81\x97\xE6\x96\x99", ContextAwareConvert(
      "\xE3\x81\xAE",
      "\xE3\x81\xAE",
      "\xE3\x81\x97\xE3\x82\x8A\xE3\x82\x87\xE3\x81\x86"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 13000, 8, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 13000, 7, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 8000, 7, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 13);

  // EXPECT_NE("し礼賛", ContextAwareConvert("ぼくと", "僕と", "しらいさん"));
  EXPECT_NE("\xE3\x81\x97\xE7\xA4\xBC\xE8\xB3\x9B", ContextAwareConvert(
      "\xE3\x81\xBC\xE3\x81\x8F\xE3\x81\xA8",
      "\xE5\x83\x95\xE3\x81\xA8",
      "\xE3\x81\x97\xE3\x82\x89\xE3\x81\x84\xE3\x81\x95\xE3\x82\x93"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 15000, 9, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 15000, 8, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 9000, 8, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 15);
}

TEST_F(ConverterTest, CommitSegmentValue) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  {
    // Prepare a segment, with candidates "1" and "2";
    Segment *segment = segments.add_segment();
    segment->add_candidate()->value = "1";
    segment->add_candidate()->value = "2";
  }
  {
    // Prepare a segment, with candidates "3" and "4";
    Segment *segment = segments.add_segment();
    segment->add_candidate()->value = "3";
    segment->add_candidate()->value = "4";
  }
  {
    // Commit the candidate whose value is "2".
    EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 1));
    EXPECT_EQ(2, segments.segments_size());
    EXPECT_EQ(0, segments.history_segments_size());
    EXPECT_EQ(2, segments.conversion_segments_size());
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(Segment::FIXED_VALUE, segment.segment_type());
    EXPECT_EQ("2", segment.candidate(0).value);
    EXPECT_TRUE(
        segment.candidate(0).attributes & Segment::Candidate::RERANKED);
  }
  {
    // Make the segment SUBMITTED
    segments.mutable_conversion_segment(0)->
        set_segment_type(Segment::SUBMITTED);
    EXPECT_EQ(2, segments.segments_size());
    EXPECT_EQ(1, segments.history_segments_size());
    EXPECT_EQ(1, segments.conversion_segments_size());
  }
  {
    // Commit the candidate whose value is "3".
    EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 0));
    EXPECT_EQ(2, segments.segments_size());
    EXPECT_EQ(1, segments.history_segments_size());
    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(Segment::FIXED_VALUE, segment.segment_type());
    EXPECT_EQ("3", segment.candidate(0).value);
    EXPECT_FALSE(
        segment.candidate(0).attributes & Segment::Candidate::RERANKED);
  }
}

TEST_F(ConverterTest, CommitFirstSegment) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  // History segment.
  {
    Segment *segment = segments.add_segment();
    // "あした"
    segment->set_key("\xE3\x81\x82\xE3\x81\x97\xE3\x81\x9F");
    segment->set_segment_type(Segment::HISTORY);

    Segment::Candidate *candidate = segment->add_candidate();
    // "あした"
    candidate->key = "\xE3\x81\x82\xE3\x81\x97\xE3\x81\x9F";
    // "明日"
    candidate->value = "\xE4\xBB\x8A\xE6\x97\xA5";
  }

  {
    Segment *segment = segments.add_segment();
    // "かつこうに"
    segment->set_key(
        "\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab");
    Segment::Candidate *candidate = segment->add_candidate();
    // "学校に"
    candidate->value = "\xe5\xad\xa6\xe6\xa0\xa1\xE3\x81\xAB";
    // "がっこうに"
    candidate->key =
        "\xe3\x81\x8c\xe3\x81\xa3\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab";
  }

  {
    Segment *segment = segments.add_segment();
    // "いく"
    segment->set_key("\xE3\x81\x84\xE3\x81\x8F");
    Segment::Candidate *candidate = segment->add_candidate();
    // "行く"
    candidate->value = "\xE8\xA1\x8C\xE3\x81\x8F";
    // "いく"
    candidate->key = "\xE3\x81\x84\xE3\x81\x8F";
  }

  converter->CommitFirstSegment(&segments, 0);

  EXPECT_EQ(2, segments.history_segments_size());
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(Segment::HISTORY, segments.history_segment(0).segment_type());
  EXPECT_EQ(Segment::SUBMITTED, segments.history_segment(1).segment_type());

  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 3000, 1, 3000, 3000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 3000, 1, 3000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 1000, 1, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 3);
}

TEST_F(ConverterTest, CommitPartialSuggestionSegmentValue) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  {
    // Prepare a segment, with candidates "1" and "2";
    Segment *segment = segments.add_segment();
    segment->add_candidate()->value = "1";
    segment->add_candidate()->value = "2";
  }
  {
    // Prepare a segment, with candidates "3" and "4";
    Segment *segment = segments.add_segment();
    segment->add_candidate()->value = "3";
    segment->add_candidate()->value = "4";
  }
  {
    // Commit the candidate whose value is "2".
    EXPECT_TRUE(converter->CommitPartialSuggestionSegmentValue(
                &segments, 0, 1, "left2", "right2"));
    EXPECT_EQ(3, segments.segments_size());
    EXPECT_EQ(1, segments.history_segments_size());
    EXPECT_EQ(2, segments.conversion_segments_size());
    {
      // The tail segment of the history segments uses
      // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
      // and contains original value.
      const Segment &segment =
          segments.history_segment(segments.history_segments_size() - 1);
      EXPECT_EQ(Segment::SUBMITTED, segment.segment_type());
      EXPECT_EQ("2", segment.candidate(0).value);
      EXPECT_EQ("left2", segment.key());
      EXPECT_TRUE(
          segment.candidate(0).attributes & Segment::Candidate::RERANKED);
    }
    {
      // The head segment of the conversion segments uses |new_segment_key|.
      const Segment &segment = segments.conversion_segment(0);
      EXPECT_EQ(Segment::FREE, segment.segment_type());
      EXPECT_EQ("right2", segment.key());
    }
  }
}

TEST_F(ConverterTest, CommitPartialSuggestionUsageStats) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  // History segment.
  {
    Segment *segment = segments.add_segment();
    // "あした"
    segment->set_key("\xE3\x81\x82\xE3\x81\x97\xE3\x81\x9F");
    segment->set_segment_type(Segment::HISTORY);

    Segment::Candidate *candidate = segment->add_candidate();
    // "あした"
    candidate->key = "\xE3\x81\x82\xE3\x81\x97\xE3\x81\x9F";
    // "明日"
    candidate->value = "\xE4\xBB\x8A\xE6\x97\xA5";
  }

  {
    Segment *segment = segments.add_segment();
    // "かつこうに"
    segment->set_key(
        "\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab");

    Segment::Candidate *candidate = segment->add_candidate();
    // "学校に"
    candidate->value = "\xe5\xad\xa6\xe6\xa0\xa1\xE3\x81\xAB";
    // "がっこうに"
    candidate->key =
        "\xe3\x81\x8c\xe3\x81\xa3\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab";

    candidate = segment->add_candidate();
    // "格好に"
    candidate->value = "\xe6\xa0\xbc\xe5\xa5\xbd\xe3\x81\xab";
    // "かっこうに"
    candidate->key =
        "\xe3\x81\x8b\xe3\x81\xa3\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab";

    candidate = segment->add_candidate();
    // "かつこうに"
    candidate->value =
        "\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab";
    // "かつこうに"
    candidate->key =
        "\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab";
  }

  EXPECT_STATS_NOT_EXIST("CommitPartialSuggestion");
  EXPECT_TRUE(converter->CommitPartialSuggestionSegmentValue(
      // "かつこうに", "いく"
      &segments, 0, 1,
      "\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab",
      "\xe3\x81\x84\xe3\x81\x8f"));
  EXPECT_EQ(2, segments.history_segments_size());
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(Segment::HISTORY, segments.history_segment(0).segment_type());
  EXPECT_EQ(Segment::SUBMITTED, segments.history_segment(1).segment_type());
  {
    // The tail segment of the history segments uses
    // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
    // and contains original value.
    const Segment &segment =
        segments.history_segment(segments.history_segments_size() - 1);
    EXPECT_EQ(Segment::SUBMITTED, segment.segment_type());
    // "格好に"
    EXPECT_EQ("\xe6\xa0\xbc\xe5\xa5\xbd\xe3\x81\xab",
              segment.candidate(0).value);
    // "かっこうに"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xa3\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab",
              segment.candidate(0).key);
    // "かつこうに"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab",
              segment.key());
    EXPECT_TRUE(
        segment.candidate(0).attributes & Segment::Candidate::RERANKED);
  }
  {
    // The head segment of the conversion segments uses |new_segment_key|.
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(Segment::FREE, segment.segment_type());
    // "いく"
    EXPECT_EQ("\xe3\x81\x84\xe3\x81\x8f", segment.key());
  }

  EXPECT_COUNT_STATS("CommitPartialSuggestion", 1);
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 3000, 1, 3000, 3000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 3000, 1, 3000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 1000, 1, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 3);
}

TEST_F(ConverterTest, CommitAutoPartialSuggestionUsageStats) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  {
    Segment *segment = segments.add_segment();
    // "かつこうにいく"
    segment->set_key(
        "\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3"
        "\x81\xab\xe3\x81\x84\xe3\x81\x8f");

    Segment::Candidate *candidate = segment->add_candidate();
    // "学校にいく"
    candidate->value =
        "\xe5\xad\xa6\xe6\xa0\xa1\xe3\x81\xab\xe3\x81\x84\xe3\x81\x8f";
    // "がっこうにいく"
    candidate->key =
        "\xe3\x81\x8c\xe3\x81\xa3\xe3\x81\x93\xe3\x81\x86\xe3"
        "\x81\xab\xe3\x81\x84\xe3\x81\x8f";

    candidate = segment->add_candidate();
    // "学校に行く"
    candidate->value =
        "\xe5\xad\xa6\xe6\xa0\xa1\xe3\x81\xab\xe8\xa1\x8c\xe3\x81\x8f";
    // "がっこうにいく"
    candidate->key =
        "\xe3\x81\x8c\xe3\x81\xa3\xe3\x81\x93\xe3\x81\x86\xe3"
        "\x81\xab\xe3\x81\x84\xe3\x81\x8f";

    candidate = segment->add_candidate();
    // "学校に"
    candidate->value = "\xe5\xad\xa6\xe6\xa0\xa1\xe3\x81\xab";
    // "がっこうに"
    candidate->key =
        "\xe3\x81\x8c\xe3\x81\xa3\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab";
  }

  EXPECT_STATS_NOT_EXIST("CommitPartialSuggestion");
  EXPECT_TRUE(converter->CommitPartialSuggestionSegmentValue(
      // "かつこうに", "いく"
      &segments, 0, 2,
      "\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab",
      "\xe3\x81\x84\xe3\x81\x8f"));
  EXPECT_EQ(2, segments.segments_size());
  EXPECT_EQ(1, segments.history_segments_size());
  EXPECT_EQ(1, segments.conversion_segments_size());
  {
    // The tail segment of the history segments uses
    // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
    // and contains original value.
    const Segment &segment =
        segments.history_segment(segments.history_segments_size() - 1);
    EXPECT_EQ(Segment::SUBMITTED, segment.segment_type());
    // "学校に"
    EXPECT_EQ("\xe5\xad\xa6\xe6\xa0\xa1\xe3\x81\xab",
              segment.candidate(0).value);
    // "がっこうに"
    EXPECT_EQ("\xe3\x81\x8c\xe3\x81\xa3\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab",
              segment.candidate(0).key);
    // "かつこうに"
    EXPECT_EQ("\xe3\x81\x8b\xe3\x81\xa4\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab",
              segment.key());
    EXPECT_TRUE(
        segment.candidate(0).attributes & Segment::Candidate::RERANKED);
  }
  {
    // The head segment of the conversion segments uses |new_segment_key|.
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(Segment::FREE, segment.segment_type());
    // "いく"
    EXPECT_EQ("\xe3\x81\x84\xe3\x81\x8f", segment.key());
  }

  EXPECT_COUNT_STATS("CommitAutoPartialSuggestion", 1);
}

TEST_F(ConverterTest, CandidateKeyTest) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  // "わたしは"
  EXPECT_TRUE(converter->StartConversion(
      &segments,
      string("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF")));
  EXPECT_EQ(1, segments.segments_size());
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).key);
  // "わたし"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97",
            segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, Regression3437022) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  // "けいたい"
  const string kKey1 = "\xe3\x81\x91\xe3\x81\x84\xe3\x81\x9f\xe3\x81\x84";
  // "でんわ"
  const string kKey2 = "\xe3\x81\xa7\xe3\x82\x93\xe3\x82\x8f";

  // "携帯"
  const string kValue1 = "\xe6\x90\xba\xe5\xb8\xaf";
  // "電話"
  const string kValue2 = "\xe9\x9b\xbb\xe8\xa9\xb1";

  {
    // Make sure converte result is one segment
    EXPECT_TRUE(converter->StartConversion(
        &segments, kKey1 + kKey2));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(kValue1 + kValue2,
              segments.conversion_segment(0).candidate(0).value);
  }
  {
    // Make sure we can convert first part
    segments.Clear();
    EXPECT_TRUE(converter->StartConversion(
        &segments, kKey1));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(kValue1, segments.conversion_segment(0).candidate(0).value);
  }
  {
    // Make sure we can convert last part
    segments.Clear();
    EXPECT_TRUE(converter->StartConversion(
        &segments, kKey2));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(kValue2, segments.conversion_segment(0).candidate(0).value);
  }

  // Add compound entry to suppression dictionary
  segments.Clear();

  SuppressionDictionary *dic = engine->GetSuppressionDictionary();
  dic->Lock();
  dic->AddEntry(kKey1 + kKey2, kValue1 + kValue2);
  dic->UnLock();

  EXPECT_TRUE(converter->StartConversion(
      &segments, kKey1 + kKey2));

  int rest_size = 0;
  for (size_t i = 1; i < segments.conversion_segments_size(); ++i) {
    rest_size += Util::CharsLen(
        segments.conversion_segment(i).candidate(0).key);
  }

  // Expand segment so that the entire part will become one segment
  if (rest_size > 0) {
    const ConversionRequest default_request;
    EXPECT_TRUE(converter->ResizeSegment(
        &segments, default_request, 0, rest_size));
  }

  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_NE(kValue1 + kValue2,
            segments.conversion_segment(0).candidate(0).value);

  dic->Lock();
  dic->Clear();
  dic->UnLock();
}

TEST_F(ConverterTest, CompletePOSIds) {
  const char *kTestKeys[] = {
    // "きょうと",
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8",
    // "いきます",
    "\xE3\x81\x84\xE3\x81\x8D\xE3\x81\xBE\xE3\x81\x99",
    // "うつくしい",
    "\xE3\x81\x86\xE3\x81\xA4\xE3\x81\x8F\xE3\x81\x97\xE3\x81\x84",
    // "おおきな",
    "\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x8D\xE3\x81\xAA",
    // "いっちゃわない"
    "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\xA1\xE3\x82\x83\xE3\x82\x8F"
    "\xE3\x81\xAA\xE3\x81\x84\xE3\x81\xAD",
    // "わたしのなまえはなかのです"
    "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAE\xE3\x81\xAA"
    "\xE3\x81\xBE\xE3\x81\x88\xE3\x81\xAF\xE3\x81\xAA\xE3\x81\x8B"
    "\xE3\x81\xAE\xE3\x81\xA7\xE3\x81\x99"
  };

  scoped_ptr<ConverterAndData> converter_and_data(
      CreateStubbedConverterAndData());
  ConverterImpl *converter = converter_and_data->converter.get();
  for (size_t i = 0; i < arraysize(kTestKeys); ++i) {
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *seg = segments.add_segment();
    seg->set_key(kTestKeys[i]);
    seg->set_segment_type(Segment::FREE);
    segments.set_max_prediction_candidates_size(20);
    CHECK(converter_and_data->immutable_converter->Convert(&segments));
    const int lid = segments.segment(0).candidate(0).lid;
    const int rid = segments.segment(0).candidate(0).rid;
    Segment::Candidate candidate;
    candidate.value = segments.segment(0).candidate(0).value;
    candidate.key = segments.segment(0).candidate(0).key;
    candidate.lid = 0;
    candidate.rid = 0;
    converter->CompletePOSIds(&candidate);
    EXPECT_EQ(lid, candidate.lid);
    EXPECT_EQ(rid, candidate.rid);
    EXPECT_NE(candidate.lid, 0);
    EXPECT_NE(candidate.rid, 0);
  }

  {
    Segment::Candidate candidate;
    candidate.key = "test";
    candidate.value = "test";
    candidate.lid = 10;
    candidate.rid = 11;
    converter->CompletePOSIds(&candidate);
    EXPECT_EQ(10, candidate.lid);
    EXPECT_EQ(11, candidate.rid);
  }
}

TEST_F(ConverterTest, Regression3046266) {
  // Shouldn't correct nodes at the beginning of a sentence.
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  // Can be any string that has "ん" at the end
  // "かん"
  const char kKey1[] = "\xE3\x81\x8B\xE3\x82\x93";

  // Can be any string that has a vowel at the beginning
  // "あか"
  const char kKey2[] = "\xE3\x81\x82\xE3\x81\x8B";

  // "中"
  const char kValueNotExpected[] = "\xE4\xB8\xAD";

  EXPECT_TRUE(converter->StartConversion(
      &segments, kKey1));
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_TRUE(converter->CommitSegmentValue(
      &segments, 0, 0));

  // TODO(team): Use StartConversionForRequest instead of StartConversion.
  const ConversionRequest default_request;
  EXPECT_TRUE(converter->FinishConversion(default_request, &segments));

  EXPECT_TRUE(converter->StartConversion(
      &segments, kKey2));
  EXPECT_EQ(1, segments.conversion_segments_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_NE(segment.candidate(i).value, kValueNotExpected);
  }
}

TEST_F(ConverterTest, Regression5502496) {
  // Make sure key correction works for the first word of a sentence.
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  // "みんあ"
  const char kKey[] = "\xE3\x81\xBF\xE3\x82\x93\xE3\x81\x82";

  // "みんな"
  const char kValueExpected[] = "\xE3\x81\xBF\xE3\x82\x93\xE3\x81\xAA";

  EXPECT_TRUE(converter->StartConversion(&segments, kKey));
  EXPECT_EQ(1, segments.conversion_segments_size());
  const Segment &segment = segments.conversion_segment(0);
  bool found = false;
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == kValueExpected) {
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(ConverterTest, EmoticonsAboveSymbols) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  // "かおもじ"
  const char kKey[] = "\xE3\x81\x8B\xE3\x81\x8A\xE3\x82\x82\xE3\x81\x98";

  const char kEmoticon[] = "^^;";
  // "☹": A platform-dependent symbol
  const char kSymbol[] = "\xE2\x98\xB9";

  EXPECT_TRUE(converter->StartConversion(
      &segments, kKey));
  EXPECT_EQ(1, segments.conversion_segments_size());
  const Segment &segment = segments.conversion_segment(0);
  bool found_emoticon = false;
  bool found_symbol = false;

  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == kEmoticon) {
      found_emoticon = true;
    } else if (segment.candidate(i).value == kSymbol) {
      found_symbol = true;
    }
    if (found_symbol) {
      break;
    }
  }
  EXPECT_TRUE(found_emoticon);
}

TEST_F(ConverterTest, StartSuggestionForRequest) {
  commands::Request client_request;
  client_request.set_mixed_conversion(true);

  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);

  // "し"
  const string kShi = "\xE3\x81\x97";

  composer::Table table;
  table.AddRule("si", kShi, "");
  table.AddRule("shi", kShi, "");

  {
    composer::Composer composer(&table, &client_request);

    composer.InsertCharacter("shi");

    Segments segments;
    const ConversionRequest request(&composer, &client_request);
    EXPECT_TRUE(converter->StartSuggestionForRequest(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    ASSERT_TRUE(segments.segment(0).meta_candidates_size() >=
                transliteration::HALF_ASCII);
    EXPECT_EQ("shi",
              segments.segment(0).meta_candidate(
                  transliteration::HALF_ASCII).value);
  }

  {
    composer::Composer composer(&table, &client_request);

    composer.InsertCharacter("si");

    Segments segments;
    const ConversionRequest request(&composer, &client_request);
    EXPECT_TRUE(converter->StartSuggestionForRequest(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    ASSERT_TRUE(segments.segment(0).meta_candidates_size() >=
                transliteration::HALF_ASCII);
    EXPECT_EQ("si",
              segments.segment(0).meta_candidate(
                  transliteration::HALF_ASCII).value);
  }
}

TEST_F(ConverterTest, StartPartialPrediction) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  // "わたしは"
  EXPECT_TRUE(converter->StartPartialPrediction(
              &segments,
              "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF"));
  EXPECT_EQ(1, segments.segments_size());
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).key);
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, StartPartialSuggestion) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  // "わたしは"
  EXPECT_TRUE(converter->StartPartialSuggestion(
              &segments,
              "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF"));
  EXPECT_EQ(1, segments.segments_size());
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).key);
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, StartPartialPrediction_mobile) {
  scoped_ptr<EngineInterface> engine(CreateEngineWithMobilePredictor());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  // "わたしは"
  EXPECT_TRUE(converter->StartPartialPrediction(
              &segments,
              "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF"));
  EXPECT_EQ(1, segments.segments_size());
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).key);
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, StartPartialSuggestion_mobile) {
  scoped_ptr<EngineInterface> engine(CreateEngineWithMobilePredictor());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  // "わたしは"
  EXPECT_TRUE(converter->StartPartialSuggestion(
              &segments,
              "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF"));
  EXPECT_EQ(1, segments.segments_size());
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).key);
  // "わたしは"
  EXPECT_EQ("\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\xE3\x81\xAF",
            segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, MaybeSetConsumedKeySizeToSegment) {
  const size_t consumed_key_size = 5;
  const size_t original_consumed_key_size = 10;

  Segment segment;
  // 1st candidate without PARTIALLY_KEY_CONSUMED
  segment.push_back_candidate();
  // 2nd candidate with PARTIALLY_KEY_CONSUMED
  Segment::Candidate *candidate2 = segment.push_back_candidate();
  candidate2->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  candidate2->consumed_key_size = original_consumed_key_size;
  // 1st meta candidate without PARTIALLY_KEY_CONSUMED
  segment.add_meta_candidate();
  // 2nd meta candidate with PARTIALLY_KEY_CONSUMED
  Segment::Candidate *meta_candidate2 = segment.add_meta_candidate();
  meta_candidate2->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  meta_candidate2->consumed_key_size = original_consumed_key_size;

  ConverterImpl::MaybeSetConsumedKeySizeToSegment(consumed_key_size, &segment);
  EXPECT_TRUE(segment.candidate(0).attributes &
              Segment::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_EQ(consumed_key_size, segment.candidate(0).consumed_key_size);
  EXPECT_TRUE(segment.candidate(1).attributes &
              Segment::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_EQ(original_consumed_key_size,
            segment.candidate(1).consumed_key_size);
  EXPECT_TRUE(segment.meta_candidate(0).attributes &
              Segment::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_EQ(consumed_key_size, segment.meta_candidate(0).consumed_key_size);
  EXPECT_TRUE(segment.meta_candidate(1).attributes &
              Segment::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_EQ(original_consumed_key_size,
            segment.meta_candidate(1).consumed_key_size);
}

TEST_F(ConverterTest, Predict_SetKey) {
  const char kPredictionKey[] = "prediction key";
  const char kPredictionKey2[] = "prediction key2";
  // Tests whether SetKey method is called or not.
  struct TestData {
    const Segments::RequestType request_type_;
    const char *key_;
    const bool expect_reset_;
  };
  const TestData test_data_list[] = {
      {Segments::CONVERSION, NULL, true},
      {Segments::CONVERSION, kPredictionKey, true},
      {Segments::CONVERSION, kPredictionKey2, true},
      {Segments::REVERSE_CONVERSION, NULL, true},
      {Segments::REVERSE_CONVERSION, kPredictionKey, true},
      {Segments::REVERSE_CONVERSION, kPredictionKey2, true},
      {Segments::PREDICTION, NULL, true},
      {Segments::PREDICTION, kPredictionKey2, true},
      {Segments::SUGGESTION, NULL, true},
      {Segments::SUGGESTION, kPredictionKey2, true},
      {Segments::PARTIAL_PREDICTION, NULL, true},
      {Segments::PARTIAL_PREDICTION, kPredictionKey2, true},
      {Segments::PARTIAL_SUGGESTION, NULL, true},
      {Segments::PARTIAL_SUGGESTION, kPredictionKey2, true},
      // If we are predicting, and one or more segment exists,
      // and the segments's key equals to the input key, then do not reset.
      {Segments::PREDICTION, kPredictionKey, false},
      {Segments::SUGGESTION, kPredictionKey, true},
      {Segments::PARTIAL_PREDICTION, kPredictionKey, false},
      {Segments::PARTIAL_SUGGESTION, kPredictionKey, true},
  };

  scoped_ptr<ConverterAndData> converter_and_data(
      CreateStubbedConverterAndData());
  ConverterImpl *converter = converter_and_data->converter.get();
  ASSERT_TRUE(converter);

  // Note that TearDown method will reset above stubs.

  for (size_t i = 0; i < arraysize(test_data_list); ++i) {
    const TestData &test_data = test_data_list[i];
    Segments segments;
    segments.set_request_type(test_data.request_type_);

    if (test_data.key_) {
      Segment *seg = segments.add_segment();
      seg->Clear();
      seg->set_key(test_data.key_);
      // The segment has a candidate.
      seg->add_candidate();
    }
    const ConversionRequest request;
    converter->Predict(request, kPredictionKey,
                       Segments::PREDICTION, &segments);

    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(kPredictionKey, segments.conversion_segment(0).key());
    EXPECT_EQ(test_data.expect_reset_ ? 0 : 1,
              segments.conversion_segment(0).candidates_size());
  }
}

TEST_F(ConverterTest, VariantExpansionForSuggestion) {
  // Create Converter with mock user dictionary
  testing::MockDataManager data_manager;
  scoped_ptr<DictionaryMock> mock_user_dictionary(new DictionaryMock);

  mock_user_dictionary->AddLookupPredictive(
      // "てすと"
      "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8",
      // "てすと"
      "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8",
      "<>!?",
      (Node::USER_DICTIONARY | Node::NO_VARIANTS_EXPANSION));
  mock_user_dictionary->AddLookupPrefix(
      // "てすと"
      "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8",
      // "てすと"
      "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8",
      "<>!?",
      (Node::USER_DICTIONARY | Node::NO_VARIANTS_EXPANSION));
  scoped_ptr<SuppressionDictionary> suppression_dictionary(
      new SuppressionDictionary);

  const char *dictionary_data = NULL;
  int dictionary_size = 0;
  data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);

  scoped_ptr<DictionaryInterface> dictionary(new DictionaryImpl(
      SystemDictionary::CreateSystemDictionaryFromImage(
          dictionary_data, dictionary_size),
      ValueDictionary::CreateValueDictionaryFromImage(
          *data_manager.GetPOSMatcher(), dictionary_data, dictionary_size),
      mock_user_dictionary.get(),
      suppression_dictionary.get(),
      data_manager.GetPOSMatcher()));
  scoped_ptr<const PosGroup> pos_group(
      new PosGroup(data_manager.GetPosGroupData()));
  scoped_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  scoped_ptr<const ConnectorInterface> connector(
      ConnectorBase::CreateFromDataManager(data_manager));
  scoped_ptr<const SegmenterInterface> segmenter(
      SegmenterBase::CreateFromDataManager(data_manager));
  scoped_ptr<const SuggestionFilter> suggestion_filter(
      CreateSuggestionFilter(data_manager));
  scoped_ptr<ImmutableConverterInterface> immutable_converter(
      new ImmutableConverterImpl(dictionary.get(),
                                 suffix_dictionary.get(),
                                 suppression_dictionary.get(),
                                 connector.get(),
                                 segmenter.get(),
                                 data_manager.GetPOSMatcher(),
                                 pos_group.get(),
                                 suggestion_filter.get()));
  scoped_ptr<const SuggestionFilter> suggegstion_filter(
      CreateSuggestionFilter(data_manager));
  scoped_ptr<ConverterImpl> converter(new ConverterImpl);
  const DictionaryInterface *kNullDictionary = NULL;
  converter->Init(data_manager.GetPOSMatcher(),
                  suppression_dictionary.get(),
                  DefaultPredictor::CreateDefaultPredictor(
                      new DictionaryPredictor(
                          converter.get(),
                          immutable_converter.get(),
                          dictionary.get(),
                          suffix_dictionary.get(),
                          connector.get(),
                          segmenter.get(),
                          data_manager.GetPOSMatcher(),
                          suggegstion_filter.get()),
                      new UserHistoryPredictor(dictionary.get(),
                                               data_manager.GetPOSMatcher(),
                                               suppression_dictionary.get())),
                  new RewriterImpl(converter.get(),
                                   &data_manager,
                                   pos_group.get(),
                                   kNullDictionary),
                  immutable_converter.get());

  Segments segments;
  {
    // Dictionary suggestion
    EXPECT_TRUE(converter->StartSuggestion(
        &segments,
        // "てすと"
        "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8"));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_TRUE(FindCandidateByValue("<>!?", segments.conversion_segment(0)));
    EXPECT_FALSE(FindCandidateByValue(
        // "＜＞！？"
        "\xef\xbc\x9c\xef\xbc\x9e\xef\xbc\x81\xef\xbc\x9f",
        segments.conversion_segment(0)));
  }
  {
    // Realtime conversion
    segments.Clear();
    EXPECT_TRUE(converter->StartSuggestion(
        &segments,
        // "てすとの"
        "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8\xe3\x81\xae"));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_LE(1, segments.conversion_segment(0).candidates_size());
    // "<>!?の"
    EXPECT_TRUE(FindCandidateByValue("\x3c\x3e\x21\x3f\xe3\x81\xae",
                                     segments.conversion_segment(0)));
    EXPECT_FALSE(FindCandidateByValue(
        // "＜＞！？の"
        // "＜＞！？の"
        "\xef\xbc\x9c\xef\xbc\x9e\xef\xbc\x81\xef\xbc\x9f\xe3\x81\xae",
        segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, ComposerKeySelection) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  composer::Table table;
  {
    Segments segments;
    composer::Composer composer(&table, &default_request());
    composer.InsertCharacterPreedit(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\x68");  // "わたしh"
    ConversionRequest request(&composer, &default_request());
    request.set_composer_key_selection(ConversionRequest::CONVERSION_KEY);
    converter->StartConversionForRequest(request, &segments);
    EXPECT_EQ(2, segments.conversion_segments_size());
    EXPECT_EQ("\xE7\xA7\x81",  // "私"
              segments.conversion_segment(0).candidate(0).value);
    EXPECT_EQ("h", segments.conversion_segment(1).candidate(0).value);
  }
  {
    Segments segments;
    composer::Composer composer(&table, &default_request());
    composer.InsertCharacterPreedit(
        "\xE3\x82\x8F\xE3\x81\x9F\xE3\x81\x97\x68");  // "わたしh"
    ConversionRequest request(&composer, &default_request());
    request.set_composer_key_selection(ConversionRequest::PREDICTION_KEY);
    converter->StartConversionForRequest(request, &segments);
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ("\xE7\xA7\x81",  // "私"
              segments.conversion_segment(0).candidate(0).value);
  }
}

TEST_F(ConverterTest, SuppressionDictionaryForRewriter) {
  scoped_ptr<ConverterAndData> ret(
      CreateConverterAndDataWithInsertDummyWordsRewriter());

  // Set up suppression dictionary
  ret->suppression_dictionary->Lock();
  ret->suppression_dictionary->AddEntry("tobefiltered", "ToBeFiltered");
  ret->suppression_dictionary->UnLock();
  EXPECT_FALSE(ret->suppression_dictionary->IsEmpty());

  // Convert
  composer::Table table;
  composer::Composer composer(&table, &default_request());
  composer.InsertCharacter("dummy");
  const ConversionRequest request(&composer, &default_request());
  Segments segments;
  EXPECT_TRUE(ret->converter->StartConversionForRequest(request, &segments));

  // Verify that words inserted by the rewriter is suppressed if its in the
  // suppression_dictionary.
  for (size_t i = 0; i < segments.conversion_segments_size(); ++i) {
    const Segment &seg = segments.conversion_segment(i);
    EXPECT_FALSE(FindCandidateByValue("ToBeFiltered", seg));
    EXPECT_TRUE(FindCandidateByValue("NotToBeFiltered", seg));
  }
}

TEST_F(ConverterTest, EmptyConvertReverse_Issue8661091) {
  // This is a test case against b/8661091.
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();

  Segments segments;
  EXPECT_FALSE(converter->StartReverseConversion(&segments, ""));
}

TEST_F(ConverterTest, StartReverseConversion) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  const ConverterInterface *converter = engine->GetConverter();

  const string kHonKanji = "\xE6\x9C\xAC";  // "本"
  const string kHonHiragana = "\xE3\x81\xBB\xE3\x82\x93";  // "ほん"
  const string kMuryouKanji = "\xE7\x84\xA1\xE6\x96\x99";  // "無料"
  const string kMuryouHiragana =
      "\xE3\x82\x80\xE3\x82\x8A\xE3\x82\x87\xE3\x81\x86";  // "むりょう"
  const string kFullWidthSpace = "\xE3\x80\x80";  // full-width space
  {
    // Test for single Kanji character.
    const string &kInput = kHonKanji;  // "本"
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana, segments.conversion_segment(0).candidate(0).value);
  }
  {
    // Test for multi-Kanji character.
    const string &kInput = kMuryouKanji;  // "無料"
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(0).candidate(0).value);
  }
  {
    // Test for multi terms separated by a space.
    const string &kInput = kHonKanji + " " + kMuryouKanji;  // "本 無料"
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana,
              segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(" ", segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for multi terms separated by multiple spaces.
    const string &kInput = kHonKanji + "   " + kMuryouKanji;  // "本   無料"
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana,
              segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ("   ", segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for leading white spaces.
    const string &kInput = "  " + kHonKanji;  // "  本"
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(2, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ("  ", segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(kHonHiragana,
              segments.conversion_segment(1).candidate(0).value);
  }
  {
    // Test for trailing white spaces.
    const string &kInput = kMuryouKanji + "  ";  // "無料  "
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(2, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ("  ",
              segments.conversion_segment(1).candidate(0).value);
  }
  {
    // Test for multi terms separated by a full-width space.
    // "本　無料"
    const string &kInput = kHonKanji + kFullWidthSpace + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana,
              segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(kFullWidthSpace,
              segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for multi terms separated by two full-width spaces.
    // "本　　無料"
    const string &kFullWidthSpace2 = kFullWidthSpace + kFullWidthSpace;
    const string &kInput = kHonKanji + kFullWidthSpace2 + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana,
              segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(kFullWidthSpace2,
              segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for multi terms separated by the mix of full- and half-width spaces.
    // "本　 無料"
    const string &kFullWidthSpace2 = kFullWidthSpace + " ";
    const string &kInput = kHonKanji + kFullWidthSpace2 + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana,
              segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(kFullWidthSpace2,
              segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for math expressions; see b/9398304.
    const string &kInputHalf = "365*24*60*60*1000=";
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInputHalf));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_EQ(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kInputHalf, segments.conversion_segment(0).candidate(0).value);

    // Test for full-width characters.
    segments.Clear();
    // "３６５＊２４＊６０＊６０＊１０００＝"
    const string &kInputFull =
        "\xEF\xBC\x93\xEF\xBC\x96\xEF\xBC\x95\xEF\xBC\x8A\xEF\xBC\x92"
        "\xEF\xBC\x94\xEF\xBC\x8A\xEF\xBC\x96\xEF\xBC\x90\xEF\xBC\x8A"
        "\xEF\xBC\x96\xEF\xBC\x90\xEF\xBC\x8A\xEF\xBC\x91\xEF\xBC\x90"
        "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x9D";
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInputFull));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_EQ(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kInputHalf, segments.conversion_segment(0).candidate(0).value);
  }
}

TEST_F(ConverterTest, GetLastConnectivePart) {
  scoped_ptr<ConverterAndData> converter_and_data(
      CreateStubbedConverterAndData());
  ConverterImpl *converter = converter_and_data->converter.get();

  {
    string key;
    string value;
    uint16 id = 0;
    EXPECT_FALSE(converter->GetLastConnectivePart("", &key, &value, &id));
    EXPECT_FALSE(converter->GetLastConnectivePart(" ", &key, &value, &id));
    EXPECT_FALSE(converter->GetLastConnectivePart("  ", &key, &value, &id));
  }

  {
    string key;
    string value;
    uint16 id = 0;
    EXPECT_TRUE(converter->GetLastConnectivePart("a", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("a", value);
    EXPECT_EQ(
        converter_and_data->converter.get()->pos_matcher_->GetUniqueNounId(),
        id);

    EXPECT_TRUE(converter->GetLastConnectivePart("a ", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("a", value);

    EXPECT_FALSE(converter->GetLastConnectivePart("a  ", &key, &value, &id));

    EXPECT_TRUE(converter->GetLastConnectivePart("a ", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("a", value);

    EXPECT_TRUE(converter->GetLastConnectivePart("a10a", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("a", value);

    // "ａ"
    EXPECT_TRUE(converter->GetLastConnectivePart(
        "\xEF\xBD\x81", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("\xEF\xBD\x81", value);
  }

  {
    string key;
    string value;
    uint16 id = 0;
    EXPECT_TRUE(converter->GetLastConnectivePart("10", &key, &value, &id));
    EXPECT_EQ("10", key);
    EXPECT_EQ("10", value);
    EXPECT_EQ(
        converter_and_data->converter.get()->pos_matcher_->GetNumberId(), id);

    EXPECT_TRUE(converter->GetLastConnectivePart("10a10", &key, &value, &id));
    EXPECT_EQ("10", key);
    EXPECT_EQ("10", value);

    // "１０"
    EXPECT_TRUE(converter->GetLastConnectivePart(
        "\xEF\xBC\x91\xEF\xBC\x90", &key, &value, &id));
    EXPECT_EQ("10", key);
    EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x90", value);
  }

  {
    string key;
    string value;
    uint16 id = 0;
    // "あ"
    EXPECT_FALSE(converter->GetLastConnectivePart(
        "\xE3\x81\x82", &key, &value, &id));
  }
}

TEST_F(ConverterTest, ReconstructHistory) {
  scoped_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();

  // "１０"
  const char kTen[] = "\xEF\xBC\x91\xEF\xBC\x90";

  Segments segments;
  EXPECT_TRUE(converter->ReconstructHistory(&segments, kTen));
  EXPECT_EQ(1, segments.segments_size());
  const Segment &segment = segments.segment(0);
  EXPECT_EQ(Segment::HISTORY, segment.segment_type());
  EXPECT_EQ("10", segment.key());
  EXPECT_EQ(1, segment.candidates_size());
  const Segment::Candidate &candidate = segment.candidate(0);
  EXPECT_EQ(Segment::Candidate::NO_LEARNING, candidate.attributes);
  EXPECT_EQ("10", candidate.content_key);
  EXPECT_EQ("10", candidate.key);
  EXPECT_EQ(kTen, candidate.content_value);
  EXPECT_EQ(kTen, candidate.value);
  EXPECT_NE(0, candidate.lid);
  EXPECT_NE(0, candidate.rid);
}

}  // namespace mozc
