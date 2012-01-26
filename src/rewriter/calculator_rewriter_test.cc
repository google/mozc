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

#include <string>

#include "base/util.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/segments.h"
#include "rewriter/calculator_rewriter.h"
#include "rewriter/calculator/calculator_interface.h"
#include "rewriter/calculator/calculator_mock.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {
void AddCandidate(const string &key, const string &value, Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
  candidate->content_key = key;
}

void AddSegment(const string &key, const string &value, Segments *segments) {
  Segment *segment = segments->push_back_segment();
  segment->set_key(key);
  AddCandidate(key, value, segment);
}

void SetSegment(const string &key, const string &value, Segments *segments) {
  segments->Clear();
  AddSegment(key, value, segments);
}

// "の計算結果"
const char kPartOfCalculationDescription[] =
    "\xE3\x81\xAE\xE8\xA8\x88\xE7\xAE\x97\xE7\xB5\x90\xE6\x9E\x9C";

bool ContainsCalculatedResult(const Segment::Candidate &candidate) {
  return candidate.description.find(kPartOfCalculationDescription) !=
      string::npos;
}

// If the segment has a candidate which was inserted by CalculatorRewriter,
// then return its index. Otherwise return -1.
int GetIndexOfCalculatedCandidate(const Segments &segments) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(0).candidate(i);
    // Description includes "の計算結果"
    if (ContainsCalculatedResult(candidate)) {
      return i;
    }
  }
  return -1;
}
}  // anonymous namespace

class CalculatorRewriterTest : public testing::Test {
 protected:
  static bool InsertCandidate(const CalculatorRewriter &calculator_rewriter,
                              const string &value,
                              size_t insert_pos,
                              Segment *segment) {
    return calculator_rewriter.InsertCandidate(value, insert_pos, segment);
  }

  CalculatorMock &calculator_mock() {
    return calculator_mock_;
  }

  void SetCalculatePair(const string &key,
                        const string &value,
                        bool return_value) {
    calculator_mock_.SetCalculatePair(key, value, return_value);
  }

  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

    // use mock
    CalculatorFactory::SetCalculator(&calculator_mock_);
  }

  virtual void TearDown() {
    // Clear the mock test calculator
    CalculatorFactory::SetCalculator(NULL);
  }

 private:
  CalculatorMock calculator_mock_;
};

TEST_F(CalculatorRewriterTest, InsertCandidateTest) {
  CalculatorRewriter calculator_rewriter;

  {
    Segment segment;
    segment.set_key("key");
    // Insertion should be failed if segment has no candidate beforehand
    EXPECT_FALSE(InsertCandidate(calculator_rewriter, "value", 0, &segment));
  }

  // Test insertion at each position of candidates list
  for (int i = 0; i <= 2; ++i) {
    Segment segment;
    segment.set_key("key");
    AddCandidate("key", "test", &segment);
    AddCandidate("key", "test2", &segment);

    EXPECT_TRUE(InsertCandidate(calculator_rewriter, "value", i, &segment));
    const Segment::Candidate &candidate = segment.candidate(i);
    EXPECT_EQ(candidate.value, "value");
    EXPECT_EQ(candidate.content_value, "value");
    EXPECT_EQ(candidate.content_key, "key");
    EXPECT_TRUE(candidate.attributes & Segment::Candidate::NO_LEARNING);
    // Description should be "key の計算結果"
    EXPECT_EQ(candidate.description, "key " +
              string(kPartOfCalculationDescription));
  }
}

TEST_F(CalculatorRewriterTest, BasicTest) {
  // Pretend "key" is calculated to "value".
  calculator_mock().SetCalculatePair("key", "value", true);

  CalculatorRewriter calculator_rewriter;
  const int counter_at_first = calculator_mock().calculation_counter();

  Segments segments;
  SetSegment("test", "test", &segments);
  calculator_rewriter.Rewrite(&segments);
  EXPECT_EQ(GetIndexOfCalculatedCandidate(segments), -1);
  EXPECT_EQ(calculator_mock().calculation_counter(), counter_at_first + 1);

  SetSegment("key", "key", &segments);
  calculator_rewriter.Rewrite(&segments);
  int index = GetIndexOfCalculatedCandidate(segments);
  EXPECT_NE(index, -1);
  EXPECT_EQ(segments.segment(0).candidate(index).value, "value");
  EXPECT_EQ(calculator_mock().calculation_counter(), counter_at_first + 2);
}

// CalculatorRewriter should convert an expression which is separated to
// multiple conversion segments. This test verifies it.
TEST_F(CalculatorRewriterTest, SeparatedSegmentsTest) {
  // Pretend "1+1=" is calculated to "2".
  calculator_mock().SetCalculatePair("1+1=", "2", true);

  CalculatorRewriter calculator_rewriter;

  // Push back separated segments.
  Segments segments;
  AddSegment("1", "1", &segments);
  AddSegment("+", "+", &segments);
  AddSegment("1", "1", &segments);
  AddSegment("=", "=", &segments);

  calculator_rewriter.Rewrite(&segments);
  EXPECT_EQ(segments.segments_size(), 1);  // merged

  int index = GetIndexOfCalculatedCandidate(segments);
  EXPECT_NE(index, -1);

  // Secondary result with expression (description: "1+1=2");
  EXPECT_TRUE(ContainsCalculatedResult(
      segments.segment(0).candidate(index + 1)));

  EXPECT_EQ("2", segments.segment(0).candidate(index).value);
  EXPECT_EQ("1+1=2", segments.segment(0).candidate(index + 1).value);
}

// Verify the description of calculator candidate.
TEST_F(CalculatorRewriterTest, DescriptionCheckTest) {
  // "５・（８／４）ー７％３＋６＾−１＊９＝"
  const char kExpression[] =
      "\xEF\xBC\x95\xE3\x83\xBB\xEF\xBC\x88\xEF\xBC\x98\xEF\xBC\x8F"
      "\xEF\xBC\x94\xEF\xBC\x89\xE3\x83\xBC\xEF\xBC\x97\xEF\xBC\x85"
      "\xEF\xBC\x93\xEF\xBC\x8B\xEF\xBC\x96\xEF\xBC\xBE\xE2\x88\x92"
      "\xEF\xBC\x91\xEF\xBC\x8A\xEF\xBC\x99\xEF\xBC\x9D";
  // Expected description
  const string description =
      "5/(8/4)-7%3+6^-1*9= " + string(kPartOfCalculationDescription);

  // Pretend kExpression is calculated to "3"
  calculator_mock().SetCalculatePair(kExpression, "3", true);

  CalculatorRewriter calculator_rewriter;

  Segments segments;
  AddSegment(kExpression, kExpression, &segments);

  calculator_rewriter.Rewrite(&segments);
  const int index = GetIndexOfCalculatedCandidate(segments);

  EXPECT_EQ(segments.segment(0).candidate(index).description, description);
  EXPECT_TRUE(ContainsCalculatedResult(
      segments.segment(0).candidate(index + 1)));
}

TEST_F(CalculatorRewriterTest, ConfigTest) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);

  calculator_mock().SetCalculatePair("1+1=", "2", true);
  CalculatorRewriter calculator_rewriter;

  {
    Segments segments;
    AddSegment("1", "1", &segments);
    AddSegment("+", "+", &segments);
    AddSegment("1", "1", &segments);
    AddSegment("=", "=", &segments);
    config.set_use_calculator(true);
    config::ConfigHandler::SetConfig(config);
    EXPECT_TRUE(calculator_rewriter.Rewrite(&segments));
  }

  {
    Segments segments;
    AddSegment("1", "1", &segments);
    AddSegment("+", "+", &segments);
    AddSegment("1", "1", &segments);
    AddSegment("=", "=", &segments);
    config.set_use_calculator(false);
    config::ConfigHandler::SetConfig(config);
    EXPECT_FALSE(calculator_rewriter.Rewrite(&segments));
  }
}
}  // namespace mozc
