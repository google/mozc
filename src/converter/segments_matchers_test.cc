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

#include "converter/segments_matchers.h"

#include <string>
#include <vector>

#include "testing/gmock.h"
#include "testing/gunit.h"
#include "converter/segments.h"

namespace mozc {
namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::StrEq;

Segment::Candidate MakeCandidate(const std::string &key,
                                 const std::string &value) {
  Segment::Candidate cand;
  cand.key = key;
  cand.value = value;
  return cand;
}

Segment MakeSegment(const std::string &key,
                    const std::vector<std::string> &values) {
  Segment seg;
  seg.set_key(key);
  for (const std::string &val : values) {
    Segment::Candidate *cand = seg.add_candidate();
    cand->key = key;
    cand->value = val;
  }
  return seg;
}

TEST(SegmentsMatchersTest, EqualsCandidate) {
  const Segment::Candidate x = MakeCandidate("key_x", "value_x");
  const Segment::Candidate y = MakeCandidate("key_y", "value_y");
  EXPECT_THAT(x, EqualsCandidate(x));
  EXPECT_THAT(x, Not(EqualsCandidate(y)));

  // Partially different.
  const Segment::Candidate z = MakeCandidate("key_x", "value_z");
  EXPECT_THAT(x, Not(EqualsCandidate(z)));
}

TEST(SegmentsMatchersTest, EqualsSegment) {
  const Segment x = MakeSegment("key", {"value"});
  const Segment y = MakeSegment("key", {"value1", "value2"});
  EXPECT_THAT(x, EqualsSegment(x));
  EXPECT_THAT(x, Not(EqualsSegment(y)));
}

TEST(SegmentsMatchersTest, CandidatesAreArray) {
  const Segment x = MakeSegment("key", {"value1", "value2"});
  EXPECT_THAT(x, CandidatesAreArray({
                     Field(&Segment::Candidate::value, "value1"),
                     Field(&Segment::Candidate::key, "key"),
                 }));
}

TEST(SegmentsMatchersTest, CandidatesAreArrayWithCustomMatcher) {
  const Segment x = MakeSegment("key", {"value1", "value2"});
  constexpr auto KeyAndValueAre =
      [](const auto &key,
         const auto &value) -> Matcher<const Segment::Candidate *> {
    return Pointee(AllOf(Field(&Segment::Candidate::key, key),
                         Field(&Segment::Candidate::value, value)));
  };
  EXPECT_THAT(x, CandidatesAreArray({
                     KeyAndValueAre("key", "value1"),
                     KeyAndValueAre("key", "value2"),
                 }));
}

TEST(SegmentsMatchersTest, HasSingleCandidate) {
  const Segment x = MakeSegment("key", {"value"});
  const Segment y = MakeSegment("key", {"value1", "value2"});
  EXPECT_THAT(x,
              HasSingleCandidate(Field(&Segment::Candidate::value, "value")));
  EXPECT_THAT(
      y, Not(HasSingleCandidate(Field(&Segment::Candidate::value, "value1"))));
}

TEST(SegmentsMatchersTest, ContainsCandidate) {
  const Segment x = MakeSegment("key", {"value1", "value2", "value3"});
  constexpr auto ValueIs = [](const auto &value) {
    return Field(&Segment::Candidate::value, StrEq(value));
  };
  EXPECT_THAT(x, ContainsCandidate(ValueIs("value1")));
  EXPECT_THAT(x, ContainsCandidate(ValueIs("value2")));
  EXPECT_THAT(x, ContainsCandidate(ValueIs("value3")));
  EXPECT_THAT(x, Not(ContainsCandidate(ValueIs("value4"))));
}

TEST(SegmentsMatchersTest, EqualsSegments) {
  Segments x;
  *x.add_segment() = MakeSegment("key1", {"value1", "value2"});

  Segments y;
  *y.add_segment() = MakeSegment("key1", {"value1_1", "value1_2"});
  *y.add_segment() = MakeSegment("key2", {"value2_1", "value2_2", "value2_3"});

  EXPECT_THAT(x, EqualsSegments(x));
  EXPECT_THAT(x, Not(EqualsSegments(y)));
}

}  // namespace
}  // namespace mozc
