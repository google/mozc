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

// Tool to pack multiple files into one file.
//
// Usage
// $ ./path/to/artifacts/dataset_writer_main
//   --magic=\xNN\xNN\xNN
//   --output=/path/to/output
//   [arg1, [arg2, ...]]
//
// Here, each argument has the following form:
//
// name:alignment:/path/to/infile
//
// where alignment must be a power of 2 greater than or equal to 8 (i.e., 8, 16,
// 32, 64, ...). Each packed file can be retrieved by DataSetReader through its
// name.

#include <ios>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/init_mozc.h"
#include "base/number_util.h"
#include "base/vlog.h"
#include "data_manager/dataset_writer.h"

ABSL_FLAG(std::string, magic, "", "Hex-encoded magic number to be embedded");
ABSL_FLAG(std::string, output, "", "Output file");

int main(int argc, char** argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  std::string magic;
  CHECK(absl::CUnescape(absl::GetFlag(FLAGS_magic), &magic))
      << "magic number is not a proper hex-escaped string: "
      << absl::GetFlag(FLAGS_magic);

  struct Input {
    Input(const std::string& n, int a, const std::string& f)
        : name(n), alignment(a), filename(f) {}

    std::string name;
    int alignment;
    std::string filename;
  };

  std::vector<Input> inputs;
  for (int i = 1; i < argc; ++i) {
    // InitMozc doesn't remove flags from argv, so ignore flags here.
    if (absl::StartsWith(argv[i], "--")) {
      continue;
    }
    std::vector<std::string> params =
        absl::StrSplit(argv[i], ':', absl::SkipEmpty());
    CHECK_EQ(3, params.size()) << "Unexpected arg[" << i << "] = " << argv[i];
    inputs.emplace_back(params[0], mozc::NumberUtil::SimpleAtoi(params[1]),
                        params[2]);
  }

  CHECK(!absl::GetFlag(FLAGS_output).empty()) << "--output is required";

  // DataSetWriter directly writes to the specified stream, so if it fails for
  // an input, the output contains a partial result.  To avoid such partial file
  // creation, write to a temporary file then rename it.
  const std::string tmpfile = absl::GetFlag(FLAGS_output) + ".tmp";
  {
    mozc::DataSetWriter writer(magic);
    for (const auto& input : inputs) {
      MOZC_VLOG(1) << "Writing " << input.name
                   << ", alignment = " << input.alignment
                   << ", file = " << input.filename;
      writer.AddFile(input.name, input.alignment, input.filename);
    }
    mozc::OutputFileStream output(tmpfile,
                                  std::ios_base::out | std::ios_base::binary);
    writer.Finish(&output);
    output.close();
  }
  absl::Status s =
      mozc::FileUtil::AtomicRename(tmpfile, absl::GetFlag(FLAGS_output));
  CHECK_OK(s) << ": Atomic rename failed. from: " << tmpfile
              << " to: " << absl::GetFlag(FLAGS_output);

  return 0;
}
