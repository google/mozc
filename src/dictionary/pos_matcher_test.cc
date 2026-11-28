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

#include "dictionary/pos_matcher.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "data_manager/testing/mock_data_manager.h"
#include "testing/gunit.h"

namespace mozc::dictionary {
namespace {

TEST(PosMatcherTest, ValidMockDataPassesVerificationAndLookups) {
  const testing::MockDataManager data_manager;
  EXPECT_TRUE(PosMatcher::VerifyData(data_manager.GetPosMatcherData()));

  const PosMatcher matcher(data_manager.GetPosMatcherData());
  const uint16_t unknown_id = matcher.GetUnknownId();
  EXPECT_TRUE(matcher.IsUnknown(unknown_id));
}

TEST(PosMatcherTest, VerifyDataRejectsMalformedTables) {
  const size_t num_rules = PosMatcher::num_rules();
  ASSERT_GT(num_rules, 1u);

  EXPECT_FALSE(PosMatcher::VerifyData({}));

  // Shorter than 2 * num_rules header.
  std::vector<uint16_t> truncated(2 * num_rules - 1, 0);
  EXPECT_FALSE(PosMatcher::VerifyData(truncated));

  // Valid minimal table: each rule has its own contiguous 0xFFFF sentinel.
  std::vector<uint16_t> valid(3 * num_rules, 0);
  for (size_t i = 0; i < num_rules; ++i) {
    valid[num_rules + i] = static_cast<uint16_t>(2 * num_rules + i);
    valid[2 * num_rules + i] = 0xFFFFu;
  }
  EXPECT_TRUE(PosMatcher::VerifyData(valid));

  // Missing 0xFFFF sentinel at end of last rule's range list.
  const size_t last_rule_pos = 3 * num_rules - 1;
  std::vector<uint16_t> missing_sentinel = valid;
  missing_sentinel[last_rule_pos] = 10;
  EXPECT_FALSE(PosMatcher::VerifyData(missing_sentinel));

  // Incomplete pair before 0xFFFF sentinel (only 1 element instead of 2).
  std::vector<uint16_t> half_pair = valid;
  half_pair[last_rule_pos] = 10;
  half_pair.push_back(0xFFFFu);
  EXPECT_FALSE(PosMatcher::VerifyData(half_pair));

  // Complete pair followed by 0xFFFF sentinel succeeds.
  std::vector<uint16_t> one_pair = valid;
  one_pair[last_rule_pos] = 10;
  one_pair.push_back(20);
  one_pair.push_back(0xFFFFu);
  EXPECT_TRUE(PosMatcher::VerifyData(one_pair));

  // Offset pointing inside header (< 2 * num_rules) or past data.size() fails.
  std::vector<uint16_t> offset_in_header = valid;
  offset_in_header[num_rules] = 0;
  EXPECT_FALSE(PosMatcher::VerifyData(offset_in_header));

  std::vector<uint16_t> offset_past_end = valid;
  offset_past_end[num_rules] = static_cast<uint16_t>(valid.size());
  EXPECT_FALSE(PosMatcher::VerifyData(offset_past_end));

  // Overlapping / non-monotonic rule offsets fail.
  std::vector<uint16_t> shared_offset = valid;
  shared_offset[num_rules + 1] = shared_offset[num_rules];
  EXPECT_FALSE(PosMatcher::VerifyData(shared_offset));

  // Trailing data after the last rule's sentinel fails.
  std::vector<uint16_t> trailing_data = valid;
  trailing_data.push_back(0xFFFFu);
  EXPECT_FALSE(PosMatcher::VerifyData(trailing_data));
}

TEST(PosMatcherTest, DirectConstructionGuardsAgainstEmptyAndTruncatedSpans) {
  const PosMatcher empty_matcher(absl::Span<const uint16_t>{});
  EXPECT_EQ(empty_matcher.GetUnknownId(), 0);
  EXPECT_EQ(empty_matcher.GetSuggestOnlyWordId(), 0);
  EXPECT_FALSE(empty_matcher.IsUnknown(0));

  const size_t num_rules = PosMatcher::num_rules();

  // Partially truncated header: rule ID table is present, but offset table is
  // missing (offset_index >= data_.size()).
  std::vector<uint16_t> truncated_header(num_rules, 42);
  const PosMatcher truncated_header_matcher(truncated_header);
  EXPECT_EQ(truncated_header_matcher.GetUnknownId(), 42);
  EXPECT_FALSE(truncated_header_matcher.IsUnknown(42));

  // Unverified table whose rule offsets point past data_.size().
  std::vector<uint16_t> out_of_range_offset(2 * num_rules + 1, 0);
  for (size_t i = 0; i < num_rules; ++i) {
    out_of_range_offset[num_rules + i] =
        static_cast<uint16_t>(out_of_range_offset.size());
  }
  const PosMatcher out_of_range_matcher(out_of_range_offset);
  EXPECT_FALSE(out_of_range_matcher.IsUnknown(0));

  // Unverified table ending with an incomplete pair (1 element, no sentinel),
  // exercising the pos + 1 >= data_.size() guard.
  std::vector<uint16_t> half_pair_no_sentinel(2 * num_rules + 1, 5);
  for (size_t i = 0; i < num_rules; ++i) {
    half_pair_no_sentinel[num_rules + i] = static_cast<uint16_t>(2 * num_rules);
  }
  const PosMatcher half_pair_matcher(half_pair_no_sentinel);
  EXPECT_FALSE(half_pair_matcher.IsUnknown(5));

  // Unverified table with a complete pair and no 0xFFFF sentinel.
  std::vector<uint16_t> no_sentinel(2 * num_rules + 2, 5);
  for (size_t i = 0; i < num_rules; ++i) {
    no_sentinel[num_rules + i] = static_cast<uint16_t>(2 * num_rules);
  }
  const PosMatcher unverified_matcher(no_sentinel);
  EXPECT_TRUE(unverified_matcher.IsUnknown(5));
  EXPECT_FALSE(unverified_matcher.IsUnknown(99));
}

}  // namespace
}  // namespace mozc::dictionary
