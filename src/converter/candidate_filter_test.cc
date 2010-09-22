// Copyright 2010, Google Inc.
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
#include "base/util.h"
#include "converter/segments.h"
#include "testing/base/public/gunit.h"

namespace mozc {

class CandidateFilterTest : public testing::Test {
 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {}

  Segment::Candidate *GetCandidate() {
    Segment::Candidate *c = new Segment::Candidate;
    c->Init();
    c->cost = 100;
    c->structure_cost = 100;
    return c;
  }
};

TEST_F(CandidateFilterTest, FilterTest) {
  CandidateFilter filter;
  scoped_ptr<Segment::Candidate> c1(GetCandidate());
  c1->value = "abc";
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c1.get()));

  scoped_ptr<Segment::Candidate> c2(GetCandidate());
  c2->value = "abc";
  // Same value candidate should be rejected.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c2.get()));

  scoped_ptr<Segment::Candidate> c3(GetCandidate());
  c3->structure_cost = INT_MAX;
  c3->value = "def";
  // High structure cost candidate should be rejected.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c3.get()));

  scoped_ptr<Segment::Candidate> c4(GetCandidate());
  // Checks if a canidate is active before appending many candidates.
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c4.get()));

  // Though CandidateFilter may change its limit, 1000 should
  // be always above the limit.
  vector<Segment::Candidate *> vec;
  for (int i = 0; i < 1000; ++i) {
    Segment::Candidate *cand = GetCandidate();
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", i);
    cand->value = string(buf);
    filter.FilterCandidate(cand);
    vec.push_back(cand);
  }
  // There will be no more candidates.
  EXPECT_EQ(CandidateFilter::STOP_ENUMERATION,
            filter.FilterCandidate(c4.get()));


  // Deletes them manually instead of STLDeleteContainerPointers.
  for (int i = 0; i < 1000; ++i) {
    delete vec[i];
  }
}

TEST_F(CandidateFilterTest, MayHaveMoreCandidates) {
  CandidateFilter filter;
  scoped_ptr<Segment::Candidate> c1(GetCandidate());
  c1->value = "abc";
  EXPECT_EQ(CandidateFilter::GOOD_CANDIDATE,
            filter.FilterCandidate(c1.get()));

  scoped_ptr<Segment::Candidate> c2(GetCandidate());
  c2->value = "abc";
  // Though same value candidate is rejected, enumeration should continue.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c2.get()));

  scoped_ptr<Segment::Candidate> c3(GetCandidate());
  c3->structure_cost = INT_MAX;
  c3->value = "def";
  // High structure cost should not Stop enumeration.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c3.get()));

  scoped_ptr<Segment::Candidate> c4(GetCandidate());
  c4->cost = INT_MAX;
  c4->structure_cost = INT_MAX;
  c4->value = "ghi";
  // High cost candidate should be rejected.
  EXPECT_EQ(CandidateFilter::BAD_CANDIDATE,
            filter.FilterCandidate(c4.get()));

  // Insert many valid candidates
  for (int i = 0; i < 50; ++i) {
    scoped_ptr<Segment::Candidate> tmp(GetCandidate());
    tmp->value = Util::SimpleItoa(i) + "test";
    filter.FilterCandidate(tmp.get());
  }

  scoped_ptr<Segment::Candidate> c5(GetCandidate());
  c5->cost = INT_MAX;
  c5->structure_cost = INT_MAX;
  c5->value = "ghi";

  // finally, it returns STOP_ENUMERATION, because
  // filter has seen more than 50 good candidates.
  c5->value = "ghi2";
  EXPECT_EQ(CandidateFilter::STOP_ENUMERATION,
            filter.FilterCandidate(c5.get()));
}
}  // namespace mozc
