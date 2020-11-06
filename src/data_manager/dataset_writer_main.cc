// Copyright 2010-2020, Google Inc.
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
// $ ./path/to/artifacts/dataset_writer_main \
//   --magic=\xNN\xNN\xNN \
//   --output=/path/to/output \
//   [arg1, [arg2, ...]]
//
// Here, each argument has the following form:
//
// name:alignment:/path/to/infile
//
// where alignment must be one of {8, 16, 32, 64}.  Each packed file can be
// retrieved by DataSetReader through its name.

#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "data_manager/dataset_writer.h"

DEFINE_string(magic, "", "Hex-encoded magic number to be embedded");
DEFINE_string(output, "", "Output file");

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  std::string magic;
  CHECK(mozc::Util::Unescape(FLAGS_magic, &magic))
      << "magic number is not a proper hex-escaped string: " << FLAGS_magic;

  struct Input {
    Input(const std::string &n, int a, const std::string &f)
        : name(n), alignment(a), filename(f) {}

    std::string name;
    int alignment;
    std::string filename;
  };

  std::vector<Input> inputs;
  for (int i = 1; i < argc; ++i) {
    // InitMozc doesn't remove flags from argv, so ignore flags here.
    if (mozc::Util::StartsWith(argv[i], "--")) {
      continue;
    }
    std::vector<std::string> params;
    mozc::Util::SplitStringUsing(argv[i], ":", &params);
    CHECK_EQ(3, params.size()) << "Unexpected arg[" << i << "] = " << argv[i];
    inputs.emplace_back(params[0], mozc::NumberUtil::SimpleAtoi(params[1]),
                        params[2]);
  }

  CHECK(!FLAGS_output.empty()) << "--output is required";

  // DataSetWriter directly writes to the specified stream, so if it fails for
  // an input, the output contains a partial result.  To avoid such partial file
  // creation, write to a temporary file then rename it.
  const std::string tmpfile = FLAGS_output + ".tmp";
  {
    mozc::DataSetWriter writer(magic);
    for (const auto &input : inputs) {
      VLOG(1) << "Writing " << input.name << ", alignment = " << input.alignment
              << ", file = " << input.filename;
      writer.AddFile(input.name, input.alignment, input.filename);
    }
    mozc::OutputFileStream output(tmpfile.c_str(),
                                  std::ios_base::out | std::ios_base::binary);
    writer.Finish(&output);
    output.close();
  }
  CHECK(mozc::FileUtil::AtomicRename(tmpfile, FLAGS_output))
      << "Failed to rename " << tmpfile << " to " << FLAGS_output;

  return 0;
}
