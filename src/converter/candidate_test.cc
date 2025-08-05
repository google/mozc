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

#include "converter/candidate.h"

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/number_util.h"
#include "converter/inner_segment.h"
#include "testing/gunit.h"

namespace mozc {
namespace converter {

TEST(CandidateTest, IsValid) {
  Candidate c;
  EXPECT_TRUE(c.IsValid());

  c.key = "key";
  c.value = "value";
  c.content_key = "content_key";
  c.content_value = "content_value";
  c.prefix = "prefix";
  c.suffix = "suffix";
  c.description = "description";
  c.usage_title = "usage_title";
  c.usage_description = "usage_description";
  c.cost = 1;
  c.wcost = 2;
  c.structure_cost = 3;
  c.lid = 4;
  c.rid = 5;
  c.attributes = 6;
  c.style = NumberUtil::NumberString::NUMBER_CIRCLED;
  c.command = Candidate::DISABLE_PRESENTATION_MODE;
  EXPECT_TRUE(c.IsValid());  // Empty inner_segment_boundary

  // Valid inner_segment_boundary.
  EXPECT_FALSE(
      BuildInnerSegmentBoundary({{1, 3, 1, 3}, {2, 2, 1, 1}}, c.key, c.value)
          .empty());

  // Invalid inner_segment_boundary.
  EXPECT_TRUE(BuildInnerSegmentBoundary(
                  {{1, 1, 1, 1}, {2, 2, 2, 2}, {3, 3, 1, 1}}, c.key, c.value)
                  .empty());
}

TEST(CandidateTest, functional_key) {
  Candidate candidate;

  candidate.key = "testfoobar";
  candidate.content_key = "test";
  EXPECT_EQ(candidate.functional_key(), "foobar");

  candidate.key = "testfoo";
  candidate.content_key = "test";
  EXPECT_EQ(candidate.functional_key(), "foo");

  // This is unexpected key/context_key.
  // This method doesn't check the prefix part.
  candidate.key = "abcdefg";
  candidate.content_key = "test";
  EXPECT_EQ(candidate.functional_key(), "efg");

  candidate.key = "test";
  candidate.content_key = "test";
  EXPECT_EQ(candidate.functional_key(), "");

  candidate.key = "test";
  candidate.content_key = "testfoobar";
  EXPECT_EQ(candidate.functional_key(), "");

  candidate.key = "";
  candidate.content_key = "";
  EXPECT_EQ(candidate.functional_key(), "");
}

TEST(CandidateTest, functional_value) {
  Candidate candidate;

  candidate.value = "testfoobar";
  candidate.content_value = "test";
  EXPECT_EQ(candidate.functional_value(), "foobar");

  candidate.value = "testfoo";
  candidate.content_value = "test";
  EXPECT_EQ(candidate.functional_value(), "foo");

  // This is unexpected value/context_value.
  // This method doesn't check the prefix part.
  candidate.value = "abcdefg";
  candidate.content_value = "test";
  EXPECT_EQ(candidate.functional_value(), "efg");

  candidate.value = "test";
  candidate.content_value = "test";
  EXPECT_EQ(candidate.functional_value(), "");

  candidate.value = "test";
  candidate.content_value = "testfoobar";
  EXPECT_EQ(candidate.functional_value(), "");

  candidate.value = "";
  candidate.content_value = "";
  EXPECT_EQ(candidate.functional_value(), "");
}

TEST(CandidateTest, InnerSegmentIterator) {
  {
    // For empty inner_segment_boundary, the initial state is done.
    Candidate candidate;
    candidate.key = "testfoobar";
    candidate.value = "redgreenblue";
    EXPECT_EQ(candidate.inner_segments().size(), 1);
    for (const auto &iter : candidate.inner_segments()) {
      EXPECT_EQ(iter.GetKey(), candidate.key);
      EXPECT_EQ(iter.GetValue(), candidate.value);
      EXPECT_EQ(iter.GetContentKey(), candidate.key);
      EXPECT_EQ(iter.GetContentValue(), candidate.value);
    }
  }
  {
    //           key: test | foobar
    //         value:  red | greenblue
    //   content key: test | foo
    // content value:  red | green
    Candidate candidate;
    candidate.key = "testfoobar";
    candidate.value = "redgreenblue";
    candidate.inner_segment_boundary = BuildInnerSegmentBoundary(
        {{4, 3, 4, 3}, {6, 9, 3, 5}}, candidate.key, candidate.value);
    std::vector<absl::string_view> keys, values, content_keys, content_values,
        functional_keys, functional_values;
    for (const auto &iter : candidate.inner_segments()) {
      keys.push_back(iter.GetKey());
      values.push_back(iter.GetValue());
      content_keys.push_back(iter.GetContentKey());
      content_values.push_back(iter.GetContentValue());
      functional_keys.push_back(iter.GetFunctionalKey());
      functional_values.push_back(iter.GetFunctionalValue());
    }

    ASSERT_EQ(keys.size(), 2);
    EXPECT_EQ(keys[0], "test");
    EXPECT_EQ(keys[1], "foobar");

    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], "red");
    EXPECT_EQ(values[1], "greenblue");

    ASSERT_EQ(content_keys.size(), 2);
    EXPECT_EQ(content_keys[0], "test");
    EXPECT_EQ(content_keys[1], "foo");

    ASSERT_EQ(content_values.size(), 2);
    EXPECT_EQ(content_values[0], "red");
    EXPECT_EQ(content_values[1], "green");

    ASSERT_EQ(functional_keys.size(), 2);
    EXPECT_EQ(functional_keys[0], "");
    EXPECT_EQ(functional_keys[1], "bar");

    ASSERT_EQ(functional_values.size(), 2);
    EXPECT_EQ(functional_values[0], "");
    EXPECT_EQ(functional_values[1], "blue");
  }
}
}  // namespace converter
}  // namespace mozc
