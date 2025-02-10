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

#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file/temp_dir.h"
#include "base/init_mozc.h"
#include "base/system_util.h"
#include "converter/quality_regression_util.h"
#include "engine/engine.h"
#include "engine/eval_engine_factory.h"
#include "protocol/commands.pb.h"
#include "request/request_test_util.h"

ABSL_FLAG(std::vector<std::string>, test_files, {}, "regression test files");
ABSL_FLAG(std::string, data_file, "", "engine data file");
ABSL_FLAG(std::string, data_type, "", "engine data type");
ABSL_FLAG(std::string, engine_type, "desktop", "engine type");
ABSL_FLAG(std::string, output, "", "output file");

namespace {

using ::mozc::Engine;
using ::mozc::TempDirectory;
using ::mozc::quality_regression::QualityRegressionUtil;

absl::Status Run(std::ostream &out, const Engine &engine,
                 absl::string_view engine_type,
                 absl::Span<const QualityRegressionUtil::TestItem> items) {
  QualityRegressionUtil util(engine.GetConverter());
  if (engine_type == "mobile") {
    mozc::commands::Request request;
    mozc::request_test_util::FillMobileRequest(&request);
    util.SetRequest(request);
  }
  for (const QualityRegressionUtil::TestItem &item : items) {
    std::string actual_value;
    const absl::StatusOr<bool> result =
        util.ConvertAndTest(item, &actual_value);
    if (!result.ok()) {
      LOG(INFO) << "Failed to convert: " << item.key;
      return result.status();
    }
    out << (result.value() ? "OK:\t" : "FAILED:\t") << item.key << "\t"
        << actual_value << "\t" << item.command;
    if (item.expected_rank != 0) {
      out << " " << item.expected_rank;
    }
    out << "\t" << item.expected_value << "\t" << std::endl;
  }
  return absl::OkStatus();
}

}  // namespace

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  absl::StatusOr<TempDirectory> temp_dir =
      TempDirectory::Default().CreateTempDirectory();
  CHECK_OK(temp_dir);
  mozc::SystemUtil::SetUserProfileDirectory(temp_dir->path());

  absl::StatusOr<std::unique_ptr<Engine>> create_result =
      mozc::CreateEvalEngine(absl::GetFlag(FLAGS_data_file),
                             absl::GetFlag(FLAGS_data_type),
                             absl::GetFlag(FLAGS_engine_type));
  if (!create_result.ok()) {
    LOG(ERROR) << create_result.status();
    return static_cast<int>(create_result.status().code());
  }

  std::vector<QualityRegressionUtil::TestItem> items;
  const absl::Status parse_result = QualityRegressionUtil::ParseFiles(
      absl::GetFlag(FLAGS_test_files), &items);
  if (!parse_result.ok()) {
    LOG(ERROR) << parse_result;
    return static_cast<int>(parse_result.code());
  }

  absl::Status status;
  if (!absl::GetFlag(FLAGS_output).empty()) {
    std::ofstream out(absl::GetFlag(FLAGS_output));
    status = Run(out, *create_result.value(), absl::GetFlag(FLAGS_engine_type),
                 items);
  } else {
    status = Run(std::cout, *create_result.value(),
                 absl::GetFlag(FLAGS_engine_type), items);
  }
  if (!status.ok()) {
    LOG(ERROR) << status;
    return static_cast<int>(status.code());
  }

  return 0;
}
