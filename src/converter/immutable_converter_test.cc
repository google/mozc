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

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "converter/connector.h"
#include "converter/lattice.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary_stub.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using dictionary::DictionaryImpl;
using dictionary::DictionaryInterface;
using dictionary::PosGroup;
using dictionary::SuffixDictionary;
using dictionary::SuppressionDictionary;
using dictionary::SystemDictionary;
using dictionary::UserDictionaryStub;
using dictionary::ValueDictionary;

void SetCandidate(absl::string_view key, absl::string_view value,
                  Segment *segment) {
  segment->set_key(key);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
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

class MockDataAndImmutableConverter {
 public:
  // Initializes data and immutable converter with given dictionaries. If
  // nullptr is passed, the default mock dictionary is used. This class owns the
  // first argument dictionary but doesn't the second because the same
  // dictionary may be passed to the arguments.
  explicit MockDataAndImmutableConverter(
      const DictionaryInterface *dictionary = nullptr,
      const DictionaryInterface *suffix_dictionary = nullptr) {
    data_manager_ = std::make_unique<testing::MockDataManager>();

    pos_matcher_.Set(data_manager_->GetPosMatcherData());

    suppression_dictionary_ = std::make_unique<SuppressionDictionary>();
    CHECK(suppression_dictionary_.get());

    if (dictionary) {
      dictionary_.reset(dictionary);
    } else {
      const char *dictionary_data = nullptr;
      int dictionary_size = 0;
      data_manager_->GetSystemDictionaryData(&dictionary_data,
                                             &dictionary_size);
      std::unique_ptr<SystemDictionary> sysdic =
          SystemDictionary::Builder(dictionary_data, dictionary_size)
              .Build()
              .value();
      auto value_dic = std::make_unique<ValueDictionary>(pos_matcher_,
                                                         &sysdic->value_trie());
      dictionary_ = std::make_unique<DictionaryImpl>(
          std::move(sysdic), std::move(value_dic), &user_dictionary_stub_,
          suppression_dictionary_.get(), &pos_matcher_);
    }
    CHECK(dictionary_.get());

    if (!suffix_dictionary) {
      absl::string_view suffix_key_array_data, suffix_value_array_data;
      const uint32_t *token_array;
      data_manager_->GetSuffixDictionaryData(
          &suffix_key_array_data, &suffix_value_array_data, &token_array);
      suffix_dictionary_ = std::make_unique<SuffixDictionary>(
          suffix_key_array_data, suffix_value_array_data, token_array);
      suffix_dictionary = suffix_dictionary_.get();
    }
    CHECK(suffix_dictionary);

    connector_ = Connector::CreateFromDataManager(*data_manager_).value();

    segmenter_ = Segmenter::CreateFromDataManager(*data_manager_);
    CHECK(segmenter_.get());

    pos_group_ = std::make_unique<PosGroup>(data_manager_->GetPosGroupData());
    CHECK(pos_group_.get());

    {
      const char *data = nullptr;
      size_t size = 0;
      data_manager_->GetSuggestionFilterData(&data, &size);
      suggestion_filter_ = std::make_unique<SuggestionFilter>(data, size);
    }

    immutable_converter_ = std::make_unique<ImmutableConverterImpl>(
        dictionary_.get(), suffix_dictionary, suppression_dictionary_.get(),
        connector_, segmenter_.get(), &pos_matcher_, pos_group_.get(),
        suggestion_filter_.get());
    CHECK(immutable_converter_.get());
  }

  ImmutableConverterImpl *GetConverter() { return immutable_converter_.get(); }

 private:
  std::unique_ptr<const DataManagerInterface> data_manager_;
  std::unique_ptr<const SuppressionDictionary> suppression_dictionary_;
  Connector connector_;
  std::unique_ptr<const Segmenter> segmenter_;
  std::unique_ptr<const DictionaryInterface> suffix_dictionary_;
  std::unique_ptr<const DictionaryInterface> dictionary_;
  std::unique_ptr<const PosGroup> pos_group_;
  std::unique_ptr<const SuggestionFilter> suggestion_filter_;
  std::unique_ptr<ImmutableConverterImpl> immutable_converter_;
  UserDictionaryStub user_dictionary_stub_;
  dictionary::PosMatcher pos_matcher_;
};

}  // namespace

TEST(ImmutableConverterTest, KeepKeyForPrediction) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  ConversionRequest request;
  request.set_request_type(ConversionRequest::PREDICTION);
  request.set_max_conversion_candidates_size(10);
  Segment *segment = segments.add_segment();
  const std::string kRequestKey = "よろしくおねがいしま";
  segment->set_key(kRequestKey);
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(request,
                                                                    &segments));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_GT(segments.segment(0).candidates_size(), 0);
  EXPECT_EQ(segments.segment(0).key(), kRequestKey);
}

TEST(ImmutableConverterTest, DummyCandidatesCost) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segment segment;
  SetCandidate("てすと", "test", &segment);
  data_and_converter->GetConverter()->InsertDummyCandidates(&segment, 10);
  EXPECT_GE(segment.candidates_size(), 3);
  EXPECT_LT(segment.candidate(0).wcost, segment.candidate(1).wcost);
  EXPECT_LT(segment.candidate(0).wcost, segment.candidate(2).wcost);
}

TEST(ImmutableConverterTest, DummyCandidatesInnerSegmentBoundary) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segment segment;
  SetCandidate("てすと", "test", &segment);
  Segment::Candidate *c = segment.mutable_candidate(0);
  c->PushBackInnerSegmentBoundary(3, 2, 3, 2);
  c->PushBackInnerSegmentBoundary(6, 2, 6, 2);
  EXPECT_TRUE(c->IsValid());

  data_and_converter->GetConverter()->InsertDummyCandidates(&segment, 10);
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

TEST(ImmutableConverterTest, PredictiveNodesOnlyForConversionKey) {
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    segment->set_key("いいんじゃな");
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->key = "いいんじゃな";
    candidate->value = "いいんじゃな";

    segment = segments.add_segment();
    segment->set_key("いか");

    EXPECT_EQ(segments.history_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segments_size(), 1);
  }

  Lattice lattice;
  lattice.SetKey("いいんじゃないか");

  KeyCheckDictionary *dictionary = new KeyCheckDictionary("ないか");
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter(dictionary, dictionary));
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();
  const ConversionRequest request;
  converter->MakeLatticeNodesForPredictiveNodes(segments, request, &lattice);
  EXPECT_FALSE(dictionary->received_target_query());
}

TEST(ImmutableConverterTest, AddPredictiveNodes) {
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    segment->set_key("よろしくおねがいしま");

    EXPECT_EQ(segments.conversion_segments_size(), 1);
  }

  Lattice lattice;
  lattice.SetKey("よろしくおねがいしま");

  KeyCheckDictionary *dictionary = new KeyCheckDictionary("しま");
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter(dictionary, dictionary));
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();

  {
    ConversionRequest request;
    request.set_request_type(ConversionRequest::CONVERSION);
    converter->MakeLatticeNodesForPredictiveNodes(segments, request, &lattice);
    EXPECT_TRUE(dictionary->received_target_query());
  }
}

TEST(ImmutableConverterTest, InnerSegmenBoundaryForPrediction) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  Segment *segment = segments.add_segment();
  const std::string kRequestKey = "わたしのなまえはなかのです";
  segment->set_key(kRequestKey);
  ConversionRequest request;
  request.set_request_type(ConversionRequest::PREDICTION);
  request.set_max_conversion_candidates_size(1);
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(request,
                                                                    &segments));
  ASSERT_EQ(1, segments.segments_size());
  ASSERT_EQ(1, segments.segment(0).candidates_size());

  // Result will be, "私の|名前は|中ノです" with mock dictionary.
  const Segment::Candidate &cand = segments.segment(0).candidate(0);
  EXPECT_TRUE(cand.IsValid());
  std::vector<absl::string_view> keys, values, content_keys, content_values;
  for (Segment::Candidate::InnerSegmentIterator iter(&cand); !iter.Done();
       iter.Next()) {
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
  EXPECT_TRUE(data_and_converter->GetConverter()->Convert(&segments));
  EXPECT_LE(1, segments.segments_size());
  EXPECT_LT(0, segments.segment(0).candidates_size());
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &cand = segments.segment(0).candidate(i);
    EXPECT_TRUE(cand.inner_segment_boundary.empty());
  }
}

TEST(ImmutableConverterTest, NotConnectedTest) {
  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();

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
  converter->MakeLattice(request, &segments, &lattice);

  std::vector<uint16_t> group;
  converter->MakeGroup(segments, &group);
  converter->Viterbi(segments, &lattice);

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
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
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
  EXPECT_TRUE(data_and_converter->GetConverter()->Convert(&segments));
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
  ConversionRequest conversion_request = request;
  conversion_request.set_request_type(ConversionRequest::PREDICTION);
  conversion_request.set_max_conversion_candidates_size(10);
  Segment *segment = segments.add_segment();
  const std::string kRequestKey = "わたしのなまえはなかのです";
  segment->set_key(kRequestKey);
  EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
      conversion_request, &segments));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_LT(0, segments.segment(0).candidates_size());
  bool includes_only_first = false;
  const std::string &segment_key = segments.segment(0).key();
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &cand = segments.segment(0).candidate(i);
    if (cand.key.size() < segment_key.size() &&
        absl::StartsWith(segment_key, cand.key)) {
      includes_only_first = true;
      break;
    }
  }
  return includes_only_first;
}
}  // namespace

TEST(ImmutableConverterTest, EnableAutoPartialSuggestion) {
  const commands::Request request;
  ConversionRequest conversion_request;
  conversion_request.set_request(&request);
  conversion_request.set_create_partial_candidates(true);

  EXPECT_TRUE(AutoPartialSuggestionTestHelper(conversion_request));
}

TEST(ImmutableConverterTest, DisableAutoPartialSuggestion) {
  const commands::Request request;
  ConversionRequest conversion_request;
  conversion_request.set_request(&request);
  conversion_request.set_create_partial_candidates(false);

  EXPECT_FALSE(AutoPartialSuggestionTestHelper(conversion_request));
}

TEST(ImmutableConverterTest, AutoPartialSuggestionDefault) {
  const commands::Request request;
  ConversionRequest conversion_request;
  conversion_request.set_request(&request);

  EXPECT_FALSE(AutoPartialSuggestionTestHelper(conversion_request));
}

TEST(ImmutableConverterTest, AutoPartialSuggestionForSingleSegment) {
  const commands::Request request;
  ConversionRequest conversion_request;
  conversion_request.set_request_type(ConversionRequest::PREDICTION);
  conversion_request.set_request(&request);
  conversion_request.set_create_partial_candidates(true);
  conversion_request.set_max_conversion_candidates_size(10);

  std::unique_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  const std::string kRequestKeys[] = {
      "たかまち",
      "なのは",
      "まほうしょうじょ",
  };
  for (size_t testcase = 0; testcase < std::size(kRequestKeys); ++testcase) {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kRequestKeys[testcase]);
    EXPECT_TRUE(data_and_converter->GetConverter()->ConvertForRequest(
        conversion_request, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_LT(0, segments.segment(0).candidates_size());
    const std::string &segment_key = segments.segment(0).key();
    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      const Segment::Candidate &cand = segments.segment(0).candidate(i);
      if (cand.attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED) {
        EXPECT_LT(cand.key.size(), segment_key.size()) << cand.DebugString();
      } else {
        EXPECT_GE(cand.key.size(), segment_key.size()) << cand.DebugString();
      }
    }
  }
}

}  // namespace mozc
