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

#include "converter/candidate_filter.h"

#include <climits>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/freelist.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "testing/base/public/gunit.h"

namespace mozc {

class CandidateFilterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    candidate_freelist_.reset(new FreeList<Segment::Candidate>(1024));
    node_freelist_.reset(new FreeList<Node>(1024));
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
    n1->lid = POSMatcher::GetUnknownId();
    n1->rid = POSMatcher::GetUnknownId();
    nodes->push_back(n1);

    Node *n2 = NewNode();
    // "てすと"
    n2->value = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
    n2->lid = POSMatcher::GetFunctionalId();
    n2->rid = POSMatcher::GetFunctionalId();
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

 private:
  scoped_ptr<FreeList<Segment::Candidate> > candidate_freelist_;
  scoped_ptr<FreeList<Node> > node_freelist_;
};

TEST_F(CandidateFilterTest, FilterTest) {
  CandidateFilter filter;
  vector<const Node *> n;

  GetDefaultNodes(&n);
  Segment::Candidate *c1 = NewCandidate();
  c1->lid = 1;
  c1->rid = 1;
  c1->value = "abc";
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c1, n));

  Segment::Candidate *c2 = NewCandidate();
  c2->value = "abc";
  // Same value candidate should be rejected.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c2, n));

  Segment::Candidate *c3 = NewCandidate();
  c3->structure_cost = INT_MAX;
  c3->value = "def";
  // High structure cost candidate should be rejected.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c3, n));

  Segment::Candidate *c4 = NewCandidate();
  // Checks if a canidate is active before appending many candidates.
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c4, n));

  // Don't filter if lid/rid the same as the top candidate.
  Segment::Candidate *c5 = NewCandidate();
  c5->value = "foo";
  c5->lid = 1;
  c5->rid = 1;
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c5, n));


  // Though CandidateFilter may change its limit, 1000 should
  // be always above the limit.
  for (int i = 0; i < 1000; ++i) {
    Segment::Candidate *cand = NewCandidate();
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", i);
    cand->value = string(buf);
    filter.FilterCandidate(cand, n);
  }
  // There will be no more candidates.
  EXPECT_EQ(CandidateFilter::STOP_ENUMERATION,
            filter.FilterCandidate(c4, n));
}

TEST_F(CandidateFilterTest, KatakanaT13N) {
  {
    CandidateFilter filter;
    vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[0] is KatakanaT13N
    Segment::Candidate *c = NewCandidate();
    c->value = "abc";
    Node *n = NewNode();
    n->lid = POSMatcher::GetUnknownId();
    n->rid = POSMatcher::GetUnknownId();
    n->value = "abc";
    nodes[0] = n;
    EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
              filter.FilterCandidate(c, nodes));
  }

  {
    CandidateFilter filter;
    vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[1] is KatakanaT13N
    Segment::Candidate *c = NewCandidate();
    c->value = "abc";
    Node *n = NewNode();
    n->lid = POSMatcher::GetFunctionalId();
    n->rid = POSMatcher::GetFunctionalId();
    n->value = "abc";
    nodes[1] = n;
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter.FilterCandidate(c, nodes));
  }

  {
    CandidateFilter filter;
    vector<const Node *> nodes;
    GetDefaultNodes(&nodes);
    // nodes[1] is not a functional word
    Segment::Candidate *c = NewCandidate();
    c->value = "abc";
    Node *n1 = NewNode();
    n1->lid = POSMatcher::GetUnknownId();
    n1->rid = POSMatcher::GetUnknownId();
    n1->value = "abc";
    nodes[0] = n1;
    Node *n2 = NewNode();
    n2->lid = POSMatcher::GetUnknownId();
    n2->rid = POSMatcher::GetUnknownId();
    // "てすと";
    n2->value = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
    nodes[1] = n2;
    EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
              filter.FilterCandidate(c, nodes));
  }
}

TEST_F(CandidateFilterTest, IsolatedWord) {
  CandidateFilter filter;
  vector<const Node *> nodes;
  Segment::Candidate *c = NewCandidate();
  c->value = "abc";

  Node *node = NewNode();
  nodes.push_back(node);
  node->prev = NewNode();
  node->next = NewNode();
  node->lid = POSMatcher::GetIsolatedWordId();
  node->rid = POSMatcher::GetIsolatedWordId();
  node->key = "test";
  node->value = "test";

  node->prev->node_type = Node::NOR_NODE;
  node->next->node_type = Node::EOS_NODE;
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c, nodes));

  node->prev->node_type = Node::BOS_NODE;
  node->next->node_type = Node::NOR_NODE;
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c, nodes));

  node->prev->node_type = Node::NOR_NODE;
  node->next->node_type = Node::NOR_NODE;
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c, nodes));

  node->prev->node_type = Node::BOS_NODE;
  node->next->node_type = Node::EOS_NODE;
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c, nodes));
}

TEST_F(CandidateFilterTest, MayHaveMoreCandidates) {
  CandidateFilter filter;
  vector<const Node *> n;
  GetDefaultNodes(&n);

  Segment::Candidate *c1 = NewCandidate();
  c1->value = "abc";
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c1, n));

  Segment::Candidate *c2 = NewCandidate();
  c2->value = "abc";
  // Though same value candidate is rejected, enumeration should continue.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c2, n));

  Segment::Candidate *c3 = NewCandidate();
  c3->structure_cost = INT_MAX;
  c3->value = "def";
  // High structure cost should not Stop enumeration.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c3, n));

  Segment::Candidate *c4 = NewCandidate();
  c4->cost = INT_MAX;
  c4->structure_cost = INT_MAX;
  c4->value = "ghi";
  // High cost candidate should be rejected.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c4, n));

  // Insert many valid candidates
  for (int i = 0; i < 50; ++i) {
    Segment::Candidate *tmp = NewCandidate();
    tmp->value = Util::SimpleItoa(i) + "test";
    filter.FilterCandidate(tmp, n);
  }

  Segment::Candidate *c5 = NewCandidate();
  c5->cost = INT_MAX;
  c5->structure_cost = INT_MAX;
  c5->value = "ghi";

  // finally, it returns STOP_ENUMERATION, because
  // filter has seen more than 50 good candidates.
  c5->value = "ghi2";
  EXPECT_EQ(CandidateFilter::STOP_ENUMERATION,
            filter.FilterCandidate(c5, n));
}

TEST_F(CandidateFilterTest, Regression3437022) {
  CandidateFilter filter;
  vector<const Node *> n;
  GetDefaultNodes(&n);

  Segment::Candidate *c1 = NewCandidate();
  c1->key = "test_key";
  c1->value = "test_value";

  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c1, n));

  SuppressionDictionary *dic
      = SuppressionDictionary::GetSuppressionDictionary();
  dic->Lock();
  dic->AddEntry("test_key", "test_value");
  dic->UnLock();

  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c1, n));

  c1->key = "test_key_suffix";
  c1->value = "test_value_suffix";
  c1->content_key = "test_key";
  c1->content_value = "test_value";

  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c1, n));

  dic->Lock();
  dic->Clear();
  dic->UnLock();

  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c1, n));
}

TEST_F(CandidateFilterTest, FilterRealtimeConversionTest) {
  CandidateFilter filter;
  vector<const Node *> n;

  n.clear();
  Node *n1 = NewNode();

  n1->key = "PC";
  n1->value = "PC";
  n1->lid = POSMatcher::GetUnknownId();
  n1->rid = POSMatcher::GetUnknownId();
  n.push_back(n1);

  Node *n2 = NewNode();
  // "てすと"
  n2->value = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
  n2->lid = POSMatcher::GetUnknownId();
  n2->rid = POSMatcher::GetUnknownId();
  n.push_back(n2);

  Segment::Candidate *c1 = NewCandidate();
  c1->attributes |= Segment::Candidate::REALTIME_CONVERSION;
  // "PCテスト"
  c1->value = "PC\xE3\x83\x86\xE3\x82\xB9\xE3\x83\x88";
  // Don't filter a candidate because it starts with alphabets and
  // is followed by a non-functional word.
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c1, n));
}
}  // namespace mozc
