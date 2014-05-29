// Copyright 2010-2014, Google Inc.
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

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#ifdef MOZC_USE_PACKED_DICTIONARY
#include "data_manager/packed/packed_data_manager.h"
#include "data_manager/packed/packed_data_mock.h"
#endif  // MOZC_USE_PACKED_DICTIONARY
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

// To show the value of size_t, 'z' speficier should be used.
// But MSVC doesn't support it yet so use 'l' instead.
#ifdef _MSC_VER
#define SIZE_T_PRINTF_FORMAT "%lu"
#else  // _MSC_VER
#define SIZE_T_PRINTF_FORMAT "%zu"
#endif  // _MSC_VER

namespace mozc {

namespace {
// const char kKanjiDescription[]  = 漢数字";
// const char kArabicDescription[] = "数字";
// const char kOldKanjiDescription[]  = "大字";
// const char kMaruNumberDescription[] = "丸数字";
// const char kRomanCapitalDescription[] = "ローマ数字(大文字)";
// const char kRomanNoCapitalDescription[] = "ローマ数字(小文字)";

const char kKanjiDescription[]  = "\xE6\xBC\xA2\xE6\x95\xB0\xE5\xAD\x97";
const char kArabicDescription[] = "\xE6\x95\xB0\xE5\xAD\x97";
const char kOldKanjiDescription[]  = "\xE5\xA4\xA7\xE5\xAD\x97";
const char kMaruNumberDescription[] = "\xE4\xB8\xB8\xE6\x95\xB0\xE5\xAD\x97";
const char kRomanCapitalDescription[] =
    "\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x9E\xE6\x95\xB0\xE5\xAD\x97"
    "(\xE5\xA4\xA7\xE6\x96\x87\xE5\xAD\x97)";
const char kRomanNoCapitalDescription[] =
    "\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x9E\xE6\x95\xB0\xE5\xAD\x97"
    "(\xE5\xB0\x8F\xE6\x96\x87\xE5\xAD\x97)";

bool FindValue(const Segment &segment, const string &value) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == value) {
      return true;
    }
  }
  return false;
}

Segment *SetupSegments(const POSMatcher* pos_matcher,
                       const string &candidate_value, Segments *segments) {
  segments->Clear();
  Segment *segment = segments->push_back_segment();
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher->GetNumberId();
  candidate->rid = pos_matcher->GetNumberId();
  candidate->value = candidate_value;
  candidate->content_value = candidate_value;
  return segment;
}

bool HasDescription(const Segment &segment, const string &description) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).description == description) {
      return true;
    }
  }
  return false;
}

// Find candiadte id
bool FindCandidateId(const Segment &segment, const string &value, int *id) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == value) {
      *id = i;
      return true;
    }
  }
  return false;
}
}  // namespace

class NumberRewriterTest : public ::testing::Test {
 protected:
  // Explicitly define constructor to prevent Visual C++ from
  // considering this class as POD.
  NumberRewriterTest() {}

  virtual void SetUp() {
#ifdef MOZC_USE_PACKED_DICTIONARY
    // TODO(noriyukit): Currently this test uses mock data manager.  Check if we
    // can remove this registration of packed data manager.
    // Registers mocked PackedDataManager.
    scoped_ptr<packed::PackedDataManager>
        data_manager(new packed::PackedDataManager());
    CHECK(data_manager->Init(string(kPackedSystemDictionary_data,
                                    kPackedSystemDictionary_size)));
    packed::RegisterPackedDataManager(data_manager.release());
#endif  // MOZC_USE_PACKED_DICTIONARY

    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
    pos_matcher_ = mock_data_manager_.GetPOSMatcher();
  }

  virtual void TearDown() {
#ifdef MOZC_USE_PACKED_DICTIONARY
    // Unregisters mocked PackedDataManager.
    packed::RegisterPackedDataManager(NULL);
#endif  // MOZC_USE_PACKED_DICTIONARY
  }

  NumberRewriter *CreateNumberRewriter() {
    return new NumberRewriter(&mock_data_manager_);
  }

  const testing::MockDataManager mock_data_manager_;
  const POSMatcher *pos_matcher_;
  const ConversionRequest default_request_;
};

namespace {
struct ExpectResult {
  const char *value;
  const char *content_value;
  const char *description;
};
}  // namespace

TEST_F(NumberRewriterTest, BasicTest) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetNumberId();
  candidate->value = "012";
  candidate->content_value = "012";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  const ExpectResult kExpectResults[] = {
    {"012", "012", ""},
    // "〇一二"
    {"\xE3\x80\x87\xE4\xB8\x80\xE4\xBA\x8C",
     "\xE3\x80\x87\xE4\xB8\x80\xE4\xBA\x8C", kKanjiDescription},
    // "０１２"
    {"\xEF\xBC\x90\xEF\xBC\x91\xEF\xBC\x92",
     "\xEF\xBC\x90\xEF\xBC\x91\xEF\xBC\x92", kArabicDescription},
    // "十二"
    {"\xE5\x8D\x81\xE4\xBA\x8C", "\xE5\x8D\x81\xE4\xBA\x8C",
     kKanjiDescription},
    // "壱拾弐"
    {"\xE5\xA3\xB1\xE6\x8B\xBE\xE5\xBC\x90",
     "\xE5\xA3\xB1\xE6\x8B\xBE\xE5\xBC\x90", kOldKanjiDescription},
    // XII
    {"\xE2\x85\xAB", "\xE2\x85\xAB", kRomanCapitalDescription},
    // xii
    {"\xE2\x85\xBB", "\xE2\x85\xBB", kRomanNoCapitalDescription},
    // 12 with circle mark
    {"\xE2\x91\xAB", "\xE2\x91\xAB", kMaruNumberDescription},
    // "16進数"
    {"0xc", "0xc", "16""\xE9\x80\xB2\xE6\x95\xB0"},
    // "8進数"
    {"014", "014", "8""\xE9\x80\xB2\xE6\x95\xB0"},
    // "2進数"
    {"0b1100", "0b1100", "2""\xE9\x80\xB2\xE6\x95\xB0"},
  };

  const size_t kExpectResultSize = arraysize(kExpectResults);
  EXPECT_EQ(kExpectResultSize, seg->candidates_size());

  for (size_t i = 0; i < kExpectResultSize; ++i) {
    SCOPED_TRACE(Util::StringPrintf("i = " SIZE_T_PRINTF_FORMAT, i));
    EXPECT_EQ(kExpectResults[i].value, seg->candidate(i).value);
    EXPECT_EQ(kExpectResults[i].content_value,
              seg->candidate(i).content_value);
    EXPECT_EQ(kExpectResults[i].description,
              seg->candidate(i).description);
  }
  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, RequestType) {
  class TestData {
   public:
    Segments::RequestType request_type_;
    int expected_candidate_number_;
    TestData(Segments::RequestType request_type, int expected_number) :
        request_type_(request_type),
        expected_candidate_number_(expected_number) {
    }
  };
  TestData test_data_list[] = {
      TestData(Segments::CONVERSION, 11),  // 11 comes from BasicTest
      TestData(Segments::REVERSE_CONVERSION, 8),
      TestData(Segments::PREDICTION, 8),
      TestData(Segments::SUGGESTION, 8),
  };

  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  for (size_t i = 0; i < arraysize(test_data_list); ++i) {
    TestData& test_data = test_data_list[i];
    Segments segments;
    segments.set_request_type(test_data.request_type_);
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetNumberId();
    candidate->rid = pos_matcher_->GetNumberId();
    candidate->value = "012";
    candidate->content_value = "012";
    EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
    EXPECT_EQ(test_data.expected_candidate_number_, seg->candidates_size());
  }
}

TEST_F(NumberRewriterTest, BasicTestWithSuffix) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetNumberId();
  candidate->value = "012""\xE3\x81\x8C";   // "012が"
  candidate->content_value = "012";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  const ExpectResult kExpectResults[] = {
    // "012"
    {"012\xE3\x81\x8C", "012", ""},
    // "〇一二"
    {"\xE3\x80\x87\xE4\xB8\x80\xE4\xBA\x8C\xE3\x81\x8C",
     "\xE3\x80\x87\xE4\xB8\x80\xE4\xBA\x8C", kKanjiDescription},
    // "０１２"
    {"\xEF\xBC\x90\xEF\xBC\x91\xEF\xBC\x92\xE3\x81\x8C",
     "\xEF\xBC\x90\xEF\xBC\x91\xEF\xBC\x92", kArabicDescription},
    // "十二"
    {"\xE5\x8D\x81\xE4\xBA\x8C\xE3\x81\x8C",
     "\xE5\x8D\x81\xE4\xBA\x8C", kKanjiDescription},
    // "壱拾弐"
    {"\xE5\xA3\xB1\xE6\x8B\xBE\xE5\xBC\x90\xE3\x81\x8C",
     "\xE5\xA3\xB1\xE6\x8B\xBE\xE5\xBC\x90", kOldKanjiDescription},
    // XII
    {"\xE2\x85\xAB\xE3\x81\x8C", "\xE2\x85\xAB", kRomanCapitalDescription},
    // xii
    {"\xE2\x85\xBB\xE3\x81\x8C", "\xE2\x85\xBB", kRomanNoCapitalDescription},
    // 12 with circle mark
    {"\xE2\x91\xAB\xE3\x81\x8C", "\xE2\x91\xAB", kMaruNumberDescription},
    // "16進数"
    {"0xc""\xE3\x81\x8C", "0xc", "16""\xE9\x80\xB2\xE6\x95\xB0"},
    // "8進数"
    {"014""\xE3\x81\x8C", "014", "8""\xE9\x80\xB2\xE6\x95\xB0"},
    // "2進数"
    {"0b1100""\xE3\x81\x8C", "0b1100", "2""\xE9\x80\xB2\xE6\x95\xB0"},
  };

  const size_t kExpectResultSize = arraysize(kExpectResults);
  EXPECT_EQ(kExpectResultSize, seg->candidates_size());

  for (size_t i = 0; i < kExpectResultSize; ++i) {
    SCOPED_TRACE(Util::StringPrintf("i = " SIZE_T_PRINTF_FORMAT, i));
    EXPECT_EQ(kExpectResults[i].value, seg->candidate(i).value);
    EXPECT_EQ(kExpectResults[i].content_value,
              seg->candidate(i).content_value);
    EXPECT_EQ(kExpectResults[i].description,
              seg->candidate(i).description);
  }

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, BasicTestWithNumberSuffix) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetCounterSuffixWordId();
  candidate->value = "\xE5\x8D\x81\xE4\xBA\x94\xE5\x80\x8B";  // "十五個"
  candidate->content_value = "\xE5\x8D\x81\xE4\xBA\x94\xE5\x80\x8B";  // ditto

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(2, seg->candidates_size());

  // "十五個"
  EXPECT_EQ("\xE5\x8D\x81\xE4\xBA\x94\xE5\x80\x8B", seg->candidate(0).value);
  EXPECT_EQ("\xE5\x8D\x81\xE4\xBA\x94\xE5\x80\x8B",
            seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "15個"
  EXPECT_EQ("15\xE5\x80\x8B", seg->candidate(1).value);
  EXPECT_EQ("15\xE5\x80\x8B", seg->candidate(1).content_value);
  EXPECT_EQ("", seg->candidate(1).description);
  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, TestWithMultipleNumberSuffix) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetCounterSuffixWordId();
  candidate->value = "\xE5\x8D\x81\xE4\xBA\x94\xE5\x9B\x9E";  // "十五回"
  candidate->content_value = "\xE5\x8D\x81\xE4\xBA\x94\xE5\x9B\x9E";  // ditto
  candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetCounterSuffixWordId();
  candidate->value = "\xE5\x8D\x81\xE4\xBA\x94\xE9\x9A\x8E";  // "十五階"
  candidate->content_value = "\xE5\x8D\x81\xE4\xBA\x94\xE9\x9A\x8E";  // ditto

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(4, seg->candidates_size());

  // "十五回"
  EXPECT_EQ("\xE5\x8D\x81\xE4\xBA\x94\xE5\x9B\x9E", seg->candidate(0).value);
  EXPECT_EQ("\xE5\x8D\x81\xE4\xBA\x94\xE5\x9B\x9E",
            seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "15回"
  EXPECT_EQ("15\xE5\x9B\x9E", seg->candidate(1).value);
  EXPECT_EQ("15\xE5\x9B\x9E", seg->candidate(1).content_value);
  EXPECT_EQ("", seg->candidate(1).description);

  // "十五階"
  EXPECT_EQ("\xE5\x8D\x81\xE4\xBA\x94\xE9\x9A\x8E", seg->candidate(2).value);
  EXPECT_EQ("\xE5\x8D\x81\xE4\xBA\x94\xE9\x9A\x8E",
            seg->candidate(2).content_value);
  EXPECT_EQ("", seg->candidate(2).description);

  // "15階"
  EXPECT_EQ("15\xE9\x9A\x8E", seg->candidate(3).value);
  EXPECT_EQ("15\xE9\x9A\x8E", seg->candidate(3).content_value);
  EXPECT_EQ("", seg->candidate(3).description);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, SpecialFormBoundaries) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  Segments segments;

  // Special forms doesn't have zeros.
  Segment *seg = SetupSegments(pos_matcher_, "0", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_FALSE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_FALSE(HasDescription(*seg, kRomanNoCapitalDescription));

  // "1" has special forms.
  seg = SetupSegments(pos_matcher_, "1", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_TRUE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_TRUE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_TRUE(HasDescription(*seg, kRomanNoCapitalDescription));

  // "12" has every special forms.
  seg = SetupSegments(pos_matcher_, "12", &segments);
  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_TRUE(HasDescription(*seg, kMaruNumberDescription));
  EXPECT_TRUE(HasDescription(*seg, kRomanCapitalDescription));
  EXPECT_TRUE(HasDescription(*seg, kRomanNoCapitalDescription));

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
}

TEST_F(NumberRewriterTest, OneOfCandidatesIsEmpty) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *first_candidate = seg->add_candidate();
  first_candidate->Init();

  // this candidate should be skipped
  first_candidate->value = "";
  first_candidate->content_value = first_candidate->value;

  Segment::Candidate *second_candidate = seg->add_candidate();
  second_candidate->Init();

  second_candidate->value = "0";
  second_candidate->lid = pos_matcher_->GetNumberId();
  second_candidate->rid = pos_matcher_->GetNumberId();
  second_candidate->content_value = second_candidate->value;

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ("", seg->candidate(0).value);
  EXPECT_EQ("", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "0"
  EXPECT_EQ("0", seg->candidate(1).value);
  EXPECT_EQ("0", seg->candidate(1).content_value);
  EXPECT_EQ("", seg->candidate(1).description);

  // "〇"
  EXPECT_EQ("\xE3\x80\x87",
            seg->candidate(2).value);
  EXPECT_EQ("\xE3\x80\x87", seg->candidate(2).content_value);
  EXPECT_EQ(kKanjiDescription, seg->candidate(2).description);

  // "０"
  EXPECT_EQ("\xEF\xBC\x90",
            seg->candidate(3).value);
  EXPECT_EQ("\xEF\xBC\x90", seg->candidate(3).content_value);
  EXPECT_EQ(kArabicDescription, seg->candidate(3).description);

  // "零"
  EXPECT_EQ("\xE9\x9B\xB6",
            seg->candidate(4).value);
  EXPECT_EQ("\xE9\x9B\xB6", seg->candidate(4).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(4).description);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, RewriteDoesNotHappen) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();

  // タンポポ
  candidate->value = "\xe3\x82\xbf\xe3\x83\xb3\xe3\x83\x9d\xe3\x83\x9d";
  candidate->content_value = candidate->value;

  // Number rewrite should not occur
  EXPECT_FALSE(number_rewriter->Rewrite(default_request_, &segments));

  // Number of cahdidates should be maintained
  EXPECT_EQ(1, seg->candidates_size());

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIsZero) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetNumberId();
  candidate->value = "0";
  candidate->content_value = "0";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(4, seg->candidates_size());

  // "0"
  EXPECT_EQ("0", seg->candidate(0).value);
  EXPECT_EQ("0", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "〇"
  EXPECT_EQ("\xE3\x80\x87",
            seg->candidate(1).value);
  EXPECT_EQ("\xE3\x80\x87", seg->candidate(1).content_value);
  EXPECT_EQ(kKanjiDescription, seg->candidate(1).description);

  // "０"
  EXPECT_EQ("\xEF\xBC\x90",
            seg->candidate(2).value);
  EXPECT_EQ("\xEF\xBC\x90", seg->candidate(2).content_value);
  EXPECT_EQ(kArabicDescription, seg->candidate(2).description);

  // "零"
  EXPECT_EQ("\xE9\x9B\xB6",
            seg->candidate(3).value);
  EXPECT_EQ("\xE9\x9B\xB6", seg->candidate(3).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(3).description);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIsZeroZero) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetNumberId();
  candidate->value = "00";
  candidate->content_value = "00";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(4, seg->candidates_size());

  // "00"
  EXPECT_EQ("00", seg->candidate(0).value);
  EXPECT_EQ("00", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "〇〇"
  EXPECT_EQ("\xE3\x80\x87\xE3\x80\x87", seg->candidate(1).value);
  EXPECT_EQ("\xE3\x80\x87\xE3\x80\x87", seg->candidate(1).content_value);
  EXPECT_EQ(kKanjiDescription, seg->candidate(1).description);

  // "００"
  EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x90", seg->candidate(2).value);
  EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x90", seg->candidate(2).content_value);
  EXPECT_EQ(kArabicDescription, seg->candidate(2).description);

  // "零"
  EXPECT_EQ("\xE9\x9B\xB6",
            seg->candidate(3).value);
  EXPECT_EQ("\xE9\x9B\xB6", seg->candidate(3).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(3).description);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIs19Digit) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetNumberId();
  candidate->value = "1000000000000000000";
  candidate->content_value = "1000000000000000000";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  const ExpectResult kExpectResults[] = {
    {"1000000000000000000", "1000000000000000000", ""},
    // "一〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇"
    {"\xE4\xB8\x80\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87"
     "\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87"
     "\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87"
     "\xE3\x80\x87",
     "\xE4\xB8\x80\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87"
     "\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87"
     "\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87"
     "\xE3\x80\x87", kKanjiDescription},
    // "１００００００００００００００００００"
    {"\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90",
     "\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90", kArabicDescription},
    {"1,000,000,000,000,000,000", "1,000,000,000,000,000,000",
     kArabicDescription},
    // "１，０００，０００，０００，０００，０００，０００"
    {"\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C"
     "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C"
     "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90",
     "\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C"
     "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C"
     "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
     "\xEF\xBC\x90", kArabicDescription},
    // "100京"
    {"100""\xE4\xBA\xAC", "100""\xE4\xBA\xAC", kArabicDescription},
    // "１００京"
    {"\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xE4\xBA\xAC",
     "\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xE4\xBA\xAC", kArabicDescription},
    // "百京"
    {"\xE7\x99\xBE\xE4\xBA\xAC", "\xE7\x99\xBE\xE4\xBA\xAC",
     kKanjiDescription},
    // "壱百京"
    {"\xE5\xA3\xB1\xE7\x99\xBE\xE4\xBA\xAC",
     "\xE5\xA3\xB1\xE7\x99\xBE\xE4\xBA\xAC", kOldKanjiDescription},
    // "16進数"
    {"0xde0b6b3a7640000", "0xde0b6b3a7640000", "16""\xE9\x80\xB2\xE6\x95\xB0"},
    {"067405553164731000000", "067405553164731000000",
     // "8進数"
     "8""\xE9\x80\xB2\xE6\x95\xB0"},
    {"0b110111100000101101101011001110100111011001000000000000000000",
     "0b110111100000101101101011001110100111011001000000000000000000",
     // "2進数"
     "2""\xE9\x80\xB2\xE6\x95\xB0"},
  };

  const size_t kExpectResultSize = arraysize(kExpectResults);
  EXPECT_EQ(kExpectResultSize, seg->candidates_size());

  for (size_t i = 0; i < kExpectResultSize; ++i) {
    SCOPED_TRACE(Util::StringPrintf("i = " SIZE_T_PRINTF_FORMAT, i));
    EXPECT_EQ(kExpectResults[i].value, seg->candidate(i).value);
    EXPECT_EQ(kExpectResults[i].content_value,
              seg->candidate(i).content_value);
    EXPECT_EQ(kExpectResults[i].description,
              seg->candidate(i).description);
  }

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIsGreaterThanUInt64Max) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetNumberId();
  candidate->value = "18446744073709551616";  // 2^64
  candidate->content_value = "18446744073709551616";

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  const ExpectResult kExpectResults[] = {
    {"18446744073709551616", "18446744073709551616", ""},
    // "一八四四六七四四〇七三七〇九五五一六一六"
    {"\xE4\xB8\x80\xE5\x85\xAB\xE5\x9B\x9B\xE5\x9B\x9B\xE5\x85\xAD\xE4\xB8\x83"
     "\xE5\x9B\x9B\xE5\x9B\x9B\xE3\x80\x87\xE4\xB8\x83\xE4\xB8\x89\xE4\xB8\x83"
     "\xE3\x80\x87\xE4\xB9\x9D\xE4\xBA\x94\xE4\xBA\x94\xE4\xB8\x80\xE5\x85\xAD"
     "\xE4\xB8\x80\xE5\x85\xAD",
     "\xE4\xB8\x80\xE5\x85\xAB\xE5\x9B\x9B\xE5\x9B\x9B\xE5\x85\xAD\xE4\xB8\x83"
     "\xE5\x9B\x9B\xE5\x9B\x9B\xE3\x80\x87\xE4\xB8\x83\xE4\xB8\x89\xE4\xB8\x83"
     "\xE3\x80\x87\xE4\xB9\x9D\xE4\xBA\x94\xE4\xBA\x94\xE4\xB8\x80\xE5\x85\xAD"
     "\xE4\xB8\x80\xE5\x85\xAD", kKanjiDescription},
    // "１８４４６７４４０７３７０９５５１６１６"
    {"\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x96\xEF\xBC\x97"
     "\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x90\xEF\xBC\x97\xEF\xBC\x93\xEF\xBC\x97"
     "\xEF\xBC\x90\xEF\xBC\x99\xEF\xBC\x95\xEF\xBC\x95\xEF\xBC\x91\xEF\xBC\x96"
     "\xEF\xBC\x91\xEF\xBC\x96",
     "\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x96\xEF\xBC\x97"
     "\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x90\xEF\xBC\x97\xEF\xBC\x93\xEF\xBC\x97"
     "\xEF\xBC\x90\xEF\xBC\x99\xEF\xBC\x95\xEF\xBC\x95\xEF\xBC\x91\xEF\xBC\x96"
     "\xEF\xBC\x91\xEF\xBC\x96", kArabicDescription},
    {"18,446,744,073,709,551,616", "18,446,744,073,709,551,616",
     kArabicDescription},
    // "１８，４４６，７４４，０７３，７０９，５５１，６１６"
    {"\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x8C\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x96"
     "\xEF\xBC\x8C\xEF\xBC\x97\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x8C\xEF\xBC\x90"
     "\xEF\xBC\x97\xEF\xBC\x93\xEF\xBC\x8C\xEF\xBC\x97\xEF\xBC\x90\xEF\xBC\x99"
     "\xEF\xBC\x8C\xEF\xBC\x95\xEF\xBC\x95\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x96"
     "\xEF\xBC\x91\xEF\xBC\x96",
     "\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x8C\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x96"
     "\xEF\xBC\x8C\xEF\xBC\x97\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x8C\xEF\xBC\x90"
     "\xEF\xBC\x97\xEF\xBC\x93\xEF\xBC\x8C\xEF\xBC\x97\xEF\xBC\x90\xEF\xBC\x99"
     "\xEF\xBC\x8C\xEF\xBC\x95\xEF\xBC\x95\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x96"
     "\xEF\xBC\x91\xEF\xBC\x96", kArabicDescription},
    // "1844京6744兆737億955万1616"
    {"1844""\xE4\xBA\xAC""6744""\xE5\x85\x86""737""\xE5\x84\x84""955"
     "\xE4\xB8\x87""1616",
     "1844""\xE4\xBA\xAC""6744""\xE5\x85\x86""737""\xE5\x84\x84""955"
     "\xE4\xB8\x87" "1616", kArabicDescription},
    // "１８４４京６７４４兆７３７億９５５万１６１６"
    {"\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x94\xEF\xBC\x94\xE4\xBA\xAC\xEF\xBC\x96"
     "\xEF\xBC\x97\xEF\xBC\x94\xEF\xBC\x94\xE5\x85\x86\xEF\xBC\x97\xEF\xBC\x93"
     "\xEF\xBC\x97\xE5\x84\x84\xEF\xBC\x99\xEF\xBC\x95\xEF\xBC\x95\xE4\xB8\x87"
     "\xEF\xBC\x91\xEF\xBC\x96\xEF\xBC\x91\xEF\xBC\x96",
     "\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x94\xEF\xBC\x94\xE4\xBA\xAC\xEF\xBC\x96"
     "\xEF\xBC\x97\xEF\xBC\x94\xEF\xBC\x94\xE5\x85\x86\xEF\xBC\x97\xEF\xBC\x93"
     "\xEF\xBC\x97\xE5\x84\x84\xEF\xBC\x99\xEF\xBC\x95\xEF\xBC\x95\xE4\xB8\x87"
     "\xEF\xBC\x91\xEF\xBC\x96\xEF\xBC\x91\xEF\xBC\x96", kArabicDescription},
    // "千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十六"
    {"\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5\x9B\x9B\xE5\x8D\x81\xE5\x9B\x9B"
     "\xE4\xBA\xAC\xE5\x85\xAD\xE5\x8D\x83\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B"
     "\xE5\x8D\x81\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7\x99\xBE\xE4\xB8\x89"
     "\xE5\x8D\x81\xE4\xB8\x83\xE5\x84\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94"
     "\xE5\x8D\x81\xE4\xBA\x94\xE4\xB8\x87\xE5\x8D\x83\xE5\x85\xAD\xE7\x99\xBE"
     "\xE5\x8D\x81\xE5\x85\xAD",
     "\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5\x9B\x9B\xE5\x8D\x81\xE5\x9B\x9B"
     "\xE4\xBA\xAC\xE5\x85\xAD\xE5\x8D\x83\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B"
     "\xE5\x8D\x81\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7\x99\xBE\xE4\xB8\x89"
     "\xE5\x8D\x81\xE4\xB8\x83\xE5\x84\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94"
     "\xE5\x8D\x81\xE4\xBA\x94\xE4\xB8\x87\xE5\x8D\x83\xE5\x85\xAD\xE7\x99\xBE"
     "\xE5\x8D\x81\xE5\x85\xAD", kKanjiDescription},
    // "壱阡八百四拾四京六阡七百四拾四兆七百参拾七億九百五拾五萬壱阡六百壱拾六"
    {"\xE5\xA3\xB1\xE9\x98\xA1\xE5\x85\xAB\xE7\x99\xBE\xE5\x9B\x9B\xE6\x8B\xBE"
     "\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85\xAD\xE9\x98\xA1\xE4\xB8\x83\xE7\x99\xBE"
     "\xE5\x9B\x9B\xE6\x8B\xBE\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7\x99\xBE"
     "\xE5\x8F\x82\xE6\x8B\xBE\xE4\xB8\x83\xE5\x84\x84\xE4\xB9\x9D\xE7\x99\xBE"
     "\xE4\xBA\x94\xE6\x8B\xBE\xE4\xBA\x94\xE8\x90\xAC\xE5\xA3\xB1\xE9\x98\xA1"
     "\xE5\x85\xAD\xE7\x99\xBE\xE5\xA3\xB1\xE6\x8B\xBE\xE5\x85\xAD",
     "\xE5\xA3\xB1\xE9\x98\xA1\xE5\x85\xAB\xE7\x99\xBE\xE5\x9B\x9B\xE6\x8B\xBE"
     "\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85\xAD\xE9\x98\xA1\xE4\xB8\x83\xE7\x99\xBE"
     "\xE5\x9B\x9B\xE6\x8B\xBE\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7\x99\xBE"
     "\xE5\x8F\x82\xE6\x8B\xBE\xE4\xB8\x83\xE5\x84\x84\xE4\xB9\x9D\xE7\x99\xBE"
     "\xE4\xBA\x94\xE6\x8B\xBE\xE4\xBA\x94\xE8\x90\xAC\xE5\xA3\xB1\xE9\x98\xA1"
     "\xE5\x85\xAD\xE7\x99\xBE\xE5\xA3\xB1\xE6\x8B\xBE\xE5\x85\xAD",
     kOldKanjiDescription},
  };

  const size_t kExpectResultSize = arraysize(kExpectResults);
  EXPECT_EQ(kExpectResultSize, seg->candidates_size());

  for (size_t i = 0; i < kExpectResultSize; ++i) {
    SCOPED_TRACE(Util::StringPrintf("i = " SIZE_T_PRINTF_FORMAT, i));
    EXPECT_EQ(kExpectResults[i].value, seg->candidate(i).value);
    EXPECT_EQ(kExpectResults[i].content_value,
              seg->candidate(i).content_value);
    EXPECT_EQ(kExpectResults[i].description,
              seg->candidate(i).description);
  }

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, NumberIsGoogol) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->lid = pos_matcher_->GetNumberId();
  candidate->rid = pos_matcher_->GetNumberId();

  // 10^100 as "100000 ... 0"
  string input = "1";
  for (size_t i = 0; i < 100; ++i) {
    input += "0";
  }

  candidate->value = input;
  candidate->content_value = input;

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  EXPECT_EQ(6, seg->candidates_size());

  EXPECT_EQ(input, seg->candidate(0).value);
  EXPECT_EQ(input, seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // 10^100 as "一〇〇〇〇〇 ... 〇"
  string expected2 = "\xE4\xB8\x80";  // "一"
  for (size_t i = 0; i < 100; ++i) {
    expected2 += "\xE3\x80\x87";  // "〇"
  }
  EXPECT_EQ(expected2, seg->candidate(1).value);
  EXPECT_EQ(expected2, seg->candidate(1).content_value);
  EXPECT_EQ(kKanjiDescription, seg->candidate(1).description);

  // 10^100 as "１０００００ ... ０"
  string expected3 = "\xEF\xBC\x91";  // "１"
  for (size_t i = 0; i < 100; ++i) {
    expected3 += "\xEF\xBC\x90";  // "０"
  }
  EXPECT_EQ(expected3, seg->candidate(2).value);
  EXPECT_EQ(expected3, seg->candidate(2).content_value);
  EXPECT_EQ(kArabicDescription, seg->candidate(2).description);

  // 10,000, ... ,000
  string expected1 = "10";
  for (size_t i = 0; i < 100/3; ++i) {
    expected1 += ",000";
  }
  EXPECT_EQ(expected1, seg->candidate(3).value);
  EXPECT_EQ(expected1, seg->candidate(3).content_value);
  EXPECT_EQ(kArabicDescription, seg->candidate(3).description);

  // "１０，０００， ... ，０００"
  string expected4 = "\xEF\xBC\x91\xEF\xBC\x90";  // "１０"
  for (size_t i = 0; i < 100/3; ++i) {
    // "，０００"
    expected4 += "\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90";
  }
  EXPECT_EQ(expected4, seg->candidate(4).value);
  EXPECT_EQ(expected4, seg->candidate(4).content_value);
  EXPECT_EQ(kArabicDescription, seg->candidate(4).description);

  EXPECT_EQ("Googol",
            seg->candidate(5).value);
  EXPECT_EQ("Googol", seg->candidate(5).content_value);
  EXPECT_EQ("", seg->candidate(5).description);

  seg->clear_candidates();
}

TEST_F(NumberRewriterTest, RankingForKanjiCandidate) {
  // If kanji candidate is higher before we rewrite segments,
  // kanji should have higher raking.
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *segment = segments.add_segment();
    DCHECK(segment);
    // "さんびゃく"
    segment->set_key(
        "\xe3\x81\x95\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x83\xe3\x81\x8f");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate = segment->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetNumberId();
    candidate->rid = pos_matcher_->GetNumberId();
    // "さんびゃく"
    candidate->key =
        "\xe3\x81\x95\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x83\xe3\x81\x8f";
    // "三百"
    candidate->value = "\xe4\xb8\x89\xe7\x99\xbe";
    // "三百"
    candidate->content_value = "\xe4\xb8\x89\xe7\x99\xbe";
  }

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  EXPECT_NE(0, segments.segments_size());
  int kanji_pos = 0, arabic_pos = 0;
  // "三百"
  EXPECT_TRUE(FindCandidateId(segments.segment(0),
                              "\xe4\xb8\x89\xe7\x99\xbe", &kanji_pos));
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "300", &arabic_pos));
  EXPECT_LT(kanji_pos, arabic_pos);
}

TEST_F(NumberRewriterTest, ModifyExsistingRanking) {
  // Modify exsisting ranking even if the converter returns unusual results
  // due to dictionary noise, etc.
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *segment = segments.add_segment();
    DCHECK(segment);
    // "さんびゃく"
    segment->set_key(
        "\xe3\x81\x95\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x83\xe3\x81\x8f");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetNumberId();
    candidate->rid = pos_matcher_->GetNumberId();
    // "さんびゃく"
    candidate->key =
        "\xe3\x81\x95\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x83\xe3\x81\x8f";
    // "参百"
    candidate->value = "\xe5\x8f\x82\xe7\x99\xbe";
    // "参百"
    candidate->content_value = "\xe5\x8f\x82\xe7\x99\xbe";

    candidate = segment->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetNumberId();
    candidate->rid = pos_matcher_->GetNumberId();
    // "さんびゃく"
    candidate->key =
        "\xe3\x81\x95\xe3\x82\x93\xe3\x81\xb3\xe3\x82\x83\xe3\x81\x8f";
    // "三百"
    candidate->value = "\xe4\xb8\x89\xe7\x99\xbe";
    // "三百"
    candidate->content_value = "\xe4\xb8\x89\xe7\x99\xbe";
  }

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
  int kanji_pos = 0, old_kanji_pos = 0;
  EXPECT_NE(0, segments.segments_size());
  // "三百"
  EXPECT_TRUE(FindCandidateId(segments.segment(0),
                              "\xe4\xb8\x89\xe7\x99\xbe", &kanji_pos));
  // "参百"
  EXPECT_TRUE(FindCandidateId(segments.segment(0),
                              "\xe5\x8f\x82\xe7\x99\xbe", &old_kanji_pos));
  EXPECT_LT(kanji_pos, old_kanji_pos);
}

TEST_F(NumberRewriterTest, EraseExistingCandidates) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  {
    Segment *segment = segments.add_segment();
    DCHECK(segment);
    // "いち"
    segment->set_key("\xe3\x81\x84\xe3\x81\xa1");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetUnknownId();  // Not number POS
    candidate->rid = pos_matcher_->GetUnknownId();
    // "いち"
    candidate->key = "\xe3\x81\x84\xe3\x81\xa1";
    // "いち"
    candidate->content_key = "\xe3\x81\x84\xe3\x81\xa1";
    // "壱"
    candidate->value = "\xe5\xa3\xb1";
    // "壱"
    candidate->content_value = "\xe5\xa3\xb1";

    candidate = segment->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetNumberId();  // Number POS
    candidate->rid = pos_matcher_->GetNumberId();
    // "いち"
    candidate->key = "\xe3\x81\x84\xe3\x81\xa1";
    // "いち"
    candidate->content_key = "\xe3\x81\x84\xe3\x81\xa1";
    // "一"
    candidate->value = "\xe4\xb8\x80";
    // "一"
    candidate->content_value = "\xe4\xb8\x80";
  }

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));

  // "一" become the base candidate, instead of "壱"
  int base_pos = 0;
  // "一"
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "\xe4\xb8\x80", &base_pos));
  EXPECT_EQ(0, base_pos);

  // Daiji will be inserted with new correct POS ids.
  int daiji_pos = 0;
  // "壱"
  EXPECT_TRUE(FindCandidateId(segments.segment(0), "\xe5\xa3\xb1", &daiji_pos));
  EXPECT_GT(daiji_pos, 0);
  EXPECT_EQ(pos_matcher_->GetNumberId(),
            segments.segment(0).candidate(daiji_pos).lid);
  EXPECT_EQ(pos_matcher_->GetNumberId(),
            segments.segment(0).candidate(daiji_pos).rid);
}

TEST_F(NumberRewriterTest, SeparatedArabicsTest) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  // Expected data to succeed tests.
  const char* kSuccess[][3] = {
    // "１，０００"
    {"1000", "1,000",
     "\xef\xbc\x91\xef\xbc\x8c\xef\xbc\x90\xef\xbc\x90\xef\xbc\x90"},
    // "１２，３４５，６７８"
    {"12345678", "12,345,678",
     "\xef\xbc\x91\xef\xbc\x92\xef\xbc\x8c\xef\xbc\x93\xef\xbc\x94"
     "\xef\xbc\x95\xef\xbc\x8c\xef\xbc\x96\xef\xbc\x97\xef\xbc\x98"},
    // "１，２３４．５"
    {"1234.5", "1,234.5", "\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x92\xEF\xBC\x93"
     "\xEF\xBC\x94\xEF\xBC\x8E\xEF\xBC\x95"},
  };

  for (size_t i = 0; i < arraysize(kSuccess); ++i) {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetNumberId();
    candidate->rid = pos_matcher_->GetNumberId();
    candidate->value = kSuccess[i][0];
    candidate->content_value = kSuccess[i][0];
    EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
    EXPECT_TRUE(FindValue(segments.segment(0), kSuccess[i][1]))
        << "Input : " << kSuccess[i][0];
    EXPECT_TRUE(FindValue(segments.segment(0), kSuccess[i][2]))
        << "Input : " << kSuccess[i][0];
  }

  // Expected data to fail tests.
  const char* kFail[][3] = {
    // "，１２３"
    {"123", ",123", "\xef\xbc\x8c\xef\xbc\x91\xef\xbc\x92\xef\xbc\x93"},
    // "，９９９"
    {"999", ",999", "\xef\xbc\x8c\xef\xbc\x99\xef\xbc\x99\xef\xbc\x99"},
    {"0000", "0,000",
    // "０，０００"
     "\xef\xbc\x90\xef\xbc\x8c\xef\xbc\x90\xef\xbc\x90\xef\xbc\x90"},
  };

  for (size_t i = 0; i < arraysize(kFail); ++i) {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetNumberId();
    candidate->rid = pos_matcher_->GetNumberId();
    candidate->value = kFail[i][0];
    candidate->content_value = kFail[i][0];
    EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
    EXPECT_FALSE(FindValue(segments.segment(0), kFail[i][1]))
        << "Input : " << kFail[i][0];
    EXPECT_FALSE(FindValue(segments.segment(0), kFail[i][2]))
        << "Input : " << kFail[i][0];
  }
}

// Consider the case where user dictionaries contain following entry.
// - Reading: "はやぶさ"
// - Value: "8823"
// - POS: GeneralNoun (not *Number*)
// In this case, NumberRewriter should not clear
// Segment::Candidate::USER_DICTIONARY bit in the base candidate.
TEST_F(NumberRewriterTest, PreserveUserDictionaryAttibute) {
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  {
    Segments segments;
    {
      Segment *seg = segments.push_back_segment();
      Segment::Candidate *candidate = seg->add_candidate();
      candidate->Init();
      candidate->lid = pos_matcher_->GetGeneralNounId();
      candidate->rid = pos_matcher_->GetGeneralNounId();
      // "はやぶさ"
      candidate->key = "\xE3\x81\xAF\xE3\x82\x84\xE3\x81\xB6\xE3\x81\x95";
      candidate->content_key = candidate->key;
      candidate->value = "8823";
      candidate->content_value = candidate->value;
      candidate->cost = 5925;
      candidate->wcost = 5000;
      candidate->attributes =
          Segment::Candidate::USER_DICTIONARY |
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
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());

  Segments segments;
  Segment *segment = segments.push_back_segment();
  for (int i = 0; i < 20; ++i) {
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->lid = pos_matcher_->GetUnknownId();  // Not number POS
    candidate->rid = pos_matcher_->GetUnknownId();
    // "いち"
    candidate->key = "\xe3\x81\x84\xe3\x81\xa1";
    // "いち"
    candidate->content_key = "\xe3\x81\x84\xe3\x81\xa1";
    // "壱"
    candidate->value = "\xe5\xa3\xb1";
    // "壱"
    candidate->content_value = "\xe5\xa3\xb1";
  }
  for (int i = 0; i < 20; ++i) {
    Segment::Candidate *candidate = segment->add_candidate();

    candidate->Init();
    candidate->lid = pos_matcher_->GetNumberId();  // Number POS
    candidate->rid = pos_matcher_->GetNumberId();
    // "いち"
    candidate->key = "\xe3\x81\x84\xe3\x81\xa1";
    // "いち"
    candidate->content_key = "\xe3\x81\x84\xe3\x81\xa1";
    // "一"
    candidate->value = "\xe4\xb8\x80";
    // "一"
    candidate->content_value = "\xe4\xb8\x80";
  }

  EXPECT_TRUE(number_rewriter->Rewrite(default_request_, &segments));
}

TEST_F(NumberRewriterTest, MobileEnvironmentTest) {
  commands::Request input;
  scoped_ptr<NumberRewriter> rewriter(CreateNumberRewriter());

  {
    input.set_mixed_conversion(true);
    const ConversionRequest request(NULL, &input);
    EXPECT_EQ(RewriterInterface::ALL, rewriter->capability(request));
  }

  {
    input.set_mixed_conversion(false);
    const ConversionRequest request(NULL, &input);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter->capability(request));
  }
}

TEST_F(NumberRewriterTest, NonNumberNounTest) {
  // Test if "百舌鳥" is not rewritten to "100舌鳥", etc.
  scoped_ptr<NumberRewriter> number_rewriter(CreateNumberRewriter());
  Segments segments;
  Segment *segment = segments.push_back_segment();
  segment->set_key("\xE3\x82\x82\xE3\x81\x9A");  // "もず"
  Segment::Candidate *cand = segment->add_candidate();
  cand->Init();
  cand->key = "\xE3\x82\x82\xE3\x81\x9A";  // "もず"
  cand->content_key = cand->key;
  cand->value = "\xE7\x99\xBE\xE8\x88\x8C\xE9\xB3\xA5";  // "百舌鳥"
  cand->content_value = cand->value;
  cand->lid = pos_matcher_->GetGeneralNounId();
  cand->rid = pos_matcher_->GetGeneralNounId();
  EXPECT_FALSE(number_rewriter->Rewrite(default_request_, &segments));
}

}  // namespace mozc
