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

#ifndef MOZC_CONVERTER_QUALITY_REGRESSION_UTIL_H_
#define MOZC_CONVERTER_QUALITY_REGRESSION_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
class ConverterInterface;

namespace commands {
class Request;
}  // namespace commands

namespace quality_regression {

class QualityRegressionUtil {
 public:
  // bit fields
  enum Platform {
    DESKTOP = 1,
    OSS = 2,
    MOBILE = 4,
  };

  struct TestItem {
    std::string label;
    std::string key;
    std::string expected_value;
    std::string command;
    double accuracy;
    // Target platform. Can set multiple platform defined in enum |Platform|.
    int expected_rank;
    uint32_t platform;
    std::string OutputAsTSV() const;
    absl::Status ParseFromTSV(const std::string &tsv_line);
  };

  explicit QualityRegressionUtil(
      std::shared_ptr<const ConverterInterface> converter);
  QualityRegressionUtil(const QualityRegressionUtil &) = delete;
  QualityRegressionUtil &operator=(const QualityRegressionUtil &) = delete;
  virtual ~QualityRegressionUtil() = default;

  // Pase |filename| and save the all test items into |outputs|.
  static absl::Status ParseFile(const std::string &filename,
                                std::vector<TestItem> *outputs);
  static absl::Status ParseFiles(absl::Span<const std::string> filenames,
                                 std::vector<TestItem> *outputs);

  absl::StatusOr<bool> ConvertAndTest(const TestItem &item,
                                      std::string *actual_value);

  void SetRequest(const commands::Request &request);
  void SetConfig(const config::Config &config);
  static std::string GetPlatformString(uint32_t platform_bitfiled);

 private:
  std::shared_ptr<const ConverterInterface> converter_;
  commands::Request request_;
  config::Config config_;
  Segments segments_;
};

}  // namespace quality_regression
}  // namespace mozc

#endif  // MOZC_CONVERTER_QUALITY_REGRESSION_UTIL_H_
