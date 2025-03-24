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

#include "converter/converter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "converter/segments_matchers.h"
#include "data_manager/data_manager.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_dictionary.h"
#include "dictionary/user_pos.h"
#include "engine/engine.h"
#include "engine/mock_data_engine_factory.h"
#include "engine/modules.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "rewriter/date_rewriter.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::MockUserDictionary;
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::Token;
using ::mozc::dictionary::UserDictionary;
using ::mozc::prediction::DefaultPredictor;
using ::mozc::prediction::DictionaryPredictor;
using ::mozc::prediction::MobilePredictor;
using ::mozc::prediction::PredictorInterface;
using ::mozc::prediction::UserHistoryPredictor;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::StrEq;

using UserEntry = user_dictionary::UserDictionary::Entry;

Segment &AddSegment(absl::string_view key, Segment::SegmentType type,
                    Segments &segments) {
  Segment *segment = segments.add_segment();
  segment->set_key(key);
  segment->set_segment_type(type);
  return *segment;
}

void PushBackCandidate(absl::string_view text, Segment &segment) {
  Segment::Candidate *cand = segment.push_back_candidate();
  cand->key = std::string(text);
  cand->content_key = cand->key;
  cand->value = cand->key;
  cand->content_value = cand->key;
}

class StubPredictor : public PredictorInterface {
 public:
  StubPredictor() : predictor_name_("StubPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    if (segments->conversion_segments_size() == 0) {
      return false;
    }
    Segment *seg = segments->mutable_conversion_segment(0);
    if (seg->key().empty()) {
      return false;
    }
    PushBackCandidate(seg->key(), *seg);
    return true;
  }

  absl::string_view GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  const std::string predictor_name_;
};

class StubRewriter : public RewriterInterface {
  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override {
    return true;
  }
};

class InsertPlaceholderWordsRewriter : public RewriterInterface {
  bool Rewrite(const ConversionRequest &, Segments *segments) const override {
    for (Segment &segment : segments->conversion_segments()) {
      {
        Segment::Candidate *cand = segment.add_candidate();
        cand->key = "tobefiltered";
        cand->value = "ToBeFiltered";
      }
      {
        Segment::Candidate *cand = segment.add_candidate();
        cand->key = "nottobefiltered";
        cand->value = "NotToBeFiltered";
      }
    }
    return true;
  }
};

class ResizeSegmentsRewriter : public RewriterInterface {
 public:
  ResizeSegmentsRewriter(size_t segment_index,
                         std::array<uint8_t, 8> segment_sizes)
      : segment_index_(segment_index), segment_sizes_(segment_sizes) {}

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override {
    return true;
  }

  std::optional<RewriterInterface::ResizeSegmentsRequest>
  CheckResizeSegmentsRequest(const ConversionRequest &request,
                             const Segments &segments) const override {
    if (segments.resized()) {
      return std::nullopt;
    }

    RewriterInterface::ResizeSegmentsRequest resize_request = {
        .segment_index = segment_index_,
        .segment_sizes = segment_sizes_,
    };
    return resize_request;
  }

 private:
  size_t segment_index_ = 0;
  std::array<uint8_t, 8> segment_sizes_ = {0, 0, 0, 0, 0, 0, 0, 0};
};

ConversionRequest ConvReq(absl::string_view key,
                          ConversionRequest::RequestType request_type) {
  composer::Composer composer;
  composer.SetPreeditTextForTestOnly(key);
  return ConversionRequestBuilder()
      .SetComposer(composer)
      .SetRequestType(request_type)
      .Build();
}

}  // namespace

class MockPredictor : public mozc::prediction::PredictorInterface {
 public:
  MockPredictor() = default;
  ~MockPredictor() override = default;

  MOCK_METHOD(bool, PredictForRequest, (const ConversionRequest &, Segments *),
              (const, override));
  MOCK_METHOD(void, Revert, (Segments *), (override));
  MOCK_METHOD(absl::string_view, GetPredictorName, (), (const, override));
};

class MockRewriter : public RewriterInterface {
 public:
  MockRewriter() = default;
  ~MockRewriter() override = default;

  MOCK_METHOD(bool, Rewrite, (const ConversionRequest &, Segments *),
              (const, override));
  MOCK_METHOD(void, Revert, (Segments *), (override));
};

class ConverterTest : public testing::TestWithTempUserProfile {
 protected:
  enum PredictorType {
    STUB_PREDICTOR,
    DEFAULT_PREDICTOR,
    MOBILE_PREDICTOR,
  };

  struct UserDefinedEntry {
    const std::string key;
    const std::string value;
    const user_dictionary::UserDictionary::PosType pos;
    UserDefinedEntry(absl::string_view k, absl::string_view v,
                     user_dictionary::UserDictionary::PosType p)
        : key(k), value(v), pos(p) {}
  };

  // Returns initialized predictor for the given type.
  // |converter| will be initialized using predictor pointer, but predictor need
  // the pointer for |converter| for initializing. Prease see
  // mozc/engine/engine.cc for details. Caller should manage the ownership.
  std::unique_ptr<PredictorInterface> CreatePredictor(
      const engine::Modules &modules, PredictorType predictor_type,
      const ConverterInterface &converter,
      const ImmutableConverterInterface &immutable_converter) {
    if (predictor_type == STUB_PREDICTOR) {
      return std::make_unique<StubPredictor>();
    }

    std::function<std::unique_ptr<PredictorInterface>(
        std::unique_ptr<PredictorInterface>,
        std::unique_ptr<PredictorInterface>, const ConverterInterface &)>
        predictor_factory = nullptr;
    bool enable_content_word_learning = false;

    switch (predictor_type) {
      case DEFAULT_PREDICTOR:
        predictor_factory = DefaultPredictor::CreateDefaultPredictor;
        enable_content_word_learning = false;
        break;
      case MOBILE_PREDICTOR:
        predictor_factory = MobilePredictor::CreateMobilePredictor;
        enable_content_word_learning = true;
        break;
      default:
        LOG(ERROR) << "Should not come here: Invalid predictor type.";
        predictor_factory = DefaultPredictor::CreateDefaultPredictor;
        enable_content_word_learning = false;
        break;
    }

    // Create a predictor with three sub-predictors, dictionary predictor, user
    // history predictor, and extra predictor.
    auto dictionary_predictor = std::make_unique<DictionaryPredictor>(
        modules, converter, immutable_converter);
    CHECK(dictionary_predictor);

    auto user_history_predictor = std::make_unique<UserHistoryPredictor>(
        modules, enable_content_word_learning);
    CHECK(user_history_predictor);

    auto ret_predictor =
        predictor_factory(std::move(dictionary_predictor),
                          std::move(user_history_predictor), converter);
    CHECK(ret_predictor);
    return ret_predictor;
  }

  // Initializes Converter with mock data set using given |user_dictionary|.
  std::unique_ptr<Converter> CreateConverter(
      std::unique_ptr<engine::Modules> modules,
      std::unique_ptr<RewriterInterface> rewriter,
      PredictorType predictor_type) {
    return std::make_unique<Converter>(
        std::move(modules),
        [&](const engine::Modules &modules) {
          return std::make_unique<ImmutableConverter>(modules);
        },
        [&](const engine::Modules &modules, const ConverterInterface &converter,
            const ImmutableConverterInterface &immutable_converter) {
          return CreatePredictor(modules, predictor_type, converter,
                                 immutable_converter);
        },
        [&](const engine::Modules &modules) { return std::move(rewriter); });
  }

  std::unique_ptr<Converter> CreateConverter(
      std::unique_ptr<RewriterInterface> rewriter,
      PredictorType predictor_type) {
    std::unique_ptr<engine::Modules> modules =
        engine::Modules::Create(std::make_unique<testing::MockDataManager>())
            .value();
    return CreateConverter(std::move(modules), std::move(rewriter),
                           predictor_type);
  }

  std::unique_ptr<Converter> CreateStubbedConverter() {
    return CreateConverter(std::make_unique<StubRewriter>(), STUB_PREDICTOR);
  }

  std::unique_ptr<Converter> CreateConverterWithUserDefinedEntries(
      absl::Span<const UserDefinedEntry> user_defined_entries,
      PredictorType predictor_type) {
    auto data_manager = std::make_unique<testing::MockDataManager>();

    auto pos_matcher = std::make_unique<dictionary::PosMatcher>(
        data_manager->GetPosMatcherData());
    auto user_dictionary = std::make_unique<dictionary::UserDictionary>(
        dictionary::UserPos::CreateFromDataManager(*data_manager),
        *pos_matcher);
    {
      user_dictionary::UserDictionaryStorage storage;
      user_dictionary::UserDictionary *dictionary = storage.add_dictionaries();
      for (const UserDefinedEntry &user_entry : user_defined_entries) {
        UserEntry *entry = dictionary->add_entries();
        entry->set_key(user_entry.key);
        entry->set_value(user_entry.value);
        entry->set_pos(user_entry.pos);
      }
      user_dictionary->Load(storage);
    }

    std::unique_ptr<engine::Modules> modules =
        engine::ModulesPresetBuilder()
            .PresetPosMatcher(std::move(pos_matcher))
            .PresetUserDictionary(std::move(user_dictionary))
            .Build(std::move(data_manager))
            .value();

    return CreateConverter(std::move(modules), std::make_unique<StubRewriter>(),
                           predictor_type);
  }

  std::unique_ptr<Engine> CreateEngineWithMobilePredictor() {
    return Engine::CreateMobileEngine(
               std::make_unique<testing::MockDataManager>())
        .value();
  }

  bool FindCandidateByValue(absl::string_view value,
                            const Segment &segment) const {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == value) {
        return true;
      }
    }
    return false;
  }

  int GetCandidateIndexByValue(absl::string_view value,
                               const Segment &segment) const {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == value) {
        return i;
      }
    }
    return -1;  // not found
  }

  const commands::Request &default_request() const { return default_request_; }

 private:
  const commands::Request default_request_;
};

// test for issue:2209644
// just checking whether this causes segmentation fault or not.
// TODO(toshiyuki): make dictionary mock and test strictly.
TEST_F(ConverterTest, CanConvertTest) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(
        ConvReq("-", ConversionRequest::CONVERSION), &segments));
  }
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(
        ConvReq("おきておきて", ConversionRequest::CONVERSION), &segments));
  }
}

namespace {
std::string ContextAwareConvert(const std::string &first_key,
                                const std::string &first_value,
                                const std::string &second_key) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);

  Segments segments;
  EXPECT_TRUE(converter->StartConversion(
      ConvReq(first_key, ConversionRequest::CONVERSION), &segments));

  std::string converted;
  int segment_num = 0;
  while (true) {
    int position = -1;
    for (size_t i = 0; i < segments.segment(segment_num).candidates_size();
         ++i) {
      const std::string &value =
          segments.segment(segment_num).candidate(i).value;
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
  converter->FinishConversion(default_request, &segments);
  EXPECT_TRUE(converter->StartConversion(
      ConvReq(second_key, ConversionRequest::CONVERSION), &segments));
  EXPECT_EQ(segments.segments_size(), segment_num + 1);

  return segments.segment(segment_num).candidate(0).value;
}
}  // namespace

TEST_F(ConverterTest, ContextAwareConversionTest) {
  // Desirable context aware conversions
  EXPECT_EQ(ContextAwareConvert("きき", "危機", "いっぱつ"), "一髪");

  EXPECT_EQ(ContextAwareConvert("きょうと", "京都", "だい"), "大");
  EXPECT_EQ(ContextAwareConvert("もんだい", "問題", "てん"), "点");

  EXPECT_EQ(ContextAwareConvert("いのうえ", "井上", "ようすい"), "陽水");

  // Undesirable context aware conversions
  EXPECT_NE(ContextAwareConvert("19じ", "19時", "しゅうごう"), "宗号");

  EXPECT_NE(ContextAwareConvert("の", "の", "なまえ"), "な前");

  EXPECT_NE(ContextAwareConvert("の", "の", "しりょう"), "し料");

  EXPECT_NE(ContextAwareConvert("ぼくと", "僕と", "しらいさん"), "し礼賛");
}

TEST_F(ConverterTest, CommitSegmentValue) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
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
    EXPECT_EQ(segments.segments_size(), 2);
    EXPECT_EQ(segments.history_segments_size(), 0);
    EXPECT_EQ(segments.conversion_segments_size(), 2);
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(segment.segment_type(), Segment::FIXED_VALUE);
    EXPECT_EQ(segment.candidate(0).value, "2");
    EXPECT_NE(segment.candidate(0).attributes & Segment::Candidate::RERANKED,
              0);
  }
  {
    // Make the segment SUBMITTED
    segments.mutable_conversion_segment(0)->set_segment_type(
        Segment::SUBMITTED);
    EXPECT_EQ(segments.segments_size(), 2);
    EXPECT_EQ(segments.history_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segments_size(), 1);
  }
  {
    // Commit the candidate whose value is "3".
    EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 0));
    EXPECT_EQ(segments.segments_size(), 2);
    EXPECT_EQ(segments.history_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(segment.segment_type(), Segment::FIXED_VALUE);
    EXPECT_EQ(segment.candidate(0).value, "3");
    EXPECT_EQ(segment.candidate(0).attributes & Segment::Candidate::RERANKED,
              0);
  }
}

TEST_F(ConverterTest, CommitSegments) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  // History segment.
  {
    Segment *segment = segments.add_segment();
    segment->set_key("あした");
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "あした";
    candidate->value = "今日";
  }

  {
    Segment *segment = segments.add_segment();
    segment->set_key("かつこうに");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "学校に";
    candidate->key = "がっこうに";
  }

  {
    Segment *segment = segments.add_segment();
    segment->set_key("いく");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "行く";
    candidate->key = "いく";
  }

  // Test "CommitFirstSegment" feature.
  {
    // Commit 1st segment.
    std::vector<size_t> index_list;
    index_list.push_back(0);
    ASSERT_TRUE(converter->CommitSegments(&segments, index_list));

    EXPECT_EQ(segments.history_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.history_segment(0).segment_type(), Segment::HISTORY);
    EXPECT_EQ(segments.history_segment(1).segment_type(), Segment::SUBMITTED);
  }

  // Reset the test data.
  segments.mutable_history_segment(1)->set_segment_type(Segment::FREE);

  // Test "CommitHeadToFocusedSegment" feature.
  {
    // Commit 1st and 2nd segments.
    std::vector<size_t> index_list;
    index_list.push_back(0);
    index_list.push_back(0);
    ASSERT_TRUE(converter->CommitSegments(&segments, index_list));

    EXPECT_EQ(segments.history_segments_size(), 3);
    EXPECT_EQ(segments.conversion_segments_size(), 0);
    EXPECT_EQ(segments.history_segment(0).segment_type(), Segment::HISTORY);
    EXPECT_EQ(segments.history_segment(1).segment_type(), Segment::SUBMITTED);
    EXPECT_EQ(segments.history_segment(2).segment_type(), Segment::SUBMITTED);
  }
}

TEST_F(ConverterTest, CommitPartialSuggestionSegmentValue) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
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
    EXPECT_EQ(segments.segments_size(), 3);
    EXPECT_EQ(segments.history_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segments_size(), 2);
    {
      // The tail segment of the history segments uses
      // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
      // and contains original value.
      const Segment &segment =
          segments.history_segment(segments.history_segments_size() - 1);
      EXPECT_EQ(segment.segment_type(), Segment::SUBMITTED);
      EXPECT_EQ(segment.candidate(0).value, "2");
      EXPECT_EQ(segment.key(), "left2");
      EXPECT_NE(segment.candidate(0).attributes & Segment::Candidate::RERANKED,
                0);
    }
    {
      // The head segment of the conversion segments uses |new_segment_key|.
      const Segment &segment = segments.conversion_segment(0);
      EXPECT_EQ(segment.segment_type(), Segment::FREE);
      EXPECT_EQ(segment.key(), "right2");
    }
  }
}

TEST_F(ConverterTest, CandidateKeyTest) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartConversion(
      ConvReq("わたしは", ConversionRequest::CONVERSION), &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたし");
}

TEST_F(ConverterTest, Regression3437022) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  Segments segments;

  const std::string kKey1 = "けいたい";
  const std::string kKey2 = "でんわ";

  const std::string kValue1 = "携帯";
  const std::string kValue2 = "電話";

  {
    // Make sure convert result is one segment
    EXPECT_TRUE(converter->StartConversion(
        ConvReq(kKey1 + kKey2, ConversionRequest::CONVERSION), &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kValue1 + kValue2);
  }
  {
    // Make sure we can convert first part
    segments.Clear();
    EXPECT_TRUE(converter->StartConversion(
        ConvReq(kKey1, ConversionRequest::CONVERSION), &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kValue1);
  }
  {
    // Make sure we can convert last part
    segments.Clear();
    EXPECT_TRUE(converter->StartConversion(
        ConvReq(kKey2, ConversionRequest::CONVERSION), &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kValue2);
  }

  segments.Clear();

  // Add compound entry to suppression dictionary
  {
    user_dictionary::UserDictionaryStorage storage;
    UserEntry *entry = storage.add_dictionaries()->add_entries();
    entry->set_key(kKey1 + kKey2);
    entry->set_value(kValue1 + kValue2);
    entry->set_pos(user_dictionary::UserDictionary::SUPPRESSION_WORD);
    engine->GetModulesForTesting().GetUserDictionary().Load(storage);
    EXPECT_TRUE(engine->GetModulesForTesting()
                    .GetUserDictionary()
                    .HasSuppressedEntries());
  }

  EXPECT_TRUE(converter->StartConversion(
      ConvReq(kKey1 + kKey2, ConversionRequest::CONVERSION), &segments));

  int rest_size = 0;
  for (const Segment &segment : segments.conversion_segments().drop(1)) {
    rest_size += Util::CharsLen(segment.candidate(0).key);
  }

  // Expand segment so that the entire part will become one segment
  if (rest_size > 0) {
    const ConversionRequest default_request;
    EXPECT_TRUE(
        converter->ResizeSegment(&segments, default_request, 0, rest_size));
  }

  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_NE(segments.conversion_segment(0).candidate(0).value,
            kValue1 + kValue2);
}

TEST_F(ConverterTest, CompletePosIds) {
  const char *kTestKeys[] = {
      "きょうと", "いきます",         "うつくしい",
      "おおきな", "いっちゃわないね", "わたしのなまえはなかのです",
  };

  std::unique_ptr<Converter> converter = CreateStubbedConverter();
  for (size_t i = 0; i < std::size(kTestKeys); ++i) {
    Segments segments;
    Segment *seg = segments.add_segment();
    seg->set_key(kTestKeys[i]);
    seg->set_segment_type(Segment::FREE);
    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetOptions({
                .request_type = ConversionRequest::PREDICTION,
                .max_conversion_candidates_size = 20,
            })
            .Build();
    CHECK(
        converter->immutable_converter().ConvertForRequest(request, &segments));
    const int lid = segments.segment(0).candidate(0).lid;
    const int rid = segments.segment(0).candidate(0).rid;
    Segment::Candidate candidate;
    candidate.value = segments.segment(0).candidate(0).value;
    candidate.key = segments.segment(0).candidate(0).key;
    candidate.lid = 0;
    candidate.rid = 0;
    converter->CompletePosIds(&candidate);
    EXPECT_EQ(candidate.lid, lid);
    EXPECT_EQ(candidate.rid, rid);
    EXPECT_NE(candidate.lid, 0);
    EXPECT_NE(candidate.rid, 0);
  }

  {
    Segment::Candidate candidate;
    candidate.key = "test";
    candidate.value = "test";
    candidate.lid = 10;
    candidate.rid = 11;
    converter->CompletePosIds(&candidate);
    EXPECT_EQ(candidate.lid, 10);
    EXPECT_EQ(candidate.rid, 11);
  }
}

TEST_F(ConverterTest, Regression3046266) {
  // Shouldn't correct nodes at the beginning of a sentence.
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  Segments segments;

  // Can be any string that has "ん" at the end
  constexpr char kKey1[] = "かん";

  // Can be any string that has a vowel at the beginning
  constexpr char kKey2[] = "あか";

  constexpr char kValueNotExpected[] = "中";

  EXPECT_TRUE(converter->StartConversion(
      ConvReq(kKey1, ConversionRequest::CONVERSION), &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 0));

  // TODO(team): Use StartConversionForRequest instead of StartConversion.
  const ConversionRequest default_request;
  converter->FinishConversion(default_request, &segments);

  EXPECT_TRUE(converter->StartConversion(
      ConvReq(kKey2, ConversionRequest::CONVERSION), &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_NE(segment.candidate(i).value, kValueNotExpected);
  }
}

TEST_F(ConverterTest, Regression5502496) {
  // Make sure key correction works for the first word of a sentence.
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  Segments segments;

  constexpr char kKey[] = "みんあ";
  constexpr char kValueExpected[] = "みんな";

  EXPECT_TRUE(converter->StartConversion(
      ConvReq(kKey, ConversionRequest::CONVERSION), &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  const Segment &segment = segments.conversion_segment(0);
  bool found = false;
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == kValueExpected) {
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(ConverterTest, StartSuggestion) {
  commands::Request client_request;
  client_request.set_mixed_conversion(true);

  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);

  const std::string kShi = "し";

  auto table = std::make_shared<composer::Table>();
  table->AddRule("si", kShi, "");
  table->AddRule("shi", kShi, "");
  config::Config config;

  {
    composer::Composer composer(table, client_request, config);

    composer.InsertCharacter("shi");

    ConversionRequest::Options options = {.request_type =
                                              ConversionRequest::SUGGESTION};
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetComposer(composer)
                                          .SetRequestView(client_request)
                                          .SetConfigView(config)
                                          .SetOptions(std::move(options))
                                          .Build();

    Segments segments;
    EXPECT_TRUE(converter->StartPrediction(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    ASSERT_TRUE(segments.segment(0).meta_candidates_size() >=
                transliteration::HALF_ASCII);
    EXPECT_EQ(
        segments.segment(0).meta_candidate(transliteration::HALF_ASCII).value,
        "shi");
  }

  {
    composer::Composer composer(table, client_request, config);

    composer.InsertCharacter("si");

    ConversionRequest::Options options = {.request_type =
                                              ConversionRequest::SUGGESTION};
    const ConversionRequest request = ConversionRequestBuilder()
                                          .SetComposer(composer)
                                          .SetRequestView(client_request)
                                          .SetConfigView(config)
                                          .SetOptions(std::move(options))
                                          .Build();

    Segments segments;
    EXPECT_TRUE(converter->StartPrediction(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    ASSERT_TRUE(segments.segment(0).meta_candidates_size() >=
                transliteration::HALF_ASCII);
    EXPECT_EQ(
        segments.segment(0).meta_candidate(transliteration::HALF_ASCII).value,
        "si");
  }
}

TEST_F(ConverterTest, StartPartialPrediction) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPrediction(
      ConvReq("わたしは", ConversionRequest::PARTIAL_PREDICTION), &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたしは");
}

TEST_F(ConverterTest, StartPartialSuggestion) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPrediction(
      ConvReq("わたしは", ConversionRequest::PARTIAL_SUGGESTION), &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたしは");
}

TEST_F(ConverterTest, StartPartialPredictionMobile) {
  std::unique_ptr<Engine> engine = CreateEngineWithMobilePredictor();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPrediction(
      ConvReq("わたしは", ConversionRequest::PARTIAL_PREDICTION), &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたしは");
}

TEST_F(ConverterTest, StartPartialSuggestionMobile) {
  std::unique_ptr<Engine> engine = CreateEngineWithMobilePredictor();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPrediction(
      ConvReq("わたしは", ConversionRequest::PARTIAL_SUGGESTION), &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたしは");
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

  Converter::MaybeSetConsumedKeySizeToSegment(consumed_key_size, &segment);
  EXPECT_NE((segment.candidate(0).attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED),
            0);
  EXPECT_EQ(segment.candidate(0).consumed_key_size, consumed_key_size);
  EXPECT_NE((segment.candidate(1).attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED),
            0);
  EXPECT_EQ(segment.candidate(1).consumed_key_size, original_consumed_key_size);
  EXPECT_NE((segment.meta_candidate(0).attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED),
            0);
  EXPECT_EQ(segment.meta_candidate(0).consumed_key_size, consumed_key_size);
  EXPECT_NE((segment.meta_candidate(1).attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED),
            0);
  EXPECT_EQ(segment.meta_candidate(1).consumed_key_size,
            original_consumed_key_size);
}

TEST_F(ConverterTest, PredictSetKey) {
  constexpr char kPredictionKey[] = "prediction key";
  constexpr char kPredictionKey2[] = "prediction key2";
  // Tests whether SetKey method is called or not.
  struct TestData {
    // Input conditions.
    const std::optional<absl::string_view> key;  // Input key presence.

    const bool expect_set_key_is_called;
  };
  const TestData test_data_list[] = {
      {std::nullopt, true},
      {kPredictionKey2, true},
      // This is the only case where SetKey() is not called; because SetKey is
      // not requested in Request and Segments' key is already present.
      {kPredictionKey, false},
  };

  std::unique_ptr<Converter> converter = CreateStubbedConverter();

  for (const TestData &test_data : test_data_list) {
    Segments segments;
    int orig_candidates_size = 0;
    if (test_data.key) {
      Segment *seg = segments.add_segment();
      seg->set_key(*test_data.key);
      PushBackCandidate(*test_data.key, *seg);
      orig_candidates_size = seg->candidates_size();
    }

    composer::Composer composer;
    composer.SetPreeditTextForTestOnly(kPredictionKey);
    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetRequestType(ConversionRequest::PREDICTION)
            .Build();
    ASSERT_TRUE(converter->StartPrediction(request, &segments));

    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).key(), kPredictionKey);
    if (test_data.expect_set_key_is_called) {
      // If SetKey is called, the segment has only one candidate populated by
      // StubPredictor.
      EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    } else {
      // If SetKey is not called, the segment has been added one candidate by
      // StubPredictor.
      const int expected_candidates_size = orig_candidates_size + 1;
      EXPECT_EQ(segments.conversion_segment(0).candidates_size(),
                expected_candidates_size);
    }
  }
}

// An action that invokes a DictionaryInterface::Callback with the token whose
// key and value is set to the given ones.
struct InvokeCallbackWithUserDictionaryToken {
  template <class T, class U>
  void operator()(T, U, DictionaryInterface::Callback *callback) {
    const Token token(key, value, MockDictionary::kDefaultCost,
                      MockDictionary::kDefaultPosId,
                      MockDictionary::kDefaultPosId, Token::USER_DICTIONARY);
    callback->OnToken(key, key, token);
  }

  std::string key;
  std::string value;
};

TEST_F(ConverterTest, VariantExpansionForSuggestion) {
  // Create Converter with mock user dictionary
  auto mock_user_dictionary = std::make_unique<MockUserDictionary>();
  EXPECT_CALL(*mock_user_dictionary, LookupPredictive(_, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_user_dictionary, LookupPredictive(StrEq("てすと"), _, _))
      .WillRepeatedly(InvokeCallbackWithUserDictionaryToken{"てすと", "<>!?"});

  EXPECT_CALL(*mock_user_dictionary, LookupPrefix(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_user_dictionary, LookupPrefix(StrEq("てすとの"), _, _))
      .WillRepeatedly(InvokeCallbackWithUserDictionaryToken{"てすと", "<>!?"});

  std::unique_ptr<engine::Modules> modules =
      engine::ModulesPresetBuilder()
          .PresetUserDictionary(std::move(mock_user_dictionary))
          .Build(std::make_unique<testing::MockDataManager>())
          .value();

  Converter converter(
      std::move(modules),
      [&](const engine::Modules &modules) {
        return std::make_unique<ImmutableConverter>(modules);
      },
      [](const engine::Modules &modules, const ConverterInterface &converter,
         const ImmutableConverterInterface &immutable_converter) {
        return DefaultPredictor::CreateDefaultPredictor(
            std::make_unique<DictionaryPredictor>(modules, converter,
                                                  immutable_converter),
            std::make_unique<UserHistoryPredictor>(modules, false), converter);
      },
      [](const engine::Modules &modules) {
        return std::make_unique<Rewriter>(modules);
      });

  Segments segments;
  {
    // Dictionary suggestion
    EXPECT_TRUE(converter.StartPrediction(
        ConvReq("てすと", ConversionRequest::SUGGESTION), &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_TRUE(FindCandidateByValue("<>!?", segments.conversion_segment(0)));
    EXPECT_FALSE(
        FindCandidateByValue("＜＞！？", segments.conversion_segment(0)));
  }
  {
    // Realtime conversion
    segments.Clear();
    EXPECT_TRUE(converter.StartPrediction(
        ConvReq("てすとの", ConversionRequest::SUGGESTION), &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_TRUE(FindCandidateByValue("<>!?の", segments.conversion_segment(0)));
    EXPECT_FALSE(
        FindCandidateByValue("＜＞！？の", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, ComposerKeySelection) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  auto table = std::make_shared<composer::Table>();
  config::Config config;
  {
    Segments segments;
    composer::Composer composer(table, default_request(), config);
    composer.InsertCharacterPreedit("わたしh");

    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetOptions(
                {.composer_key_selection = ConversionRequest::CONVERSION_KEY})
            .Build();
    ASSERT_TRUE(converter->StartConversion(request, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "私");
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "h");
  }
  {
    Segments segments;
    composer::Composer composer(table, default_request(), config);
    composer.InsertCharacterPreedit("わたしh");

    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetOptions(
                {.composer_key_selection = ConversionRequest::PREDICTION_KEY})
            .Build();
    ASSERT_TRUE(converter->StartConversion(request, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "私");
  }
}

TEST_F(ConverterTest, SuppressionDictionaryForRewriter) {
  std::unique_ptr<Converter> converter = CreateConverter(
      std::make_unique<InsertPlaceholderWordsRewriter>(), STUB_PREDICTOR);

  engine::Modules &modules = converter->modules();

  // Set up suppression dictionary
  {
    user_dictionary::UserDictionaryStorage storage;
    UserEntry *entry = storage.add_dictionaries()->add_entries();
    entry->set_key("tobefiltered");
    entry->set_value("ToBeFiltered");
    entry->set_pos(user_dictionary::UserDictionary::SUPPRESSION_WORD);
    modules.GetUserDictionary().Load(storage);
    EXPECT_TRUE(modules.GetUserDictionary().HasSuppressedEntries());
  }

  // Convert
  auto table = std::make_shared<composer::Table>();
  config::Config config;
  composer::Composer composer(table, default_request(), config);
  composer.InsertCharacter("placeholder");
  commands::Context context;
  const ConversionRequest request = ConversionRequestBuilder()
                                        .SetComposer(composer)
                                        .SetConfig(config)
                                        .Build();
  Segments segments;
  EXPECT_TRUE(converter->StartConversion(request, &segments));

  // Verify that words inserted by the rewriter is suppressed if its in the
  // suppression_dictionary.
  for (const Segment &segment : segments.conversion_segments()) {
    EXPECT_FALSE(FindCandidateByValue("ToBeFiltered", segment));
    EXPECT_TRUE(FindCandidateByValue("NotToBeFiltered", segment));
  }
}

TEST_F(ConverterTest, EmptyConvertReverseIssue8661091) {
  // This is a test case against b/8661091.
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  Segments segments;
  EXPECT_FALSE(converter->StartReverseConversion(&segments, ""));
}

TEST_F(ConverterTest, StartReverseConversion) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  const std::shared_ptr<const ConverterInterface> converter =
      engine->GetConverter();

  const std::string kHonKanji = "本";
  const std::string kHonHiragana = "ほん";
  const std::string kMuryouKanji = "無料";
  const std::string kMuryouHiragana = "むりょう";
  const std::string kFullWidthSpace = "　";  // full-width space
  {
    // Test for single Kanji character.
    const std::string &kInput = kHonKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
  }
  {
    // Test for multi-Kanji character.
    const std::string &kInput = kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by a space.
    const std::string kInput = absl::StrCat(kHonKanji, " ", kMuryouKanji);
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, " ");
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by multiple spaces.
    const std::string kInput = kHonKanji + "   " + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "   ");
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for leading white spaces.
    const std::string kInput = "  " + kHonKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 2);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "  ");
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, kHonHiragana);
  }
  {
    // Test for trailing white spaces.
    const std::string kInput = kMuryouKanji + "  ";
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 2);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kMuryouHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "  ");
  }
  {
    // Test for multi terms separated by a full-width space.
    const std::string kInput = kHonKanji + kFullWidthSpace + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by two full-width spaces.
    const std::string kFullWidthSpace2 = kFullWidthSpace + kFullWidthSpace;
    const std::string kInput = kHonKanji + kFullWidthSpace2 + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace2);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by the mix of full- and half-width spaces.
    const std::string kFullWidthSpace2 = kFullWidthSpace + " ";
    const std::string kInput = kHonKanji + kFullWidthSpace2 + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace2);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for math expressions; see b/9398304.
    const absl::string_view kInputHalf = "365*24*60*60*1000=";
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInputHalf));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kInputHalf);

    // Test for full-width characters.
    segments.Clear();
    const absl::string_view kInputFull = "３６５＊２４＊６０＊６０＊１０００＝";
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInputFull));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kInputHalf);
  }
}

TEST_F(ConverterTest, ReconstructHistory) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  constexpr char kTen[] = "１０";

  Segments segments;
  EXPECT_TRUE(converter->ReconstructHistory(&segments, kTen));
  EXPECT_EQ(segments.segments_size(), 1);
  const Segment &segment = segments.segment(0);
  EXPECT_EQ(segment.segment_type(), Segment::HISTORY);
  EXPECT_EQ(segment.key(), "10");
  EXPECT_EQ(segment.candidates_size(), 1);
  const Segment::Candidate &candidate = segment.candidate(0);
  EXPECT_EQ(candidate.attributes, Segment::Candidate::NO_LEARNING);
  EXPECT_EQ(candidate.content_key, "10");
  EXPECT_EQ(candidate.key, "10");
  EXPECT_EQ(candidate.content_value, kTen);
  EXPECT_EQ(candidate.value, kTen);
  EXPECT_NE(candidate.lid, 0);
  EXPECT_NE(candidate.rid, 0);
}

TEST_F(ConverterTest, LimitCandidatesSize) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  auto table = std::make_shared<composer::Table>();
  const config::Config &config = config::ConfigHandler::DefaultConfig();
  mozc::commands::Request request_proto;
  mozc::composer::Composer composer(table, request_proto, config);
  composer.InsertCharacterPreedit("あ");
  const ConversionRequest request1 = ConversionRequestBuilder()
                                         .SetComposer(composer)
                                         .SetRequest(request_proto)
                                         .Build();
  Segments segments;
  ASSERT_TRUE(converter->StartConversion(request1, &segments));
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  const int original_candidates_size = segments.segment(0).candidates_size();
  const int original_meta_candidates_size =
      segments.segment(0).meta_candidates_size();
  EXPECT_LT(0, original_candidates_size - 1 - original_meta_candidates_size)
      << "original candidates size: " << original_candidates_size
      << ", original meta candidates size: " << original_meta_candidates_size;

  segments.Clear();
  request_proto.set_candidates_size_limit(original_candidates_size - 1);
  const ConversionRequest request2 = ConversionRequestBuilder()
                                         .SetComposer(composer)
                                         .SetRequest(request_proto)
                                         .Build();
  ASSERT_TRUE(converter->StartConversion(request2, &segments));
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_GE(original_candidates_size - 1,
            segments.segment(0).candidates_size());
  EXPECT_LE(original_candidates_size - 1 - original_meta_candidates_size,
            segments.segment(0).candidates_size());
  EXPECT_EQ(segments.segment(0).meta_candidates_size(),
            original_meta_candidates_size);

  segments.Clear();
  request_proto.set_candidates_size_limit(0);
  const ConversionRequest request3 = ConversionRequestBuilder()
                                         .SetComposer(composer)
                                         .SetRequest(request_proto)
                                         .Build();
  ASSERT_TRUE(converter->StartConversion(request3, &segments));
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).meta_candidates_size(),
            original_meta_candidates_size);
}

TEST_F(ConverterTest, UserEntryShouldBePromoted) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));

  std::unique_ptr<Converter> converter = CreateConverterWithUserDefinedEntries(
      user_defined_entries, STUB_PREDICTOR);

  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(
        ConvReq("あい", ConversionRequest::CONVERSION), &segments));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "哀");
  }
}

TEST_F(ConverterTest, UserEntryInMobilePrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("てすと", "google", UserDictionary::NO_POS));

  commands::Request request;
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  auto table = std::make_shared<composer::Table>();
  composer::Composer composer(table, request, config);
  request_test_util::FillMobileRequest(&request);

  std::unique_ptr<Converter> converter = CreateConverterWithUserDefinedEntries(
      user_defined_entries, MOBILE_PREDICTOR);

  {
    composer.SetPreeditTextForTestOnly("てすとが");
    ConversionRequest::Options options = {
        .request_type = ConversionRequest::PREDICTION,
    };
    const ConversionRequest conversion_request =
        ConversionRequestBuilder()
            .SetComposer(composer)
            .SetRequestView(request)
            .SetConfigView(config)
            .SetOptions(std::move(options))
            .Build();
    Segments segments;
    EXPECT_TRUE(converter->StartPrediction(conversion_request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                ContainsCandidate(
                    Field(&Segment::Candidate::value, StrEq("googleが"))));
  }
}

TEST_F(ConverterTest, UserEntryShouldBePromotedMobilePrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));

  std::unique_ptr<Converter> converter = CreateConverterWithUserDefinedEntries(
      user_defined_entries, MOBILE_PREDICTOR);

  {
    Segments segments;
    EXPECT_TRUE(converter->StartPrediction(
        ConvReq("あい", ConversionRequest::PREDICTION), &segments));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());

    // "哀" should be the top result for the key "あい".
    int first_ai_index = -1;
    for (int i = 0; i < segments.conversion_segment(0).candidates_size(); ++i) {
      if (segments.conversion_segment(0).candidate(i).key == "あい") {
        first_ai_index = i;
        break;
      }
    }
    ASSERT_NE(first_ai_index, -1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(first_ai_index).value,
              "哀");
  }
}

TEST_F(ConverterTest, SuppressionEntryShouldBePrioritized) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::SUPPRESSION_WORD));

  std::unique_ptr<Converter> converter = CreateConverterWithUserDefinedEntries(
      user_defined_entries, STUB_PREDICTOR);

  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(
        ConvReq("あい", ConversionRequest::CONVERSION), &segments));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());
    EXPECT_FALSE(FindCandidateByValue("哀", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, SuppressionEntryShouldBePrioritizedPrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::SUPPRESSION_WORD));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < std::size(types); ++i) {
    std::unique_ptr<Converter> converter =
        CreateConverterWithUserDefinedEntries(user_defined_entries, types[i]);
    {
      Segments segments;
      EXPECT_TRUE(converter->StartPrediction(
          ConvReq("あい", ConversionRequest::PREDICTION), &segments));
      ASSERT_EQ(segments.conversion_segments_size(), 1);
      ASSERT_LT(1, segments.conversion_segment(0).candidates_size());
      EXPECT_FALSE(FindCandidateByValue("哀", segments.conversion_segment(0)));
    }
  }
}

TEST_F(ConverterTest, AbbreviationShouldBeIndependent) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::ABBREVIATION));

  std::unique_ptr<Converter> converter = CreateConverterWithUserDefinedEntries(
      user_defined_entries, STUB_PREDICTOR);

  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(
        ConvReq("じゅうじか", ConversionRequest::CONVERSION), &segments));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_FALSE(
        FindCandidateByValue("Google+うじか", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, AbbreviationShouldBeIndependentPrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::ABBREVIATION));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < std::size(types); ++i) {
    std::unique_ptr<Converter> converter =
        CreateConverterWithUserDefinedEntries(user_defined_entries, types[i]);

    {
      Segments segments;
      EXPECT_TRUE(converter->StartPrediction(
          ConvReq("じゅうじか", ConversionRequest::PREDICTION), &segments));
      ASSERT_EQ(segments.conversion_segments_size(), 1);
      EXPECT_FALSE(FindCandidateByValue("Google+うじか",
                                        segments.conversion_segment(0)));
    }
  }
}

TEST_F(ConverterTest, SuggestionOnlyShouldBeIndependent) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::SUGGESTION_ONLY));

  std::unique_ptr<Converter> converter = CreateConverterWithUserDefinedEntries(
      user_defined_entries, STUB_PREDICTOR);

  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(
        ConvReq("じゅうじか", ConversionRequest::CONVERSION), &segments));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_FALSE(
        FindCandidateByValue("Google+うじか", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, SuggestionOnlyShouldBeIndependentPrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::SUGGESTION_ONLY));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < std::size(types); ++i) {
    std::unique_ptr<Converter> converter =
        CreateConverterWithUserDefinedEntries(user_defined_entries, types[i]);

    {
      Segments segments;
      EXPECT_TRUE(converter->StartConversion(
          ConvReq("じゅうじか", ConversionRequest::CONVERSION), &segments));
      ASSERT_EQ(segments.conversion_segments_size(), 1);
      EXPECT_FALSE(FindCandidateByValue("Google+うじか",
                                        segments.conversion_segment(0)));
    }
  }
}

TEST_F(ConverterTest, RewriterShouldRespectDefaultCandidates) {
  std::unique_ptr<Engine> engine = CreateEngineWithMobilePredictor();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);

  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  commands::Request request;
  request_test_util::FillMobileRequest(&request);
  auto table = std::make_shared<composer::Table>();
  composer::Composer composer(table, request, config);
  composer.SetPreeditTextForTestOnly("あい");

  ConversionRequest::Options options = {
      .request_type = ConversionRequest::PREDICTION,
  };
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetComposer(composer)
          .SetRequestView(request)
          .SetConfigView(config)
          .SetOptions(std::move(options))
          .Build();
  Segments segments;

  // Remember user history 3 times after getting the top candidate
  std::string top_candidate;
  absl::flat_hash_set<std::string> seen;
  for (int i = 0; i < 4; ++i) {
    segments.Clear();
    EXPECT_TRUE(converter->StartPrediction(conversion_request, &segments));
    const Segment &segment = segments.conversion_segment(0);
    if (i == 0) {
      top_candidate = segment.candidate(0).value;
      seen.insert(top_candidate);
      continue;
    }

    for (int index = 0; index < segment.candidates_size(); ++index) {
      const bool inserted = seen.insert(segment.candidate(index).value).second;
      if (inserted) {
        ASSERT_TRUE(converter->CommitSegmentValue(&segments, 0, index));
        break;
      }
    }
    converter->FinishConversion(conversion_request, &segments);
  }

  segments.Clear();
  EXPECT_TRUE(converter->StartPrediction(conversion_request, &segments));

  int default_candidate_index = -1;
  for (int i = 0; i < segments.conversion_segment(0).candidates_size(); ++i) {
    if (segments.conversion_segment(0).candidate(i).value == top_candidate) {
      default_candidate_index = i;
      break;
    }
  }
  ASSERT_NE(default_candidate_index, -1);
  EXPECT_LE(default_candidate_index, 3);
}

TEST_F(ConverterTest,
       DoNotPromotePrefixOfSingleEntryForEnrichPartialCandidates) {
  std::unique_ptr<Engine> engine = CreateEngineWithMobilePredictor();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);

  commands::Request request;
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  auto table = std::make_shared<composer::Table>();
  composer::Composer composer(table, request, config);
  request_test_util::FillMobileRequest(&request);
  composer.SetPreeditTextForTestOnly("おつかれ");

  ConversionRequest::Options options = {.request_type =
                                            ConversionRequest::PREDICTION};
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetComposer(composer)
          .SetRequestView(request)
          .SetConfigView(config)
          .SetOptions(std::move(options))
          .Build();

  Segments segments;

  EXPECT_TRUE(converter->StartPrediction(conversion_request, &segments));

  int o_index = GetCandidateIndexByValue("お", segments.conversion_segment(0));
  int otsukare_index =
      GetCandidateIndexByValue("お疲れ", segments.conversion_segment(0));
  EXPECT_NE(otsukare_index, -1);
  EXPECT_TRUE(o_index == -1 || (otsukare_index < o_index));
}

TEST_F(ConverterTest, DoNotAddOverlappingNodesForPrediction) {
  std::unique_ptr<Engine> engine = CreateEngineWithMobilePredictor();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();
  CHECK(converter);
  commands::Request request;
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  auto table = std::make_shared<composer::Table>();
  composer::Composer composer(table, request, config);
  request_test_util::FillMobileRequest(&request);
  const dictionary::PosMatcher pos_matcher(
      engine->GetModulesForTesting().GetDataManager().GetPosMatcherData());
  ConversionRequest::Options options = {
      .request_type = ConversionRequest::PREDICTION,
      .create_partial_candidates = true,
  };

  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetComposer(composer)
          .SetRequestView(request)
          .SetConfigView(config)
          .SetOptions(std::move(options))
          .Build();

  Segments segments;
  // History segment.
  {
    Segment *segment = segments.add_segment();
    segment->set_key("に");
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "に";
    candidate->value = "に";
    // Hack: Get POS for "助詞".
    // The POS group of the test dictionary entries, "に" and "にて" should be
    // the same to trigger overlapping lookup.
    candidate->lid = pos_matcher.GetAcceptableParticleAtBeginOfSegmentId();
  }
  composer.SetPreeditTextForTestOnly("てはい");

  EXPECT_TRUE(converter->StartPrediction(conversion_request, &segments));
  EXPECT_FALSE(FindCandidateByValue("て廃", segments.conversion_segment(0)));
}

TEST_F(ConverterTest, RevertConversion) {
  auto mock_predictor = absl::make_unique<MockPredictor>();
  auto mock_rewriter = absl::make_unique<MockRewriter>();

  EXPECT_CALL(*mock_predictor, Revert(_)).Times(1);
  EXPECT_CALL(*mock_rewriter, Revert(_)).Times(1);

  std::unique_ptr<engine::Modules> modules =
      engine::Modules::Create(std::make_unique<testing::MockDataManager>())
          .value();

  std::unique_ptr<Converter> converter = std::make_unique<Converter>(
      std::move(modules),
      [](const engine::Modules &modules) {
        return std::make_unique<ImmutableConverter>(modules);
      },
      [&mock_predictor](
          const engine::Modules &modules, const ConverterInterface &converter,
          const ImmutableConverterInterface &immutable_converter) {
        return std::move(mock_predictor);
      },
      [&mock_rewriter](const engine::Modules &modules) {
        return std::move(mock_rewriter);
      });

  Segments segments;
  segments.push_back_revert_entry();

  converter->RevertConversion(&segments);
}

TEST_F(ConverterTest, ResizeSegmentWithOffset) {
  constexpr Segment::SegmentType kFixedBoundary = Segment::FIXED_BOUNDARY;
  constexpr Segment::SegmentType kFree = Segment::FREE;

  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  {
    // Resize {"あいうえ"} to {"あいう", "え"}
    Segments segments;
    AddSegment("あいうえ", kFree, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const int offset = -1;
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(converter->ResizeSegment(&segments, convreq,
                                         start_segment_index, offset));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFree);
  }

  {
    // Resize {"あ", "い", "う", "え"} to {"あいう", "え"}
    Segments segments;
    AddSegment("あ", kFree, segments);
    AddSegment("い", kFree, segments);
    AddSegment("う", kFree, segments);
    AddSegment("え", kFree, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const int offset = 2;
    EXPECT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_TRUE(converter->ResizeSegment(&segments, convreq,
                                         start_segment_index, offset));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFree);
  }

  {
    // Resize {"あ", "い", "う", "え"} to {"あ", "いう", "え"}
    Segments segments;
    AddSegment("あ", kFree, segments);
    AddSegment("い", kFree, segments);
    AddSegment("う", kFree, segments);
    AddSegment("え", kFree, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 1;
    const int offset = 1;
    EXPECT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_TRUE(converter->ResizeSegment(&segments, convreq,
                                         start_segment_index, offset));
    ASSERT_EQ(segments.conversion_segments_size(), 3);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あ");
    EXPECT_EQ(segments.conversion_segment(1).key(), "いう");
    EXPECT_EQ(segments.conversion_segment(2).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFree);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(2).segment_type(), kFree);
  }

  {
    // Invalid offset: 0
    Segments segments;
    AddSegment("あい", kFree, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const int offset = 0;
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_FALSE(converter->ResizeSegment(&segments, convreq,
                                          start_segment_index, offset));
  }

  {
    // Invalid offset: more than the end of the segment
    Segments segments;
    AddSegment("あい", kFree, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const int offset = 1;
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_FALSE(converter->ResizeSegment(&segments, convreq,
                                          start_segment_index, offset));
  }

  {
    // Invalid offset: less than the start of the segment
    Segments segments;
    AddSegment("あい", kFree, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const int offset = -2;
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_FALSE(converter->ResizeSegment(&segments, convreq,
                                          start_segment_index, offset));
  }
}

TEST_F(ConverterTest, ResizeSegmentsWithArray) {
  constexpr Segment::SegmentType kFixedBoundary = Segment::FIXED_BOUNDARY;
  constexpr Segment::SegmentType kFree = Segment::FREE;

  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  {
    // Resize {"あいうえ"} to {"あいう", "え"}
    Segments segments;
    AddSegment("あいうえ", kFree, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const std::array<uint8_t, 2> size_array = {3, 1};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(converter->ResizeSegments(&segments, convreq,
                                          start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいうえ"} to {"あいう", "え"} with history segments.
    // Even if segments has histroy segments, arguments for ResizeSegment is not
    // changed.
    Segments segments;
    Segment &history0 = AddSegment("やゆよ", Segment::HISTORY, segments);
    PushBackCandidate("ヤユヨ", history0);
    Segment &history1 = AddSegment("わをん", Segment::HISTORY, segments);
    PushBackCandidate("ワヲン", history1);
    AddSegment("あいうえ", Segment::FREE, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const std::array<uint8_t, 2> size_array = {3, 1};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(converter->ResizeSegments(&segments, convreq,
                                          start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいうえ"} to {"あいう", "え"} where size_array contains 0
    // values.
    Segments segments;
    AddSegment("あいうえ", Segment::FREE, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const std::array<uint8_t, 8> size_array = {3, 1, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(converter->ResizeSegments(&segments, convreq,
                                          start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいうえおかきくけ"} to {"あい", "うえ", "お", "かき", "くけ"}
    Segments segments;
    AddSegment("あいうえおかきくけ", Segment::FREE, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const std::array<uint8_t, 5> size_array = {2, 2, 1, 2, 2};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(converter->ResizeSegments(&segments, convreq,
                                          start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 5);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あい");
    EXPECT_EQ(segments.conversion_segment(1).key(), "うえ");
    EXPECT_EQ(segments.conversion_segment(2).key(), "お");
    EXPECT_EQ(segments.conversion_segment(3).key(), "かき");
    EXPECT_EQ(segments.conversion_segment(4).key(), "くけ");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(2).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(3).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(4).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいう", "えお", "かき", "くけ"} to
    // {"あいうえ", "お", "かきくけ"}
    Segments segments;
    AddSegment("あいう", Segment::FREE, segments);
    AddSegment("えお", Segment::FREE, segments);
    AddSegment("かき", Segment::FREE, segments);
    AddSegment("くけ", Segment::FREE, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const std::array<uint8_t, 3> size_array = {4, 1, 4};
    EXPECT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_TRUE(converter->ResizeSegments(&segments, convreq,
                                          start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 3);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいうえ");
    EXPECT_EQ(segments.conversion_segment(1).key(), "お");
    EXPECT_EQ(segments.conversion_segment(2).key(), "かきくけ");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(2).segment_type(), kFixedBoundary);
  }

  {
    // Resize {"あいう", "えお", "かき", "くけ"} to
    // {"あいうえ", "お"} and keeping {"かき", "くけ"} as-is.
    Segments segments;
    AddSegment("あいう", Segment::FREE, segments);
    AddSegment("えお", Segment::FREE, segments);
    AddSegment("かき", Segment::FREE, segments);
    AddSegment("くけ", Segment::FREE, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const std::array<uint8_t, 2> size_array = {4, 1};
    EXPECT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_TRUE(converter->ResizeSegments(&segments, convreq,
                                          start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいうえ");
    EXPECT_EQ(segments.conversion_segment(1).key(), "お");
    EXPECT_EQ(segments.conversion_segment(2).key(), "かき");
    EXPECT_EQ(segments.conversion_segment(3).key(), "くけ");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFixedBoundary);
    EXPECT_EQ(segments.conversion_segment(2).segment_type(), kFree);
    EXPECT_EQ(segments.conversion_segment(3).segment_type(), kFree);
  }

  {
    // Resize {"あいうえ"} to {"あいう"} and keeping {"え"} as-is.
    Segments segments;
    AddSegment("あいうえ", Segment::FREE, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 0;
    const std::array<uint8_t, 1> size_array = {3};
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(converter->ResizeSegments(&segments, convreq,
                                          start_segment_index, size_array));
    ASSERT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "え");
    EXPECT_EQ(segments.conversion_segment(0).segment_type(), kFixedBoundary);
    // Non specified segment (i.e. "え") is FREE to keep the consistency
    // with ResizeSegment.
    EXPECT_EQ(segments.conversion_segment(1).segment_type(), kFree);
  }

  {
    // Resize {"あいう", "えお", "かき", "くけ"} to {"かきくけ"} while
    // {"あいう", "えお"} are free to be modified.
    Segments segments;
    AddSegment("あいう", Segment::FREE, segments);
    AddSegment("えお", Segment::FREE, segments);
    AddSegment("かき", Segment::FREE, segments);
    AddSegment("くけ", Segment::FREE, segments);
    const ConversionRequest convreq;
    const int start_segment_index = 2;
    const std::array<uint8_t, 1> size_array = {4};
    EXPECT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_TRUE(converter->ResizeSegments(&segments, convreq,
                                          start_segment_index, size_array));

    // Since {"あいう", "えお"} may be modified too, the segment index for
    // "かきくけ" may be different from 2.
    const size_t resized_size = segments.conversion_segments_size();
    const Segment &last_segment = segments.conversion_segment(resized_size - 1);
    EXPECT_EQ(last_segment.key(), "かきくけ");
    EXPECT_EQ(last_segment.segment_type(), kFixedBoundary);
  }
}

TEST_F(ConverterTest, ResizeSegmentsRequest) {
  {
    const size_t index = 0;
    const std::array<uint8_t, 8> sizes = {1, 2, 3, 0, 0, 0, 0, 0};
    std::unique_ptr<Converter> converter = CreateConverter(
        std::make_unique<ResizeSegmentsRewriter>(index, sizes), STUB_PREDICTOR);

    Segments segments;

    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("あいうえおかき").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));

    ASSERT_EQ(segments.conversion_segments_size(), 4);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あ");
    EXPECT_EQ(segments.conversion_segment(1).key(), "いう");
    EXPECT_EQ(segments.conversion_segment(2).key(), "えおか");
    EXPECT_EQ(segments.conversion_segment(3).key(), "き");
  }

  {
    const size_t index = 0;
    const std::array<uint8_t, 8> sizes = {3, 3, 0, 0, 0, 0, 0};
    std::unique_ptr<Converter> converter = CreateConverter(
        std::make_unique<ResizeSegmentsRewriter>(index, sizes), STUB_PREDICTOR);

    Segments segments;

    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("あいうえおかき").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));

    ASSERT_EQ(segments.conversion_segments_size(), 3);
    EXPECT_EQ(segments.conversion_segment(0).key(), "あいう");
    EXPECT_EQ(segments.conversion_segment(1).key(), "えおか");
    EXPECT_EQ(segments.conversion_segment(2).key(), "き");
  }

  {
    const size_t index = 1;
    const std::array<uint8_t, 8> sizes = {1, 2, 0, 0, 0, 0, 0};
    std::unique_ptr<Converter> converter = CreateConverter(
        std::make_unique<ResizeSegmentsRewriter>(index, sizes), STUB_PREDICTOR);

    Segments segments;

    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("あいうえおかき").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));

    // The total size of segments and the size of the first segment are not
    // specified.
    ASSERT_GE(segments.conversion_segments_size(), 3);
    ASSERT_LE(Util::CharsLen(segments.conversion_segment(0).key()), 4);

    EXPECT_EQ(Util::CharsLen(segments.conversion_segment(1).key()), 1);
    EXPECT_EQ(Util::CharsLen(segments.conversion_segment(2).key()), 2);
  }
}

TEST_F(ConverterTest, IntegrationWithCalculatorRewriter) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  {
    Segments segments;
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("1+1=").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "2");
  }
}

TEST_F(ConverterTest, IntegrationWithDateRewriter) {
  MockDictionary dictionary;
  // Since DateRewriter is not used in some build targets, the test needs to
  // explicitly add it to the converter.
  std::unique_ptr<Converter> converter = CreateConverter(
      std::make_unique<DateRewriter>(dictionary), STUB_PREDICTOR);

  {
    Segments segments;
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("へいせい30ねん").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(FindCandidateByValue("2018年", segments.conversion_segment(0)));
  }

  {
    Segments segments;
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("794ねん").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(
        FindCandidateByValue("延暦13年", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, IntegrationWithSymbolRewriter) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  {
    Segments segments;
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("ー>").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(FindCandidateByValue("→", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, IntegrationWithUnicodeRewriter) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  {
    Segments segments;
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("U+3042").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(FindCandidateByValue("あ", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, IntegrationWithSmallLetterRewriter) {
  std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();
  std::shared_ptr<const ConverterInterface> converter = engine->GetConverter();

  {
    Segments segments;
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetKey("^123").Build();
    ASSERT_TRUE(converter->StartConversion(convreq, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_TRUE(FindCandidateByValue("¹²³", segments.conversion_segment(0)));
  }
}
}  // namespace mozc
