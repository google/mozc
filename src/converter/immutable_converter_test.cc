// Copyright 2010-2011, Google Inc.
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

#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "converter/lattice.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/suffix_dictionary.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

namespace {

void SetCandidate(const string &key, const string &value, Segment *segment) {
  segment->set_key(key);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->key = key;
  candidate->value = value;
  candidate->content_key = key;
  candidate->content_value = value;
}

}  // namespace

class ImmutableConverterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
    immutable_converter_.reset(new ImmutableConverterImpl());
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
  }

  ImmutableConverterImpl *GetConverter() const {
    return immutable_converter_.get();
  }

 private:
  config::Config default_config_;
  scoped_ptr<ImmutableConverterImpl> immutable_converter_;
};

TEST_F(ImmutableConverterTest, KeepKeyForPrediction) {
  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  // "よろしくおねがいしま"
  const string kRequestKey =
      "\xe3\x82\x88\xe3\x82\x8d\xe3\x81\x97\xe3\x81\x8f\xe3\x81\x8a"
      "\xe3\x81\xad\xe3\x81\x8c\xe3\x81\x84\xe3\x81\x97\xe3\x81\xbe";
  segment->set_key(kRequestKey);
  EXPECT_TRUE(GetConverter()->Convert(&segments));
  EXPECT_EQ(1, segments.segments_size());
  EXPECT_GT(segments.segment(0).candidates_size(), 0);
  EXPECT_EQ(kRequestKey, segments.segment(0).key());
}

TEST_F(ImmutableConverterTest, DummyCandidatesCost) {
  Segment segment;
  // "てすと"
  SetCandidate("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", "test", &segment);
  GetConverter()->InsertDummyCandidates(&segment, 10);
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

  scoped_ptr<KeyCheckDictionary> dictionary(
      // "ないか"
      new KeyCheckDictionary("\xe3\x81\xaa\xe3\x81\x84\xe3\x81\x8b"));
  DictionaryFactory::SetDictionary(dictionary.get());
  SuffixDictionaryFactory::SetSuffixDictionary(dictionary.get());

  GetConverter()->MakeLatticeNodesForPredictiveNodes(
      &lattice, &segments);

  EXPECT_FALSE(dictionary->received_target_query());

  DictionaryFactory::SetDictionary(NULL);
  SuffixDictionaryFactory::SetSuffixDictionary(NULL);
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

  scoped_ptr<KeyCheckDictionary> dictionary(
      // "しま"
      new KeyCheckDictionary("\xe3\x81\x97\xe3\x81\xbe"));
  DictionaryFactory::SetDictionary(dictionary.get());
  SuffixDictionaryFactory::SetSuffixDictionary(dictionary.get());

  GetConverter()->MakeLatticeNodesForPredictiveNodes(
      &lattice, &segments);

  EXPECT_TRUE(dictionary->received_target_query());

  DictionaryFactory::SetDictionary(NULL);
  SuffixDictionaryFactory::SetSuffixDictionary(NULL);
}

TEST_F(ImmutableConverterTest, PromoteUserDictionaryCandidate) {
  {
    Segment segment;
    EXPECT_EQ(0, segment.candidates_size());
    GetConverter()->PromoteUserDictionaryCandidate(&segment);
    EXPECT_EQ(0, segment.candidates_size());
  }

  {
    Segment segment;
    // "てすと"
    SetCandidate("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8", "test", &segment);
    EXPECT_EQ(1, segment.candidates_size());
    EXPECT_EQ("test", segment.candidate(0).value);
    GetConverter()->PromoteUserDictionaryCandidate(&segment);
    EXPECT_EQ(1, segment.candidates_size());
    EXPECT_EQ("test", segment.candidate(0).value);
  }
  {
    // "てすと"
    const string kWord0 = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
    // "テスト"
    const string kWord1 = "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
    // "user dictionary テスト"
    const string kWord2 = "user dictionary "
        "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88";
    Segment segment;
    // "てすと"
    segment.set_key("\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8");
    Segment::Candidate *candidate = segment.add_candidate();
    candidate->Init();
    candidate->key = kWord0;
    candidate->value = kWord0;
    candidate = segment.add_candidate();
    candidate->Init();
    candidate->key = kWord0;
    candidate->value = kWord1;
    candidate = segment.add_candidate();
    candidate->Init();
    candidate->key = kWord0;
    candidate->value = kWord2;
    candidate->attributes |= Segment::Candidate::USER_DICTIONARY;

    EXPECT_EQ(3, segment.candidates_size());
    EXPECT_EQ(kWord0, segment.candidate(0).value);
    EXPECT_EQ(kWord1, segment.candidate(1).value);
    EXPECT_EQ(kWord2, segment.candidate(2).value);

    GetConverter()->PromoteUserDictionaryCandidate(&segment);

    EXPECT_EQ(3, segment.candidates_size());
    EXPECT_EQ(kWord0, segment.candidate(0).value);
    EXPECT_EQ(kWord2, segment.candidate(1).value);
    EXPECT_EQ(kWord1, segment.candidate(2).value);
  }
}

}  // namespace mozc
