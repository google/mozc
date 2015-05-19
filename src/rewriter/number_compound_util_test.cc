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

#include "rewriter/number_compound_util.h"

#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/counter_suffix.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace number_compound_util {

TEST(NumberCompoundUtilTest, SplitStringIntoNumberAndCounterSuffix) {
  const CounterSuffixEntry kCounterSuffixSortedArray[] = {
    {
      // "デシベル"
      "\xE3\x83\x87\xE3\x82\xB7\xE3\x83\x99\xE3\x83\xAB", 12,
    },
    {
      // "回"
      "\xE5\x9B\x9E", 3,
    },
    {
      // "階"
      "\xE9\x9A\x8E", 3,
    },
  };

  // Test cases for splittable compounds.
  struct {
    const char* input;
    const char* expected_number;
    const char* expected_suffix;
    uint32 expected_script_type;
  } kSplittableCases[] = {
    {
      "\xE4\xB8\x80\xE9\x9A\x8E",  //"一階",
      "\xE4\xB8\x80",  // "一"
      "\xE9\x9A\x8E",  // "階"
      number_compound_util::KANJI,
    },
    {
      "\xE5\xA3\xB1\xE9\x9A\x8E",  //"壱階",
      "\xE5\xA3\xB1",  // "壱"
      "\xE9\x9A\x8E",  // "階"
      number_compound_util::OLD_KANJI,
    },
    {
      "\xE4\xB8\x89\xE5\x8D\x81\xE4\xB8\x80\xE5\x9B\x9E",  // "三十一回"
      "\xE4\xB8\x89\xE5\x8D\x81\xE4\xB8\x80",  // "三十一",
      "\xE5\x9B\x9E",  //"回"
      number_compound_util::KANJI,
    },
    {
      "\xE4\xB8\x89\xE5\x8D\x81\xE4\xB8\x80",  // "三十一"
      "\xE4\xB8\x89\xE5\x8D\x81\xE4\xB8\x80",  // "三十一",
      "",
      number_compound_util::KANJI,
    },
    {
      "\xE3\x83\x87\xE3\x82\xB7\xE3\x83\x99\xE3\x83\xAB",  // "デシベル"
      "",
      "\xE3\x83\x87\xE3\x82\xB7\xE3\x83\x99\xE3\x83\xAB",  // "デシベル"
    },
    {
      "\xE5\x9B\x9E",  // "回"
      "",
      "\xE5\x9B\x9E",  // "回"
    },
    {
      "\xE9\x9A\x8E",  // "階"
      "",
      "\xE9\x9A\x8E",  // "階"
    },
  };
  for (size_t i = 0; i < arraysize(kSplittableCases); ++i) {
    StringPiece actual_number, actual_suffix;
    uint32 actual_script_type = 0;
    EXPECT_TRUE(SplitStringIntoNumberAndCounterSuffix(
        kCounterSuffixSortedArray, arraysize(kCounterSuffixSortedArray),
        kSplittableCases[i].input, &actual_number, &actual_suffix,
        &actual_script_type));
    EXPECT_EQ(kSplittableCases[i].expected_number, actual_number);
    EXPECT_EQ(kSplittableCases[i].expected_suffix, actual_suffix);
    EXPECT_EQ(kSplittableCases[i].expected_script_type, actual_script_type);
  }

  // Test cases for unsplittable compounds.
  const char* kUnsplittableCases[] = {
    "\xE5\x9B\x9E\xE5\x85\xAB",  // "階八"
    "Google",
    "\xE3\x82\xA2\xE4\xB8\x80\xE9\x9A\x8E",  // "ア一階"
      // "八億九千万600七十４デシベル"
      "\xE5\x85\xAB\xE5\x84\x84\xE4\xB9\x9D\xE5\x8D\x83\xE4\xB8\x87"
      "\x36\x30\x30\xE4\xB8\x83\xE5\x8D\x81\xEF\xBC\x94\xE3\x83\x87"
      "\xE3\x82\xB7\xE3\x83\x99\xE3\x83\xAB",
  };
  for (size_t i = 0; i < arraysize(kUnsplittableCases); ++i) {
    StringPiece actual_number, actual_suffix;
    uint32 actual_script_type = 0;
    EXPECT_FALSE(SplitStringIntoNumberAndCounterSuffix(
        kCounterSuffixSortedArray, arraysize(kCounterSuffixSortedArray),
        kUnsplittableCases[i], &actual_number, &actual_suffix,
        &actual_script_type));
  }
}

TEST(NumberCompoundUtilTest, IsNumber) {
  const CounterSuffixEntry kCounterSuffixSortedArray[] = {
    {
      // "回"
      "\xE5\x9B\x9E", 3,
    },
    {
      // "階"
      "\xE9\x9A\x8E", 3,
    },
  };
  const testing::MockDataManager data_manager;
  const POSMatcher *pos_matcher = data_manager.GetPOSMatcher();

  Segment::Candidate c;

  c.Init();
  c.lid = pos_matcher->GetNumberId();
  c.rid = pos_matcher->GetNumberId();
  EXPECT_TRUE(IsNumber(kCounterSuffixSortedArray,
                       arraysize(kCounterSuffixSortedArray),
                       *pos_matcher, c));

  c.Init();
  c.lid = pos_matcher->GetKanjiNumberId();
  c.rid = pos_matcher->GetKanjiNumberId();
  EXPECT_TRUE(IsNumber(kCounterSuffixSortedArray,
                       arraysize(kCounterSuffixSortedArray),
                       *pos_matcher, c));

  c.Init();
  c.lid = pos_matcher->GetNumberId();
  c.rid = pos_matcher->GetCounterSuffixWordId();
  EXPECT_TRUE(IsNumber(kCounterSuffixSortedArray,
                       arraysize(kCounterSuffixSortedArray),
                       *pos_matcher, c));

  c.Init();
  c.lid = pos_matcher->GetNumberId();
  c.rid = pos_matcher->GetParallelMarkerId();
  EXPECT_TRUE(IsNumber(kCounterSuffixSortedArray,
                       arraysize(kCounterSuffixSortedArray),
                       *pos_matcher, c));

  c.Init();
  c.value = "\xE4\xB8\x80\xE9\x9A\x8E";  //"一階"
  c.content_value = "\xE4\xB8\x80\xE9\x9A\x8E";  //"一階"
  c.lid = pos_matcher->GetNumberId();
  c.rid = pos_matcher->GetNumberId();
  EXPECT_TRUE(IsNumber(kCounterSuffixSortedArray,
                       arraysize(kCounterSuffixSortedArray),
                       *pos_matcher, c));

  c.Init();
  c.lid = pos_matcher->GetAdverbId();
  c.rid = pos_matcher->GetAdverbId();
  EXPECT_FALSE(IsNumber(kCounterSuffixSortedArray,
                        arraysize(kCounterSuffixSortedArray),
                        *pos_matcher, c));
}

}  // namespace number_compound_util
}  // namespace mozc
