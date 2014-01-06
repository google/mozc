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

#ifndef MOZC_USAGE_STATS_USAGE_STATS_TESTING_UTIL_H_
#define MOZC_USAGE_STATS_USAGE_STATS_TESTING_UTIL_H_

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace config {
class StatsConfigUtilMock;
}  // namespace config

namespace usage_stats {

namespace internal {
::testing::AssertionResult ExpectStatsExist(const char *name_string,
                                            const char *param_string,
                                            const string &name,
                                            bool expected);

::testing::AssertionResult ExpectCountStats(const char *name_string,
                                            const char *expected_string,
                                            const string &name,
                                            uint32 expected);

::testing::AssertionResult ExpectIntegerStats(const char *name_string,
                                              const char *expected_string,
                                              const string &name,
                                              int32 expected);

::testing::AssertionResult ExpectBooleanStats(const char *name_string,
                                              const char *expected_string,
                                              const string &name,
                                              bool expected);

::testing::AssertionResult ExpectTimingStats(const char *name_string,
                                             const char *expected_total_string,
                                             const char *expected_num_string,
                                             const char *expected_min_string,
                                             const char *expected_max_string,
                                             const string &name,
                                             uint64 expected_total,
                                             uint32 expected_num,
                                             uint32 expected_min,
                                             uint32 expected_max);
}  // namespace internal

#define EXPECT_STATS_EXIST(name) \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectStatsExist, \
                      name, true)

#define EXPECT_STATS_NOT_EXIST(name) \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectStatsExist, \
                      name, false)

#define EXPECT_COUNT_STATS(name, expected) \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectCountStats, \
                      name, expected)

#define EXPECT_INTEGER_STATS(name, expected) \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectIntegerStats, \
                      name, expected)

#define EXPECT_BOOLEAN_STATS(name, expected) \
  EXPECT_PRED_FORMAT2(mozc::usage_stats::internal::ExpectBooleanStats, \
                      name, expected)

#define EXPECT_TIMING_STATS(name, expected_total, expected_num, expected_min, \
                            expected_max) \
  EXPECT_PRED_FORMAT5(mozc::usage_stats::internal::ExpectTimingStats, \
                      name, expected_total, expected_num, expected_min, \
                      expected_max)

class scoped_usage_stats_enabler {
 public:
  scoped_usage_stats_enabler();
  ~scoped_usage_stats_enabler();

 private:
  scoped_ptr<mozc::config::StatsConfigUtilMock> stats_config_util_;
  DISALLOW_COPY_AND_ASSIGN(scoped_usage_stats_enabler);
};

}  // namespace usage_stats
}  // namespace mozc

#endif  // MOZC_USAGE_STATS_USAGE_STATS_TESTING_UTIL_H_
