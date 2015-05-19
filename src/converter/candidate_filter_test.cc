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

#include "converter/candidate_filter.h"

#include <climits>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/freelist.h"
#include "base/number_util.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/suggestion_filter.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace converter {
namespace {

const Segments::RequestType kRequestTypes[] = {
  Segments::CONVERSION,
  Segments::PREDICTION,
  Segments::SUGGESTION,
  Segments::PARTIAL_PREDICTION,
  Segments::PARTIAL_SUGGESTION,
  // Type Segments::REVERSE_CONVERSION is tested separately.
};

}  // namespace

class CandidateFilterTest : public ::testing::Test {
 protected:
  // Explicitly define constructor to prevent Visual C++ from
  // considering this class as POD.
  CandidateFilterTest() {}

  virtual void SetUp() {
    candidate_freelist_.reset(new FreeList<Segment::Candidate>(1024));
    node_freelist_.reset(new FreeList<Node>(1024));
    pos_matcher_ = UserPosManager::GetUserPosManager()->GetPOSMatcher();

    {
      mozc::testing::MockDataManager data_manager;
      const char *data = NULL;
      size_t size = 0;
      data_manager.GetSuggestionFilterData(&data, &size);
      suggestion_filter_.reset(new SuggestionFilter(data, size));
    }
  }

  virtual void TearDown() {
    candidate_freelist_->Free();
    node_freelist_->Free();
  }

  void GetDefaultNodes(vector<const Node *> *nodes) {
    nodes->clear();
    Node *n1 = NewNode();
    // "てすと"
    n1->value = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes->push_back(n1);

    Node *n2 = NewNode();
    // "てすと"
    n2->value = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
    n2->lid = pos_matcher().GetFunctionalId();
    n2->rid = pos_matcher().GetFunctionalId();
    nodes->push_back(n2);
  }

  Node *NewNode() {
    Node *n = node_freelist_->Alloc(1);
    n->Init();
    return n;
  }

  Segment::Candidate *NewCandidate() {
    Segment::Candidate *c = candidate_freelist_->Alloc(1);
    c->Init();
    c->cost = 100;
    c->structure_cost = 100;
    return c;
  }

  const POSMatcher &pos_matcher() const {
    return *pos_matcher_;
  }

  CandidateFilter *CreateCandidateFilter() const {
    return new CandidateFilter(&suppression_dictionary_,
                               pos_matcher_,
                               suggestion_filter_.get());
  }

  scoped_ptr<FreeList<Segment::Candidate> > candidate_freelist_;
  scoped_ptr<FreeList<Node> > node_freelist_;
  const POSMatcher *pos_matcher_;
  SuppressionDictionary suppression_dictionary_;
  scoped_ptr<SuggestionFilter> suggestion_filter_;
};

TEST_F(CandidateFilterTest, FilterTest) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
  vector<const Node *> n;

  GetDefaultNodes(&n);
  Segment::Candidate *c1 = NewCandidate();
  c1->lid = 1;
  c1->rid = 1;
  c1->key = "abc";
  c1->value = "abc";
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate("abc", c1, n, kRequestTypes[i]));
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();
  }

  // A candidate having the value seen before should be rejected.
  Segment::Candidate *c2 = NewCandidate();
  c2->key = "abc";
  c2->value = "abc";
  // Once filter "abc" so that the filter memorizes it.
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate("abc", c1, n, Segments::CONVERSION));
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("abc", c2, n, kRequestTypes[i]));
  }
  filter->Reset();

  // A candidate having high structure cost should be rejected.
  Segment::Candidate *c3 = NewCandidate();
  c3->structure_cost = INT_MAX;
  c3->key = "def";
  c3->value = "def";
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("def", c3, n, kRequestTypes[i]));
    filter->Reset();
  }

  // Check if a canidate is active before appending many candidates.
  Segment::Candidate *c4 = NewCandidate();
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate("", c4, n, kRequestTypes[i]));
    filter->Reset();
  }

  // Don't filter if lid/rid is the same as that of the top candidate.
  Segment::Candidate *c5 = NewCandidate();
  c5->key = "foo";
  c5->value = "foo";
  c5->lid = 1;
  c5->rid = 1;
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate("foo", c5, n, kRequestTypes[i]));
    filter->Reset();
  }

  // Although CandidateFilter may change its limit, 1000 should always exceed
  // the limit.
  for (int i = 0; i < 1000; ++i) {
    Segment::Candidate *cand = NewCandidate();
    cand->key = Util::StringPrintf("%d", i);
    cand->value = cand->key;
    filter->FilterCandidate(cand->key, cand, n, Segments::CONVERSION);
  }
  // There will be no more candidates.
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::STOP_ENUMERATION,
              filter->FilterCandidate("", c4, n, kRequestTypes[i]));
  }
}

TEST_F(CandidateFilterTest, KatakanaT13N) {
  {
    scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
    vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[0] is KatakanaT13N
    Segment::Candidate *c = NewCandidate();
    c->key = "abc";
    c->value = "abc";
    Node *n = NewNode();
    n->lid = pos_matcher().GetUnknownId();
    n->rid = pos_matcher().GetUnknownId();
    n->key = "abc";
    n->value = "abc";
    nodes[0] = n;
    for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
      EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
                filter->FilterCandidate("abc", c, nodes, kRequestTypes[i]));
      // Clear the internal set |seen_| to prevent "abc" from being filtered by
      // "seen" rule.
      filter->Reset();
    }
  }
  {
    scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
    vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[1] is KatakanaT13N
    Segment::Candidate *c = NewCandidate();
    c->key = "abc";
    c->value = "abc";
    Node *n = NewNode();
    n->lid = pos_matcher().GetFunctionalId();
    n->rid = pos_matcher().GetFunctionalId();
    n->key = "abc";
    n->value = "abc";
    nodes[1] = n;
    for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
      EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
                filter->FilterCandidate("abc", c, nodes, kRequestTypes[i]));
      filter->Reset();
    }
  }
  {
    scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
    vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[1] is not a functional word
    Segment::Candidate *c = NewCandidate();
    c->key = "abc";
    c->value = "abc";
    Node *n1 = NewNode();
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    n1->key = "abc";
    n1->value = "abc";
    nodes[0] = n1;
    Node *n2 = NewNode();
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    n2->key = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";  // "てすと"
    n2->value = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";  // "てすと"
    nodes[1] = n2;
    for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
      EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
                filter->FilterCandidate(
                    // "abcてすと"
                    "\x61\x62\x63\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8",
                    c, nodes, kRequestTypes[i]));
      filter->Reset();
    }
  }
}

TEST_F(CandidateFilterTest, IsolatedWord) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
  vector<const Node *> nodes;
  Segment::Candidate *c = NewCandidate();
  c->key = "abc";
  c->value = "abc";

  Node *node = NewNode();
  nodes.push_back(node);
  node->prev = NewNode();
  node->next = NewNode();
  node->lid = pos_matcher().GetIsolatedWordId();
  node->rid = pos_matcher().GetIsolatedWordId();
  node->key = "abc";
  node->value = "test";

  node->prev->node_type = Node::NOR_NODE;
  node->next->node_type = Node::EOS_NODE;
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("abc", c, nodes, kRequestTypes[i]));
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();
  }

  node->prev->node_type = Node::BOS_NODE;
  node->next->node_type = Node::NOR_NODE;
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("abc", c, nodes, kRequestTypes[i]));
    filter->Reset();
  }

  node->prev->node_type = Node::NOR_NODE;
  node->next->node_type = Node::NOR_NODE;
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("abc", c, nodes, kRequestTypes[i]));
    filter->Reset();
  }

  node->prev->node_type = Node::BOS_NODE;
  node->next->node_type = Node::EOS_NODE;
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate("abc", c, nodes, kRequestTypes[i]));
    filter->Reset();
  }
}

TEST_F(CandidateFilterTest, MayHaveMoreCandidates) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
  vector<const Node *> n;
  GetDefaultNodes(&n);

  Segment::Candidate *c1 = NewCandidate();
  c1->key = "abc";
  c1->value = "abc";
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate("abc", c1, n, kRequestTypes[i]));
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();
  }

  Segment::Candidate *c2 = NewCandidate();
  c2->key = "abc";
  c2->value = "abc";
  // Once filter "abc" so that the filter memorizes it.
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate("abc", c1, n, Segments::CONVERSION));
  // Candidates having the same value as c1 should be rejected but enumeration
  // should continue (i.e., STOP_ENUMERATION is not returned).
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("abc", c2, n, kRequestTypes[i]));
  }
  filter->Reset();

  Segment::Candidate *c3 = NewCandidate();
  c3->structure_cost = INT_MAX;
  c3->key = "def";
  c3->value = "def";
  // High structure cost should not Stop enumeration.
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("def", c3, n, kRequestTypes[i]));
    filter->Reset();
  }

  Segment::Candidate *c4 = NewCandidate();
  c4->cost = INT_MAX;
  c4->structure_cost = INT_MAX;
  c4->key = "ghi";
  c4->value = "ghi";
  // High cost candidate should be rejected.
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("ghi", c4, n, kRequestTypes[i]));
    filter->Reset();
  }

  // Insert many valid candidates
  for (int i = 0; i < 50; ++i) {
    Segment::Candidate *tmp = NewCandidate();
    tmp->key = NumberUtil::SimpleItoa(i) + "test";
    tmp->value = tmp->key;
    filter->FilterCandidate(tmp->key, tmp, n, Segments::CONVERSION);
  }

  Segment::Candidate *c5 = NewCandidate();
  c5->cost = INT_MAX;
  c5->structure_cost = INT_MAX;
  c5->value = "ghi";
  c5->value = "ghi";

  // Finally, it returns STOP_ENUMERATION, because
  // filter has seen more than 50 good candidates.
  c5->value = "ghi2";
  c5->value = "ghi2";
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::STOP_ENUMERATION,
              filter->FilterCandidate("ghi2", c5, n, kRequestTypes[i]));
  }
}

TEST_F(CandidateFilterTest, Regression3437022) {
  scoped_ptr<SuppressionDictionary> dic(new SuppressionDictionary);
  const POSMatcher *pos_matcher =
      UserPosManager::GetUserPosManager()->GetPOSMatcher();
  scoped_ptr<CandidateFilter> filter(
      new CandidateFilter(dic.get(), pos_matcher, suggestion_filter_.get()));

  vector<const Node *> n;
  GetDefaultNodes(&n);

  Segment::Candidate *c1 = NewCandidate();
  c1->key = "test_key";
  c1->value = "test_value";

  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate("test_key", c1, n, kRequestTypes[i]));
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();
  }

  dic->Lock();
  dic->AddEntry("test_key", "test_value");
  dic->UnLock();

  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(c1->key, c1, n, kRequestTypes[i]));
    filter->Reset();
  }

  c1->key = "test_key_suffix";
  c1->value = "test_value_suffix";
  c1->content_key = "test_key";
  c1->content_value = "test_value";

  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("test_key_suffix", c1, n,
                                      kRequestTypes[i]));
    filter->Reset();
  }

  dic->Lock();
  dic->Clear();
  dic->UnLock();

  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate("test_key_suffix", c1, n,
                                      kRequestTypes[i]));
    filter->Reset();
  }
}

TEST_F(CandidateFilterTest, FilterRealtimeConversionTest) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
  vector<const Node *> n;

  n.clear();
  Node *n1 = NewNode();

  n1->key = "PC";
  n1->value = "PC";
  n1->lid = pos_matcher().GetUnknownId();
  n1->rid = pos_matcher().GetUnknownId();
  n.push_back(n1);

  Node *n2 = NewNode();
  // "てすと"
  n2->value = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
  n2->lid = pos_matcher().GetUnknownId();
  n2->rid = pos_matcher().GetUnknownId();
  n.push_back(n2);

  Segment::Candidate *c1 = NewCandidate();
  c1->attributes |= Segment::Candidate::REALTIME_CONVERSION;
  c1->key = "\x50\x43\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";  // "PCてすと"
  c1->value = "PC\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";  // "PCテスト"
  // Don't filter a candidate because it starts with alphabets and
  // is followed by a non-functional word.
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate("PCてすと", c1, n, kRequestTypes[i]));
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();
  }
}

TEST_F(CandidateFilterTest, DoNotFilterExchangeableCandidates) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
  vector<const Node *> nodes;

  {
    Node *n1 = NewNode();
    // "よかっ"
    n1->key = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3";
    // "よかっ"
    n1->value = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n1);

    Node *n2 = NewNode();
    // "たり"
    n2->key = "\xe3\x81\x9f\xe3\x82\x8a";
    // "たり"
    n2->value = "\xe3\x81\x9f\xe3\x82\x8a";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n2);
  }

  Segment::Candidate *c1 = NewCandidate();
  // "よかったり"
  c1->key = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3\xe3\x81\x9f\xe3\x82\x8a";
  // "よかったり"
  c1->value = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3\xe3\x81\x9f\xe3\x82\x8a";
  // "よかっ"
  c1->content_key = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3";
  // "よかっ"
  c1->content_value = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3";
  c1->cost = 6000;
  c1->structure_cost = 1000;

  // Good top candidate
  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    ASSERT_EQ(
        CandidateFilter::GOOD_CANDIDATE,
        filter->FilterCandidate(c1->key, c1, nodes, kRequestTypes[i]));
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();
  }

  nodes.clear();
  {
    Node *n1 = NewNode();
    // "よかっ"
    n1->key = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3";
    // "良かっ"
    n1->value = "\xe8\x89\xaf\xe3\x81\x8b\xe3\x81\xa3";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n1);

    Node *n2 = NewNode();
    // "たり"
    n2->key = "\xe3\x81\x9f\xe3\x82\x8a";
    // "たり"
    n2->value = "\xe3\x81\x9f\xe3\x82\x8a";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n2);
  }

  Segment::Candidate *c2 = NewCandidate();
  // "よかったり"
  c2->key = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3\xe3\x81\x9f\xe3\x82\x8a";
  // "良かったり"
  c2->value = "\xe8\x89\xaf\xe3\x81\x8b\xe3\x81\xa3\xe3\x81\x9f\xe3\x82\x8a";
  // "よかっ"
  c2->content_key = "\xe3\x82\x88\xe3\x81\x8b\xe3\x81\xa3";
  // "良かっ"
  c2->content_value = "\xe8\x89\xaf\xe3\x81\x8b\xe3\x81\xa3";
  c2->cost = 12000;
  c2->structure_cost = 7500;  // has big structure cost

  for (size_t i = 0; i < arraysize(kRequestTypes); ++i) {
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(c2->key, c2, nodes, kRequestTypes[i]));
    filter->Reset();
  }
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilter_Conversion) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());

  // For Segments::CONVERSION, suggestion filter is not applied.
  {
    Node *n = NewNode();
    // "ふぃるたー"
    n->key = "\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B\xE3\x81\x9F\xE3\x83\xBC";
    // "フィルター"
    n->value = "\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x83\xBC";

    vector<const Node *> nodes;
    nodes.push_back(n);

    Segment::Candidate *c = NewCandidate();
    c->key = n->key;
    c->value = n->value;
    c->content_key = n->key;
    c->content_value = n->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(c->key, c, nodes, Segments::CONVERSION));
  }
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilter_Suggestion) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());

  // For Segments::SUGGESTION, suggestion filter is applied regardless of its
  // original key length. First test unigram case.
  {
    Node *n = NewNode();
    // "ふぃるたー"
    n->key = "\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B\xE3\x81\x9F\xE3\x83\xBC";
    // "フィルター"
    n->value = "\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x83\xBC";

    vector<const Node *> nodes;
    nodes.push_back(n);

    Segment::Candidate *c = NewCandidate();
    c->key = n->key;
    c->value = n->value;
    c->content_key = n->key;
    c->content_value = n->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "フィルター" is suggested from key "ふぃる".
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B",
                                      c, nodes, Segments::SUGGESTION));
    filter->Reset();
    // Test case where "フィルター" is suggested from key "ふぃるたー".
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(n->key, c, nodes, Segments::SUGGESTION));
  }
  // Next test bigram case.
  {
    filter->Reset();

    Node *n1 = NewNode();
    // "これは"
    n1->key = "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF";
    n1->value = n1->key;

    Node *n2 = NewNode();
    // "ふぃるたー"
    n2->key = "\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B\xE3\x81\x9F\xE3\x83\xBC";
    // "フィルター"
    n2->value = "\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x83\xBC";

    vector<const Node *> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);

    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key).append(n2->key);
    c->value.assign(n1->value).append(n2->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is suggested from key "これはふ".
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(
                  "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF\xE3\x81\xB5",
                  c, nodes, Segments::SUGGESTION));
    filter->Reset();
    // Test case where "これはフィルター" is suggested from the same key.
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(c->key, c, nodes, Segments::SUGGESTION));
  }
  // TODO(noriyukit): This is a limitation of current implementation. If
  // multiple nodes constitute a word in suggestion filter, it cannot be
  // filtered. Fix this.
  {
    filter->Reset();

    Node *n1 = NewNode();
    n1->key = "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF";  // "これは"
    n1->value = n1->key;

    Node *n2 = NewNode();
    n2->key = "\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B";  // "ふぃる"
    n2->value = "\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB";  // "フィル"

    Node *n3 = NewNode();
    n3->key = "\xE3\x81\x9F\xE3\x83\xBC";  // "たー"
    n3->value = "\xE3\x82\xBF\xE3\x83\xBC";  // "ター"

    vector<const Node *> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);
    nodes.push_back(n3);

    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key).append(n2->key).append(n3->key);;
    c->value.assign(n1->value).append(n2->value).append(n3->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is suggested from key "これはふ".
    // Since "フィルター" is constructed from two nodes, it cannot be filtered.
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(
                  "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF\xE3\x81\xB5",
                  c, nodes, Segments::SUGGESTION));
    filter->Reset();
    // Test case where "これはフィルター" is suggested from the same key.
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(c->key, c, nodes, Segments::SUGGESTION));
  }
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilter_Prediction) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());

  // For Segments::PREDICTION, suggestion filter is applied only when its
  // original key length is equal to the key of predicted node.  First test
  // unigram case.
  {
    Node *n = NewNode();
    // "ふぃるたー"
    n->key = "\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B\xE3\x81\x9F\xE3\x83\xBC";
    // "フィルター"
    n->value = "\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x83\xBC";

    vector<const Node *> nodes;
    nodes.push_back(n);

    Segment::Candidate *c = NewCandidate();
    c->key = n->key;
    c->value = n->value;
    c->content_key = n->key;
    c->content_value = n->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "フィルター" is predicted from key "ふぃる".
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate("\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B",
                                      c, nodes, Segments::PREDICTION));
    // Test case where "フィルター" is predicted from key "ふぃるたー". Note the
    // difference from the case of SUGGESTION, now words in suggestion filter
    // are good if its key is equal to the original key.
    filter->Reset();
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(n->key, c, nodes, Segments::PREDICTION));
  }
  // Next test bigram case.
  {
    filter->Reset();

    Node *n1 = NewNode();
    // "これは"
    n1->key = "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF";
    n1->value = n1->key;

    Node *n2 = NewNode();
    // "ふぃるたー"
    n2->key = "\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B\xE3\x81\x9F\xE3\x83\xBC";
    // "フィルター"
    n2->value = "\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB\xE3\x82\xBF\xE3\x83\xBC";

    vector<const Node *> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);

    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key).append(n2->key);
    c->value.assign(n1->value).append(n2->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is predicted from key "これはふ".
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(
                  "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF\xE3\x81\xB5",
                  c, nodes, Segments::PREDICTION));
    filter->Reset();
    // Test case where "これはフィルター" is predicted from the same key.
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(c->key, c, nodes, Segments::PREDICTION));
  }
  // TODO(noriyukit): This is a limitation of current implementation. If
  // multiple nodes constitute a word in suggestion filter, it cannot be
  // filtered. Fix this.
  {
    filter->Reset();

    filter->Reset();

    Node *n1 = NewNode();
    n1->key = "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF";  // "これは"
    n1->value = n1->key;

    Node *n2 = NewNode();
    n2->key = "\xE3\x81\xB5\xE3\x81\x83\xE3\x82\x8B";  // "ふぃる"
    n2->value = "\xE3\x83\x95\xE3\x82\xA3\xE3\x83\xAB";  // "フィル"

    Node *n3 = NewNode();
    n3->key = "\xE3\x81\x9F\xE3\x83\xBC";  // "たー"
    n3->value = "\xE3\x82\xBF\xE3\x83\xBC";  // "ター"

    vector<const Node *> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);
    nodes.push_back(n3);

    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key).append(n2->key).append(n3->key);;
    c->value.assign(n1->value).append(n2->value).append(n3->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is predicted from key "これはふ".
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(
                  "\xE3\x81\x93\xE3\x82\x8C\xE3\x81\xAF\xE3\x81\xB5",
                  c, nodes, Segments::PREDICTION));
    filter->Reset();
    // Test case where "これはフィルター" is predicted from the same key.
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(c->key, c, nodes, Segments::PREDICTION));
  }
}

TEST_F(CandidateFilterTest, ReverseConversion) {
  scoped_ptr<CandidateFilter> filter(CreateCandidateFilter());
  vector<const Node *> nodes;
  GetDefaultNodes(&nodes);

  const char kHonKanji[] = "\xE6\x9C\xAC";  // "本"
  const char kHonHiragana[] = "\xE3\x81\xBB\xE3\x82\x93";  // "ほん"

  Node *n1 = NewNode();
  n1->key = kHonKanji;
  n1->value = kHonHiragana;
  nodes.push_back(n1);

  Node *n2 = NewNode();
  n2->key = " ";
  n2->value = " ";
  nodes.push_back(n2);

  {
    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key);
    c->value.assign(n1->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(kHonHiragana, c, nodes,
                                      Segments::REVERSE_CONVERSION));
    // Duplicates should be removed.
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(kHonHiragana, c, nodes,
                                      Segments::REVERSE_CONVERSION));
  }
  {
    // White space should be valid candidate.
    Segment::Candidate *c = NewCandidate();
    c->key.assign(n2->key);
    c->value.assign(n2->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(" ", c, nodes,
                                      Segments::REVERSE_CONVERSION));
  }
}

}  // namespace converter
}  // namespace mozc
