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

#include "converter/nbest_generator.h"

#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/connector_base.h"
#include "converter/connector_interface.h"
#include "converter/conversion_request.h"
#include "converter/segmenter_base.h"
#include "converter/segmenter_interface.h"
#include "converter/segments.h"
#include "converter/immutable_converter.h"
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

#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

struct SuffixToken;

namespace {

using mozc::dictionary::DictionaryImpl;
using mozc::dictionary::SystemDictionary;
using mozc::dictionary::ValueDictionary;

class MockDataAndImmutableConverter {
 public:
  // Initializes data and immutable converter with given dictionaries.
  MockDataAndImmutableConverter() {
    data_manager_.reset(new testing::MockDataManager);

    const POSMatcher *pos_matcher = data_manager_->GetPOSMatcher();
    CHECK(pos_matcher);

    suppression_dictionary_.reset(new SuppressionDictionary);
    CHECK(suppression_dictionary_.get());

    const char *dictionary_data = NULL;
    int dictionary_size = 0;
    data_manager_->GetSystemDictionaryData(&dictionary_data,
                                           &dictionary_size);
    dictionary_.reset(new DictionaryImpl(
        SystemDictionary::CreateSystemDictionaryFromImage(
            dictionary_data, dictionary_size),
        ValueDictionary::CreateValueDictionaryFromImage(
            *pos_matcher, dictionary_data, dictionary_size),
        &user_dictionary_stub_,
        suppression_dictionary_.get(),
        pos_matcher));
    CHECK(dictionary_.get());

    const SuffixToken *tokens = NULL;
    size_t tokens_size = 0;
    data_manager_->GetSuffixDictionaryData(&tokens, &tokens_size);
    suffix_dictionary_.reset(new SuffixDictionary(tokens, tokens_size));
    CHECK(suffix_dictionary_.get());

    connector_.reset(ConnectorBase::CreateFromDataManager(*data_manager_));
    CHECK(connector_.get());

    segmenter_.reset(SegmenterBase::CreateFromDataManager(*data_manager_));
    CHECK(segmenter_.get());

    pos_group_.reset(new PosGroup(data_manager_->GetPosGroupData()));
    CHECK(pos_group_.get());

    {
      const char *data = NULL;
      size_t size = 0;
      data_manager_->GetSuggestionFilterData(&data, &size);
      suggestion_filter_.reset(new SuggestionFilter(data, size));
    }

    immutable_converter_.reset(new ImmutableConverterImpl(
        dictionary_.get(),
        suffix_dictionary_.get(),
        suppression_dictionary_.get(),
        connector_.get(),
        segmenter_.get(),
        pos_matcher,
        pos_group_.get(),
        suggestion_filter_.get()));
    CHECK(immutable_converter_.get());
  }

  ImmutableConverterImpl *GetConverter() {
    return immutable_converter_.get();
  }

  NBestGenerator *CreateNBestGenerator(const Lattice *lattice) {
    return new NBestGenerator(suppression_dictionary_.get(),
                              segmenter_.get(),
                              connector_.get(),
                              data_manager_->GetPOSMatcher(),
                              lattice,
                              suggestion_filter_.get());
  }

 private:
  scoped_ptr<const DataManagerInterface> data_manager_;
  scoped_ptr<const SuppressionDictionary> suppression_dictionary_;
  scoped_ptr<const ConnectorInterface> connector_;
  scoped_ptr<const SegmenterInterface> segmenter_;
  scoped_ptr<const DictionaryInterface> suffix_dictionary_;
  scoped_ptr<const DictionaryInterface> dictionary_;
  scoped_ptr<const PosGroup> pos_group_;
  scoped_ptr<const SuggestionFilter> suggestion_filter_;
  scoped_ptr<ImmutableConverterImpl> immutable_converter_;
  UserDictionaryStub user_dictionary_stub_;
};

}  // namespace

class NBestGeneratorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
  }

  void GatherCandidates(
      size_t size, Segments::RequestType request_type,
      NBestGenerator *nbest, Segment *segment) const {
    while (segment->candidates_size() < size) {
      Segment::Candidate *candidate = segment->push_back_candidate();
      candidate->Init();

      if (!nbest->Next(segment->key(), candidate, request_type)) {
        segment->pop_back_candidate();
        break;
      }
    }
  }

  const Node *GetEndNode(const ImmutableConverterImpl &converter,
                         const Segments &segments, const Node &begin_node,
                         const vector<uint16> &group, bool is_single_segment) {
    const Node *end_node = NULL;
    for (Node *node = begin_node.next; node->next != NULL; node = node->next) {
      end_node = node->next;
      if (converter.IsSegmentEndNode(
              segments, node, group, is_single_segment)) {
        break;
      }
    }
    return end_node;
  }

 private:
  config::Config default_config_;
};

TEST_F(NBestGeneratorTest, MultiSegmentConnectionTest) {
  scoped_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();

  Segments segments;
  segments.set_request_type(Segments::CONVERSION);
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::FIXED_BOUNDARY);
    // "しんこう"
    segment->set_key("\xe3\x81\x97\xe3\x82\x93\xe3\x81\x93\xe3\x81\x86");

    segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    // "する"
    segment->set_key("\xe3\x81\x99\xe3\x82\x8b");
  }

  Lattice lattice;
  // "しんこうする"
  lattice.SetKey("\xe3\x81\x97\xe3\x82\x93\xe3\x81\x93"
                 "\xe3\x81\x86\xe3\x81\x99\xe3\x82\x8b");
  const ConversionRequest request;
  converter->MakeLattice(request, &segments, &lattice);

  vector<uint16> group;
  converter->MakeGroup(segments, &group);
  converter->Viterbi(segments, &lattice);

  scoped_ptr<NBestGenerator> nbest_generator(
      data_and_converter->CreateNBestGenerator(&lattice));

  const bool kSingleSegment = false;  // For 'normal' conversion
  const Node *begin_node = lattice.bos_nodes();
  const Node *end_node = GetEndNode(
      *converter, segments, *begin_node, group, kSingleSegment);

  {
    nbest_generator->Reset(begin_node, end_node, NBestGenerator::STRICT);
    Segment result_segment;
    GatherCandidates(
        10, Segments::CONVERSION, nbest_generator.get(), &result_segment);
    // The top result is treated exceptionally and has no boundary check
    // in NBestGenerator.
    // The best route is calculated in ImmutalbeConverter with boundary check.
    // So, the top result should be inserted, but other candidates will be cut
    // due to boundary check between "する".
    ASSERT_EQ(1, result_segment.candidates_size());
    // "進行"
    EXPECT_EQ("\xe9\x80\xb2\xe8\xa1\x8c", result_segment.candidate(0).value);
  }

  {
    nbest_generator->Reset(begin_node, end_node, NBestGenerator::ONLY_MID);
    Segment result_segment;
    GatherCandidates(
        10, Segments::CONVERSION, nbest_generator.get(), &result_segment);
    ASSERT_EQ(3, result_segment.candidates_size());
    // "進行"
    EXPECT_EQ("\xe9\x80\xb2\xe8\xa1\x8c", result_segment.candidate(0).value);
    // "信仰"
    EXPECT_EQ("\xe4\xbf\xa1\xe4\xbb\xb0", result_segment.candidate(1).value);
    // "深耕"
    EXPECT_EQ("\xe6\xb7\xb1\xe8\x80\x95", result_segment.candidate(2).value);
  }
}

TEST_F(NBestGeneratorTest, SingleSegmentConnectionTest) {
  scoped_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();

  Segments segments;
  segments.set_request_type(Segments::CONVERSION);
  // "わたしのなまえはなかのです"
  string kText = ("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae"
                  "\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88\xe3\x81\xaf"
                  "\xe3\x81\xaa\xe3\x81\x8b\xe3\x81\xae\xe3\x81\xa7"
                  "\xe3\x81\x99");
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::FREE);
    segment->set_key(kText);
  }

  Lattice lattice;
  lattice.SetKey(kText);
  const ConversionRequest request;
  converter->MakeLattice(request, &segments, &lattice);

  vector<uint16> group;
  converter->MakeGroup(segments, &group);
  converter->Viterbi(segments, &lattice);

  scoped_ptr<NBestGenerator> nbest_generator(
      data_and_converter->CreateNBestGenerator(&lattice));


  const bool kSingleSegment = true;  // For realtime conversion
  const Node *begin_node = lattice.bos_nodes();
  const Node *end_node = GetEndNode(
      *converter, segments, *begin_node, group, kSingleSegment);

  {
    nbest_generator->Reset(begin_node, end_node, NBestGenerator::STRICT);
    Segment result_segment;
    GatherCandidates(
        10, Segments::CONVERSION, nbest_generator.get(), &result_segment);
    // Top result should be inserted, but other candidates will be cut
    // due to boundary check.
    ASSERT_EQ(1, result_segment.candidates_size());
    // "私の名前は中ノです"
    EXPECT_EQ("\xe7\xa7\x81\xe3\x81\xae\xe5\x90\x8d\xe5\x89\x8d"
              "\xe3\x81\xaf\xe4\xb8\xad\xe3\x83\x8e\xe3\x81\xa7\xe3\x81\x99",
              result_segment.candidate(0).value);
  }
  {
    nbest_generator->Reset(begin_node, end_node, NBestGenerator::ONLY_EDGE);
    Segment result_segment;
    GatherCandidates(
        10, Segments::CONVERSION, nbest_generator.get(), &result_segment);
    // We can get several candidates.
    ASSERT_LT(1, result_segment.candidates_size());
    // "私の名前は中ノです"
    EXPECT_EQ("\xe7\xa7\x81\xe3\x81\xae\xe5\x90\x8d\xe5\x89\x8d"
              "\xe3\x81\xaf\xe4\xb8\xad\xe3\x83\x8e\xe3\x81\xa7\xe3\x81\x99",
              result_segment.candidate(0).value);
  }
}
}  // namespace mozc
