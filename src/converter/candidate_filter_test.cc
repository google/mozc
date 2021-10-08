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
#include <vector>

#include "base/freelist.h"
#include "base/port.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace converter {
namespace {

using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::SuppressionDictionary;
using ::testing::WithParamInterface;

const Segments::RequestType kRequestTypes[] = {
    Segments::CONVERSION,         Segments::PREDICTION,
    Segments::SUGGESTION,         Segments::PARTIAL_PREDICTION,
    Segments::PARTIAL_SUGGESTION,
    // Type Segments::REVERSE_CONVERSION is tested separately.
};

std::string RequestParamToString(
    const ::testing::TestParamInfo<Segments::RequestType> &info) {
  switch (info.param) {
    case Segments::CONVERSION:
      return "CONVERSION";
    case Segments::PREDICTION:
      return "PREDICTION";
    case Segments::SUGGESTION:
      return "SUGGESTION";
    case Segments::PARTIAL_PREDICTION:
      return "PARTIAL_PREDICTION";
    case Segments::PARTIAL_SUGGESTION:
      return "PARTIAL_SUGGESTION";
    default:
      LOG(FATAL) << "Should not come here.";
      return "UNKNOWN";
  }
}

class CandidateFilterTest : public ::testing::Test {
 protected:
  // Explicitly define constructor to prevent Visual C++ from
  // considering this class as POD.
  CandidateFilterTest() {}

  void SetUp() override {
    candidate_freelist_ = absl::make_unique<FreeList<Segment::Candidate>>(1024);
    node_freelist_ = absl::make_unique<FreeList<Node>>(1024);
    pos_matcher_.Set(mock_data_manager_.GetPosMatcherData());
    request_ = absl::make_unique<ConversionRequest>();
    {
      const char *data = nullptr;
      size_t size = 0;
      mock_data_manager_.GetSuggestionFilterData(&data, &size);
      suggestion_filter_ = absl::make_unique<SuggestionFilter>(data, size);
    }
  }

  void TearDown() override {
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

  const PosMatcher &pos_matcher() const { return pos_matcher_; }

  CandidateFilter *CreateCandidateFilter(
      bool apply_suggestion_filter_for_exact_match) const {
    return new CandidateFilter(&suppression_dictionary_, &pos_matcher_,
                               suggestion_filter_.get(),
                               apply_suggestion_filter_for_exact_match);
  }

  std::unique_ptr<FreeList<Segment::Candidate>> candidate_freelist_;
  std::unique_ptr<FreeList<Node>> node_freelist_;
  PosMatcher pos_matcher_;
  SuppressionDictionary suppression_dictionary_;
  std::unique_ptr<SuggestionFilter> suggestion_filter_;
  std::unique_ptr<ConversionRequest> request_;

 private:
  testing::MockDataManager mock_data_manager_;
};

class CandidateFilterTestWithParam
    : public CandidateFilterTest,
      public WithParamInterface<Segments::RequestType> {};

TEST_P(CandidateFilterTestWithParam, FilterTest) {
  Segments::RequestType type = GetParam();

  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
  std::vector<const Node *> n;

  GetDefaultNodes(&n);
  Segment::Candidate *c1 = NewCandidate();
  c1->lid = 1;
  c1->rid = 1;
  c1->key = "abc";
  c1->value = "abc";

  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, "abc", c1, n, n, type));
  // Clear the internal set |seen_| to prevent "abc" from being filtered by
  // "seen" rule.
  filter->Reset();

  // A candidate having the value seen before should be rejected.
  Segment::Candidate *c2 = NewCandidate();
  c2->key = "abc";
  c2->value = "abc";

  // Once filter "abc" so that the filter memorizes it.
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, "abc", c1, n, n,
                                    Segments::CONVERSION));
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter->FilterCandidate(*request_, "abc", c2, n, n, type));

  // A candidate having high structure cost should be rejected.
  Segment::Candidate *c3 = NewCandidate();
  c3->structure_cost = INT_MAX;
  c3->key = "def";
  c3->value = "def";

  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter->FilterCandidate(*request_, "def", c3, n, n, type));

  // Check if a candidate is active before appending many candidates.
  Segment::Candidate *c4 = NewCandidate();
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, "", c4, n, n, type));

  // Don't filter if lid/rid is the same as that of the top candidate.
  Segment::Candidate *c5 = NewCandidate();
  c5->key = "foo";
  c5->value = "foo";
  c5->lid = 1;
  c5->rid = 1;
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, "foo", c5, n, n, type));

  // Although CandidateFilter may change its limit, 1000 should always exceed
  // the limit.
  for (int i = 0; i < 1000; ++i) {
    Segment::Candidate *cand = NewCandidate();
    cand->key = Util::StringPrintf("%d", i);
    cand->value = cand->key;
    filter->FilterCandidate(*request_, cand->key, cand, n, n,
                            Segments::CONVERSION);
  }
  // There will be no more candidates.
  EXPECT_EQ(CandidateFilter::STOP_ENUMERATION,
            filter->FilterCandidate(*request_, "", c4, n, n, type));
}

TEST_P(CandidateFilterTestWithParam, KatakanaT13N) {
  Segments::RequestType type = GetParam();
  {
    std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
    std::vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[0] is KatakanaT13N
    Segment::Candidate *c = NewCandidate();
    c->key = "えびし";
    c->value = "abc";
    Node *n = NewNode();
    n->lid = pos_matcher().GetUnknownId();
    n->rid = pos_matcher().GetUnknownId();
    n->key = "えびし";
    n->value = "abc";
    nodes[0] = n;
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, "abc", c, nodes, nodes, type));
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();
  }
  {
    std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
    std::vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[1] is KatakanaT13N
    Segment::Candidate *c = NewCandidate();
    c->key = "えびし";
    c->value = "abc";
    Node *n = NewNode();
    n->lid = pos_matcher().GetFunctionalId();
    n->rid = pos_matcher().GetFunctionalId();
    n->key = "えびし";
    n->value = "abc";
    nodes[1] = n;
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, "abc", c, nodes, nodes, type));
  }
  {
    std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
    std::vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[1] is not a functional word
    Segment::Candidate *c = NewCandidate();
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
    EXPECT_EQ(
        CandidateFilter::BAD_CANDIDATE,
        filter->FilterCandidate(*request_, "abcてすと", c, nodes, nodes, type));
  }
}

TEST_P(CandidateFilterTestWithParam, IsolatedWordOrGeneralSymbol) {
  Segments::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
  std::vector<const Node *> nodes;
  Segment::Candidate *c = NewCandidate();
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
  for (const uint16_t id : pos_ids) {
    node->lid = id;
    node->rid = id;

    node->prev->node_type = Node::NOR_NODE;
    node->next->node_type = Node::EOS_NODE;
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, "abc", c, nodes, nodes, type));
    // Clear the internal set |seen_| to prevent "abc" from being filtered by
    // "seen" rule.
    filter->Reset();

    node->prev->node_type = Node::BOS_NODE;
    node->next->node_type = Node::NOR_NODE;
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, "abc", c, nodes, nodes, type));
    filter->Reset();

    node->prev->node_type = Node::NOR_NODE;
    node->next->node_type = Node::NOR_NODE;
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, "abc", c, nodes, nodes, type));
    filter->Reset();

    node->prev->node_type = Node::BOS_NODE;
    node->next->node_type = Node::EOS_NODE;
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, "abc", c, nodes, nodes, type));
    filter->Reset();

    Node *backup_node = node->prev;
    node->prev = nullptr;
    node->next->node_type = Node::EOS_NODE;
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, "abc", c, nodes, nodes, type));
    filter->Reset();

    node->prev = backup_node;

    backup_node = node->next;
    node->prev->node_type = Node::BOS_NODE;
    node->next = nullptr;
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, "abc", c, nodes, nodes, type));
    filter->Reset();

    node->next = backup_node;
  }
}

TEST_F(CandidateFilterTest, IsolatedWordInMultipleNodes) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));

  Segment::Candidate *c = NewCandidate();
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
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter->FilterCandidate(*request_, "abcisolatedxyz", c, const_nodes,
                                    const_nodes, Segments::CONVERSION));
}

TEST_P(CandidateFilterTestWithParam, MayHaveMoreCandidates) {
  Segments::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
  std::vector<const Node *> n;
  GetDefaultNodes(&n);

  Segment::Candidate *c1 = NewCandidate();
  c1->key = "abc";
  c1->value = "abc";
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, "abc", c1, n, n, type));
  // Clear the internal set |seen_| to prevent "abc" from being filtered by
  // "seen" rule.
  filter->Reset();

  Segment::Candidate *c2 = NewCandidate();
  c2->key = "abc";
  c2->value = "abc";
  // Once filter "abc" so that the filter memorizes it.
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, "abc", c1, n, n,
                                    Segments::CONVERSION));
  // Candidates having the same value as c1 should be rejected but enumeration
  // should continue (i.e., STOP_ENUMERATION is not returned).
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter->FilterCandidate(*request_, "abc", c2, n, n, type));

  Segment::Candidate *c3 = NewCandidate();
  c3->structure_cost = INT_MAX;
  c3->key = "def";
  c3->value = "def";
  // High structure cost should not Stop enumeration.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter->FilterCandidate(*request_, "def", c3, n, n, type));

  Segment::Candidate *c4 = NewCandidate();
  c4->cost = INT_MAX;
  c4->structure_cost = INT_MAX;
  c4->key = "ghi";
  c4->value = "ghi";
  // High cost candidate should be rejected.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter->FilterCandidate(*request_, "ghi", c4, n, n, type));

  // Insert many valid candidates
  for (int i = 0; i < 50; ++i) {
    Segment::Candidate *tmp = NewCandidate();
    tmp->key = std::to_string(i) + "test";
    tmp->value = tmp->key;
    filter->FilterCandidate(*request_, tmp->key, tmp, n, n,
                            Segments::CONVERSION);
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
  EXPECT_EQ(CandidateFilter::STOP_ENUMERATION,
            filter->FilterCandidate(*request_, "ghi2", c5, n, n, type));
}

TEST_P(CandidateFilterTestWithParam, Regression3437022) {
  Segments::RequestType type = GetParam();
  std::unique_ptr<SuppressionDictionary> dic(new SuppressionDictionary);
  std::unique_ptr<CandidateFilter> filter(new CandidateFilter(
      dic.get(), &pos_matcher_, suggestion_filter_.get(), true));

  std::vector<const Node *> n;
  GetDefaultNodes(&n);

  Segment::Candidate *c1 = NewCandidate();
  c1->key = "test_key";
  c1->value = "test_value";

  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, "test_key", c1, n, n, type));
  // Clear the internal set |seen_| to prevent "test_key" from being filtered by
  // "seen" rule.
  filter->Reset();

  dic->Lock();
  dic->AddEntry("test_key", "test_value");
  dic->UnLock();

  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter->FilterCandidate(*request_, c1->key, c1, n, n, type));

  c1->key = "test_key_suffix";
  c1->value = "test_value_suffix";
  c1->content_key = "test_key";
  c1->content_value = "test_value";

  EXPECT_EQ(
      CandidateFilter::BAD_CANDIDATE,
      filter->FilterCandidate(*request_, "test_key_suffix", c1, n, n, type));

  dic->Lock();
  dic->Clear();
  dic->UnLock();

  EXPECT_EQ(
      CandidateFilter::GOOD_CANDIDATE,
      filter->FilterCandidate(*request_, "test_key_suffix", c1, n, n, type));
}

TEST_P(CandidateFilterTestWithParam, FilterRealtimeConversionTest) {
  Segments::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
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

  Segment::Candidate *c1 = NewCandidate();
  c1->attributes |= Segment::Candidate::REALTIME_CONVERSION;
  c1->key = "PCてすと";
  c1->value = "PCテスト";
  // Don't filter a candidate because it starts with alphabets and
  // is followed by a non-functional word.
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, "PCてすと", c1, n, n, type));
}

TEST_P(CandidateFilterTestWithParam, DoNotFilterExchangeableCandidates) {
  Segments::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
  std::vector<const Node *> top_nodes, nodes;

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

  Segment::Candidate *c1 = NewCandidate();
  c1->key = "よかったり";
  c1->value = "よかったり";
  c1->content_key = "よかっ";
  c1->content_value = "よかっ";
  c1->cost = 6000;
  c1->structure_cost = 1000;

  // Good top candidate
  ASSERT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, c1->key, c1, top_nodes,
                                    top_nodes, type));

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

  Segment::Candidate *c2 = NewCandidate();
  c2->key = "よかったり";
  c2->value = "良かったり";
  c2->content_key = "よかっ";
  c2->content_value = "良かっ";
  c2->cost = 12000;
  c2->structure_cost = 7500;  // has big structure cost

  EXPECT_EQ(
      CandidateFilter::GOOD_CANDIDATE,
      filter->FilterCandidate(*request_, c2->key, c2, top_nodes, nodes, type));

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

  Segment::Candidate *c3 = NewCandidate();
  c3->key = "よかったり";
  c3->value = "よ買ったり";
  c3->content_key = "よかっ";
  c3->content_value = "よ買っ";
  c3->cost = 12000;
  c3->structure_cost = 7500;  // has big structure cost

  EXPECT_EQ(
      CandidateFilter::GOOD_CANDIDATE,
      filter->FilterCandidate(*request_, c3->key, c3, top_nodes, nodes, type));
}

TEST_P(CandidateFilterTestWithParam,
       DoNotFilterExchangeableCandidates_StrictMode) {
  Segments::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
  std::vector<const Node *> top_nodes, nodes;
  commands::Request req;
  req.mutable_decoder_experiment_params()->set_enable_strict_candidate_filter(
      true);
  request_->set_request(&req);

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

  Segment::Candidate *c1 = NewCandidate();
  c1->key = "よかったり";
  c1->value = "よかったり";
  c1->content_key = "よかっ";
  c1->content_value = "よかっ";
  c1->cost = 6000;
  c1->structure_cost = 1000;

  // Good top candidate
  ASSERT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, c1->key, c1, top_nodes,
                                    top_nodes, type));

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

  Segment::Candidate *c2 = NewCandidate();
  c2->key = "よかったり";
  c2->value = "よ買ったり";
  c2->content_key = "よかっ";
  c2->content_value = "よ買っ";
  c2->cost = 12000;
  c2->structure_cost = 7500;  // has big structure cost

  EXPECT_EQ(
      CandidateFilter::BAD_CANDIDATE,
      filter->FilterCandidate(*request_, c2->key, c2, top_nodes, nodes, type));
}

TEST_P(CandidateFilterTestWithParam, FilterCandidatesForStrictMode) {
  Segments::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
  std::vector<const Node *> top_nodes, nodes;
  commands::Request req;
  req.mutable_decoder_experiment_params()->set_enable_strict_candidate_filter(
      true);
  request_->set_request(&req);

  {
    Node *n1 = NewNode();
    n1->key = "すごく";
    n1->value = "すごく";
    n1->lid = pos_matcher().GetUnknownId();
    n1->rid = pos_matcher().GetUnknownId();
    top_nodes.push_back(n1);
  }

  Segment::Candidate *c1 = NewCandidate();
  c1->key = "すごく";
  c1->value = "すごく";
  c1->content_key = "すごく";
  c1->content_value = "すごく";
  c1->lid = pos_matcher().GetUnknownId();
  c1->rid = pos_matcher().GetUnknownId();
  c1->cost = 6000;
  c1->structure_cost = 0;

  // Good top candidate
  ASSERT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter->FilterCandidate(*request_, c1->key, c1, top_nodes,
                                    top_nodes, type));

  {
    Node *n1 = NewNode();
    n1->key = "す";
    n1->value = "す";
    n1->lid = pos_matcher().GetGeneralNounId();
    n1->rid = pos_matcher().GetGeneralNounId();
    nodes.push_back(n1);

    Node *n2 = NewNode();
    n2->key = "ごく";
    n2->value = "国";
    n2->lid = pos_matcher().GetUnknownId();
    n2->rid = pos_matcher().GetUnknownId();
    nodes.push_back(n2);
  }

  Segment::Candidate *c2 = NewCandidate();
  c2->key = "すごく";
  c2->value = "す国";
  c2->content_key = "す";
  c2->content_value = "す";
  c2->lid = pos_matcher().GetGeneralNounId();
  c2->rid = pos_matcher().GetUnknownId();
  c2->cost = 6000;
  c2->structure_cost = 100;  // small structure cost

  EXPECT_EQ(
      CandidateFilter::BAD_CANDIDATE,
      filter->FilterCandidate(*request_, c2->key, c2, top_nodes, nodes, type));
}

TEST_P(CandidateFilterTestWithParam,
       DoNotFilterExchangeableCandidates_NoisyNonContentWord) {
  Segments::RequestType type = GetParam();
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
  std::vector<const Node *> nodes1;

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

  Segment::Candidate *c1 = NewCandidate();
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

  Segment::Candidate *c2 = NewCandidate();
  c2->key = "ようずみ";
  c2->value = "洋済み";
  c2->content_key = "よう";
  c2->content_value = "洋";
  c2->cost = 12000;
  c2->structure_cost = 8000;  // has big structure cost

  EXPECT_EQ(
      CandidateFilter::GOOD_CANDIDATE,
      filter->FilterCandidate(*request_, c1->key, c1, nodes1, nodes1, type));
  EXPECT_EQ(
      CandidateFilter::BAD_CANDIDATE,
      filter->FilterCandidate(*request_, c2->key, c2, nodes1, nodes2, type));
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilter_Conversion) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));

  // For Segments::CONVERSION, suggestion filter is not applied.
  {
    Node *n = NewNode();
    n->key = "ふぃるたー";
    n->value = "フィルター";

    std::vector<const Node *> nodes;
    nodes.push_back(n);

    Segment::Candidate *c = NewCandidate();
    c->key = n->key;
    c->value = n->value;
    c->content_key = n->key;
    c->content_value = n->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, c->key, c, nodes, nodes,
                                      Segments::CONVERSION));
  }
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilter_Suggestion) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));

  // For Segments::SUGGESTION, suggestion filter is applied regardless of its
  // original key length. First test unigram case.
  {
    Node *n = NewNode();
    n->key = "ふぃるたー";
    n->value = "フィルター";

    std::vector<const Node *> nodes;
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
              filter->FilterCandidate(*request_, "ふぃる", c, nodes, nodes,
                                      Segments::SUGGESTION));
    filter->Reset();
    // Test case where "フィルター" is suggested from key "ふぃるたー".
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, n->key, c, nodes, nodes,
                                      Segments::SUGGESTION));
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

    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key).append(n2->key);
    c->value.assign(n1->value).append(n2->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is suggested from key "これはふ".
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, "これはふ", c, nodes, nodes,
                                      Segments::SUGGESTION));
    filter->Reset();
    // Test case where "これはフィルター" is suggested from the same key.
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, c->key, c, nodes, nodes,
                                      Segments::SUGGESTION));
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

    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key).append(n2->key).append(n3->key);
    ;
    c->value.assign(n1->value).append(n2->value).append(n3->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is suggested from key "これはふ".
    // Since "フィルター" is constructed from two nodes, it cannot be filtered.
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, "これはふ", c, nodes, nodes,
                                      Segments::SUGGESTION));
    filter->Reset();
    // Test case where "これはフィルター" is suggested from the same key.
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, c->key, c, nodes, nodes,
                                      Segments::SUGGESTION));
  }
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilter_Suggestion_Mobile) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(false));

  // For Mobile Segments::SUGGESTION, suggestion filter is NOT applied for
  // exact match.
  {
    Node *n = NewNode();
    n->key = "ふぃるたー";
    n->value = "フィルター";

    std::vector<const Node *> nodes;
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
              filter->FilterCandidate(*request_, "ふぃる", c, nodes, nodes,
                                      Segments::SUGGESTION));
    filter->Reset();
    // Test case where "フィルター" is suggested from key "ふぃるたー".
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, n->key, c, nodes, nodes,
                                      Segments::SUGGESTION));
  }
}

TEST_F(CandidateFilterTest, CapabilityOfSuggestionFilter_Prediction) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));

  // For Segments::PREDICTION, suggestion filter is applied only when its
  // original key length is equal to the key of predicted node.  First test
  // unigram case.
  {
    Node *n = NewNode();
    n->key = "ふぃるたー";
    n->value = "フィルター";

    std::vector<const Node *> nodes;
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
              filter->FilterCandidate(*request_, "ふぃる", c, nodes, nodes,
                                      Segments::PREDICTION));
    // Test case where "フィルター" is predicted from key "ふぃるたー". Note the
    // difference from the case of SUGGESTION, now words in suggestion filter
    // are good if its key is equal to the original key.
    filter->Reset();
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, n->key, c, nodes, nodes,
                                      Segments::PREDICTION));
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

    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key).append(n2->key);
    c->value.assign(n1->value).append(n2->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is predicted from key "これはふ".
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, "これはふ", c, nodes, nodes,
                                      Segments::PREDICTION));
    filter->Reset();
    // Test case where "これはフィルター" is predicted from the same key.
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, c->key, c, nodes, nodes,
                                      Segments::PREDICTION));
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

    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key).append(n2->key).append(n3->key);
    c->value.assign(n1->value).append(n2->value).append(n3->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;

    // Test case where "これはフィルター" is predicted from key "これはふ".
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, "これはふ", c, nodes, nodes,
                                      Segments::PREDICTION));
    filter->Reset();
    // Test case where "これはフィルター" is predicted from the same key.
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, c->key, c, nodes, nodes,
                                      Segments::PREDICTION));
  }
}

TEST_F(CandidateFilterTest, ReverseConversion) {
  std::unique_ptr<CandidateFilter> filter(CreateCandidateFilter(true));
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
    Segment::Candidate *c = NewCandidate();
    c->key.assign(n1->key);
    c->value.assign(n1->value);
    c->content_key = c->key;
    c->content_value = c->value;
    c->cost = 1000;
    c->structure_cost = 2000;
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter->FilterCandidate(*request_, kHonHiragana, c, nodes, nodes,
                                      Segments::REVERSE_CONVERSION));
    // Duplicates should be removed.
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter->FilterCandidate(*request_, kHonHiragana, c, nodes, nodes,
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
              filter->FilterCandidate(*request_, " ", c, nodes, nodes,
                                      Segments::REVERSE_CONVERSION));
  }
}

INSTANTIATE_TEST_SUITE_P(TestForRequest, CandidateFilterTestWithParam,
                         ::testing::ValuesIn(kRequestTypes),
                         RequestParamToString);

}  // namespace
}  // namespace converter
}  // namespace mozc
