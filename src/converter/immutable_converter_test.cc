// Copyright 2010-2012, Google Inc.
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

#include "base/base.h"
#include "base/logging.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/lattice.h"
#include "converter/segmenter_interface.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_dictionary_stub.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

using mozc::dictionary::DictionaryImpl;

void SetCandidate(const string &key, const string &value, Segment *segment) {
  segment->set_key(key);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->key = key;
  candidate->value = value;
  candidate->content_key = key;
  candidate->content_value = value;
}

class MockDataAndImmutableConverter {
 public:
  // Initializes data and immutable converter with given dictionaries. If NULL
  // is passed, the default mock dictionary is used. This class owns the first
  // argument dictionary but doesn't the second (because in the current design,
  // suffix dictionary is assumed to be a singleton).
  MockDataAndImmutableConverter(
      const DictionaryInterface *dictionary = NULL,
      const DictionaryInterface *suffix_dictionary = NULL) {
    data_manager_.reset(new testing::MockDataManager);

    const POSMatcher *pos_matcher = data_manager_->GetPOSMatcher();
    CHECK(pos_matcher);

    suppression_dictionary_.reset(new SuppressionDictionary);
    CHECK(suppression_dictionary_.get());

    if (dictionary) {
      dictionary_.reset(dictionary);
    } else {
      dictionary_.reset(
          new DictionaryImpl(data_manager_->CreateSystemDictionary(),
                             data_manager_->CreateValueDictionary(),
                             &user_dictionary_stub_,
                             suppression_dictionary_.get(),
                             pos_matcher));
    }
    CHECK(dictionary_.get());

    if (!suffix_dictionary) {
      suffix_dictionary = data_manager_->GetSuffixDictionary();
    }
    CHECK(suffix_dictionary);

    immutable_converter_.reset(new ImmutableConverterImpl(
        dictionary_.get(),
        suffix_dictionary,
        suppression_dictionary_.get(),
        data_manager_->GetConnector(),
        data_manager_->GetSegmenter(),
        pos_matcher,
        data_manager_->GetPosGroup()));
    CHECK(immutable_converter_.get());
  }

  ImmutableConverterImpl *GetConverter() {
    return immutable_converter_.get();
  }

  const SegmenterInterface *GetSegmenter() const {
    return data_manager_->GetSegmenter();
  }

 private:
  scoped_ptr<const DataManagerInterface> data_manager_;
  scoped_ptr<const SuppressionDictionary> suppression_dictionary_;
  scoped_ptr<const DictionaryInterface> dictionary_;
  scoped_ptr<ImmutableConverterImpl> immutable_converter_;
  UserDictionaryStub user_dictionary_stub_;
};

}  // namespace

class ImmutableConverterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
  }

 private:
  config::Config default_config_;
};

TEST_F(ImmutableConverterTest, KeepKeyForPrediction) {
  scoped_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  // "よろしくおねがいしま"
  const string kRequestKey =
      "\xe3\x82\x88\xe3\x82\x8d\xe3\x81\x97\xe3\x81\x8f\xe3\x81\x8a"
      "\xe3\x81\xad\xe3\x81\x8c\xe3\x81\x84\xe3\x81\x97\xe3\x81\xbe";
  segment->set_key(kRequestKey);
  EXPECT_TRUE(data_and_converter->GetConverter()->Convert(&segments));
  EXPECT_EQ(1, segments.segments_size());
  EXPECT_GT(segments.segment(0).candidates_size(), 0);
  EXPECT_EQ(kRequestKey, segments.segment(0).key());
}

TEST_F(ImmutableConverterTest, DummyCandidatesCost) {
  scoped_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  Segment segment;
  // "てすと"
  SetCandidate("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", "test", &segment);
  data_and_converter->GetConverter()->InsertDummyCandidates(&segment, 10);
  EXPECT_GE(segment.candidates_size(), 3);
  EXPECT_LT(segment.candidate(0).wcost, segment.candidate(1).wcost);
  EXPECT_LT(segment.candidate(0).wcost, segment.candidate(2).wcost);
}

namespace {
class KeyCheckDictionary : public DictionaryInterface {
 public:
  explicit KeyCheckDictionary(const string &query)
      : target_query_(query), received_target_query_(false) {}
  virtual ~KeyCheckDictionary() {}

  virtual Node *LookupPredictive(const char *str, int size,
                                 NodeAllocatorInterface *allocator) const {
    string key(str, size);
    if (key == target_query_) {
      received_target_query_ = true;
    }
    return NULL;
  }

  virtual Node *LookupPredictiveWithLimit(
      const char *str, int size, const Limit &limit,
      NodeAllocatorInterface *allocator) const {
    return LookupPredictive(str, size, allocator);
  }

  virtual Node *LookupPrefix(const char *str, int size,
                             NodeAllocatorInterface *allocator) const {
    // No check
    return NULL;
  }

  virtual Node *LookupPrefixWithLimit(const char *str, int size,
                                      const Limit &limit,
                                      NodeAllocatorInterface *allocator) const {
    // No check
    return NULL;
  }

  virtual Node *LookupExact(const char *str, int size,
                            NodeAllocatorInterface *allocator) const {
    // No check
    return NULL;
  }

  virtual Node *LookupReverse(const char *str, int size,
                              NodeAllocatorInterface *allocator) const {
    // No check
    return NULL;
  }

  bool received_target_query() const {
    return received_target_query_;
  }

 private:
  const string target_query_;
  mutable bool received_target_query_;
};
}  // namespace

TEST_F(ImmutableConverterTest, PredictiveNodesOnlyForConversionKey) {
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    // "いいんじゃな"
    segment->set_key("\xe3\x81\x84\xe3\x81\x84\xe3\x82\x93\xe3\x81\x98"
                     "\xe3\x82\x83\xe3\x81\xaa");
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    // "いいんじゃな"
    candidate->key =
        "\xe3\x81\x84\xe3\x81\x84\xe3\x82\x93\xe3\x81\x98"
        "\xe3\x82\x83\xe3\x81\xaa";
    // "いいんじゃな"
    candidate->value =
        "\xe3\x81\x84\xe3\x81\x84\xe3\x82\x93\xe3\x81\x98"
        "\xe3\x82\x83\xe3\x81\xaa";

    segment = segments.add_segment();
    // "いか"
    segment->set_key("\xe3\x81\x84\xe3\x81\x8b");

    EXPECT_EQ(1, segments.history_segments_size());
    EXPECT_EQ(1, segments.conversion_segments_size());
  }

  Lattice lattice;
  // "いいんじゃないか"
  lattice.SetKey("\xe3\x81\x84\xe3\x81\x84\xe3\x82\x93\xe3\x81\x98"
                 "\xe3\x82\x83\xe3\x81\xaa\xe3\x81\x84\xe3\x81\x8b");

  // "ないか"
  KeyCheckDictionary *dictionary =
      new KeyCheckDictionary("\xe3\x81\xaa\xe3\x81\x84\xe3\x81\x8b");
  scoped_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter(dictionary, dictionary));
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();
  converter->MakeLatticeNodesForPredictiveNodes(&lattice, &segments);
  EXPECT_FALSE(dictionary->received_target_query());
}

TEST_F(ImmutableConverterTest, AddPredictiveNodes) {
  Segments segments;
  {
    Segment *segment = segments.add_segment();
    // "よろしくおねがいしま"
    segment->set_key("\xe3\x82\x88\xe3\x82\x8d\xe3\x81\x97\xe3\x81\x8f"
                     "\xe3\x81\x8a\xe3\x81\xad\xe3\x81\x8c\xe3\x81\x84"
                     "\xe3\x81\x97\xe3\x81\xbe");

    EXPECT_EQ(1, segments.conversion_segments_size());
  }

  Lattice lattice;
  // "よろしくおねがいしま"
  lattice.SetKey("\xe3\x82\x88\xe3\x82\x8d\xe3\x81\x97\xe3\x81\x8f"
                 "\xe3\x81\x8a\xe3\x81\xad\xe3\x81\x8c\xe3\x81\x84"
                 "\xe3\x81\x97\xe3\x81\xbe");

  // "しま"
  KeyCheckDictionary *dictionary =
      new KeyCheckDictionary("\xe3\x81\x97\xe3\x81\xbe");
  scoped_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter(dictionary, dictionary));
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();
  converter->MakeLatticeNodesForPredictiveNodes(&lattice, &segments);
  EXPECT_TRUE(dictionary->received_target_query());
}

class ConnectionTypeHandlerTest : public ::testing::Test {
 protected:
  void MakeGroup(const Segments &segments, vector<uint16> *group) {
    group->clear();
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      for (size_t j = 0; j < segments.segment(i).key().size(); ++j) {
        group->push_back(static_cast<uint16>(i));
      }
    }
    group->push_back(static_cast<uint16>(segments.segments_size() - 1));
  }
};

TEST_F(ConnectionTypeHandlerTest, GetConnectionType) {
  scoped_ptr<MockDataAndImmutableConverter> data_and_converter(
      new MockDataAndImmutableConverter);
  ImmutableConverterImpl *converter = data_and_converter->GetConverter();

  Segments segments;
  segments.set_request_type(Segments::CONVERSION);

  Segment *segment = segments.add_segment();
  segment->set_segment_type(Segment::HISTORY);
  // "あやうく"
  segment->set_key("\xe3\x81\x82\xe3\x82\x84\xe3\x81\x86\xe3\x81\x8f");
  Segment::Candidate *candidate = segment->add_candidate();
  // "あやうく"
  candidate->key = "\xe3\x81\x82\xe3\x82\x84\xe3\x81\x86\xe3\x81\x8f";
  // "危うく"
  candidate->value = "\xe5\x8d\xb1\xe3\x81\x86\xe3\x81\x8f";

  segment = segments.add_segment();
  segment->set_segment_type(Segment::HISTORY);
  // "きき"
  segment->set_key("\xe3\x81\x8d\xe3\x81\x8d");
  candidate = segment->add_candidate();
  // "きき"
  candidate->key = "\xe3\x81\x8d\xe3\x81\x8d";
  // "危機"
  candidate->value = "\xe5\x8d\xb1\xe6\xa9\x9f";

  segment = segments.add_segment();
  segment->set_segment_type(Segment::FIXED_BOUNDARY);
  // "いっぱつの"
  segment->set_key(
      "\xe3\x81\x84\xe3\x81\xa3\xe3\x81\xb1\xe3\x81\xa4\xe3\x81\xae");

  segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  // "ところで"
  segment->set_key("\xe3\x81\xa8\xe3\x81\x93\xe3\x82\x8d\xe3\x81\xa7");

  vector<uint16> group;
  MakeGroup(segments, &group);

  Lattice lattice;
  // "あやうくききいっぱつのところで"
  lattice.SetKey(
      "\xe3\x81\x82\xe3\x82\x84\xe3\x81\x86\xe3\x81\x8f\xe3\x81\x8d\xe3\x81\x8d"
      "\xe3\x81\x84\xe3\x81\xa3\xe3\x81\xb1\xe3\x81\xa4\xe3\x81\xae\xe3\x81\xa8"
      "\xe3\x81\x93\xe3\x82\x8d\xe3\x81\xa7");
  converter->MakeLattice(&lattice, &segments);

  const SegmenterInterface *segmenter = data_and_converter->GetSegmenter();

  ConnectionTypeHandler connection_type_handler(
      group, &segments, segmenter);
  bool connected_found = false;
  bool weak_connected_found = false;
  bool not_connected_found = false;

  for (size_t pos = 0; pos <= lattice.key().size(); ++pos) {
    for (Node *rnode = lattice.begin_nodes(pos);
         rnode != NULL; rnode = rnode->bnext) {
      connection_type_handler.SetRNode(rnode);
      for (Node *lnode = lattice.end_nodes(pos);
           lnode != NULL; lnode = lnode->enext) {
        ConnectionTypeHandler::ConnectionType naive_result =
            ConnectionTypeHandler::GetConnectionTypeForUnittest(
                lnode, rnode, group, &segments, segmenter);
        ConnectionTypeHandler::ConnectionType result =
            connection_type_handler.GetConnectionType(lnode);
        EXPECT_EQ(naive_result, result);
        switch (naive_result) {
          case ConnectionTypeHandler::CONNECTED:
            connected_found = true;
            break;
          case ConnectionTypeHandler::WEAK_CONNECTED:
            weak_connected_found = true;
            break;
          case ConnectionTypeHandler::NOT_CONNECTED:
            not_connected_found = true;
            break;
          default:
            CHECK(false) << "Should not come here.";
        }
      }
    }
  }
  EXPECT_TRUE(connected_found);
  EXPECT_TRUE(weak_connected_found);
  EXPECT_TRUE(not_connected_found);
}
}  // namespace mozc
