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

#include "converter/nbest_generator.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "config/config_handler.h"
#include "converter/connector.h"
#include "converter/immutable_converter.h"
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
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using dictionary::DictionaryImpl;
using dictionary::DictionaryInterface;
using dictionary::PosGroup;
using dictionary::PosMatcher;
using dictionary::SuffixDictionary;
using dictionary::SuppressionDictionary;
using dictionary::SystemDictionary;
using dictionary::UserDictionaryStub;
using dictionary::ValueDictionary;

class MockDataAndImmutableConverter {
 public:
  // Initializes data and immutable converter with given dictionaries.
  MockDataAndImmutableConverter() {
    data_manager_ = absl::make_unique<testing::MockDataManager>();

    pos_matcher_.Set(data_manager_->GetPosMatcherData());

    suppression_dictionary_ = absl::make_unique<SuppressionDictionary>();
    CHECK(suppression_dictionary_);

    const char *dictionary_data = nullptr;
    int dictionary_size = 0;
    data_manager_->GetSystemDictionaryData(&dictionary_data, &dictionary_size);
    std::unique_ptr<SystemDictionary> sysdic =
        SystemDictionary::Builder(dictionary_data, dictionary_size)
            .Build()
            .value();
    auto value_dic =
        absl::make_unique<ValueDictionary>(pos_matcher_, &sysdic->value_trie());
    dictionary_ = absl::make_unique<DictionaryImpl>(
        std::move(sysdic), std::move(value_dic), &user_dictionary_stub_,
        suppression_dictionary_.get(), &pos_matcher_);
    CHECK(dictionary_);

    absl::string_view suffix_key_array_data, suffix_value_array_data;
    const uint32_t *token_array = nullptr;
    data_manager_->GetSuffixDictionaryData(
        &suffix_key_array_data, &suffix_value_array_data, &token_array);
    suffix_dictionary_ = absl::make_unique<SuffixDictionary>(
        suffix_key_array_data, suffix_value_array_data, token_array);
    CHECK(suffix_dictionary_);

    connector_ = Connector::CreateFromDataManager(*data_manager_).value();

    segmenter_.reset(Segmenter::CreateFromDataManager(*data_manager_));
    CHECK(segmenter_);

    pos_group_ = absl::make_unique<PosGroup>(data_manager_->GetPosGroupData());
    CHECK(pos_group_);

    {
      const char *data = nullptr;
      size_t size = 0;
      data_manager_->GetSuggestionFilterData(&data, &size);
      suggestion_filter_ = absl::make_unique<SuggestionFilter>(data, size);
    }

    immutable_converter_ = absl::make_unique<ImmutableConverterImpl>(
        dictionary_.get(), suffix_dictionary_.get(),
        suppression_dictionary_.get(), connector_.get(), segmenter_.get(),
        &pos_matcher_, pos_group_.get(), suggestion_filter_.get());
    CHECK(immutable_converter_);
  }

  ImmutableConverterImpl *GetConverter() { return immutable_converter_.get(); }

  std::unique_ptr<NBestGenerator> CreateNBestGenerator(const Lattice *lattice) {
    return absl::make_unique<NBestGenerator>(
        suppression_dictionary_.get(), segmenter_.get(), connector_.get(),
        &pos_matcher_, lattice, suggestion_filter_.get(), true);
  }

 private:
  std::unique_ptr<const DataManagerInterface> data_manager_;
  std::unique_ptr<const SuppressionDictionary> suppression_dictionary_;
  std::unique_ptr<const Connector> connector_;
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

class NBestGeneratorTest : public ::testing::Test {
 protected:
  void GatherCandidates(size_t size, Segments::RequestType request_type,
                        NBestGenerator *nbest, Segment *segment) const {
    while (segment->candidates_size() < size) {
      Segment::Candidate *candidate = segment->push_back_candidate();
      candidate->Init();

      if (!nbest->Next(request_, segment->key(), candidate, request_type)) {
        segment->pop_back_candidate();
        break;
      }
    }
  }

  const Node *GetEndNode(const ImmutableConverterImpl &converter,
                         const Segments &segments, const Node &begin_node,
                         const std::vector<uint16_t> &group,
                         bool is_single_segment) {
    const Node *end_node = nullptr;
    for (Node *node = begin_node.next; node->next != nullptr;
         node = node->next) {
      end_node = node->next;
      if (converter.IsSegmentEndNode(segments, node, group,
                                     is_single_segment)) {
        break;
      }
    }
    return end_node;
  }

  const ConversionRequest request_;
};

TEST_F(NBestGeneratorTest, MultiSegmentConnectionTest) {
  auto data_and_converter = absl::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();

  Segments segments;
  segments.set_request_type(Segments::CONVERSION);
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::FIXED_BOUNDARY);
    segment->set_key("しんこう");

    segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key("する");
  }

  Lattice lattice;
  lattice.SetKey("しんこうする");
  const ConversionRequest request;
  converter->MakeLattice(request, &segments, &lattice);

  std::vector<uint16_t> group;
  converter->MakeGroup(segments, &group);
  converter->Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(&lattice);

  constexpr bool kSingleSegment = false;  // For 'normal' conversion
  const Node *begin_node = lattice.bos_nodes();
  const Node *end_node =
      GetEndNode(*converter, segments, *begin_node, group, kSingleSegment);

  {
    nbest_generator->Reset(begin_node, end_node, NBestGenerator::STRICT);
    Segment result_segment;
    GatherCandidates(10, Segments::CONVERSION, nbest_generator.get(),
                     &result_segment);
    // The top result is treated exceptionally and has no boundary check
    // in NBestGenerator.
    // The best route is calculated in ImmutalbeConverter with boundary check.
    // So, the top result should be inserted, but other candidates will be cut
    // due to boundary check between "する".
    ASSERT_EQ(1, result_segment.candidates_size());
    EXPECT_EQ("進行", result_segment.candidate(0).value);
  }

  {
    nbest_generator->Reset(begin_node, end_node, NBestGenerator::ONLY_MID);
    Segment result_segment;
    GatherCandidates(10, Segments::CONVERSION, nbest_generator.get(),
                     &result_segment);
    ASSERT_EQ(3, result_segment.candidates_size());
    EXPECT_EQ("進行", result_segment.candidate(0).value);
    EXPECT_EQ("信仰", result_segment.candidate(1).value);
    EXPECT_EQ("深耕", result_segment.candidate(2).value);
  }
}

TEST_F(NBestGeneratorTest, SingleSegmentConnectionTest) {
  auto data_and_converter = absl::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();

  Segments segments;
  segments.set_request_type(Segments::CONVERSION);
  std::string kText = "わたしのなまえはなかのです";
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key(kText);
  }

  Lattice lattice;
  lattice.SetKey(kText);
  const ConversionRequest request;
  converter->MakeLattice(request, &segments, &lattice);

  std::vector<uint16_t> group;
  converter->MakeGroup(segments, &group);
  converter->Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(&lattice);

  constexpr bool kSingleSegment = true;  // For realtime conversion
  const Node *begin_node = lattice.bos_nodes();
  const Node *end_node =
      GetEndNode(*converter, segments, *begin_node, group, kSingleSegment);

  {
    nbest_generator->Reset(begin_node, end_node, NBestGenerator::STRICT);
    Segment result_segment;
    GatherCandidates(10, Segments::CONVERSION, nbest_generator.get(),
                     &result_segment);
    // Top result should be inserted, but other candidates will be cut
    // due to boundary check.
    ASSERT_EQ(1, result_segment.candidates_size());
    EXPECT_EQ("私の名前は中ノです", result_segment.candidate(0).value);
  }
  {
    nbest_generator->Reset(begin_node, end_node, NBestGenerator::ONLY_EDGE);
    Segment result_segment;
    GatherCandidates(10, Segments::CONVERSION, nbest_generator.get(),
                     &result_segment);
    // We can get several candidates.
    ASSERT_LT(1, result_segment.candidates_size());
    EXPECT_EQ("私の名前は中ノです", result_segment.candidate(0).value);
  }
}

TEST_F(NBestGeneratorTest, InnerSegmentBoundary) {
  auto data_and_converter = absl::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  const std::string kInput = "とうきょうかなごやにいきたい";
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key(kInput);
  }

  Lattice lattice;
  lattice.SetKey(kInput);
  const ConversionRequest request;
  converter->MakeLattice(request, &segments, &lattice);

  std::vector<uint16_t> group;
  converter->MakeGroup(segments, &group);
  converter->Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(&lattice);

  constexpr bool kSingleSegment = true;  // For realtime conversion
  const Node *begin_node = lattice.bos_nodes();
  const Node *end_node =
      GetEndNode(*converter, segments, *begin_node, group, kSingleSegment);

  nbest_generator->Reset(begin_node, end_node, NBestGenerator::ONLY_EDGE);
  Segment result_segment;
  GatherCandidates(10, Segments::PREDICTION, nbest_generator.get(),
                   &result_segment);
  ASSERT_LE(1, result_segment.candidates_size());

  const Segment::Candidate &top_cand = result_segment.candidate(0);
  EXPECT_EQ(kInput, top_cand.key);
  EXPECT_EQ("東京か名古屋に行きたい", top_cand.value);

  std::vector<absl::string_view> keys, values, content_keys, content_values;
  for (Segment::Candidate::InnerSegmentIterator iter(&top_cand); !iter.Done();
       iter.Next()) {
    keys.push_back(iter.GetKey());
    values.push_back(iter.GetValue());
    content_keys.push_back(iter.GetContentKey());
    content_values.push_back(iter.GetContentValue());
  }
  ASSERT_EQ(3, keys.size());
  ASSERT_EQ(3, values.size());
  ASSERT_EQ(3, content_keys.size());
  ASSERT_EQ(3, content_values.size());

  // Inner segment 0
  EXPECT_EQ("とうきょうか", keys[0]);
  EXPECT_EQ("東京か", values[0]);
  EXPECT_EQ("とうきょう", content_keys[0]);
  EXPECT_EQ("東京", content_values[0]);

  // Inner segment 1
  EXPECT_EQ("なごやに", keys[1]);
  EXPECT_EQ("名古屋に", values[1]);
  EXPECT_EQ("なごや", content_keys[1]);
  EXPECT_EQ("名古屋", content_values[1]);

  // Inner segment 2: In the original segment, "行きたい" has the form
  // "行き" (content word) + "たい" (functional).  However, since "行き" is
  // Yougen, our rule for inner segment boundary doesn't handle it as a content
  // value.  Thus, "行きたい" becomes the content value.
  EXPECT_EQ("いきたい", keys[2]);
  EXPECT_EQ("行きたい", values[2]);
  EXPECT_EQ("いきたい", content_keys[2]);
  EXPECT_EQ("行きたい", content_values[2]);
}

}  // namespace mozc
