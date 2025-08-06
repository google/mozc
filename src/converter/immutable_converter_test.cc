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

#include "converter/immutable_converter.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
#include "converter/lattice.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "converter/segments_matchers.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "engine/modules.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/test_peer.h"

namespace mozc {

class ImmutableConverterTestPeer : testing::TestPeer<ImmutableConverter> {
 public:
  explicit ImmutableConverterTestPeer(ImmutableConverter &converter)
      : testing::TestPeer<ImmutableConverter>(converter) {}

  // Make them public via peer class.
  PEER_METHOD(MakeLattice);
  PEER_METHOD(Viterbi);
  PEER_METHOD(InsertDummyCandidates);
};

namespace {

using converter::Attribute;
using converter::Candidate;
using dictionary::DictionaryInterface;
using ::testing::StrEq;

void SetCandidate(absl::string_view key, absl::string_view value,
                  Segment *segment) {
  segment->set_key(key);
  Candidate *candidate = segment->add_candidate();
#ifdef ABSL_USES_STD_STRING_VIEW
  candidate->key = key;
  candidate->value = value;
  candidate->content_key = key;
  candidate->content_value = value;
#else   // ABSL_USES_STD_STRING_VIEW
  candidate->key = std::string(key);
  candidate->value = std::string(value);
  candidate->content_key = std::string(key);
  candidate->content_value = std::string(value);
#endif  // ABSL_USES_STD_STRING_VIEW
}

int GetCandidateIndexByValue(absl::string_view value, const Segment &segment) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == value) {
      return i;
    }
  }
  return -1;  // not found
}

class MockDataAndImmutableConverter {
 public:
  // Initializes data and immutable converter with given dictionaries. If
  // nullptr is passed, the default mock dictionary is used. This class owns the
  // first argument dictionary but doesn't the second because the same
  // dictionary may be passed to the arguments.
  MockDataAndImmutableConverter() {
    modules_ =
        engine::Modules::Create(std::make_unique<testing::MockDataManager>())
            .value();
    immutable_converter_ = std::make_unique<ImmutableConverter>(*modules_);
    CHECK(immutable_converter_);
  }

  MockDataAndImmutableConverter(
      std::unique_ptr<DictionaryInterface> dictionary,
      std::unique_ptr<DictionaryInterface> suffix_dictionary) {
    modules_ = engine::ModulesPresetBuilder()
                   .PresetDictionary(std::move(dictionary))
                   .PresetSuffixDictionary(std::move(suffix_dictionary))
                   .Build(std::make_unique<testing::MockDataManager>())
                   .value();
    immutable_converter_ = std::make_unique<ImmutableConverter>(*modules_);
    CHECK(immutable_converter_);
  }

  ImmutableConverter *GetConverter() { return immutable_converter_.get(); }
  ImmutableConverterTestPeer GetConverterTestPeer() {
    return ImmutableConverterTestPeer(*immutable_converter_);
  }

 private:
  std::unique_ptr<engine::Modules> modules_;
  std::unique_ptr<ImmutableConverter> immutable_converter_;
};

}  // namespace

TEST(ImmutableConverterTest, KeepKeyForPrediction) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::PREDICTION,
                       .max_conversion_candidates_size = 10})
          .Build();
  Segment *segment = segments.add_segment();
  const std::string kRequestKey = "よろしくおねがいしま";
  segment->set_key(kRequestKey);
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(request,
                                                                    &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_GT(segments.segment(0).candidates_size(), 0);
  EXPECT_EQ(segments.segment(0).key(), kRequestKey);
}

TEST(ImmutableConverterTest, ResegmentTest) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::CONVERSION,
                       .max_conversion_candidates_size = 10})
          .Build();

  {
    segments.Clear();
    Segment *segment = segments.add_segment();
    const std::string kRequestKey = "1ねんせい";
    segment->set_key(kRequestKey);
    EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
        request, &segments));
    EXPECT_EQ(segments.segments_size(), 2);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "1");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "年生");
  }

  {
    segments.Clear();
    Segment *segment = segments.add_segment();
    const std::string kRequestKey = "ちゅう2";
    segment->set_key(kRequestKey);
    EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
        request, &segments));
    EXPECT_EQ(segments.segments_size(), 2);
    EXPECT_EQ(segments.segment(0).candidate(0).value, "中");
    EXPECT_EQ(segments.segment(1).candidate(0).value, "2");
  }
}

TEST(ImmutableConverterTest, DummyCandidatesCost) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segment segment;
  SetCandidate("てすと", "test", &segment);
  data_and_converter->GetConverterTestPeer().InsertDummyCandidates(&segment,
                                                                   10);
  EXPECT_GE(segment.candidates_size(), 3);
  EXPECT_LT(segment.candidate(0).wcost, segment.candidate(1).wcost);
  EXPECT_LT(segment.candidate(0).wcost, segment.candidate(2).wcost);
}

TEST(ImmutableConverterTest, DummyCandidatesInnerSegmentBoundary) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segment segment;
  SetCandidate("てすと", "test", &segment);
  Candidate *c = segment.mutable_candidate(0);
  c->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
      {{3, 2, 3, 2}, {6, 2, 6, 2}}, c->key, c->value);
  EXPECT_EQ(c->inner_segment_boundary.size(), 2);

  data_and_converter->GetConverterTestPeer().InsertDummyCandidates(&segment,
                                                                   10);
  ASSERT_GE(segment.candidates_size(), 3);
  for (size_t i = 1; i < 3; ++i) {
    EXPECT_TRUE(segment.candidate(i).inner_segment_boundary.empty());
    EXPECT_TRUE(segment.candidate(i).IsValid());
  }
}

namespace {
class KeyCheckDictionary : public DictionaryInterface {
 public:
  explicit KeyCheckDictionary(absl::string_view query)
      : target_query_(query), received_target_query_(false) {}
  ~KeyCheckDictionary() override = default;

  bool HasKey(absl::string_view key) const override { return false; }
  bool HasValue(absl::string_view value) const override { return false; }

  void LookupPredictive(absl::string_view key, const ConversionRequest &convreq,
                        Callback *callback) const override {
    if (key == target_query_) {
      received_target_query_ = true;
    }
  }

  void LookupPrefix(absl::string_view key, const ConversionRequest &convreq,
                    Callback *callback) const override {
    // No check
  }

  void LookupExact(absl::string_view key, const ConversionRequest &convreq,
                   Callback *callback) const override {
    // No check
  }

  void LookupReverse(absl::string_view str, const ConversionRequest &convreq,
                     Callback *callback) const override {
    // No check
  }

  bool received_target_query() const { return received_target_query_; }

  void clear_received_target_query() { received_target_query_ = false; }

 private:
  const std::string target_query_;
  mutable bool received_target_query_;
};
}  // namespace

TEST(ImmutableConverterTest, InnerSegmenBoundaryForPrediction) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  Segment *segment = segments.add_segment();
  const std::string kRequestKey = "わたしのなまえはなかのです";
  segment->set_key(kRequestKey);
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetOptions({.request_type = ConversionRequest::PREDICTION,
                       .max_conversion_candidates_size = 1})
          .Build();
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(request,
                                                                    &segments));
  ASSERT_EQ(1, segments.segments_size());
  ASSERT_EQ(1, segments.segment(0).candidates_size());

  // Result will be, "私の|名前は|中ノです" with mock dictionary.
  const Candidate &cand = segments.segment(0).candidate(0);
  EXPECT_TRUE(cand.IsValid());
  std::vector<absl::string_view> keys, values, content_keys, content_values;
  for (const auto &iter : cand.inner_segments()) {
    keys.push_back(iter.GetKey());
    values.push_back(iter.GetValue());
    content_keys.push_back(iter.GetContentKey());
    content_values.push_back(iter.GetContentValue());
  }
  ASSERT_EQ(keys.size(), 3);
  EXPECT_EQ(keys[0], "わたしの");
  EXPECT_EQ(keys[1], "なまえは");
  EXPECT_EQ(keys[2], "なかのです");

  ASSERT_EQ(values.size(), 3);
  EXPECT_EQ(values[0], "私の");
  EXPECT_EQ(values[1], "名前は");
  EXPECT_EQ(values[2], "中ノです");

  ASSERT_EQ(3, content_keys.size());
  EXPECT_EQ(content_keys[0], "わたし");
  EXPECT_EQ(content_keys[1], "なまえ");
  EXPECT_EQ(content_keys[2], "なかの");

  ASSERT_EQ(content_values.size(), 3);
  EXPECT_EQ(content_values[0], "私");
  EXPECT_EQ(content_values[1], "名前");
  EXPECT_EQ(content_values[2], "中ノ");
}

TEST(ImmutableConverterTest, NoInnerSegmenBoundaryForConversion) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  Segment *segment = segments.add_segment();
  const std::string kRequestKey = "わたしのなまえはなかのです";
  segment->set_key(kRequestKey);
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetRequestType(ConversionRequest::CONVERSION)
          .Build();
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(request,
                                                                    &segments));
  EXPECT_LE(1, segments.segments_size());
  EXPECT_LT(0, segments.segment(0).candidates_size());
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Candidate &cand = segments.segment(0).candidate(i);
    EXPECT_TRUE(cand.inner_segment_boundary.empty());
  }
}

TEST(ImmutableConverterTest, MakeLatticeKatakana) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  ImmutableConverterTestPeer converter =
      data_and_converter->GetConverterTestPeer();

  Segments segments;

  Segment *segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  segment->set_key("カタカナです");

  Lattice lattice;
  lattice.SetKey("カタカナです");
  const ConversionRequest request;
  converter.MakeLattice(request, &segments, &lattice);

  // If the first character of a node is `ALPHABET` or `KATAKANA`,
  // `AddCharacterTypeBasedNodes` should create a node of the character type.
  Node *node = lattice.begin_nodes(0);
  EXPECT_EQ(node->key, "カタカナ");
  EXPECT_EQ(node->value, "カタカナ");
}

TEST(ImmutableConverterTest, NotConnectedTest) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  ImmutableConverterTestPeer converter =
      data_and_converter->GetConverterTestPeer();

  Segments segments;

  Segment *segment = segments.add_segment();
  segment->set_segment_type(Segment::FIXED_BOUNDARY);
  segment->set_key("しょうめい");

  segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  segment->set_key("できる");

  Lattice lattice;
  lattice.SetKey("しょうめいできる");
  const ConversionRequest request;
  converter.MakeLattice(request, &segments, &lattice);

  converter.Viterbi(segments, &lattice);

  // Intentionally segmented position - 1
  const size_t pos = strlen("しょうめ");
  bool tested = false;
  for (Node *rnode = lattice.begin_nodes(pos); rnode != nullptr;
       rnode = rnode->bnext) {
    if (Util::CharsLen(rnode->key) <= 1) {
      continue;
    }
    // If len(rnode->value) > 1, that node should cross over the boundary
    EXPECT_TRUE(rnode->prev == nullptr);
    tested = true;
  }
  EXPECT_TRUE(tested);
}

TEST(ImmutableConverterTest, HistoryKeyLengthIsVeryLong) {
  // "あ..." (100 times)
  const std::string kA100 =
      "あああああああああああああああああああああああああ"
      "あああああああああああああああああああああああああ"
      "あああああああああああああああああああああああああ"
      "あああああああああああああああああああああああああ";

  // Set up history segments.
  Segments segments;
  for (int i = 0; i < 4; ++i) {
    Segment *segment = segments.add_segment();
    segment->set_key(kA100);
    segment->set_segment_type(Segment::HISTORY);
    Candidate *candidate = segment->add_candidate();
    candidate->key = kA100;
    candidate->value = kA100;
  }

  // Set up a conversion segment.
  Segment *segment = segments.add_segment();
  const std::string kRequestKey = "あ";
  segment->set_key(kRequestKey);

  // Verify that history segments are cleared due to its length limit and at
  // least one candidate is generated.
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  const ConversionRequest request =
      ConversionRequestBuilder()
          .SetRequestType(ConversionRequest::CONVERSION)
          .Build();
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(request,
                                                                    &segments));
  EXPECT_EQ(segments.history_segments_size(), 0);
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_GT(segments.segment(0).candidates_size(), 0);
  EXPECT_EQ(segments.segment(0).key(), kRequestKey);
}

namespace {
bool AutoPartialSuggestionTestHelper(const ConversionRequest &request) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  ConversionRequest::Options options = request.options();
  options.request_type = ConversionRequest::PREDICTION;
  options.max_conversion_candidates_size = 10;
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetConversionRequest(request)
          .SetOptions(std::move(options))
          .Build();
  Segment *segment = segments.add_segment();
  const std::string kRequestKey = "わたしのなまえはなかのです";
  segment->set_key(kRequestKey);
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
      conversion_request, &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_LT(0, segments.segment(0).candidates_size());
  bool includes_only_first = false;
  absl::string_view segment_key = segments.segment(0).key();
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Candidate &cand = segments.segment(0).candidate(i);
    if (cand.key.size() < segment_key.size() &&
        segment_key.starts_with(cand.key)) {
      includes_only_first = true;
      break;
    }
  }
  return includes_only_first;
}
}  // namespace

TEST(ImmutableConverterTest, EnableAutoPartialSuggestion) {
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetOptions({.create_partial_candidates = true})
          .Build();
  EXPECT_TRUE(conversion_request.create_partial_candidates());
  EXPECT_TRUE(AutoPartialSuggestionTestHelper(conversion_request));
}

TEST(ImmutableConverterTest, DisableAutoPartialSuggestion) {
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetOptions({.create_partial_candidates = false})
          .Build();
  EXPECT_FALSE(AutoPartialSuggestionTestHelper(conversion_request));
}

TEST(ImmutableConverterTest, AutoPartialSuggestionDefault) {
  const ConversionRequest conversion_request;
  EXPECT_FALSE(AutoPartialSuggestionTestHelper(conversion_request));
}

TEST(ImmutableConverterTest, FirstInnerSegment) {
  commands::Request request;
  request_test_util::FillMobileRequest(&request);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetRequest(request)
          .SetOptions({
              .request_type = ConversionRequest::PREDICTION,
              .max_conversion_candidates_size = 100,
              .create_partial_candidates = true,
          })
          .Build();

  auto data_and_converter = std::make_unique<MockDataAndImmutableConverter>();

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("くるまでこうどうした");
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
      conversion_request, &segments));

  constexpr auto KeyIs = [](const auto &key) {
    return Field(&Candidate::key, StrEq(key));
  };

  EXPECT_THAT(*segment, ContainsCandidate(KeyIs("くるまでこうどうした")));
  EXPECT_THAT(*segment, ContainsCandidate(KeyIs("くるまで")));
  EXPECT_THAT(*segment, ContainsCandidate(KeyIs("くる")));
}

TEST(ImmutableConverterTest, FirstInnerSegmentFiltering) {
  commands::Request request;
  request_test_util::FillMobileRequest(&request);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetRequest(request)
          .SetOptions({
              .request_type = ConversionRequest::PREDICTION,
              .max_conversion_candidates_size = 100,
              .create_partial_candidates = true,
          })
          .Build();

  auto data_and_converter = std::make_unique<MockDataAndImmutableConverter>();
  constexpr auto ValueIs = [](const auto &value) {
    return Field(&Candidate::value, StrEq(value));
  };

  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("したとき");
    EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
        conversion_request, &segments));

    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("した時")));
    // The same segment structure, but included by char coverage rule.
    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("したとき")));
  }
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("のとき");
    EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
        conversion_request, &segments));

    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("の時")));
    // The same segment structure, included by char coverage.
    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("のとき")));
  }
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("かえる");
    EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
        conversion_request, &segments));

    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("換える")));
    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("代える")));
    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("買える")));
    // Included by cost diff
    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("飼える")));
  }
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("くるまでこうどうした");
    EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
        conversion_request, &segments));

    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("車で行動した")));
    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("車で")));
    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("来るまで")));
    EXPECT_THAT(*segment, ContainsCandidate(ValueIs("くるまで")));
  }
}

// Confirm t13n (Hiragana to English) conversions twice work (b/427316871).
TEST(ImmutableConverterTest, T13nConversionTwice) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    segment->set_key("ぐうぐる");
  }
  const ConversionRequest request;
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(request,
                                                                    &segments));
  ASSERT_EQ(segments.segments_size(), 1);

  const int index =
      GetCandidateIndexByValue("Google", segments.conversion_segment(0));
  ASSERT_NE(index, -1);

  {  // Make the existing segment HISTORY
    Segment &segment = *segments.mutable_segment(0);
    segment.set_segment_type(Segment::HISTORY);
    segment.move_candidate(index, 0);
    if (index != 0) {
      segment.mutable_candidate(0)->attributes |= Attribute::RERANKED;
    }
  }

  {  // Add a new segment for t13n conversion again.
    Segment *segment = segments.add_segment();
    segment->set_key("ぐーぐる");
  }
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(request,
                                                                    &segments));
  ASSERT_EQ(segments.segments_size(), 2);
  ASSERT_EQ(segments.conversion_segments_size(), 1);

  ASSERT_NE(GetCandidateIndexByValue("Google", segments.conversion_segment(0)),
            -1);
}

}  // namespace mozc
