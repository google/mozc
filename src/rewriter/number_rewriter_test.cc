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

#include "rewriter/number_rewriter.h"

#include <array>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "base/number_util.h"
#include "base/strings/assign.h"
#include "config/character_form_manager.h"
#include "converter/segments.h"
#include "converter/segments_matchers.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

using ::mozc::dictionary::PosMatcher;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::WithParamInterface;

constexpr absl::string_view kKanjiDescription = "漢数字";
constexpr absl::string_view kArabicDescription = "数字";
constexpr absl::string_view kOldKanjiDescription = "大字";
constexpr absl::string_view kMaruNumberDescription = "丸数字";
constexpr absl::string_view kRomanCapitalDescription = "ローマ数字(大文字)";
constexpr absl::string_view kRomanNoCapitalDescription = "ローマ数字(小文字)";
constexpr absl::string_view kSuperscriptDescription = "上付き文字";
constexpr absl::string_view kSubscriptDescription = "下付き文字";

Matcher<const Segment::Candidate *> ValueIs(absl::string_view value) {
  return Field(&Segment::Candidate::value, value);
}

bool FindValue(const Segment &segment, const absl::string_view value) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == value) {
      return true;
    }
  }
  return false;
}

Segment *SetupSegments(const PosMatcher &pos_matcher,
                       const absl::string_view candidate_value,
                       Segments *segments) {
  segments->Clear();
  Segment *segment = segments->push_back_segment();
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->lid = pos_matcher.GetNumberId();
  candidate->rid = pos_matcher.GetNumberId();
  strings::Assign(candidate->value, candidate_value);
  strings::Assign(candidate->content_value, candidate_value);
  return segment;
}

bool HasDescription(const Segment &segment,
                    const absl::string_view description) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).description == description) {
      return true;
    }
  }
  return false;
}

// Find candidate id
bool FindCandidateId(const Segment &segment, const absl::string_view value,
                     int *id) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == value) {
      *id = i;
      return true;
    }
  }
  return false;
}
}  // namespace

class NumberRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    config::CharacterFormManager::GetCharacterFormManager()->ClearHistory();
    pos_matcher_.Set(mock_data_manager_.GetPosMatcherData());
  }

  void TearDown() override {
    config::CharacterFormManager::GetCharacterFormManager()->ClearHistory();
  }

  std::unique_ptr<NumberRewriter> CreateNumberRewriter() {
    return std::make_unique<NumberRewriter>(mock_data_manager_);
  }

  const testing::MockDataManager mock_data_manager_;
  PosMatcher pos_matcher_;
  const ConversionRequest default_request_;
};

namespace {
struct ExpectResult {
  absl::string_view value;
  absl::string_view content_value;
  absl::string_view description;
};
}  // namespace

TEST_F(NumberRewriterTest, BasicTest) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetNumberId();
  candidate->value = "012";
  candidate->content_value = "012";
  candidate->key = "012";
  candidate->content_key = "012";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  constexpr ExpectResult kExpectResults[] = {
      {"012", "012", ""},
      {"〇一二", "〇一二", kKanjiDescription},
      {"０１２", "０１２", kArabicDescription},
      {"十二", "十二", kKanjiDescription},
      {"壱拾弐", "壱拾弐", kOldKanjiDescription},
      {"Ⅻ", "Ⅻ", kRomanCapitalDescription},
      {"ⅻ", "ⅻ", kRomanNoCapitalDescription},
      {"⑫", "⑫", kMaruNumberDescription},
      {"0xc", "0xc", "16進数"},
      {"014", "014", "8進数"},
      {"0b1100", "0b1100", "2進数"},
  };

  constexpr size_t kExpectResultSize = std::size(kExpectResults);
  EXPECT_EQ(seg->candidates_size(), kExpectResultSize);

  for (size_t i = 0; i < kExpectResultSize; ++i) {
    SCOPED_TRACE(absl::StrFormat("i = %d", i));
    EXPECT_EQ(seg->candidate(i).value, kExpectResults[i].value);
    EXPECT_EQ(seg->candidate(i).content_value, kExpectResults[i].content_value);
    EXPECT_EQ(seg->candidate(i).description, kExpectResults[i].description);
  }
  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, RequestType) {
  class TestData {
   public:
    ConversionRequest::RequestType request_type_;
    int expected_candidate_number_;
    TestData(ConversionRequest::RequestType request_type, int expected_number)
        : request_type_(request_type),
          expected_candidate_number_(expected_number) {}
  };
  TestData test_data_list[] = {
      TestData(ConversionRequest::CONVERSION, 11),  // 11 comes from BasicTest
      TestData(ConversionRequest::REVERSE_CONVERSION, 8),
      TestData(ConversionRequest::PREDICTION, 8),
      TestData(ConversionRequest::SUGGESTION, 8),
  };

  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  for (size_t i = 0; i < std::size(test_data_list); ++i) {
    TestData &test_data = test_data_list[i];
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->value = "012";
    candidate->content_value = "012";
    const ConversionRequest request =
        ConversionRequestBuilder().SetRequestType(test_data.request_type_)
            .Build();
    EXPECT_TRUE(number_rewriter->Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), test_data.expected_candidate_number_);
  }
}

TEST_F(NumberRewriterTest, BasicTestWithSuffix) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetNumberId();
  candidate->value = "012が";
  candidate->content_value = "012";
  candidate->key = "012が";
  candidate->content_key = "012";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  constexpr ExpectResult kExpectResults[] = {
      {"012が", "012", ""},
      {"〇一二が", "〇一二", kKanjiDescription},
      {"０１２が", "０１２", kArabicDescription},
      {"十二が", "十二", kKanjiDescription},
      {"壱拾弐が", "壱拾弐", kOldKanjiDescription},
      {"Ⅻが", "Ⅻ", kRomanCapitalDescription},
      {"ⅻが", "ⅻ", kRomanNoCapitalDescription},
      {"⑫が", "⑫", kMaruNumberDescription},
      {"0xcが", "0xc", "16進数"},
      {"014が", "014", "8進数"},
      {"0b1100が", "0b1100", "2進数"},
  };

  constexpr size_t kExpectResultSize = std::size(kExpectResults);
  EXPECT_EQ(seg->candidates_size(), kExpectResultSize);

  for (size_t i = 0; i < kExpectResultSize; ++i) {
    SCOPED_TRACE(absl::StrFormat("i = %d", i));
    EXPECT_EQ(seg->candidate(i).value, kExpectResults[i].value);
    EXPECT_EQ(seg->candidate(i).content_value, kExpectResults[i].content_value);
    EXPECT_EQ(seg->candidate(i).description, kExpectResults[i].description);
  }

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, BasicTestWithNumberSuffix) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetCounterSuffixWordId();
  candidate->value = "十五個";
  candidate->content_value = "十五個";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(seg->candidates_size(), 2);

  EXPECT_EQ(seg->candidate(0).value, "十五個");
  EXPECT_EQ(seg->candidate(0).content_value, "十五個");
  EXPECT_EQ(seg->candidate(0).description, "");

  EXPECT_EQ(seg->candidate(1).value, "15個");
  EXPECT_EQ(seg->candidate(1).content_value, "15個");
  EXPECT_EQ(seg->candidate(1).description, "");
  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, TestWithMultipleNumberSuffix) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetCounterSuffixWordId();
  candidate->value = "十五回";
  candidate->content_value = "十五回";
  candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetCounterSuffixWordId();
  candidate->value = "十五階";
  candidate->content_value = "十五階";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(seg->candidates_size(), 4);

  EXPECT_EQ(seg->candidate(0).value, "十五回");
  EXPECT_EQ(seg->candidate(0).content_value, "十五回");
  EXPECT_EQ(seg->candidate(0).description, "");

  EXPECT_EQ(seg->candidate(1).value, "15回");
  EXPECT_EQ(seg->candidate(1).content_value, "15回");
  EXPECT_EQ(seg->candidate(1).description, "");

  EXPECT_EQ(seg->candidate(2).value, "十五階");
  EXPECT_EQ(seg->candidate(2).content_value, "十五階");
  EXPECT_EQ(seg->candidate(2).description, "");

  EXPECT_EQ(seg->candidate(3).value, "15階");
  EXPECT_EQ(seg->candidate(3).content_value, "15階");
  EXPECT_EQ(seg->candidate(3).description, "");

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, SpecialFormBoundaries) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  Segments segments;

  // These special forms doesn't have zeros.
  Segment *seg = SetupSegments(pos_matcher_, "0", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_FALSE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanNoCapitalDescription));
  // "0" has superscripts and subscripts
  EXPECT_TRUE(HasDescription(*seg, kSuperscriptDescription));
  EXPECT_TRUE(HasDescription(*seg, kSubscriptDescription));

  // "1" has special forms.
  seg = SetupSegments(pos_matcher_, "1", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_TRUE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_TRUE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_TRUE(HasDescription(*seg, kRomanNoCapitalDescription));
  EXPECT_TRUE(HasDescription(*seg, kSuperscriptDescription));
  EXPECT_TRUE(HasDescription(*seg, kSubscriptDescription));

  // "12" has every special forms except superscript and subscript.
  seg = SetupSegments(pos_matcher_, "12", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_TRUE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_TRUE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_TRUE(HasDescription(*seg, kRomanNoCapitalDescription));
  EXPECT_FALSE(HasDescription(*seg, kSuperscriptDescription));
  EXPECT_FALSE(HasDescription(*seg, kSubscriptDescription));

  // "13" doesn't have roman forms.
  seg = SetupSegments(pos_matcher_, "13", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_TRUE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanNoCapitalDescription));

  // "50" has circled numerics.
  seg = SetupSegments(pos_matcher_, "50", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_TRUE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanNoCapitalDescription));

  // "51" doesn't have special forms.
  seg = SetupSegments(pos_matcher_, "51", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_FALSE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanNoCapitalDescription));
  EXPECT_FALSE(HasDescription(*seg, kSuperscriptDescription));
  EXPECT_FALSE(HasDescription(*seg, kSubscriptDescription));
}

TEST_F(NumberRewriterTest, OneOfCandidatesIsEmpty) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *first_candidate = seg->add_candidate();

  // this candidate should be skipped
  first_candidate->value = "";
  first_candidate->content_value = first_candidate->value;

  Segment::Candidate *second_candidate = seg->add_candidate();

  second_candidate->value = "0";
  second_candidate->lid = pos_matcher_.GetNumberId();
  second_candidate->rid = pos_matcher_.GetNumberId();
  second_candidate->content_value = second_candidate->value;
  second_candidate->key = "0";
  second_candidate->content_key = "0";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(seg->candidate(0).value, "");
  EXPECT_EQ(seg->candidate(0).content_value, "");
  EXPECT_EQ(seg->candidate(0).description, "");

  EXPECT_EQ(seg->candidate(1).value, "0");
  EXPECT_EQ(seg->candidate(1).content_value, "0");
  EXPECT_EQ(seg->candidate(1).description, "");

  EXPECT_EQ(seg->candidate(2).value, "〇");
  EXPECT_EQ(seg->candidate(2).content_value, "〇");
  EXPECT_EQ(seg->candidate(2).description, kKanjiDescription);

  EXPECT_EQ(seg->candidate(3).value, "０");
  EXPECT_EQ(seg->candidate(3).content_value, "０");
  EXPECT_EQ(seg->candidate(3).description, kArabicDescription);

  EXPECT_EQ(seg->candidate(4).value, "零");
  EXPECT_EQ(seg->candidate(4).content_value, "零");
  EXPECT_EQ(seg->candidate(4).description, kOldKanjiDescription);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, RewriteDoesNotHappen) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();

  candidate->value = "タンポポ";
  candidate->content_value = candidate->value;

  // Number rewrite should not occur
  EXPECT_FALSE(number_rewriter->Rewrite(default_request_, &segments));

  // Number of candidates should be maintained
  EXPECT_EQ(seg->candidates_size(), 1);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIsZero) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetNumberId();
  candidate->value = "0";
  candidate->content_value = "0";
  candidate->key = "0";
  candidate->content_key = "0";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(seg->candidates_size(), 6);

  EXPECT_EQ(seg->candidate(0).value, "0");
  EXPECT_EQ(seg->candidate(0).content_value, "0");
  EXPECT_EQ(seg->candidate(0).description, "");

  EXPECT_EQ(seg->candidate(1).value, "〇");
  EXPECT_EQ(seg->candidate(1).content_value, "〇");
  EXPECT_EQ(seg->candidate(1).description, kKanjiDescription);

  EXPECT_EQ(seg->candidate(2).value, "０");
  EXPECT_EQ(seg->candidate(2).content_value, "０");
  EXPECT_EQ(seg->candidate(2).description, kArabicDescription);

  EXPECT_EQ(seg->candidate(3).value, "零");
  EXPECT_EQ(seg->candidate(3).content_value, "零");
  EXPECT_EQ(seg->candidate(3).description, kOldKanjiDescription);

  EXPECT_EQ(seg->candidate(4).value, "⁰");
  EXPECT_EQ(seg->candidate(4).content_value, "⁰");
  EXPECT_EQ(seg->candidate(4).description, kSuperscriptDescription);

  EXPECT_EQ(seg->candidate(5).value, "₀");
  EXPECT_EQ(seg->candidate(5).content_value, "₀");
  EXPECT_EQ(seg->candidate(5).description, kSubscriptDescription);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIsZeroZero) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetNumberId();
  candidate->value = "00";
  candidate->content_value = "00";
  candidate->key = "00";
  candidate->content_key = "00";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(seg->candidates_size(), 6);

  EXPECT_EQ(seg->candidate(0).value, "00");
  EXPECT_EQ(seg->candidate(0).content_value, "00");
  EXPECT_EQ(seg->candidate(0).description, "");

  EXPECT_EQ(seg->candidate(1).value, "〇〇");
  EXPECT_EQ(seg->candidate(1).content_value, "〇〇");
  EXPECT_EQ(seg->candidate(1).description, kKanjiDescription);

  EXPECT_EQ(seg->candidate(2).value, "００");
  EXPECT_EQ(seg->candidate(2).content_value, "００");
  EXPECT_EQ(seg->candidate(2).description, kArabicDescription);

  EXPECT_EQ(seg->candidate(3).value, "零");
  EXPECT_EQ(seg->candidate(3).content_value, "零");
  EXPECT_EQ(seg->candidate(3).description, kOldKanjiDescription);

  EXPECT_EQ(seg->candidate(4).value, "⁰");
  EXPECT_EQ(seg->candidate(4).content_value, "⁰");
  EXPECT_EQ(seg->candidate(4).description, kSuperscriptDescription);

  EXPECT_EQ(seg->candidate(5).value, "₀");
  EXPECT_EQ(seg->candidate(5).content_value, "₀");
  EXPECT_EQ(seg->candidate(5).description, kSubscriptDescription);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIs19Digit) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetNumberId();
  candidate->value = "1000000000000000000";
  candidate->content_value = "1000000000000000000";
  candidate->key = "1000000000000000000";
  candidate->content_key = "1000000000000000000";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  constexpr ExpectResult kExpectResults[] = {
      {"1000000000000000000", "1000000000000000000", ""},
      {"一〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇",
       "一〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇", kKanjiDescription},
      {"１００００００００００００００００００",
       "１００００００００００００００００００", kArabicDescription},
      {"1,000,000,000,000,000,000", "1,000,000,000,000,000,000",
       kArabicDescription},
      {"１，０００，０００，０００，０００，０００，０００",
       "１，０００，０００，０００，０００，０００，０００",
       kArabicDescription},
      {"100京", "100京", kArabicDescription},
      {"１００京", "１００京", kArabicDescription},
      {"百京", "百京", kKanjiDescription},
      {"壱百京", "壱百京", kOldKanjiDescription},
      {"0xde0b6b3a7640000", "0xde0b6b3a7640000", "16進数"},
      {"067405553164731000000", "067405553164731000000", "8進数"},
      {"0b110111100000101101101011001110100111011001000000000000000000",
       "0b110111100000101101101011001110100111011001000000000000000000",
       "2進数"},
  };

  constexpr size_t kExpectResultSize = std::size(kExpectResults);
  EXPECT_EQ(seg->candidates_size(), kExpectResultSize);

  for (size_t i = 0; i < kExpectResultSize; ++i) {
    SCOPED_TRACE(absl::StrFormat("i = %d", i));
    EXPECT_EQ(seg->candidate(i).value, kExpectResults[i].value);
    EXPECT_EQ(seg->candidate(i).content_value, kExpectResults[i].content_value);
    EXPECT_EQ(seg->candidate(i).description, kExpectResults[i].description);
  }

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIsGreaterThanUInt64Max) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetNumberId();
  candidate->value = "18446744073709551616";  // 2^64
  candidate->content_value = "18446744073709551616";
  candidate->key = "18446744073709551616";
  candidate->content_key = "18446744073709551616";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  constexpr ExpectResult kExpectResults[] = {
      {"18446744073709551616", "18446744073709551616", ""},
      {"一八四四六七四四〇七三七〇九五五一六一六",
       "一八四四六七四四〇七三七〇九五五一六一六", kKanjiDescription},
      {"１８４４６７４４０７３７０９５５１６１６",
       "１８４４６７４４０７３７０９５５１６１６", kArabicDescription},
      {"18,446,744,073,709,551,616", "18,446,744,073,709,551,616",
       kArabicDescription},
      {"１８，４４６，７４４，０７３，７０９，５５１，６１６",
       "１８，４４６，７４４，０７３，７０９，５５１，６１６",
       kArabicDescription},
      {"1844京6744兆737億955万1616", "1844京6744兆737億955万1616",
       kArabicDescription},
      {"１８４４京６７４４兆７３７億９５５万１６１６",
       "１８４４京６７４４兆７３７億９５５万１６１６", kArabicDescription},
      {"千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十六",
       "千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十六",
       kKanjiDescription},
      {"壱阡八百四拾四京六阡七百四拾四兆七百参拾七億九百五拾五萬壱阡六百壱拾六",
       "壱阡八百四拾四京六阡七百四拾四兆七百参拾七億九百五拾五萬壱阡六百壱拾六",
       kOldKanjiDescription},
  };

  constexpr size_t kExpectResultSize = std::size(kExpectResults);
  EXPECT_EQ(seg->candidates_size(), kExpectResultSize);

  for (size_t i = 0; i < kExpectResultSize; ++i) {
    SCOPED_TRACE(absl::StrFormat("i = %d", i));
    EXPECT_EQ(seg->candidate(i).value, kExpectResults[i].value);
    EXPECT_EQ(seg->candidate(i).content_value, kExpectResults[i].content_value);
    EXPECT_EQ(seg->candidate(i).description, kExpectResults[i].description);
  }

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIsGoogol) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetNumberId();

  // 10^100 as "100000 ... 0"
  std::string input = "1";
  for (size_t i = 0; i < 100; ++i) {
    input += "0";
  }

  candidate->key = input;
  candidate->content_key = input;
  candidate->value = input;
  candidate->content_value = input;

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(seg->candidates_size(), 6);

  EXPECT_EQ(seg->candidate(0).value, input);
  EXPECT_EQ(seg->candidate(0).content_value, input);
  EXPECT_EQ(seg->candidate(0).description, "");

  // 10^100 as "一〇〇〇〇〇 ... 〇"
  std::string expected2 = "一";
  for (size_t i = 0; i < 100; ++i) {
    expected2 += "〇";
  }
  EXPECT_EQ(seg->candidate(1).value, expected2);
  EXPECT_EQ(seg->candidate(1).content_value, expected2);
  EXPECT_EQ(seg->candidate(1).description, kKanjiDescription);

  // 10^100 as "１０００００ ... ０"
  std::string expected3 = "１";
  for (size_t i = 0; i < 100; ++i) {
    expected3 += "０";
  }
  EXPECT_EQ(seg->candidate(2).value, expected3);
  EXPECT_EQ(seg->candidate(2).content_value, expected3);
  EXPECT_EQ(seg->candidate(2).description, kArabicDescription);

  // 10,000, ... ,000
  std::string expected1 = "10";
  for (size_t i = 0; i < 100 / 3; ++i) {
    expected1 += ",000";
  }
  EXPECT_EQ(seg->candidate(3).value, expected1);
  EXPECT_EQ(seg->candidate(3).content_value, expected1);
  EXPECT_EQ(seg->candidate(3).description, kArabicDescription);

  // "１０，０００， ... ，０００"
  std::string expected4 = "１０";  // "１０"
  for (size_t i = 0; i < 100 / 3; ++i) {
    expected4 += "，０００";
  }
  EXPECT_EQ(seg->candidate(4).value, expected4);
  EXPECT_EQ(seg->candidate(4).content_value, expected4);
  EXPECT_EQ(seg->candidate(4).description, kArabicDescription);

  EXPECT_EQ(seg->candidate(5).value, "Googol");
  EXPECT_EQ(seg->candidate(5).content_value, "Googol");
  EXPECT_EQ(seg->candidate(5).description, "");

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, RankingForKanjiCandidate) {
  // If kanji candidate is higher before we rewrite segments,
  // kanji should have higher raking.
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *segment = segments.add_segment();
    DCHECK(segment);
    segment->set_key("さんびゃく");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate = segment->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = "さんびゃく";
    candidate->value = "三百";
    candidate->content_value = "三百";
  }

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_NE(segments.segments_size(), 0);
  int kanji_pos = 0, arabic_pos = 0;
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "三百", &kanji_pos));
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "300", &arabic_pos));
  EXPECT_LT(kanji_pos, arabic_pos);
}

TEST_F(NumberRewriterTest, ModifyExsistingRanking) {
  // Modify existing ranking even if the converter returns unusual results
  // due to dictionary noise, etc.
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *segment = segments.add_segment();
    DCHECK(segment);
    segment->set_key("さんびゃく");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = "さんびゃく";
    candidate->value = "参百";
    candidate->content_value = "参百";

    candidate = segment->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = "さんびゃく";
    candidate->value = "三百";
    candidate->content_value = "三百";
  }

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  int kanji_pos = 0, old_kanji_pos = 0;
  EXPECT_NE(segments.segments_size(), 0);
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "三百", &kanji_pos));
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "参百", &old_kanji_pos));
  EXPECT_LT(kanji_pos, old_kanji_pos);
}

TEST_F(NumberRewriterTest, EraseExistingCandidates) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *segment = segments.add_segment();
    DCHECK(segment);
    segment->set_key("いち");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->lid = pos_matcher_.GetUnknownId();  // Not number POS
    candidate->rid = pos_matcher_.GetUnknownId();
    candidate->key = "いち";
    candidate->content_key = "いち";
    candidate->value = "壱";
    candidate->content_value = "壱";

    candidate = segment->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();  // Number POS
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = "いち";
    candidate->content_key = "いち";
    candidate->value = "一";
    candidate->content_value = "一";
  }

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  // "一" becomes the base candidate, instead of "壱"
  int base_pos = 0;
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "一", &base_pos));
  EXPECT_EQ(base_pos, 0);

  // Daiji will be inserted with new correct POS ids.
  int daiji_pos = 0;
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "壱", &daiji_pos));
  EXPECT_GT(daiji_pos, 0);
  EXPECT_EQ(pos_matcher_.GetNumberId(),
            segments.segment(0).candidate(daiji_pos).lid);
  EXPECT_EQ(pos_matcher_.GetNumberId(),
            segments.segment(0).candidate(daiji_pos).rid);
}

TEST_F(NumberRewriterTest, SeparatedArabicsTest) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  // Expected data to succeed tests.
  constexpr std::array<std::array<absl::string_view, 3>, 3> kSuccess = {{
      {"1000", "1,000", "１，０００"},
      {"12345678", "12,345,678", "１２，３４５，６７８"},
      {"1234.5", "1,234.5", "１，２３４．５"},
  }};

  for (const std::array<absl::string_view, 3> &success : kSuccess) {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = success[0];
    candidate->content_key = success[0];
    candidate->value = success[0];
    candidate->content_value = success[0];

    EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
    EXPECT_TRUE(FindValue(segments.segment(0), success[1]))
        << "Input : " << success[0];
    EXPECT_TRUE(FindValue(segments.segment(0), success[2]))
        << "Input : " << success[0];
  }

  // Expected data to fail tests.
  constexpr std::array<std::array<absl::string_view, 3>, 3> kFail = {{
      {"123", ",123", "，１２３"},
      {"999", ",999", "，９９９"},
      {"0000", "0,000", "０，０００"},
  }};

  for (const std::array<absl::string_view, 3> &fail : kFail) {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = fail[0];
    candidate->content_key = fail[0];
    candidate->value = fail[0];
    candidate->content_value = fail[0];

    EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
    EXPECT_FALSE(FindValue(segments.segment(0), fail[1]))
        << "Input : " << fail[0];
    EXPECT_FALSE(FindValue(segments.segment(0), fail[2]))
        << "Input : " << fail[0];
  }
}

// Consider the case where user dictionaries contain following entry.
// - Reading: "はやぶさ"
// - Value: "8823"
// - POS: GeneralNoun (not *Number*)
// In this case, NumberRewriter should not clear
// Segment::Candidate::USER_DICTIONARY bit in the base candidate.
TEST_F(NumberRewriterTest, PreserveUserDictionaryAttribute) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  {
    Segments segments;
    {
      Segment *seg = segments.push_back_segment();
      Segment::Candidate *candidate = seg->add_candidate();
      candidate->lid = pos_matcher_.GetGeneralNounId();
      candidate->rid = pos_matcher_.GetGeneralNounId();
      candidate->key = "はやぶさ";
      candidate->content_key = candidate->key;
      candidate->value = "8823";
      candidate->content_value = candidate->value;
      candidate->cost = 5925;
      candidate->wcost = 5000;
      candidate->attributes = Segment::Candidate::USER_DICTIONARY |
                              Segment::Candidate::NO_VARIANTS_EXPANSION;
    }

    EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
    bool base_candidate_found = false;
    {
      const Segment &segment = segments.segment(0);
      for (size_t i = 0; i < segment.candidates_size(); ++i) {
        const Segment::Candidate &candidate = segment.candidate(i);
        if (candidate.value == "8823" &&
            (candidate.attributes & Segment::Candidate::USER_DICTIONARY)) {
          base_candidate_found = true;
          break;
        }
      }
    }
    EXPECT_TRUE(base_candidate_found);
  }
}

TEST_F(NumberRewriterTest, DuplicateCandidateTest) {
  // To reproduce issue b/6714268.
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  commands::Request request;
  std::unique_ptr<NumberRewriter> rewriter(CreateNumberRewriter());

  {
    request.set_mixed_conversion(true);
    const ConversionRequest convreq =
      ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    const ConversionRequest convreq =
      ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::CONVERSION);
  }
}

TEST_F(NumberRewriterTest, NonNumberNounTest) {
  // Test if "百舌鳥" is not rewritten to "100舌鳥", etc.
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  Segments segments;
  Segment *segment = segments.push_back_segment();
  segment->set_key("");
  Segment::Candidate *cand = segment->add_candidate();
  cand->key = "もず";
  cand->content_key = cand->key;
  cand->value = "百舌鳥";
  cand->content_value = cand->value;
  cand->lid = pos_matcher_.GetGeneralNounId();
  cand->rid = pos_matcher_.GetGeneralNounId();
  EXPECT_FALSE(number_rewriter->Rewrite(default_request_, &segments));
}

TEST_F(NumberRewriterTest, RewriteArabicNumberTest) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  Segments segments;
  Segment *segment = segments.push_back_segment();
  segment->set_key("いち");
  struct CandData {
    const absl::string_view value;
    int pos_id;
  };
  const CandData kCandList[] = {
      {"1", pos_matcher_.GetNumberId()},
      {"一", pos_matcher_.GetNumberId()},
      {"位置", pos_matcher_.GetGeneralNounId()},
      {"イチ", pos_matcher_.GetGeneralNounId()},
      {"壱", pos_matcher_.GetGeneralNounId()},
  };

  for (const auto &cand_data : kCandList) {
    Segment::Candidate *cand = segment->add_candidate();
    cand->key = "いち";
    cand->content_key = cand->key;
    cand->value = cand_data.value;
    cand->content_value = cand->value;
    cand->lid = cand_data.pos_id;
    cand->rid = cand_data.pos_id;
  }
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_GT(segment->candidates_size(), 1);
  EXPECT_EQ(segment->candidate(0).value, "一");
  EXPECT_EQ(segment->candidate(1).value, "位置");
}

TEST_F(NumberRewriterTest, RewriteForPartialSuggestion_b16765535) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = "090";
    candidate->value = "090";
    candidate->content_key = "090";
    candidate->content_value = "090";
    candidate->attributes = (Segment::Candidate::PARTIALLY_KEY_CONSUMED |
                             Segment::Candidate::NO_SUGGEST_LEARNING);
    candidate->consumed_key_size = 3;
  }
  {
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->key = "-";
    candidate->value = "-";
    candidate->content_key = "-";
    candidate->content_value = "-";
  }
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  ASSERT_EQ(segments.conversion_segments_size(), 2);
  const Segment &seg = segments.conversion_segment(0);
  ASSERT_LE(2, seg.candidates_size());
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    const Segment::Candidate &candidate = seg.candidate(i);
    EXPECT_TRUE(candidate.attributes &
                Segment::Candidate::PARTIALLY_KEY_CONSUMED);
    EXPECT_TRUE(candidate.attributes & Segment::Candidate::NO_SUGGEST_LEARNING);
  }
}

TEST_F(NumberRewriterTest, RewriteForPartialSuggestion_b19470020) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *seg = segments.push_back_segment();
    seg->set_key("ひとりひとぱっく");
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = "ひとり";
    candidate->value = "一人";
    candidate->content_key = "ひとり";
    candidate->content_value = "一人";
    candidate->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    candidate->consumed_key_size = 3;
  }
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  ASSERT_EQ(segments.conversion_segments_size(), 1);
  const Segment &seg = segments.conversion_segment(0);
  ASSERT_LE(2, seg.candidates_size());
  bool found_halfwidth = false;
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    const Segment::Candidate &candidate = seg.candidate(i);
    if (candidate.value != "1人") {
      continue;
    }
    found_halfwidth = true;
    EXPECT_EQ(candidate.consumed_key_size, 3);
    EXPECT_TRUE(candidate.attributes &
                Segment::Candidate::PARTIALLY_KEY_CONSUMED);
  }
  EXPECT_TRUE(found_halfwidth);
}

TEST_F(NumberRewriterTest, RewritePhonePrefix_b16668386) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetNumberId();
  candidate->rid = pos_matcher_.GetGeneralSymbolId();
  candidate->key = "090-";
  candidate->value = "090-";
  candidate->content_key = "090-";
  candidate->content_value = "090-";

  EXPECT_FALSE(number_rewriter->Rewrite(default_request_, &segments));
}

TEST_F(NumberRewriterTest, RewriteFromPhoneticNumber) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *seg = segments.push_back_segment();
    seg->set_key("じゅうまん");
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = "じゅうまん";
    candidate->content_key = "じゅうまん";
    candidate->value = "10万";
    candidate->content_value = "10万";
  }
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  ASSERT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_THAT(segments.conversion_segment(0),
              ContainsCandidate(ValueIs("100000")));
}

TEST_F(NumberRewriterTest, DoNotGenerateLongCandidatesForPhoneticNumber) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *seg = segments.push_back_segment();
    seg->set_key("ひゃくまん");
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->lid = pos_matcher_.GetNumberId();
    candidate->rid = pos_matcher_.GetNumberId();
    candidate->key = "ひゃくまん";
    candidate->content_key = "ひゃくまん";
    candidate->value = "100万";
    candidate->content_value = "100万";
  }
  EXPECT_FALSE(number_rewriter->Rewrite(default_request_, &segments));
}

// Creates Segments which have one conversion segment.
// The number candidate will be inserted at the `base_pos`.
Segments PrepareNumberSegments(absl::string_view segment_key,
                               absl::string_view number_value, int base_pos,
                               const PosMatcher &pos_matcher) {
  Segments segments;
  {
    Segment *seg = segments.add_segment();
    seg->set_key(segment_key);
    for (int i = 0; i < base_pos; ++i) {
      Segment::Candidate *c = seg->add_candidate();
      strings::Assign(c->key, segment_key);
      c->content_key = c->key;
      c->value = absl::StrCat("placeholder", i);
      c->content_value = c->value;
    }
    Segment::Candidate *c = seg->add_candidate();
    strings::Assign(c->key, segment_key);
    c->content_key = c->key;
    strings::Assign(c->value, number_value);
    c->content_value = c->value;
    c->lid = pos_matcher.GetNumberId();
    c->rid = pos_matcher.GetNumberId();
  }

  return segments;
}

void LearnNumberStyle(const ConversionRequest &request,
                      const PosMatcher &pos_matcher, NumberRewriter &rewriter) {
  Segments segments = PrepareNumberSegments("3000", "3000", 3, pos_matcher);
  rewriter.Rewrite(request, &segments);
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  ASSERT_GT(segments.conversion_segment(0).candidates_size(), 9);
  ASSERT_EQ(segments.conversion_segment(0).candidate(3).style,
            NumberUtil::NumberString::DEFAULT_STYLE);
  ASSERT_EQ(segments.conversion_segment(0).candidate(3).value, "3000");
  ASSERT_EQ(segments.conversion_segment(0).candidate(4).style,
            NumberUtil::NumberString::NUMBER_KANJI_ARABIC);
  ASSERT_EQ(segments.conversion_segment(0).candidate(4).value, "三〇〇〇");
  ASSERT_EQ(segments.conversion_segment(0).candidate(5).style,
            NumberUtil::NumberString::DEFAULT_STYLE);
  ASSERT_EQ(segments.conversion_segment(0).candidate(5).value, "３０００");
  ASSERT_EQ(segments.conversion_segment(0).candidate(6).style,
            NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH);
  ASSERT_EQ(segments.conversion_segment(0).candidate(6).value, "3,000");
  ASSERT_EQ(segments.conversion_segment(0).candidate(7).style,
            NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH);
  ASSERT_EQ(segments.conversion_segment(0).candidate(7).value, "３，０００");

  segments.mutable_conversion_segment(0)->move_candidate(6, 0);
  segments.mutable_conversion_segment(0)->set_segment_type(
      Segment::FIXED_VALUE);
  rewriter.Finish(request, &segments);
}

TEST_F(NumberRewriterTest, NumberStyleLearningNotEnabled) {
  std::unique_ptr<NumberRewriter> rewriter(CreateNumberRewriter());
  const ConversionRequest convreq;
  LearnNumberStyle(convreq, pos_matcher_, *rewriter);

  {
    Segments new_segments =
        PrepareNumberSegments("2000", "2000", 3, pos_matcher_);
    rewriter->Rewrite(convreq, &new_segments);
    ASSERT_EQ(new_segments.conversion_segments_size(), 1);
    ASSERT_GT(new_segments.conversion_segment(0).candidates_size(), 3);
    // Learned style should not be applied.
    ASSERT_NE(new_segments.conversion_segment(0).candidate(3).value, "2,000");
  }
}

class NumberStyleLearningTest : public NumberRewriterTest,
                                public WithParamInterface<commands::Request> {};

INSTANTIATE_TEST_SUITE_P(
    NumberStyleLearningTestForRequest, NumberStyleLearningTest,
    ::testing::Values(
        []() {
          commands::Request request;
          request_test_util::FillMobileRequest(&request);
          return request;
        }(),
        []() {
          commands::Request request;
          request_test_util::FillMobileRequestWithHardwareKeyboard(&request);
          return request;
        }()));

TEST_P(NumberStyleLearningTest, NumberRewriterTest) {
  std::unique_ptr<NumberRewriter> rewriter(CreateNumberRewriter());
  const commands::Request request = GetParam();
  const ConversionRequest convreq =
      ConversionRequestBuilder().SetRequest(request).Build();

  LearnNumberStyle(convreq, pos_matcher_, *rewriter);

  {
    Segments new_segments =
        PrepareNumberSegments("2000", "2000", 3, pos_matcher_);
    rewriter->Rewrite(convreq, &new_segments);
    ASSERT_EQ(new_segments.conversion_segments_size(), 1);
    ASSERT_GT(new_segments.conversion_segment(0).candidates_size(), 6);
    // Learned style should be applied.
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(3).style,
              NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH);
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(3).value, "2,000");
    ASSERT_TRUE(new_segments.conversion_segment(0).candidate(3).attributes &
                Segment::Candidate::NO_VARIANTS_EXPANSION);

    ASSERT_EQ(new_segments.conversion_segment(0).candidate(4).style,
              NumberUtil::NumberString::DEFAULT_STYLE);
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(4).value, "2000");
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(5).style,
              NumberUtil::NumberString::NUMBER_KANJI_ARABIC);
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(5).value,
              "二〇〇〇");
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(6).style,
              NumberUtil::NumberString::DEFAULT_STYLE);
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(6).value,
              "２０００");
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(7).style,
              NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH);
    ASSERT_EQ(new_segments.conversion_segment(0).candidate(7).value,
              "２，０００");
  }

  {
    // Should not apply the learned number style for the multiple segments.
    Segments new_segments =
        PrepareNumberSegments("2000", "2000", 3, pos_matcher_);
    Segment *seg = new_segments.add_segment();
    seg->set_key("かい");
    Segment::Candidate *c = seg->add_candidate();
    c->key = "かい";
    c->content_key = c->key;
    c->value = "回";
    c->content_value = c->value;
    rewriter->Rewrite(convreq, &new_segments);
    ASSERT_EQ(new_segments.conversion_segments_size(), 2);
    ASSERT_GT(new_segments.conversion_segment(0).candidates_size(), 3);
    ASSERT_NE(new_segments.conversion_segment(0).candidate(3).value, "2,000");
  }
}

TEST_F(NumberRewriterTest, NoModification) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  Segments segments;
  Segment *seg = segments.push_back_segment();
  for (int i = 0; i < 3; ++i) {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->lid = pos_matcher_.GetGeneralNounId();
    candidate->rid = pos_matcher_.GetGeneralNounId();
    candidate->key = "さん";
    candidate->content_key = candidate->key;
    candidate->value = "3";
    candidate->content_value = candidate->value;
    candidate->cost = 5925;
    candidate->wcost = 5000;
    if (i == 0) {
      candidate->attributes = Segment::Candidate::NO_MODIFICATION;
    }
  }

  EXPECT_EQ(seg->candidates_size(), 3);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_EQ(seg->candidate(0).value, "3");
  EXPECT_GT(seg->candidates_size(), 3);
}

TEST_F(NumberRewriterTest, RewriteMultipleTimes) {
  std::unique_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  Segments segments;
  Segment *seg = segments.push_back_segment();
  seg->set_key("１２");

  Segment::Candidate *candidate = seg->add_candidate();
  candidate->lid = pos_matcher_.GetGeneralNounId();
  candidate->rid = pos_matcher_.GetGeneralNounId();
  candidate->key = "１２";
  candidate->content_key = candidate->key;
  candidate->value = "１２";
  candidate->content_value = candidate->value;
  candidate->cost = 5925;
  candidate->wcost = 5000;

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_EQ(seg->candidates_size(), 11);
  EXPECT_EQ(seg->candidate(0).value, "12");
  EXPECT_EQ(seg->candidate(1).value, "一二");
  EXPECT_EQ(seg->candidate(2).value, "１２");
  EXPECT_EQ(seg->candidate(3).value, "十二");
  EXPECT_EQ(seg->candidate(4).value, "壱拾弐");
  EXPECT_EQ(seg->candidate(5).value, "Ⅻ");
  EXPECT_EQ(seg->candidate(6).value, "ⅻ");
  EXPECT_EQ(seg->candidate(7).value, "⑫");
  EXPECT_EQ(seg->candidate(8).value, "0xc");
  EXPECT_EQ(seg->candidate(9).value, "014");
  EXPECT_EQ(seg->candidate(10).value, "0b1100");

  // Rewriting multiple times should not change the result.
  EXPECT_FALSE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_EQ(seg->candidates_size(), 11);
  EXPECT_EQ(seg->candidate(0).value, "12");
  EXPECT_EQ(seg->candidate(1).value, "一二");
  EXPECT_EQ(seg->candidate(2).value, "１２");
  EXPECT_EQ(seg->candidate(3).value, "十二");
  EXPECT_EQ(seg->candidate(4).value, "壱拾弐");
  EXPECT_EQ(seg->candidate(5).value, "Ⅻ");
  EXPECT_EQ(seg->candidate(6).value, "ⅻ");
  EXPECT_EQ(seg->candidate(7).value, "⑫");
  EXPECT_EQ(seg->candidate(8).value, "0xc");
  EXPECT_EQ(seg->candidate(9).value, "014");
  EXPECT_EQ(seg->candidate(10).value, "0b1100");
}
}  // namespace mozc
