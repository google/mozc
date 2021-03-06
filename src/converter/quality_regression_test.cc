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

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "config/config_handler.h"
#include "converter/quality_regression_util.h"
#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/request_test_util.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"

namespace mozc {

// Test data is provided in external file.
struct TestCase {
  const bool enabled;
  const char *line;
};
extern TestCase kTestData[];

namespace {

using quality_regression::QualityRegressionUtil;

class QualityRegressionTest : public ::testing::Test {
 protected:
  static void RunTestForPlatform(uint32_t platform,
                                 QualityRegressionUtil *util) {
    CHECK(util);
    std::map<std::string, std::vector<std::pair<float, std::string>>> results,
        disabled_results;

    int num_executed_cases = 0, num_disabled_cases = 0;
    for (size_t i = 0; kTestData[i].line; ++i) {
      const std::string &tsv_line = kTestData[i].line;
      QualityRegressionUtil::TestItem item;
      CHECK(item.ParseFromTSV(tsv_line));
      if (!(item.platform & platform)) {
        continue;
      }
      std::string actual_value;
      const bool test_result = util->ConvertAndTest(item, &actual_value);

      std::map<std::string, std::vector<std::pair<float, std::string>>> *table =
          nullptr;
      if (kTestData[i].enabled) {
        ++num_executed_cases;
        table = &results;
      } else {
        LOG(INFO) << "DISABLED: " << kTestData[i].line;
        ++num_disabled_cases;
        table = &disabled_results;
      }

      const std::string &label = item.label;
      std::string line = tsv_line;
      line.append("\tActual: ").append(actual_value);
      if (test_result) {
        // use "-1.0" as a dummy expected ratio
        (*table)[label].push_back(std::make_pair(-1.0, line));
      } else {
        (*table)[label].push_back(std::make_pair(item.accuracy, line));
      }
    }

    ExamineResults(true, platform, &results);
    ExamineResults(false, platform, &disabled_results);

    const int total_cases = num_executed_cases + num_disabled_cases;
    LOG(INFO) << "Tested " << num_executed_cases << " / " << total_cases
              << " entries.";
  }

  // If |enabled| parameter is true, then actual conversion results are tested
  // and any failure is reported as test failure.  If false, actual conversion
  // results don't affect test results but closable issues are reported.
  static void ExamineResults(
      const bool enabled, uint32_t platform,
      std::map<std::string, std::vector<std::pair<float, std::string>>>
          *results) {
    for (auto it = results->begin(); it != results->end(); ++it) {
      std::vector<std::pair<float, std::string>> *values = &it->second;
      std::sort(values->begin(), values->end());
      size_t correct = 0;
      bool all_passed = true;
      for (const auto &value : *values) {
        const float accuracy = value.first;
        if (accuracy < 0) {
          ++correct;
          continue;
        }
        // Print failed example for failed label
        const float actual_ratio = 1.0 * correct / values->size();
        if (enabled) {
          EXPECT_LT(accuracy, actual_ratio)
              << value.second << " " << accuracy << " " << actual_ratio;
        } else {
          if (accuracy < actual_ratio) {
            LOG(INFO) << "PASSED (DISABLED): " << it->first << ": "
                      << value.second;
          } else {
            LOG(INFO) << "FAILED (DISABLED): " << it->first << ": "
                      << value.second;
            all_passed = false;
          }
        }
      }
      LOG(INFO) << "Accuracy: " << it->first << " "
                << 1.0 * correct / values->size();
      if (!enabled && all_passed) {
        LOG(INFO) << "CLOSED ISSUE [platform = "
                  << QualityRegressionUtil::GetPlatformString(platform)
                  << "]: " << it->first << " with " << it->second.size()
                  << " cases";
      }
    }
  }

 private:
  const testing::ScopedTmpUserProfileDirectory scoped_profile_dir_;
};

std::unique_ptr<EngineInterface> CreateEngine(const std::string &data_file_path,
                                              const std::string &magic_number,
                                              const std::string &engine_type) {
  std::unique_ptr<DataManager> data_manager(new DataManager);
  const auto status = data_manager->InitFromFile(data_file_path, magic_number);
  if (status != DataManager::Status::OK) {
    LOG(ERROR) << "Failed to load " << data_file_path << ": "
               << DataManager::StatusCodeToString(status);
    return nullptr;
  }
  if (engine_type == "desktop") {
    return Engine::CreateDesktopEngine(std::move(data_manager)).value();
  }
  if (engine_type == "mobile") {
    return Engine::CreateMobileEngine(std::move(data_manager)).value();
  }
  LOG(ERROR) << "Invalid engine type: " << engine_type;
  return nullptr;
}

}  // namespace
}  // namespace mozc
