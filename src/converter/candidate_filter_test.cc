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

#include "converter/candidate_filter.h"

#include <climits>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "base/container/freelist.h"
#include "converter/candidate.h"
#include "converter/node.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_matcher.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"

namespace mozc {
namespace converter {
namespace {

using ::mozc::dictionary::MockUserDictionary;
using ::mozc::dictionary::PosMatcher;
using ::testing::_;
using ::testing::Return;
using ::testing::WithParamInterface;

const ConversionRequest::RequestType kRequestTypes[] = {
    ConversionRequest::CONVERSION,
    ConversionRequest::PREDICTION,
    ConversionRequest::SUGGESTION,
    ConversionRequest::PARTIAL_PREDICTION,
    ConversionRequest::PARTIAL_SUGGESTION,
    // Type ConversionRequest::REVERSE_CONVERSION is tested separately.
};

std::string RequestParamToString(
    const ::testing::TestParamInfo<ConversionRequest::RequestType> &info) {
  switch (info.param) {
    case ConversionRequest::CONVERSION:
      return "CONVERSION";
    case ConversionRequest::PREDICTION:
      return "PREDICTION";
    case ConversionRequest::SUGGESTION:
      return "SUGGESTION";
    case ConversionRequest::PARTIAL_PREDICTION:
      return "PARTIAL_PREDICTION";
    case ConversionRequest::PARTIAL_SUGGESTION:
      return "PARTIAL_SUGGESTION";
    default:
      LOG(FATAL) << "Should not come here.";
      return "UNKNOWN";
  }
}

class CandidateFilterTest : public ::testing::Test {
 protected:
  CandidateFilterTest()
      : pos_matcher_(mock_data_manager_.GetPosMatcherData()),
        suggestion_filter_(SuggestionFilter::CreateOrDie(
            mock_data_manager_.GetSuggestionFilterData())) {}

  void SetUp() override {
    candidate_freelist_ = std::make_unique<FreeList<Candidate>>(1024);
    node_freelist_ = std::make_unique<FreeList<Node>>(1024);
  }

  void TearDown() override {
    ::testing::Mock::VerifyAndClearExpectations(&mock_user_dictionary_);
    candidate_freelist_->Free();
    node_freelist_->Free();
  }

  void GetDefaultNodes(std::vector<const Node *> *nodes) {
    nodes->clear();
    Node *n1 = NewNode();
    n1->value = "てすと";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes->push_back(n1);

    Node *n2 = NewNode();
    n2->value = "てすと";
    n2->lid = pos_matcher().GetFunctionalId();
    n2->rid = pos_matcher().GetFunctionalId();
    nodes->push_back(n2);
  }

  Node *NewNode() {
    Node *n = node_freelist_->Alloc();
    n->Init();
    return n;
  }

  Candidate *NewCandidate() {
    Candidate *c = candidate_freelist_->Alloc();
    c->cost = 100;
    c->structure_cost = 100;
    return c;
  }

  const PosMatcher &pos_matcher() const { return pos_matcher_; }

  CandidateFilter *CreateCandidateFilter() const {
    EXPECT_CALL(mock_user_dictionary_, IsSuppressedEntry(_, _))
        .WillRepeatedly(Return(false));
    return new CandidateFilter(mock_user_dictionary_, pos_matcher_,
                               suggestion_filter_);
  }

 private:
  testing::MockDataManager mock_data_manager_;

 protected:
  const PosMatcher pos_matcher_;
  MockUserDictionary mock_user_dictionary_;
  const SuggestionFilter suggestion_filter_;

  std::unique_ptr<FreeList<Candidate>> candidate_freelist_;
  std::unique_ptr<FreeList<Node>> node_freelist_;
};

class CandidateFilterTestWithParam
    : public CandidateFilterTest,
      public WithParamInterface<ConversionRequest::RequestType> {};

ConversionRequest ConvReq(ConversionRequest::RequestType request_type) {
  return ConversionRequestBuilder().SetRequestType(request_type).Build();
}

TEST_P(CandidateFilterTestWithParam, FilterTest) {
  ConversionRequest::RequestType type = GetParam();

  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> n;

  GetDefaultNodes(&n);
  Candidate *c1 = NewCandidate();
  c1->lid = 1;
  c1->rid = 1;
  c1->key = "abc";
  c1->value = "abc";

  const ConversionRequest convreq_type = ConvReq(type);
  const ConversionRequest convreq_conv = ConvReq(ConversionRequest::CONVERSION);

  EXPECT_EQ(filter->FilterCandidate(convreq_type, "abc", c1, n, n),
            CandidateFilter::GOOD_CANDIDATE);
  // Clear the internal set |seen_| to prevent "abc" from being filtered by
  // "seen" rule.
  filter->Reset();

  // A candidate having the value seen before should be rejected.
  Candidate *c2 = NewCandidate();
  c2->lid = 1;
  c2->rid = 1;
  c2->key = "abc";
  c2->value = "abc";

  // Once filter "abc" so that the filter memorizes it.
  EXPECT_EQ(filter->FilterCandidate(convreq_conv, "abc", c1, n, n),
            CandidateFilter::GOOD_CANDIDATE);
  EXPECT_EQ(filter->FilterCandidate(convreq_type, "abc", c2, n, n),
            CandidateFilter::BAD_CANDIDATE);

  // A candidate having high structure cost should be rejected.
  Candidate *c3 = NewCandidate();
  c3->structure_cost = INT_MAX;
  c3->key = "def";
  c3->value = "def";

  EXPECT_EQ(filter->FilterCandidate(convreq_conv, "def", c3, n, n),
            CandidateFilter::BAD_CANDIDATE);

  // Check if a candidate is active before appending many candidates.
  Candidate *c4 = NewCandidate();
  EXPECT_EQ(filter->FilterCandidate(convreq_conv, "", c4, n, n),
            CandidateFilter::GOOD_CANDIDATE);

  // Don't filter if lid/rid is the same as that of the top candidate.
  Candidate *c5 = NewCandidate();
  c5->key = "foo";
  c5->value = "foo";
  c5->lid = 1;
  c5->rid = 1;
  EXPECT_EQ(filter->FilterCandidate(convreq_conv, "foo", c5, n, n),
            CandidateFilter::GOOD_CANDIDATE);

  // Although CandidateFilter may change its limit, 1000 should always exceed
  // the limit.
  for (int i = 0; i < 1000; ++i) {
    Candidate *cand = NewCandidate();
    cand->key = absl::StrFormat("%d", i);
    cand->value = cand->key;
    filter->FilterCandidate(convreq_conv, cand->key, cand, n, n);
  }
  // There will be no more candidates.
  EXPECT_EQ(filter->FilterCandidate(convreq_type, "", c4, n, n),
            CandidateFilter::STOP_ENUMERATION);
}

TEST_P(CandidateFilterTestWithParam, DeduplicationTest) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());

  ConversionRequest::RequestType type = GetParam();
  const ConversionRequest convreq = ConvReq(type);

  std::vector<const Node *> n;
  GetDefaultNodes(&n);

  {
    Candidate *cand = NewCandidate();
    cand->lid = 1;
    cand->rid = 1;
    cand->key = "abc";
    cand->value = "abc";
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", cand, n, n),
              CandidateFilter::GOOD_CANDIDATE);
  }

  {
    // If all of lid, rid and value are the same with existing candidates,
    // it is filtered.
    Candidate *cand = NewCandidate();
    cand->lid = 1;
    cand->rid = 1;
    cand->key = "abc";
    cand->value = "abc";
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", cand, n, n),
              CandidateFilter::BAD_CANDIDATE);
  }

  {
    // lid is different from existing candidates.
    Candidate *cand = NewCandidate();
    cand->lid = 2;
    cand->rid = 1;
    cand->key = "abc";
    cand->value = "abc";
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", cand, n, n),
              CandidateFilter::GOOD_CANDIDATE);
  }

  {
    // rid is different from existing candidates.
    Candidate *cand = NewCandidate();
    cand->lid = 1;
    cand->rid = 2;
    cand->key = "abc";
    cand->value = "abc";
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", cand, n, n),
              CandidateFilter::GOOD_CANDIDATE);
  }

  {
    // value is different from existing candidates.
    Candidate *cand = NewCandidate();
    cand->lid = 1;
    cand->rid = 1;
    cand->key = "abc";
    cand->value = "xyz";
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", cand, n, n),
              CandidateFilter::GOOD_CANDIDATE);
  }
}

TEST_P(CandidateFilterTestWithParam, KatakanaT13N) {
  ConversionRequest::RequestType type = GetParam();
  const ConversionRequest convreq = ConvReq(type);
  {
    std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
    std::vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[0] is KatakanaT13N
    Candidate *c = NewCandidate();
    c->key = "えびし";
    c->value = "abc";
    Node *n = NewNode();
    n->lid = pos_matcher().GetUnknownId();
    n->rid = pos_matcher().GetUnknownId();
    n->key = "えびし";
    n->value = "abc";
    nodes[0] = n;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();
  }
  {
    std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
    std::vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[1] is KatakanaT13N
    Candidate *c = NewCandidate();
    c->key = "えびし";
    c->value = "abc";
    Node *n = NewNode();
    n->lid = pos_matcher().GetFunctionalId();
    n->rid = pos_matcher().GetFunctionalId();
    n->key = "えびし";
    n->value = "abc";
    nodes[1] = n;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
  }
  {
    std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
    std::vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[1] is not a functional word
    Candidate *c = NewCandidate();
    c->key = "えびし";
    c->value = "abc";
    Node *n1 = NewNode();
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    n1->key = "えびし";
    n1->value = "abc";
    nodes[0] = n1;
    Node *n2 = NewNode();
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    n2->key = "てすと";
    n2->value = "てすと";
    nodes[1] = n2;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abcてすと", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
  }
}

TEST_P(CandidateFilterTestWithParam, IsolatedWordOrGeneralSymbol) {
  ConversionRequest::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> nodes;
  Candidate *c = NewCandidate();
  c->key = "abc";
  c->value = "abc";

  Node *node = NewNode();
  nodes.push_back(node);
  node->prev = NewNode();
  node->next = NewNode();
  node->key = "abc";
  node->value = "test";

  const uint16_t pos_ids[] = {
      pos_matcher().GetIsolatedWordId(),
      pos_matcher().GetGeneralSymbolId(),
  };
  // Perform the same test for the above POS IDs.
  const ConversionRequest convreq = ConvReq(type);
  for (const uint16_t id : pos_ids) {
    node->lid = id;
    node->rid = id;

    node->prev->node_type = Node::NOR_NODE;
    node->next->node_type = Node::EOS_NODE;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();

    node->prev->node_type = Node::BOS_NODE;
    node->next->node_type = Node::NOR_NODE;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
    filter->Reset();

    node->prev->node_type = Node::NOR_NODE;
    node->next->node_type = Node::NOR_NODE;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
    filter->Reset();

    node->prev->node_type = Node::BOS_NODE;
    node->next->node_type = Node::EOS_NODE;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
    filter->Reset();

    Node *backup_node = node->prev;
    node->prev = nullptr;
    node->next->node_type = Node::EOS_NODE;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
    filter->Reset();

    node->prev = backup_node;

    backup_node = node->next;
    node->prev->node_type = Node::BOS_NODE;
    node->next = nullptr;
    EXPECT_EQ(filter->FilterCandidate(convreq, "abc", c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
    filter->Reset();

    node->next = backup_node;
  }
}

TEST_F(CandidateFilterTest, IsolatedWordInMultipleNodes) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());

  Candidate *c = NewCandidate();
  c->key = "abcisolatedxyz";
  c->value = "abcisolatedxyz";

  std::vector<Node *> nodes = {NewNode(), NewNode(), NewNode()};

  nodes[0]->prev = nullptr;
  nodes[0]->next = nodes[1];
  nodes[0]->lid = pos_matcher().GetUnknownId();
  nodes[0]->rid = pos_matcher().GetUnknownId();
  nodes[0]->key = "abc";
  nodes[0]->value = "abc";

  nodes[1]->prev = nodes[0];
  nodes[1]->next = nodes[2];
  nodes[1]->lid = pos_matcher().GetIsolatedWordId();
  nodes[1]->rid = pos_matcher().GetIsolatedWordId();
  nodes[1]->key = "isolated";
  nodes[1]->value = "isolated";

  nodes[2]->prev = nodes[1];
  nodes[2]->next = nullptr;
  nodes[2]->lid = pos_matcher().GetUnknownId();
  nodes[2]->rid = pos_matcher().GetUnknownId();
  nodes[2]->key = "xyz";
  nodes[2]->value = "xyz";

  const std::vector<const Node *> const_nodes(nodes.begin(), nodes.end());
  const ConversionRequest convreq = ConvReq(ConversionRequest::CONVERSION);
  EXPECT_EQ(filter->FilterCandidate(convreq, "abcisolatedxyz", c, const_nodes,
                                    const_nodes),
            CandidateFilter::BAD_CANDIDATE);
}

TEST_P(CandidateFilterTestWithParam, MayHaveMoreCandidates) {
  ConversionRequest::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> n;
  GetDefaultNodes(&n);

  Candidate *c1 = NewCandidate();
  c1->key = "abc";
  c1->value = "abc";
  const ConversionRequest convreq1 = ConvReq(type);
  EXPECT_EQ(filter->FilterCandidate(convreq1, "abc", c1, n, n),
            CandidateFilter::GOOD_CANDIDATE);
  // Clear the internal set |seen_| to prevent "abc" from being filtered by
  // "seen" rule.
  filter->Reset();

  Candidate *c2 = NewCandidate();
  c2->key = "abc";
  c2->value = "abc";
  // Once filter "abc" so that the filter memorizes it.
  const ConversionRequest convreq2 = ConvReq(ConversionRequest::CONVERSION);
  EXPECT_EQ(filter->FilterCandidate(convreq2, "abc", c1, n, n),
            CandidateFilter::GOOD_CANDIDATE);
  // Candidates having the same value as c1 should be rejected but enumeration
  // should continue (i.e., STOP_ENUMERATION is not returned).
  const ConversionRequest convreq3 = ConvReq(type);
  EXPECT_EQ(filter->FilterCandidate(convreq3, "abc", c2, n, n),
            CandidateFilter::BAD_CANDIDATE);

  Candidate *c3 = NewCandidate();
  c3->structure_cost = INT_MAX;
  c3->key = "def";
  c3->value = "def";
  // High structure cost should not Stop enumeration.
  EXPECT_EQ(filter->FilterCandidate(convreq3, "def", c3, n, n),
            CandidateFilter::BAD_CANDIDATE);

  Candidate *c4 = NewCandidate();
  c4->cost = INT_MAX;
  c4->structure_cost = INT_MAX;
  c4->key = "ghi";
  c4->value = "ghi";
  // High cost candidate should be rejected.
  EXPECT_EQ(filter->FilterCandidate(convreq3, "ghi", c4, n, n),
            CandidateFilter::BAD_CANDIDATE);

  // Insert many valid candidates
  const ConversionRequest convreq4 = ConvReq(ConversionRequest::CONVERSION);
  for (int i = 0; i < 50; ++i) {
    Candidate *tmp = NewCandidate();
    tmp->key = std::to_string(i) + "test";
    tmp->value = tmp->key;
    filter->FilterCandidate(convreq4, tmp->key, tmp, n, n);
  }

  Candidate *c5 = NewCandidate();
  c5->cost = INT_MAX;
  c5->structure_cost = INT_MAX;
  c5->value = "ghi";
  c5->value = "ghi";

  // Finally, it returns STOP_ENUMERATION, because
  // filter has seen more than 50 good candidates.
  c5->value = "ghi2";
  c5->value = "ghi2";
  const ConversionRequest convreq5 = ConvReq(type);
  EXPECT_EQ(filter->FilterCandidate(convreq5, "ghi2", c5, n, n),
            CandidateFilter::STOP_ENUMERATION);
}

TEST_P(CandidateFilterTestWithParam, Regression3437022) {
  ConversionRequest::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());

  std::vector<const Node *> n;
  GetDefaultNodes(&n);

  Candidate *c1 = NewCandidate();
  c1->key = "test_key";
  c1->value = "test_value";

  const ConversionRequest convreq = ConvReq(type);
  EXPECT_EQ(filter->FilterCandidate(convreq, "test_key", c1, n, n),
            CandidateFilter::GOOD_CANDIDATE);
  // Clear the internal set |seen_| to prevent "test_key" from being filtered by
  // "seen" rule.
  filter->Reset();

  EXPECT_CALL(mock_user_dictionary_, IsSuppressedEntry(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_user_dictionary_,
              IsSuppressedEntry("test_key", "test_value"))
      .WillRepeatedly(Return(true));

  EXPECT_EQ(filter->FilterCandidate(convreq, c1->key, c1, n, n),
            CandidateFilter::BAD_CANDIDATE);

  c1->key = "test_key_suffix";
  c1->value = "test_value_suffix";
  c1->content_key = "test_key";
  c1->content_value = "test_value";

  EXPECT_EQ(filter->FilterCandidate(convreq, "test_key_suffix", c1, n, n),
            CandidateFilter::BAD_CANDIDATE);

  ::testing::Mock::VerifyAndClearExpectations(&mock_user_dictionary_);
  EXPECT_CALL(mock_user_dictionary_, IsSuppressedEntry(_, _))
      .WillRepeatedly(Return(false));

  EXPECT_EQ(filter->FilterCandidate(convreq, "test_key_suffix", c1, n, n),
            CandidateFilter::GOOD_CANDIDATE);
}

TEST_P(CandidateFilterTestWithParam, FilterRealtimeConversionTest) {
  ConversionRequest::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> n;

  n.clear();
  Node *n1 = NewNode();

  n1->key = "PC";
  n1->value = "PC";
  n1->lid = pos_matcher().GetUnknownId();
  n1->rid = pos_matcher().GetUnknownId();
  n.push_back(n1);

  Node *n2 = NewNode();
  n2->value = "てすと";
  n2->lid = pos_matcher().GetUnknownId();
  n2->rid = pos_matcher().GetUnknownId();
  n.push_back(n2);

  Candidate *c1 = NewCandidate();
  c1->attributes |= Candidate::REALTIME_CONVERSION;
  c1->key = "PCてすと";
  c1->value = "PCテスト";
  // Don't filter a candidate because it starts with alphabets and
  // is followed by a non-functional word.
  const ConversionRequest convreq = ConvReq(type);
  EXPECT_EQ(filter->FilterCandidate(convreq, "PCてすと", c1, n, n),
            CandidateFilter::GOOD_CANDIDATE);
}

TEST_P(CandidateFilterTestWithParam, DoNotFilterExchangeableCandidates) {
  ConversionRequest::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> top_nodes, nodes;
  const ConversionRequest convreq = ConvReq(type);

  {
    Node *n1 = NewNode();
    n1->key = "よかっ";
    n1->value = "よかっ";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    top_nodes.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "たり";
    n2->value = "たり";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    top_nodes.push_back(n2);
  }

  Candidate *c1 = NewCandidate();
  c1->key = "よかったり";
  c1->value = "よかったり";
  c1->content_key = "よかっ";
  c1->content_value = "よかっ";
  c1->cost = 6000;
  c1->structure_cost = 1000;

  // Good top candidate
  ASSERT_EQ(
      CandidateFilter::GOOD_CANDIDATE,
      filter->FilterCandidate(convreq, c1->key, c1, top_nodes, top_nodes));

  {
    Node *n1 = NewNode();
    n1->key = "よかっ";
    n1->value = "良かっ";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "たり";
    n2->value = "たり";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n2);
  }

  Candidate *c2 = NewCandidate();
  c2->key = "よかったり";
  c2->value = "良かったり";
  c2->content_key = "よかっ";
  c2->content_value = "良かっ";
  c2->cost = 12000;
  c2->structure_cost = 7500;  // has big structure cost

  EXPECT_EQ(filter->FilterCandidate(convreq, c2->key, c2, top_nodes, nodes),
            CandidateFilter::GOOD_CANDIDATE);

  nodes.clear();
  {
    Node *n1 = NewNode();
    n1->key = "よ";
    n1->value = "よ";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "かっ";
    n2->value = "買っ";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n2);

    Node *n3 = NewNode();
    n3->key = "たり";
    n3->value = "たり";
    n3->lid = pos_matcher().GetUnknownId();
    n3->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n3);
  }

  Candidate *c3 = NewCandidate();
  c3->key = "よかったり";
  c3->value = "よ買ったり";
  c3->content_key = "よかっ";
  c3->content_value = "よ買っ";
  c3->cost = 12000;
  c3->structure_cost = 7500;  // has big structure cost

  EXPECT_EQ(filter->FilterCandidate(convreq, c3->key, c3, top_nodes, nodes),
            CandidateFilter::GOOD_CANDIDATE);
}

TEST_P(CandidateFilterTestWithParam,
       DoNotFilterExchangeableCandidatesNoisyNonContentWord) {
  ConversionRequest::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> nodes1;
  const ConversionRequest convreq = ConvReq(type);

  {
    Node *n1 = NewNode();
    n1->key = "よう";
    n1->value = "用";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes1.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "ずみ";
    n2->value = "済み";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    nodes1.push_back(n2);
  }

  Candidate *c1 = NewCandidate();
  c1->key = "ようずみ";
  c1->value = "用済み";
  c1->content_key = "よう";
  c1->content_value = "用";
  c1->cost = 6000;
  c1->structure_cost = 500;

  std::vector<const Node *> nodes2;
  {
    Node *n1 = NewNode();
    n1->key = "よう";
    n1->value = "洋";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes2.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "ずみ";
    n2->value = "済み";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    nodes2.push_back(n2);
  }

  Candidate *c2 = NewCandidate();
  c2->key = "ようずみ";
  c2->value = "洋済み";
  c2->content_key = "よう";
  c2->content_value = "洋";
  c2->cost = 12000;
  c2->structure_cost = 8000;  // has big structure cost

  EXPECT_EQ(filter->FilterCandidate(convreq, c1->key, c1, nodes1, nodes1),
            CandidateFilter::GOOD_CANDIDATE);
  EXPECT_EQ(filter->FilterCandidate(convreq, c2->key, c2, nodes1, nodes2),
            CandidateFilter::BAD_CANDIDATE);
}

TEST_P(CandidateFilterTestWithParam, FilterMultipleNumberNodesWord) {
  ConversionRequest::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> nodes1;
  const ConversionRequest convreq = ConvReq(type);

  {
    Node *n1 = NewNode();
    n1->key = "に";
    n1->value = "2";
    n1->lid = pos_matcher().GetNumberId();
    n1->rid = pos_matcher().GetNumberId();
    nodes1.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "じゅうさんじゅう";
    n2->value = "十三重";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    nodes1.push_back(n2);
  }

  Candidate *c1 = NewCandidate();
  c1->key = "にじゅうさんじゅう";
  c1->value = "2十三重";
  c1->content_key = "に";
  c1->content_value = "2";
  c1->cost = 6000;
  c1->structure_cost = 500;

  EXPECT_EQ(filter->FilterCandidate(convreq, c1->key, c1, nodes1, nodes1),
            CandidateFilter::BAD_CANDIDATE);

  std::vector<const Node *> nodes2;
  {
    Node *n1 = NewNode();
    n1->key = "にじゅうさんじゅう";
    n1->value = "二重三重";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    nodes2.push_back(n1);
  }

  Candidate *c2 = NewCandidate();
  c2->key = "にじゅうさんじゅう";
  c2->value = "二重三重";
  c2->content_key = "にじゅうさんじゅう";
  c2->content_value = "二重三重";
  c2->cost = 6000;
  c2->structure_cost = 5000;

  EXPECT_EQ(filter->FilterCandidate(convreq, c2->key, c2, nodes1, nodes2),
            CandidateFilter::GOOD_CANDIDATE);

  std::vector<const Node *> nodes3;
  {
    Node *n1 = NewNode();
    n1->key = "1";
    n1->value = "1";
    n1->lid = pos_matcher().GetNumberId();
    n1->rid = pos_matcher().GetNumberId();
    nodes3.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "0";
    n2->value = "0";
    n2->lid = pos_matcher().GetNumberId();
    n2->rid = pos_matcher().GetNumberId();
    nodes3.push_back(n2);

    Node *n3 = NewNode();
    n3->key = "まん";
    n3->value = "万";
    n3->lid = pos_matcher().GetKanjiNumberId();
    n3->rid = pos_matcher().GetKanjiNumberId();
    nodes3.push_back(n3);

    Node *n4 = NewNode();
    n4->key = "えん";
    n4->value = "円";
    n4->lid = pos_matcher().GetUnknownId();
    n4->rid = pos_matcher().GetUnknownId();
    nodes3.push_back(n4);
  }

  Candidate *c3 = NewCandidate();
  c3->key = "10まんえん";
  c3->value = "10万円";
  c3->content_key = "10";
  c3->content_value = "10";
  c3->cost = 6000;
  c3->structure_cost = 500;

  EXPECT_EQ(filter->FilterCandidate(convreq, c3->key, c3, nodes1, nodes3),
            CandidateFilter::GOOD_CANDIDATE);
}

TEST_P(CandidateFilterTestWithParam, FilterNoisyNumberCandidate) {
  ConversionRequest::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> nodes1;
  ConversionRequest::Options options = {
      .request_type = type,
      .create_partial_candidates = true,
  };
  const ConversionRequest convreq =
      ConversionRequestBuilder().SetOptions(std::move(options)).Build();
  {
    Node *n1 = NewNode();
    n1->key = "さん";
    n1->value = "3";
    n1->lid = pos_matcher().GetNumberId();
    n1->rid = pos_matcher().GetNumberId();
    nodes1.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "です";
    n2->value = "です";
    n2->lid = pos_matcher().GetSuffixWordId();
    n2->rid = pos_matcher().GetSuffixWordId();
    nodes1.push_back(n2);
  }

  Candidate *c1 = NewCandidate();
  c1->key = "さんです";
  c1->value = "3です";
  c1->content_key = "さん";
  c1->content_value = "3";
  c1->cost = 1000;
  c1->structure_cost = 50;

  EXPECT_EQ(filter->FilterCandidate(convreq, c1->key, c1, nodes1, nodes1),
            CandidateFilter::BAD_CANDIDATE);

  std::vector<const Node *> nodes2;
  {
    Node *n1 = NewNode();
    n1->key = "しんじゅく";
    n1->value = "新宿";
    n1->lid = pos_matcher().GetUniqueNounId();
    n1->rid = pos_matcher().GetUniqueNounId();
    nodes2.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "に";
    n2->value = "二";
    n2->lid = pos_matcher().GetNumberId();
    n2->rid = pos_matcher().GetNumberId();
    nodes2.push_back(n2);
  }

  Candidate *c2 = NewCandidate();
  c2->key = "しんじゅくに";
  c2->value = "新宿二";
  c2->content_key = "しんじゅく";
  c2->content_value = "新宿";
  c2->cost = 1000;
  c2->structure_cost = 50;

  EXPECT_EQ(filter->FilterCandidate(convreq, c2->key, c2, nodes2, nodes2),
            CandidateFilter::BAD_CANDIDATE);

  std::vector<const Node *> nodes3;
  {
    Node *n1 = NewNode();
    n1->key = "ginza";
    n1->value = "GINZA";
    n1->lid = pos_matcher().GetUniqueNounId();
    n1->rid = pos_matcher().GetUniqueNounId();
    nodes3.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "7";
    n2->value = "7";
    n2->lid = pos_matcher().GetNumberId();
    n2->rid = pos_matcher().GetNumberId();
    nodes3.push_back(n2);
  }

  Candidate *c3 = NewCandidate();
  c3->key = "ginza7";
  c3->value = "GINZA7";
  c3->content_key = "ginza";
  c3->content_value = "GINZA";
  c3->cost = 1000;
  c3->structure_cost = 50;

  EXPECT_EQ(filter->FilterCandidate(convreq, c3->key, c3, nodes3, nodes3),
            CandidateFilter::GOOD_CANDIDATE);

  std::vector<const Node *> nodes4;
  {
    Node *n1 = NewNode();
    n1->key = "に";
    n1->value = "2";
    n1->lid = pos_matcher().GetNumberId();
    n1->rid = pos_matcher().GetNumberId();
    nodes4.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "ねん";
    n2->value = "年";
    n2->lid = pos_matcher().GetCounterSuffixWordId();
    n2->rid = pos_matcher().GetCounterSuffixWordId();
    nodes4.push_back(n2);

    Node *n3 = NewNode();
    n3->key = "ご";
    n3->value = "後";
    nodes4.push_back(n3);
  }

  Candidate *c4 = NewCandidate();
  c4->key = "にねんご";
  c4->value = "2年後";
  c4->content_key = "に";
  c4->content_value = "2";
  c4->cost = 1000;
  c4->structure_cost = 50;

  EXPECT_EQ(filter->FilterCandidate(convreq, c4->key, c4, nodes4, nodes4),
            CandidateFilter::GOOD_CANDIDATE);
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilterConversion) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());

  // For ConversionRequest::CONVERSION, suggestion filter is not applied.
  {
    Node *n = NewNode();
    n->key = "ふぃるたー";
    n->value = "フィルター";

    std::vector<const Node *> nodes;
    nodes.push_back(n);

    Candidate *c = NewCandidate();
    c->key = n->key;
    c->value = n->value;
    c->content_key = n->key;
    c->content_value = n->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    const ConversionRequest convreq = ConvReq(ConversionRequest::CONVERSION);
    EXPECT_EQ(filter->FilterCandidate(convreq, c->key, c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
  }
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilterSuggestion) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  const ConversionRequest convreq = ConvReq(ConversionRequest::SUGGESTION);

  // For ConversionRequest::SUGGESTION, suggestion filter is applied regardless
  // of its original key length. First test unigram case.
  {
    Node *n = NewNode();
    n->key = "ふぃるたー";
    n->value = "フィルター";

    std::vector<const Node *> nodes;
    nodes.push_back(n);

    Candidate *c = NewCandidate();
    c->key = n->key;
    c->value = n->value;
    c->content_key = n->key;
    c->content_value = n->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "フィルター" is suggested from key "ふぃる".
    EXPECT_EQ(filter->FilterCandidate(convreq, "ふぃる", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
    filter->Reset();
    // Test case where "フィルター" is suggested from key "ふぃるたー".
    EXPECT_EQ(filter->FilterCandidate(convreq, n->key, c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
  }
  // Next test bigram case.
  {
    filter->Reset();

    Node *n1 = NewNode();
    n1->key = "これは";
    n1->value = n1->key;

    Node *n2 = NewNode();
    n2->key = "ふぃるたー";
    n2->value = "フィルター";

    std::vector<const Node *> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);

    Candidate *c = NewCandidate();
    c->key = absl::StrCat(n1->key, n2->key);
    c->value = absl::StrCat(n1->value, n2->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is suggested from key "これはふ".
    EXPECT_EQ(filter->FilterCandidate(convreq, "これはふ", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
    filter->Reset();
    // Test case where "これはフィルター" is suggested from the same key.
    EXPECT_EQ(filter->FilterCandidate(convreq, c->key, c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
  }
  // TODO(noriyukit): This is a limitation of current implementation. If
  // multiple nodes constitute a word in suggestion filter, it cannot be
  // filtered. Fix this.
  {
    filter->Reset();

    Node *n1 = NewNode();
    n1->key = "これは";
    n1->value = n1->key;

    Node *n2 = NewNode();
    n2->key = "ふぃる";
    n2->value = "フィル";

    Node *n3 = NewNode();
    n3->key = "たー";
    n3->value = "ター";

    std::vector<const Node *> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);
    nodes.push_back(n3);

    Candidate *c = NewCandidate();
    c->key = absl::StrCat(n1->key, n2->key, n3->key);
    c->value = absl::StrCat(n1->value, n2->value, n3->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is suggested from key "これはふ".
    // Since "フィルター" is constructed from two nodes, it cannot be filtered.
    EXPECT_EQ(filter->FilterCandidate(convreq, "これはふ", c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
    filter->Reset();
    // Test case where "これはフィルター" is suggested from the same key.
    EXPECT_EQ(filter->FilterCandidate(convreq, c->key, c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
  }
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilterPrediction) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  const ConversionRequest convreq = ConvReq(ConversionRequest::PREDICTION);

  // For ConversionRequest::PREDICTION, suggestion filter is applied only when
  // its original key length is equal to the key of predicted node.  First test
  // unigram case.
  {
    Node *n = NewNode();
    n->key = "ふぃるたー";
    n->value = "フィルター";

    std::vector<const Node *> nodes;
    nodes.push_back(n);

    Candidate *c = NewCandidate();
    c->key = n->key;
    c->value = n->value;
    c->content_key = n->key;
    c->content_value = n->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "フィルター" is predicted from key "ふぃる".
    EXPECT_EQ(filter->FilterCandidate(convreq, "ふぃる", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
    // Test case where "フィルター" is predicted from key "ふぃるたー". Note the
    // difference from the case of SUGGESTION, now words in suggestion filter
    // are good if its key is equal to the original key.
    filter->Reset();
    EXPECT_EQ(filter->FilterCandidate(convreq, n->key, c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
  }
  // Next test bigram case.
  {
    filter->Reset();

    Node *n1 = NewNode();
    n1->key = "これは";
    n1->value = n1->key;

    Node *n2 = NewNode();
    n2->key = "ふぃるたー";
    n2->value = "フィルター";

    std::vector<const Node *> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);

    Candidate *c = NewCandidate();
    c->key = absl::StrCat(n1->key, n2->key);
    c->value = absl::StrCat(n1->value, n2->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is predicted from key "これはふ".
    EXPECT_EQ(filter->FilterCandidate(convreq, "これはふ", c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
    filter->Reset();
    // Test case where "これはフィルター" is predicted from the same key.
    EXPECT_EQ(filter->FilterCandidate(convreq, c->key, c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
  }
  // TODO(noriyukit): This is a limitation of current implementation. If
  // multiple nodes constitute a word in suggestion filter, it cannot be
  // filtered. Fix this.
  {
    filter->Reset();
    filter->Reset();

    Node *n1 = NewNode();
    n1->key = "これは";
    n1->value = n1->key;

    Node *n2 = NewNode();
    n2->key = "ふぃる";
    n2->value = "フィル";

    Node *n3 = NewNode();
    n3->key = "たー";
    n3->value = "ター";

    std::vector<const Node *> nodes;
    nodes.push_back(n1);
    nodes.push_back(n2);
    nodes.push_back(n3);

    Candidate *c = NewCandidate();
    c->key = absl::StrCat(n1->key, n2->key, n3->key);
    c->value = absl::StrCat(n1->value, n2->value, n3->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is predicted from key "これはふ".
    EXPECT_EQ(filter->FilterCandidate(convreq, "これはふ", c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
    filter->Reset();
    // Test case where "これはフィルター" is predicted from the same key.
    EXPECT_EQ(filter->FilterCandidate(convreq, c->key, c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
  }
}

TEST_F(CandidateFilterTest, ReverseConversion) {
  const ConversionRequest convreq =
      ConvReq(ConversionRequest::REVERSE_CONVERSION);
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter());
  std::vector<const Node *> nodes;
  GetDefaultNodes(&nodes);

  constexpr char kHonKanji[] = "本";
  constexpr char kHonHiragana[] = "ほん";

  Node *n1 = NewNode();
  n1->key = kHonKanji;
  n1->value = kHonHiragana;
  nodes.push_back(n1);

  Node *n2 = NewNode();
  n2->key = " ";
  n2->value = " ";
  nodes.push_back(n2);

  {
    Candidate *c = NewCandidate();
    c->key.assign(n1->key);
    c->value.assign(n1->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;
    EXPECT_EQ(filter->FilterCandidate(convreq, kHonHiragana, c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
    // Duplicates should be removed.
    EXPECT_EQ(filter->FilterCandidate(convreq, kHonHiragana, c, nodes, nodes),
              CandidateFilter::BAD_CANDIDATE);
  }
  {
    // White space should be valid candidate.
    Candidate *c = NewCandidate();
    c->key.assign(n2->key);
    c->value.assign(n2->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;
    EXPECT_EQ(filter->FilterCandidate(convreq, " ", c, nodes, nodes),
              CandidateFilter::GOOD_CANDIDATE);
  }
}

INSTANTIATE_TEST_SUITE_P(TestForRequest, CandidateFilterTestWithParam,
                         ::testing::ValuesIn(kRequestTypes),
                         RequestParamToString);

}  // namespace
}  // namespace converter
}  // namespace mozc
