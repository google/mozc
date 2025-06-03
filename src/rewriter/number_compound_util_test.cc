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

#include "rewriter/number_compound_util.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>

#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "converter/candidate.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "testing/gunit.h"

using mozc::dictionary::PosMatcher;

namespace mozc {
namespace number_compound_util {

TEST(NumberCompoundUtilTest, SplitStringIntoNumberAndCounterSuffix) {
  std::unique_ptr<uint32_t[]> buf;
  const absl::string_view data = SerializedStringArray::SerializeToBuffer(
      {
          "デシベル",
          "回",
          "階",
      },
      &buf);
  SerializedStringArray suffix_array;
  ASSERT_TRUE(suffix_array.Init(data));

  // Test cases for splittable compounds.
  struct {
    const char* input;
    const char* expected_number;
    const char* expected_suffix;
    uint32_t expected_script_type;
  } kSplittableCases[] = {
      {
          "一階",
          "一",
          "階",
          number_compound_util::KANJI,
      },
      {
          "壱階",
          "壱",
          "階",
          number_compound_util::OLD_KANJI,
      },
      {
          "三十一回",
          "三十一",
          "回",
          number_compound_util::KANJI,
      },
      {
          "三十一",
          "三十一",
          "",
          number_compound_util::KANJI,
      },
      {
          "デシベル",
          "",
          "デシベル",
      },
      {
          "回",
          "",
          "回",
      },
      {
          "階",
          "",
          "階",
      },
  };
  for (size_t i = 0; i < std::size(kSplittableCases); ++i) {
    absl::string_view actual_number, actual_suffix;
    uint32_t actual_script_type = 0;
    EXPECT_TRUE(SplitStringIntoNumberAndCounterSuffix(
        suffix_array, kSplittableCases[i].input, &actual_number, &actual_suffix,
        &actual_script_type));
    EXPECT_EQ(actual_number, kSplittableCases[i].expected_number);
    EXPECT_EQ(actual_suffix, kSplittableCases[i].expected_suffix);
    EXPECT_EQ(actual_script_type, kSplittableCases[i].expected_script_type);
  }

  // Test cases for unsplittable compounds.
  const char* kUnsplittableCases[] = {
      "回八",
      "Google",
      "ア一階",
      "八億九千万600七十４デシベル",
  };
  for (size_t i = 0; i < std::size(kUnsplittableCases); ++i) {
    absl::string_view actual_number, actual_suffix;
    uint32_t actual_script_type = 0;
    EXPECT_FALSE(SplitStringIntoNumberAndCounterSuffix(
        suffix_array, kUnsplittableCases[i], &actual_number, &actual_suffix,
        &actual_script_type));
  }
}

TEST(NumberCompoundUtilTest, IsNumber) {
  std::unique_ptr<uint32_t[]> buf;
  const absl::string_view data = SerializedStringArray::SerializeToBuffer(
      {
          // Should be sorted
          "回",
          "行",
          "階",
      },
      &buf);
  SerializedStringArray suffix_array;
  ASSERT_TRUE(suffix_array.Init(data));

  const testing::MockDataManager data_manager;
  const PosMatcher pos_matcher(data_manager.GetPosMatcherData());

  converter::Candidate c;

  c.lid = pos_matcher.GetNumberId();
  c.rid = pos_matcher.GetNumberId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c = converter::Candidate();
  c.lid = pos_matcher.GetKanjiNumberId();
  c.rid = pos_matcher.GetKanjiNumberId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c = converter::Candidate();
  c.lid = pos_matcher.GetNumberId();
  c.rid = pos_matcher.GetCounterSuffixWordId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c = converter::Candidate();
  c.lid = pos_matcher.GetNumberId();
  c.rid = pos_matcher.GetParallelMarkerId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c = converter::Candidate();
  c.value = "一階";
  c.content_value = "一階";
  c.lid = pos_matcher.GetNumberId();
  c.rid = pos_matcher.GetNumberId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c = converter::Candidate();
  c.lid = pos_matcher.GetAdverbId();
  c.rid = pos_matcher.GetAdverbId();
  EXPECT_FALSE(IsNumber(suffix_array, pos_matcher, c));

  c = converter::Candidate();
  c.key = "いちぎょう";
  c.content_key = "いちぎょう";
  c.value = "一行";
  c.content_value = "一行";
  c.lid = pos_matcher.GetGeneralNounId();
  c.rid = pos_matcher.GetGeneralNounId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  // Exception entry
  c = converter::Candidate();
  c.key = "いっこう";
  c.content_key = "いっこう";
  c.value = "一行";
  c.content_value = "一行";
  c.lid = pos_matcher.GetGeneralNounId();
  c.rid = pos_matcher.GetGeneralNounId();
  EXPECT_FALSE(IsNumber(suffix_array, pos_matcher, c));
}

}  // namespace number_compound_util
}  // namespace mozc
