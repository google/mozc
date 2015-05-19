// Copyright 2010-2013, Google Inc.
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

#include <cstring>
#include <string>
#include <vector>

#include "base/number_util.h"
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

namespace {

string GetExistFailure(const string &name, Stats stats) {
  string type_string, value_string;
  switch (stats.type()) {
    case Stats::COUNT:
      type_string = "Count";
      value_string = NumberUtil::SimpleItoa(stats.count());
      break;
    case Stats::INTEGER:
      type_string = "Integer";
      value_string = NumberUtil::SimpleItoa(stats.int_value());
      break;
    case Stats::BOOLEAN:
      type_string = "Boolean";
      value_string = stats.boolean_value() ? "True" : "False";
      break;
    case Stats::TIMING:
      type_string = "Timing";
      value_string = string() +
          "num:" + NumberUtil::SimpleItoa(stats.num_timings()) + " " +
          "total:" + NumberUtil::SimpleItoa(stats.total_time()) + " " +
          "min:" + NumberUtil::SimpleItoa(stats.min_time()) + " " +
          "max:" + NumberUtil::SimpleItoa(stats.max_time());
      break;
    case Stats::VIRTUAL_KEYBOARD:
      type_string = "Virtual Keyboard";
      break;
  }

  string message =  "Usage stats \"" + name + "\" exists.\n" +
      " type: " + type_string + "\n";
  if (!value_string.empty()) {
    message += "value: " + value_string;
  }
  return message;
}

string GetNotExistFailure(const string &name) {
  return "Usage stats \"" + name + "\" doesn't exist.";
}

string GetError(const string &name, const string &expected_string,
                const string &expected, const string &actual) {
  string message = "Usage stats \"" + name + "\" doesn't match.\n" +
      "  Actual: " + actual + "\n";
  if (expected_string != expected) {
    message += "Value of: " + expected_string + "\n";
  }
  message += "Expected: " + expected;
  return message;
}

string GetNumberError(const string &name, const string &expected_string,
                      int expected, int actual) {
  return GetError(name, expected_string, NumberUtil::SimpleItoa(expected),
                  NumberUtil::SimpleItoa(actual));
}

string GetBooleanError(const string &name, const string &expected_string,
                       bool expected, bool actual) {
  const string expected_value = expected ? "true" : "false";
  const string actual_value = actual ? "true" : "false";
  return GetError(name, expected_string, expected_value, actual_value);
}

}  // namespace

::testing::AssertionResult ExpectStatsExist(const char *name_string,
                                            const char *param_string,
                                            const string &name,
                                            bool expected) {
  Stats stats;
  if (UsageStats::GetStatsForTest(name, &stats) == expected) {
    return AssertionSuccess();
  }

  if (expected) {
    return AssertionFailure() << GetNotExistFailure(name);
  } else {
    return AssertionFailure() << GetExistFailure(name, stats);
  }
}

::testing::AssertionResult ExpectCountStats(const char *name_string,
                                            const char *expected_string,
                                            const string &name,
                                            uint32 expected) {
  uint32 actual = 0;
  if (!UsageStats::GetCountForTest(name, &actual)) {
    if (expected == 0) {
      return AssertionSuccess();
    } else {
      return AssertionFailure() << GetNotExistFailure(name);
    }
  }

  if (actual == expected) {
    return AssertionSuccess();
  } else {
    return AssertionFailure()
        << GetNumberError(name, expected_string, expected, actual);
  }
}

::testing::AssertionResult ExpectIntegerStats(const char *name_string,
                                              const char *expected_string,
                                              const string &name,
                                              int32 expected) {
  int32 actual = 0;
  if (!UsageStats::GetIntegerForTest(name, &actual)) {
    return AssertionFailure() << GetNotExistFailure(name);
  }

  if (actual == expected) {
    return AssertionSuccess();
  } else {
    return AssertionFailure()
        << GetNumberError(name, expected_string, expected, actual);
  }
}

::testing::AssertionResult ExpectBooleanStats(const char *name_string,
                                              const char *expected_string,
                                              const string &name,
                                              bool expected) {
  bool actual = false;
  if (!UsageStats::GetBooleanForTest(name, &actual)) {
    return AssertionFailure() << GetNotExistFailure(name);
  }

  if (actual == expected) {
    return AssertionSuccess();
  } else {
    return AssertionFailure()
        << GetBooleanError(name, expected_string, expected, actual);
  }
}

::testing::AssertionResult ExpectTimingStats(const char *name_string,
                                             const char *expected_total_string,
                                             const char *expected_num_string,
                                             const char *expected_min_string,
                                             const char *expected_max_string,
                                             const string &name,
                                             uint64 expected_total,
                                             uint32 expected_num,
                                             uint32 expected_min,
                                             uint32 expected_max) {
  uint64 actual_total = 0;
  uint32 actual_num = 0;
  uint32 actual_min = 0;
  uint32 actual_max = 0;
  vector<string> errors;
  if (!UsageStats::GetTimingForTest(name, &actual_total, &actual_num, NULL,
                                    &actual_min, &actual_max)) {
    if (expected_num == 0) {
      return AssertionSuccess();
    } else {
      return AssertionFailure() << GetNotExistFailure(name);
    }
  }

  if (actual_total != expected_total) {
    errors.push_back(GetNumberError(name + ":total", expected_total_string,
                                    expected_total, actual_total));
  }
  if (actual_num != expected_num) {
    errors.push_back(GetNumberError(name + ":num", expected_num_string,
                                    expected_num, actual_num));
  }
  if (actual_min != expected_min) {
    errors.push_back(GetNumberError(name + ":min", expected_min_string,
                                    expected_min, actual_min));
  }
  if (actual_max != expected_max) {
    errors.push_back(GetNumberError(name + ":max", expected_max_string,
                                    expected_max, actual_max));
  }

  if (errors.empty()) {
    return AssertionSuccess();
  } else {
    string message;
    Util::JoinStrings(errors, "\n", &message);
    return AssertionFailure() << message;
  }
}

}  // namespace internal

scoped_usage_stats_enabler::scoped_usage_stats_enabler()
    : stats_config_util_(new mozc::config::StatsConfigUtilMock) {
  mozc::config::StatsConfigUtil::SetHandler(stats_config_util_.get());
}

scoped_usage_stats_enabler::~scoped_usage_stats_enabler() {
  mozc::config::StatsConfigUtil::SetHandler(NULL);
}

}  // namespace usage_stats
}  // namespace mozc
