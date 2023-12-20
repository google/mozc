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

#ifndef MOZC_USAGE_STATS_USAGE_STATS_TESTING_UTIL_H_
#define MOZC_USAGE_STATS_USAGE_STATS_TESTING_UTIL_H_

#include <cstdint>
#include <memory>

#include "absl/strings/string_view.h"
#include "config/stats_config_util_mock.h"
#include "testing/gunit.h"

namespace mozc {
namespace usage_stats {
namespace internal {
::testing::AssertionResult ExpectStatsExist(absl::string_view name_string,
                                            absl::string_view param_string,
                                            absl::string_view name,
                                            bool expected);

::testing::AssertionResult ExpectCountStats(absl::string_view name_string,
                                            absl::string_view expected_string,
                                            absl::string_view name,
                                            uint32_t expected);

::testing::AssertionResult ExpectIntegerStats(absl::string_view name_string,
                                              absl::string_view expected_string,
                                              absl::string_view name,
                                              int32_t expected);

::testing::AssertionResult ExpectBooleanStats(absl::string_view name_string,
                                              absl::string_view expected_string,
                                              absl::string_view name,
                                              bool expected);

::testing::AssertionResult ExpectTimingStats(
    absl::string_view name_string, absl::string_view expected_total_string,
    absl::string_view expected_num_string,
    absl::string_view expected_min_string,
    absl::string_view expected_max_string, absl::string_view name,
    uint64_t expected_total, uint32_t expected_num, uint32_t expected_min,
    uint32_t expected_max);
}  // namespace internal

#define EXPECT_STATS_EXIST(name) \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectStatsExist, name, true)

#define EXPECT_STATS_NOT_EXIST(name)                                       \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectStatsExist, name, \
                      false)

#define EXPECT_COUNT_STATS(name, expected)                                 \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectCountStats, name, \
                      expected)

#define EXPECT_INTEGER_STATS(name, expected)                                 \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectIntegerStats, name, \
                      expected)

#define EXPECT_BOOLEAN_STATS(name, expected)                                 \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectBooleanStats, name, \
                      expected)

#define EXPECT_TIMING_STATS(name, expected_total, expected_num, expected_min, \
                            expected_max)                                     \
  EXPECT_PRED_FORMAT5(mozc::usage_stats::internal::ExpectTimingStats, name,   \
                      expected_total, expected_num, expected_min,             \
                      expected_max)

class scoped_usage_stats_enabler {
 public:
  scoped_usage_stats_enabler();
  scoped_usage_stats_enabler(const scoped_usage_stats_enabler &) = delete;
  scoped_usage_stats_enabler &operator=(const scoped_usage_stats_enabler &) =
      delete;
  ~scoped_usage_stats_enabler();

 private:
  std::unique_ptr<mozc::config::StatsConfigUtilMock> stats_config_util_;
};

}  // namespace usage_stats
}  // namespace mozc

#endif  // MOZC_USAGE_STATS_USAGE_STATS_TESTING_UTIL_H_
