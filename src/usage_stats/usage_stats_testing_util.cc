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

#include "usage_stats/usage_stats_testing_util.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/util.h"
#include "config/stats_config_util.h"
#include "config/stats_config_util_mock.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats.pb.h"

using ::testing::AssertionFailure;
using ::testing::AssertionSuccess;

namespace mozc {
namespace usage_stats {
namespace internal {

::testing::AssertionResult ExpectStatsExist(const char *name_string,
                                            const char *param_string,
                                            const std::string &name,
                                            bool expected) {
  // Always returns success for now.
  // TODO(toshiyuki): Remove all caller test code.
  return AssertionSuccess();
}

::testing::AssertionResult ExpectCountStats(const char *name_string,
                                            const char *expected_string,
                                            const std::string &name,
                                            uint32_t expected) {
  // Always returns success for now.
  // TODO(toshiyuki): Remove all caller test code.
  return AssertionSuccess();
}

::testing::AssertionResult ExpectIntegerStats(const char *name_string,
                                              const char *expected_string,
                                              const std::string &name,
                                              int32_t expected) {
  // Always returns success for now.
  // TODO(toshiyuki): Remove all caller test code.
  return AssertionSuccess();
}

::testing::AssertionResult ExpectBooleanStats(const char *name_string,
                                              const char *expected_string,
                                              const std::string &name,
                                              bool expected) {
  // Always returns success for now.
  // TODO(toshiyuki): Remove all caller test code.
  return AssertionSuccess();
}

::testing::AssertionResult ExpectTimingStats(
    const char *name_string, const char *expected_total_string,
    const char *expected_num_string, const char *expected_min_string,
    const char *expected_max_string, const std::string &name,
    uint64_t expected_total, uint32_t expected_num, uint32_t expected_min,
    uint32_t expected_max) {
  // Always returns success for now.
  // TODO(toshiyuki): Remove all caller test code.
  return AssertionSuccess();
}

}  // namespace internal

scoped_usage_stats_enabler::scoped_usage_stats_enabler()
    : stats_config_util_(new mozc::config::StatsConfigUtilMock) {
  mozc::config::StatsConfigUtil::SetHandler(stats_config_util_.get());
}

scoped_usage_stats_enabler::~scoped_usage_stats_enabler() {
  mozc::config::StatsConfigUtil::SetHandler(nullptr);
}

}  // namespace usage_stats
}  // namespace mozc
