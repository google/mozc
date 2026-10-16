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

#include "converter/quality_regression_util.h"

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "converter/candidate.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace quality_regression {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

TEST(QualityRegressionUtilTest, ParseFromTSV) {
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(item.ParseFromTSV(
        "label\tかたがこる\t肩が(こ|凝)る\tConversion Match"));
    EXPECT_EQ(item.label, "label");
    EXPECT_EQ(item.key, "かたがこる");
    EXPECT_EQ(item.expected_value, "肩が(こ|凝)る");
    EXPECT_EQ(item.command, "Conversion Match");
    EXPECT_EQ(item.expected_rank, 0);
  }
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(
        item.ParseFromTSV("label\tかたがこる\t.*型.*\tConversion Not Match"));
    EXPECT_EQ(item.command, "Conversion Not Match");
    EXPECT_EQ(item.expected_rank, 0);
  }
}

TEST(QualityRegressionUtilTest, ConvertAndTestMatchSingleSegment) {
  auto mock = std::make_shared<MockConverter>();
  QualityRegressionUtil util(mock);

  Segments segments;
  Segment* seg = segments.add_segment();
  seg->set_key("かたがこる");
  // Candidate 0: "型が凝る"
  {
    converter::Candidate* cand = seg->add_candidate();
    cand->value = "型が凝る";
  }

  EXPECT_CALL(*mock, StartConversion(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  // Conversion Match: candidate 0 is "型が凝る", expected is "肩が(こ|凝)る" ->
  // fail
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(item.ParseFromTSV(
        "label\tかたがこる\t肩が(こ|凝)る\tConversion Match"));
    std::string actual_value;
    absl::StatusOr<bool> result = util.ConvertAndTest(item, &actual_value);
    ASSERT_OK(result);
    EXPECT_FALSE(*result);
    EXPECT_EQ(actual_value, "型が凝る");
  }

  // Conversion Match: candidate 0 is "型が凝る", expected is ".*型.*" ->
  // succeed
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(item.ParseFromTSV("label\tかたがこる\t.*型.*\tConversion Match"));
    std::string actual_value;
    absl::StatusOr<bool> result = util.ConvertAndTest(item, &actual_value);
    ASSERT_OK(result);
    EXPECT_TRUE(*result);
    EXPECT_EQ(actual_value, "型が凝る");
  }

  // Conversion Not Match: candidate 0 does not match ".*肩.*" -> succeed
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(
        item.ParseFromTSV("label\tかたがこる\t.*肩.*\tConversion Not Match"));
    std::string actual_value;
    absl::StatusOr<bool> result = util.ConvertAndTest(item, &actual_value);
    ASSERT_OK(result);
    EXPECT_TRUE(*result);
  }

  // Conversion Not Match: candidate 0 matches ".*型.*" -> fail
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(
        item.ParseFromTSV("label\tかたがこる\t.*型.*\tConversion Not Match"));
    std::string actual_value;
    absl::StatusOr<bool> result = util.ConvertAndTest(item, &actual_value);
    ASSERT_OK(result);
    EXPECT_FALSE(*result);
  }

  // Conversion Match with invalid regex pattern
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(item.ParseFromTSV("label\tかたがこる\t(\tConversion Match"));
    std::string actual_value;
    absl::StatusOr<bool> result = util.ConvertAndTest(item, &actual_value);
    EXPECT_FALSE(result.ok());
  }
}

TEST(QualityRegressionUtilTest, ConvertAndTestMatchMultiSegment) {
  auto mock = std::make_shared<MockConverter>();
  QualityRegressionUtil util(mock);

  Segments segments;
  // Segment 0
  Segment* seg0 = segments.add_segment();
  seg0->set_key("かたが");
  {
    converter::Candidate* cand = seg0->add_candidate();
    cand->value = "肩が";
  }
  {
    converter::Candidate* cand = seg0->add_candidate();
    cand->value = "型が";
  }

  // Segment 1
  Segment* seg1 = segments.add_segment();
  seg1->set_key("こる");
  {
    converter::Candidate* cand = seg1->add_candidate();
    cand->value = "こる";
  }
  {
    converter::Candidate* cand = seg1->add_candidate();
    cand->value = "凝る";
  }

  EXPECT_CALL(*mock, StartConversion(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  // "肩が" + "こる" matches "肩が(こ|凝)る" at rank 0
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(item.ParseFromTSV(
        "label\tかたがこる\t肩が(こ|凝)る\tConversion Match"));
    std::string actual_value;
    absl::StatusOr<bool> result = util.ConvertAndTest(item, &actual_value);
    ASSERT_OK(result);
    EXPECT_TRUE(*result);
    EXPECT_EQ(actual_value, "肩がこる");
  }

  // Substring match via ".*(こ|凝)る"
  {
    QualityRegressionUtil::TestItem item;
    ASSERT_OK(
        item.ParseFromTSV("label\tかたがこる\t.*(こ|凝)る\tConversion Match"));
    std::string actual_value;
    absl::StatusOr<bool> result = util.ConvertAndTest(item, &actual_value);
    ASSERT_OK(result);
    EXPECT_TRUE(*result);
  }
}

}  // namespace
}  // namespace quality_regression
}  // namespace mozc
