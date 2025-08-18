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

// Converts a dictionary file into existence filter header file.
// The header file will be used to suppress "Ateji".
//
// input format:
// <reading>[TAB]<cost>[TAB]<rid>[TAB]<lid>[TAB]<surface form>[...]
// ...
// (cost, rid and lid are ignored.)
//
// example:
// ./gen_collocation_suppression_data_main.cc
// --suppression_data=collocation_suppression.txt
// > embedded_collocation_suppression_data.h

#include <ios>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_split.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "rewriter/gen_existence_data.h"

ABSL_FLAG(std::string, suppression_data, "", "suppression data text");
ABSL_FLAG(std::string, output, "", "output file name (default: stdout)");
ABSL_FLAG(double, error_rate, 0.00001, "error rate");
ABSL_FLAG(bool, binary_mode, false, "outputs binary file");

namespace mozc {
namespace {

void Convert() {
  constexpr char kSeparator[] = "\t";
  std::vector<std::string> entries;

  if (absl::GetFlag(FLAGS_suppression_data).empty()) {
    const std::string kDummyStr = "__NO_DATA__";
    entries.push_back(kDummyStr + kSeparator + kDummyStr);
  } else {
    InputFileStream ifs(absl::GetFlag(FLAGS_suppression_data));
    std::string line;

    while (!std::getline(ifs, line).fail()) {
      if (line.empty()) {
        continue;
      }
      const std::vector<std::string> fields =
          absl::StrSplit(line, kSeparator, absl::SkipEmpty());
      CHECK_GE(fields.size(), 2);
      entries.push_back(fields[0] + kSeparator + fields[1]);
    }
  }

  std::ostream* ofs = &std::cout;
  if (!absl::GetFlag(FLAGS_output).empty()) {
    if (absl::GetFlag(FLAGS_binary_mode)) {
      ofs = new OutputFileStream(absl::GetFlag(FLAGS_output),
                                 std::ios::out | std::ios::binary);
    } else {
      ofs = new OutputFileStream(absl::GetFlag(FLAGS_output));
    }
  }

  if (absl::GetFlag(FLAGS_binary_mode)) {
    OutputExistenceBinary(entries, ofs, absl::GetFlag(FLAGS_error_rate));
  } else {
    const std::string kNameSpace = "CollocationSuppressionData";
    OutputExistenceHeader(entries, kNameSpace, ofs,
                          absl::GetFlag(FLAGS_error_rate));
  }

  if (ofs != &std::cout) {
    delete ofs;
  }
}
}  // namespace
}  // namespace mozc

int main(int argc, char* argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);

  LOG(INFO) << absl::GetFlag(FLAGS_suppression_data);

  mozc::Convert();

  return 0;
}
