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

#include "rewriter/calculator_rewriter.h"

#include <memory>
#include <string>

#include "base/flags.h"
#include "base/logging.h"
#include "base/system_util.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/calculator/calculator_interface.h"
#include "rewriter/calculator/calculator_mock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace mozc {
namespace {
void AddCandidate(const std::string &key, const std::string &value,
                  Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
  candidate->content_key = key;
}

void AddSegment(const std::string &key, const std::string &value,
                Segments *segments) {
  Segment *segment = segments->push_back_segment();
  segment->set_key(key);
  AddCandidate(key, value, segment);
}

void SetSegment(const std::string &key, const std::string &value,
                Segments *segments) {
  segments->Clear();
  AddSegment(key, value, segments);
}

const char kCalculationDescription[] = "計算結果";

bool ContainsCalculatedResult(const Segment::Candidate &candidate) {
  return candidate.description.find(kCalculationDescription) !=
         std::string::npos;
}

// If the segment has a candidate which was inserted by CalculatorRewriter,
// then return its index. Otherwise return -1.
int GetIndexOfCalculatedCandidate(const Segments &segments) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(0).candidate(i);
    if (ContainsCalculatedResult(candidate)) {
      return i;
    }
  }
  return -1;
}
}  // namespace

class CalculatorRewriterTest : public ::testing::Test {
 protected:
  CalculatorRewriterTest() {
    convreq_.set_request(&request_);
    convreq_.set_config(&config_);
  }

  static bool InsertCandidate(const CalculatorRewriter &calculator_rewriter,
                              const std::string &value, size_t insert_pos,
                              Segment *segment) {
    return calculator_rewriter.InsertCandidate(value, insert_pos, segment);
  }

  CalculatorMock &calculator_mock() { return calculator_mock_; }

  CalculatorRewriter *BuildCalculatorRewriterWithConverterMock() {
    converter_mock_ = absl::make_unique<ConverterMock>();
    return new CalculatorRewriter(converter_mock_.get());
  }

  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(mozc::GetFlag(FLAGS_test_tmpdir));

    // use mock
    CalculatorFactory::SetCalculator(&calculator_mock_);
    request_.Clear();
    config::ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_calculator(true);
  }

  void TearDown() override {
    // Clear the mock test calculator
    CalculatorFactory::SetCalculator(nullptr);
  }

  ConversionRequest convreq_;
  commands::Request request_;
  config::Config config_;

 private:
  CalculatorMock calculator_mock_;
  std::unique_ptr<ConverterInterface> converter_mock_;
};

TEST_F(CalculatorRewriterTest, InsertCandidateTest) {
  std::unique_ptr<CalculatorRewriter> calculator_rewriter(
      BuildCalculatorRewriterWithConverterMock());

  {
    Segment segment;
    segment.set_key("key");
    // Insertion should be failed if segment has no candidate beforehand
    EXPECT_FALSE(InsertCandidate(*calculator_rewriter, "value", 0, &segment));
  }

  // Test insertion at each position of candidates list
  for (int i = 0; i <= 2; ++i) {
    Segment segment;
    segment.set_key("key");
    AddCandidate("key", "test", &segment);
    AddCandidate("key", "test2", &segment);

    EXPECT_TRUE(InsertCandidate(*calculator_rewriter, "value", i, &segment));
    const Segment::Candidate &candidate = segment.candidate(i);
    EXPECT_EQ(candidate.value, "value");
    EXPECT_EQ(candidate.content_value, "value");
    EXPECT_EQ(candidate.content_key, "key");
    EXPECT_NE(0, candidate.attributes & Segment::Candidate::NO_LEARNING);
    // Description should be "計算結果"
    EXPECT_EQ(candidate.description, kCalculationDescription);
  }
}

TEST_F(CalculatorRewriterTest, BasicTest) {
  // Pretend "key" is calculated to "value".
  calculator_mock().SetCalculatePair("key", "value", true);

  std::unique_ptr<CalculatorRewriter> calculator_rewriter(
      BuildCalculatorRewriterWithConverterMock());
  const int counter_at_first = calculator_mock().calculation_counter();

  Segments segments;
  SetSegment("test", "test", &segments);
  calculator_rewriter->Rewrite(convreq_, &segments);
  EXPECT_EQ(GetIndexOfCalculatedCandidate(segments), -1);
  EXPECT_EQ(calculator_mock().calculation_counter(), counter_at_first + 1);

  SetSegment("key", "key", &segments);
  calculator_rewriter->Rewrite(convreq_, &segments);
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

  // Since this test depends on the actual implementation of
  // Converter::ResizeSegments(), we cannot use converter mock here. However,
  // the test itself is independent of data.
  std::unique_ptr<EngineInterface> engine_(MockDataEngineFactory::Create());
  std::unique_ptr<CalculatorRewriter> calculator_rewriter(
      new CalculatorRewriter(engine_->GetConverter()));

  // Push back separated segments.
  Segments segments;
  AddSegment("1", "1", &segments);
  AddSegment("+", "+", &segments);
  AddSegment("1", "1", &segments);
  AddSegment("=", "=", &segments);

  calculator_rewriter->Rewrite(convreq_, &segments);
  EXPECT_EQ(segments.segments_size(), 1);  // merged

  int index = GetIndexOfCalculatedCandidate(segments);
  EXPECT_NE(index, -1);

  // Secondary result with expression (description: "1+1=2");
  EXPECT_TRUE(
      ContainsCalculatedResult(segments.segment(0).candidate(index + 1)));

  EXPECT_EQ("2", segments.segment(0).candidate(index).value);
  EXPECT_EQ("1+1=2", segments.segment(0).candidate(index + 1).value);
}

// CalculatorRewriter should convert an expression starting with '='.
TEST_F(CalculatorRewriterTest, ExpressionStartingWithEqualTest) {
  // Pretend "=1+1" is calculated to "2".
  calculator_mock().SetCalculatePair("=1+1", "2", true);

  std::unique_ptr<CalculatorRewriter> calculator_rewriter(
      BuildCalculatorRewriterWithConverterMock());
  const ConversionRequest request;

  Segments segments;
  SetSegment("=1+1", "=1+1", &segments);
  calculator_rewriter->Rewrite(request, &segments);
  int index = GetIndexOfCalculatedCandidate(segments);
  EXPECT_NE(-1, index);
  EXPECT_EQ("2", segments.segment(0).candidate(index).value);
  EXPECT_TRUE(
      ContainsCalculatedResult(segments.segment(0).candidate(index + 1)));
  // CalculatorRewriter should append the result to the side '=' exists.
  EXPECT_EQ("2=1+1", segments.segment(0).candidate(index + 1).value);
}

// Verify the description of calculator candidate.
TEST_F(CalculatorRewriterTest, DescriptionCheckTest) {
  const char kExpression[] = "５・（８／４）ー７％３＋６＾−１＊９＝";
  // Expected description
  const std::string description = kCalculationDescription;

  // Pretend kExpression is calculated to "3"
  calculator_mock().SetCalculatePair(kExpression, "3", true);

  std::unique_ptr<CalculatorRewriter> calculator_rewriter(
      BuildCalculatorRewriterWithConverterMock());

  Segments segments;
  AddSegment(kExpression, kExpression, &segments);

  calculator_rewriter->Rewrite(convreq_, &segments);
  const int index = GetIndexOfCalculatedCandidate(segments);

  EXPECT_EQ(segments.segment(0).candidate(index).description, description);
  EXPECT_TRUE(
      ContainsCalculatedResult(segments.segment(0).candidate(index + 1)));
}

TEST_F(CalculatorRewriterTest, ConfigTest) {
  calculator_mock().SetCalculatePair("1+1=", "2", true);

  // Since this test depends on the actual implementation of
  // Converter::ResizeSegments(), we cannot use converter mock here. However,
  // the test itself is independent of data.
  std::unique_ptr<EngineInterface> engine_(MockDataEngineFactory::Create());
  std::unique_ptr<CalculatorRewriter> calculator_rewriter(
      new CalculatorRewriter(engine_->GetConverter()));
  {
    Segments segments;
    AddSegment("1", "1", &segments);
    AddSegment("+", "+", &segments);
    AddSegment("1", "1", &segments);
    AddSegment("=", "=", &segments);
    config_.set_use_calculator(true);
    EXPECT_TRUE(calculator_rewriter->Rewrite(convreq_, &segments));
  }

  {
    Segments segments;
    AddSegment("1", "1", &segments);
    AddSegment("+", "+", &segments);
    AddSegment("1", "1", &segments);
    AddSegment("=", "=", &segments);
    config_.set_use_calculator(false);
    EXPECT_FALSE(calculator_rewriter->Rewrite(convreq_, &segments));
  }
}

TEST_F(CalculatorRewriterTest, MobileEnvironmentTest) {
  std::unique_ptr<EngineInterface> engine_(MockDataEngineFactory::Create());
  std::unique_ptr<CalculatorRewriter> rewriter(
      new CalculatorRewriter(engine_->GetConverter()));

  {
    request_.set_mixed_conversion(true);
    EXPECT_EQ(RewriterInterface::ALL, rewriter->capability(convreq_));
  }

  {
    request_.set_mixed_conversion(false);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter->capability(convreq_));
  }
}

TEST_F(CalculatorRewriterTest, EmptyKeyTest) {
  std::unique_ptr<EngineInterface> engine_(MockDataEngineFactory::Create());
  std::unique_ptr<CalculatorRewriter> calculator_rewriter(
      new CalculatorRewriter(engine_->GetConverter()));
  {
    Segments segments;
    AddSegment("", "1", &segments);
    config_.set_use_calculator(true);
    EXPECT_FALSE(calculator_rewriter->Rewrite(convreq_, &segments));
  }
}

}  // namespace mozc
