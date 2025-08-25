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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/candidate.h"
#include "converter/immutable_converter.h"
#include "converter/lattice.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "converter/segments_matchers.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/test_peer.h"

namespace mozc {

using ::mozc::converter::Candidate;

class ImmutableConverterTestPeer : testing::TestPeer<ImmutableConverter> {
 public:
  explicit ImmutableConverterTestPeer(ImmutableConverter& converter)
      : testing::TestPeer<ImmutableConverter>(converter) {}

  // Make them public via peer class.
  PEER_METHOD(IsSegmentEndNode);
  PEER_METHOD(MakeLattice);
  PEER_METHOD(Viterbi);
  PEER_METHOD(MakeGroup);
};

namespace {

class MockDataAndImmutableConverter {
 public:
  // Initializes data and immutable converter with given dictionaries.
  MockDataAndImmutableConverter() {
    modules_ =
        engine::Modules::Create(std::make_unique<testing::MockDataManager>())
            .value();
    immutable_converter_ = std::make_unique<ImmutableConverter>(*modules_);
  }

  ImmutableConverter* GetConverter() { return immutable_converter_.get(); }
  ImmutableConverterTestPeer GetConverterTestPeer() {
    return ImmutableConverterTestPeer(*immutable_converter_);
  }

  std::unique_ptr<NBestGenerator> CreateNBestGenerator(const Lattice& lattice) {
    return std::make_unique<NBestGenerator>(
        modules_->GetUserDictionary(), modules_->GetSegmenter(),
        modules_->GetConnector(), modules_->GetPosMatcher(), lattice,
        modules_->GetSuggestionFilter());
  }

 private:
  std::unique_ptr<const engine::Modules> modules_;
  std::unique_ptr<ImmutableConverter> immutable_converter_;
};

ConversionRequest ConvReq(ConversionRequest::RequestType request_type) {
  return ConversionRequestBuilder().SetRequestType(request_type).Build();
}

}  // namespace

class NBestGeneratorTest : public ::testing::Test {
 protected:
  const Node* GetEndNode(const ConversionRequest& request,
                         ImmutableConverterTestPeer& converter,
                         const Segments& segments, const Node& begin_node,
                         absl::Span<const uint16_t> group,
                         bool is_single_segment) {
    const Node* end_node = nullptr;
    for (const Node* node = begin_node.next; node->next != nullptr;
         node = node->next) {
      end_node = node->next;
      if (converter.IsSegmentEndNode(request, segments, node, group,
                                     is_single_segment)) {
        break;
      }
    }
    return end_node;
  }
};

TEST_F(NBestGeneratorTest, MultiSegmentConnectionTest) {
  auto data_and_converter = std::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterTestPeer converter =
      data_and_converter->GetConverterTestPeer();

  Segments segments;
  {
    Segment* segment = segments.add_segment();
    segment->set_segment_type(Segment::FIXED_BOUNDARY);
    segment->set_key("しんこう");

    segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key("する");
  }

  Lattice lattice;
  lattice.SetKey("しんこうする");
  const ConversionRequest request = ConvReq(ConversionRequest::CONVERSION);
  converter.MakeLattice(request, &segments, &lattice);

  const std::vector<uint16_t> group = converter.MakeGroup(segments);
  converter.Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(lattice);

  constexpr bool kSingleSegment = false;  // For 'normal' conversion
  const Node* begin_node = lattice.bos_node();
  const Node* end_node = GetEndNode(request, converter, segments, *begin_node,
                                    group, kSingleSegment);
  {
    nbest_generator->Reset(
        begin_node, end_node,
        {NBestGenerator::STRICT, NBestGenerator::CANDIDATE_MODE_NONE});
    Segment result_segment;
    nbest_generator->SetCandidates(request, "", 10, &result_segment);
    // The top result is treated exceptionally and has no boundary check
    // in NBestGenerator.
    // The best route is calculated in ImmutalbeConverter with boundary check.
    // So, the top result should be inserted, but other candidates will be cut
    // due to boundary check between "する".
    ASSERT_EQ(result_segment.candidates_size(), 1);
    EXPECT_EQ(result_segment.candidate(0).value, "進行");
  }

  {
    nbest_generator->Reset(
        begin_node, end_node,
        {NBestGenerator::ONLY_MID, NBestGenerator::CANDIDATE_MODE_NONE});
    Segment result_segment;
    nbest_generator->SetCandidates(request, "", 10, &result_segment);
    ASSERT_EQ(result_segment.candidates_size(), 3);
    EXPECT_EQ(result_segment.candidate(0).value, "進行");
    EXPECT_EQ(result_segment.candidate(1).value, "信仰");
    EXPECT_EQ(result_segment.candidate(2).value, "深耕");
  }
}

TEST_F(NBestGeneratorTest, SingleSegmentConnectionTest) {
  auto data_and_converter = std::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterTestPeer converter =
      data_and_converter->GetConverterTestPeer();

  Segments segments;
  std::string kText = "わたしのなまえはなかのです";
  {
    Segment* segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key(kText);
  }

  Lattice lattice;
  lattice.SetKey(kText);
  const ConversionRequest request = ConvReq(ConversionRequest::CONVERSION);
  converter.MakeLattice(request, &segments, &lattice);

  const std::vector<uint16_t> group = converter.MakeGroup(segments);
  converter.Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(lattice);

  constexpr bool kSingleSegment = true;  // For real time conversion
  const Node* begin_node = lattice.bos_node();
  const Node* end_node = GetEndNode(request, converter, segments, *begin_node,
                                    group, kSingleSegment);
  {
    nbest_generator->Reset(
        begin_node, end_node,
        {NBestGenerator::STRICT, NBestGenerator::CANDIDATE_MODE_NONE});
    Segment result_segment;
    nbest_generator->SetCandidates(request, "", 10, &result_segment);
    // Top result should be inserted, but other candidates will be cut
    // due to boundary check.
    ASSERT_EQ(result_segment.candidates_size(), 1);
    EXPECT_EQ(result_segment.candidate(0).value, "私の名前は中ノです");
  }
  {
    nbest_generator->Reset(
        begin_node, end_node,
        {NBestGenerator::ONLY_EDGE, NBestGenerator::FILL_INNER_SEGMENT_INFO});
    Segment result_segment;
    nbest_generator->SetCandidates(request, "", 10, &result_segment);
    // We can get several candidates.
    ASSERT_LT(1, result_segment.candidates_size());
    EXPECT_EQ(result_segment.candidate(0).value, "私の名前は中ノです");
  }
}

TEST_F(NBestGeneratorTest, InnerSegmentBoundary) {
  auto data_and_converter = std::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterTestPeer converter =
      data_and_converter->GetConverterTestPeer();

  Segments segments;
  const std::string kInput = "とうきょうかなごやにいきたい";
  {
    Segment* segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key(kInput);
  }

  Lattice lattice;
  lattice.SetKey(kInput);
  const ConversionRequest request = ConvReq(ConversionRequest::PREDICTION);
  converter.MakeLattice(request, &segments, &lattice);

  const std::vector<uint16_t> group = converter.MakeGroup(segments);
  converter.Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(lattice);

  constexpr bool kSingleSegment = true;  // For real time conversion
  const Node* begin_node = lattice.bos_node();
  const Node* end_node = GetEndNode(request, converter, segments, *begin_node,
                                    group, kSingleSegment);

  nbest_generator->Reset(
      begin_node, end_node,
      {NBestGenerator::ONLY_EDGE, NBestGenerator::FILL_INNER_SEGMENT_INFO});
  Segment result_segment;
  nbest_generator->SetCandidates(request, "", 10, &result_segment);
  ASSERT_LE(1, result_segment.candidates_size());

  const Candidate& top_cand = result_segment.candidate(0);
  EXPECT_EQ(top_cand.key, kInput);
  EXPECT_EQ(top_cand.value, "東京か名古屋に行きたい");

  std::vector<absl::string_view> keys, values, content_keys, content_values;
  for (const auto& iter : top_cand.inner_segments()) {
    keys.push_back(iter.GetKey());
    values.push_back(iter.GetValue());
    content_keys.push_back(iter.GetContentKey());
    content_values.push_back(iter.GetContentValue());
  }
  ASSERT_EQ(keys.size(), 3);
  ASSERT_EQ(values.size(), 3);
  ASSERT_EQ(content_keys.size(), 3);
  ASSERT_EQ(content_values.size(), 3);

  // Inner segment 0
  EXPECT_EQ(keys[0], "とうきょうか");
  EXPECT_EQ(values[0], "東京か");
  EXPECT_EQ(content_keys[0], "とうきょう");
  EXPECT_EQ(content_values[0], "東京");

  // Inner segment 1
  EXPECT_EQ(keys[1], "なごやに");
  EXPECT_EQ(values[1], "名古屋に");
  EXPECT_EQ(content_keys[1], "なごや");
  EXPECT_EQ(content_values[1], "名古屋");

  // Inner segment 2: In the original segment, "行きたい" has the form
  // "行き" (content word) + "たい" (functional).  However, since "行き" is
  // Yougen, our rule for inner segment boundary doesn't handle it as a content
  // value.  Thus, "行きたい" becomes the content value.
  EXPECT_EQ(keys[2], "いきたい");
  EXPECT_EQ(values[2], "行きたい");
  EXPECT_EQ(content_keys[2], "いきたい");
  EXPECT_EQ(content_values[2], "行きたい");
}

TEST_F(NBestGeneratorTest, NoPartialCandidateBetweenAlphabets) {
  auto data_and_converter = std::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterTestPeer converter =
      data_and_converter->GetConverterTestPeer();

  Segments segments;
  const std::string kInput = "AAA";
  {
    Segment* segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key(kInput);
  }

  Lattice lattice;
  lattice.SetKey(kInput);
  const ConversionRequest request = ConvReq(ConversionRequest::PREDICTION);
  converter.MakeLattice(request, &segments, &lattice);

  const std::vector<uint16_t> group = converter.MakeGroup(segments);
  converter.Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(lattice);

  constexpr bool kSingleSegment = true;  // For real time conversion
  const Node* begin_node = lattice.bos_node();
  const Node* end_node = GetEndNode(request, converter, segments, *begin_node,
                                    group, kSingleSegment);

  // Since the test dictionary contains "A", partial candidates "A" and "AA" can
  // be generated but they should be suppressed because they are split between
  // alphabets.
  const NBestGenerator::Options options = {
      .boundary_mode = NBestGenerator::ONLY_EDGE,
      .candidate_mode = NBestGenerator::BUILD_FROM_ONLY_FIRST_INNER_SEGMENT |
                        NBestGenerator::FILL_INNER_SEGMENT_INFO,
  };
  nbest_generator->Reset(begin_node, end_node, options);
  Segment result_segment;
  nbest_generator->SetCandidates(request, "", 10, &result_segment);
  EXPECT_THAT(result_segment, HasSingleCandidate(::testing::Field(
                                  "value", &Candidate::value, "AAA")));
}

TEST_F(NBestGeneratorTest, NoAlphabetsConnection2Nodes) {
  auto data_and_converter = std::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterTestPeer converter =
      data_and_converter->GetConverterTestPeer();

  Segments segments;
  std::string kText = "eupho";
  {
    Segment* segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key(kText);
  }

  Lattice lattice;
  lattice.SetKey(kText);
  const ConversionRequest request = ConvReq(ConversionRequest::CONVERSION);
  converter.MakeLattice(request, &segments, &lattice);

  const std::vector<uint16_t> group = converter.MakeGroup(segments);
  converter.Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(lattice);

  constexpr bool kSingleSegment = true;  // For real time conversion
  const Node* begin_node = lattice.bos_node();
  const Node* end_node = GetEndNode(request, converter, segments, *begin_node,
                                    group, kSingleSegment);
  nbest_generator->Reset(
      begin_node, end_node,
      {NBestGenerator::ONLY_EDGE, NBestGenerator::FILL_INNER_SEGMENT_INFO});
  Segment result_segment;
  nbest_generator->SetCandidates(request, "", 10, &result_segment);
  // The test dictionary contains key value pairs (eu, EU) and (pho, pho), but
  // "EUpho" should not be generated as it is a concatenation of two alphabet
  // words. The only expected candidate is (eupho, eupho).
  EXPECT_THAT(result_segment, HasSingleCandidate(::testing::Field(
                                  "value", &Candidate::value, "eupho")));
}

TEST_F(NBestGeneratorTest, NoAlphabetsConnection3Nodes) {
  auto data_and_converter = std::make_unique<MockDataAndImmutableConverter>();
  ImmutableConverterTestPeer converter =
      data_and_converter->GetConverterTestPeer();

  Segments segments;
  std::string kText = "euphoとうきょう";
  {
    Segment* segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key(kText);
  }

  Lattice lattice;
  lattice.SetKey(kText);
  const ConversionRequest request = ConvReq(ConversionRequest::CONVERSION);
  converter.MakeLattice(request, &segments, &lattice);

  const std::vector<uint16_t> group = converter.MakeGroup(segments);
  converter.Viterbi(segments, &lattice);

  std::unique_ptr<NBestGenerator> nbest_generator =
      data_and_converter->CreateNBestGenerator(lattice);

  constexpr bool kSingleSegment = true;  // For real time conversion
  const Node* begin_node = lattice.bos_node();
  const Node* end_node = GetEndNode(request, converter, segments, *begin_node,
                                    group, kSingleSegment);
  nbest_generator->Reset(
      begin_node, end_node,
      {NBestGenerator::ONLY_EDGE, NBestGenerator::FILL_INNER_SEGMENT_INFO});
  Segment result_segment;
  nbest_generator->SetCandidates(request, "", 10, &result_segment);
  // Tne top candidate consists of two elements, "eupho" and "東京". Such
  // connection from English word to a normal word is possible.
  ASSERT_GE(result_segment.candidates_size(), 1);
  EXPECT_EQ(result_segment.candidate(0).value, "eupho東京");
  EXPECT_EQ(result_segment.candidate(0).inner_segment_boundary.size(), 2);
  // However, we should not concatenate "EU", "pho", and "東京".
  for (size_t i = 0; i < result_segment.candidates_size(); ++i) {
    EXPECT_NE(result_segment.candidate(i).value, "EUpho東京");
  }
}

}  // namespace mozc
