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

#ifndef MOZC_CONVERTER_SEGMENTS_MATCHERS_H_
#define MOZC_CONVERTER_SEGMENTS_MATCHERS_H_

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {

// Checks if a candidate exactly matches the given candidate except for `log`
// field. Note: this is more useful than defining operator==() in testing as it
// can display which field is different.
//
// Usage:
//   converter::Candidate actual_candidate = ...;
//   EXPECT_THAT(actual_candidate, EqualsCandidate(expected_candidate));
MATCHER_P(EqualsCandidate, candidate, "") {
#define COMPARE_FIELD(field)                                         \
  if (arg.field != candidate.field) {                                \
    *result_listener << "where the field '" #field "' is different"; \
    return false;                                                    \
  }
  COMPARE_FIELD(key);
  COMPARE_FIELD(value);
  COMPARE_FIELD(content_key);
  COMPARE_FIELD(content_value);
  COMPARE_FIELD(consumed_key_size);
  COMPARE_FIELD(prefix);
  COMPARE_FIELD(suffix);
  COMPARE_FIELD(description);
  COMPARE_FIELD(a11y_description);
  COMPARE_FIELD(usage_id);
  COMPARE_FIELD(usage_title);
  COMPARE_FIELD(usage_description);
  COMPARE_FIELD(cost);
  COMPARE_FIELD(wcost);
  COMPARE_FIELD(structure_cost);
  COMPARE_FIELD(lid);
  COMPARE_FIELD(rid);
  COMPARE_FIELD(attributes);
  COMPARE_FIELD(category);
  COMPARE_FIELD(style);
  COMPARE_FIELD(command);
  COMPARE_FIELD(inner_segment_boundary);
#undef COMPARE_FIELD
  return true;
}

// Checks if a segment exactly matches the given segment except for the
// following two fields:
//   * removed_candidates_for_debug_
//   * pool_
// Note: this is more useful than defining operator==() in testing as it can
// display which field is different.
//
// Usage:
//   Segment actual_segment = ...;
//   EXPECT_THAT(actual_segment, EqualsSegment(expected_segment));
MATCHER_P(EqualsSegment, segment, "") {
#define COMPARE_PROPERTY(property)                                         \
  if (arg.property() != segment.property()) {                              \
    *result_listener << "where the property '" #property "' is different"; \
    return false;                                                          \
  }
  COMPARE_PROPERTY(segment_type);
  COMPARE_PROPERTY(key);
  COMPARE_PROPERTY(key_len);
#undef COMPARE_PROPERTY

  // Compare candidates.
  const size_t common_candidate_size =
      std::min(arg.candidates_size(), segment.candidates_size());
  for (int i = 0; i < common_candidate_size; ++i) {
    if (!ExplainMatchResult(EqualsCandidate(segment.candidate(i)),
                            arg.candidate(i), result_listener)) {
      *result_listener << " for the " << i << "-th candidate";
      return false;
    }
  }
  if (arg.candidates_size() != segment.candidates_size()) {
    *result_listener
        << "where the actual has more or less candidates than the expected: "
        << arg.candidates_size() << " vs " << segment.candidates_size();
    return false;
  }

  // Compare meta candidates.
  const size_t common_meta_candidate_size =
      std::min(arg.meta_candidates_size(), segment.meta_candidates_size());
  for (size_t i = 0; i < common_meta_candidate_size; ++i) {
    if (!ExplainMatchResult(EqualsCandidate(segment.meta_candidate(i)),
                            arg.meta_candidate(i), result_listener)) {
      *result_listener << " for the " << i << "-th meta candidate";
      return false;
    }
  }
  if (arg.meta_candidates_size() != segment.meta_candidates_size()) {
    *result_listener << "where the actual has more or less meta candidates "
                        "than the expected: "
                     << arg.meta_candidates_size() << " vs "
                     << segment.meta_candidates_size();
    return false;
  }

  return true;
}

namespace internal {

// A helper function to print a container of candidate matchers.
template <typename Container>
std::string PrintCandidateMatcherArray(const Container& matchers) {
  std::string str = "candidates are:\n";
  int index = 0;
  for (const auto& matcher : matchers) {
    str += absl::StrFormat("  cand %d %s\n", index++,
                           ::testing::PrintToString(matcher));
  }
  return str;
}

}  // namespace internal

// A matcher for Segment to test if its candidates are equal to the given
// matchers in order. Use CandidatesAreArray() helpers to deduce matcher types.
MATCHER_P(CandidatesAreArrayMatcherImpl, matcher_array,
          ::mozc::internal::PrintCandidateMatcherArray(matcher_array)) {
  return ::testing::Matches(::testing::ElementsAreArray(matcher_array))(
      arg.candidates());
}

// Checks if a segment contains a candidate list that matches the given matchers
// in the given order. Example usage:
//
// Segment segment = ...;
// EXPECT_THAT(segment, CandidatesAreArray({
//     Field(&converter::Candidate::value, "value"),
//     Field(&converter::Candidate::key, "key"),
// });
//
// The above code checks if the first candidate's value is "value" and the
// second candidate's key is "key".
//
// Remark: Each candidate matcher should test against the pointer type
// (converter::Candidate *). Note: Field() from gMock is applicable to pointer
// types too. See SegmentsMatchersTest.CandidatesAreArrayWithCustomMatcher for
// example.
template <typename T>
::testing::Matcher<Segment> CandidatesAreArray(
    std::initializer_list<T> matchers) {
  return CandidatesAreArrayMatcherImpl(matchers);
}

template <typename Container>
::testing::Matcher<Segment> CandidatesAreArray(const Container& matchers) {
  return CandidatesAreArrayMatcherImpl(matchers);
}

// Checks if a segment contains one and only one candidate that matches the
// given matcher.
template <typename T>
::testing::Matcher<Segment> HasSingleCandidate(const T& matcher) {
  using CandidateMatcherType = ::testing::Matcher<const converter::Candidate*>;
  return CandidatesAreArray(std::vector<CandidateMatcherType>{matcher});
}

// Checks if a segment contains a candidate that matches the
// given matcher.
MATCHER_P(ContainsCandidate, matcher, "") {
  return ::testing::Matches(::testing::Contains(matcher))(arg.candidates());
}

// Checks if a segments exactly matches the given segments except for the
// following three fields:
//   * pool_
//   * revert_entries_
// Note: this is more useful than defining operator==() in testing as it can
// display which field is different.
//
// Usage:
//   Segments actual_segments = ...;
//   EXPECT_THAT(actual_segments, EqualsSegments(expected_segments));
MATCHER_P(EqualsSegments, segments, "") {
#define COMPARE_PROPERTY(property)                                         \
  if (arg.property() != segments.property()) {                             \
    *result_listener << "where the property '" #property "' is different"; \
    return false;                                                          \
  }
  COMPARE_PROPERTY(max_history_segments_size);
  COMPARE_PROPERTY(resized);
#undef COMPARE_PROPERTY

  const size_t common_segments_size =
      std::min(arg.segments_size(), segments.segments_size());
  for (size_t i = 0; i < common_segments_size; ++i) {
    if (!ExplainMatchResult(EqualsSegment(segments.segment(i)), arg.segment(i),
                            result_listener)) {
      *result_listener << " of the " << i << "-th segment";
      return false;
    }
  }
  if (arg.segments_size() != segments.segments_size()) {
    *result_listener
        << "where the actual has more or less segments than the expected: "
        << arg.segments_size() << " vs " << segments.segments_size();
    return false;
  }

  return true;
}

}  // namespace mozc

#endif  // MOZC_CONVERTER_SEGMENTS_MATCHERS_H_
