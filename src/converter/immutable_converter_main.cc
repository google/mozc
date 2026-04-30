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

// immutable_converter_main.cc
//
// A tool to convert a query using ImmutableConverter to dump the lattice in TSV
// format.
//
// Usage:
// immutable_converter_main --dictionary oss --query へんかん
//   --output /tmp/lattice.tsv
//
// clang-format off
// Output:
/*
id	key	value	begin_pos	end_pos	lid	rid	wcost	cost	bnext	enext	prev	next
1		BOS	0	0	0	0	0	0	0	0	0	2
3		POS	0	0	0	0	0	0	4	0	0	5
5		POS	3	3	0	0	0	0	6	0	0	7
7		POS	6	6	0	0	0	0	8	0	0	0
4	も	も	0	3	1841	1841	32767	34068	9	0	1	0
9	もじ	捩	0	6	577	577	11584	15810	10	0	1	0
10	もじ	もじ	0	6	577	577	8697	12923	2	9	1	0
2	もじ	文字	0	6	1851	1851	3953	5044	11	10	1	8
11	もじ	モジ	0	6	1851	1851	7487	8578	12	2	1	0
12	もじ	門司	0	6	1920	1920	5714	8904	13	11	1	0
13	もじ	門司	0	6	1923	1923	5752	8371	14	12	1	0
14	もじ	門司	0	6	1924	1924	4505	7301	15	13	1	0
15	もじ	茂地	0	6	1924	1924	7372	10168	16	14	1	0
...
*/
// clang-format on

#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "converter/debug_util.h"
#include "converter/immutable_converter.h"
#include "converter/lattice.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/oss/oss_data_manager.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "request/conversion_request.h"

ABSL_FLAG(std::string, query, "", "Query input to be converted");
ABSL_FLAG(std::string, dictionary, "", "Dictionary: 'oss' or 'test'");
ABSL_FLAG(std::string, output, "", "Output file");

namespace mozc {
namespace converter {
namespace {

bool ExecCommand(const ImmutableConverter& immutable_converter,
                 absl::string_view query, absl::string_view output) {
  ConversionRequest::Options options = {
      .request_type = ConversionRequest::CONVERSION,
      .use_actual_converter_for_realtime_conversion = true,
      .create_partial_candidates = false,
  };
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetOptions(std::move(options))
          .SetKey(query)
          .Build();

  Segments segments;
  Lattice lattice;
  segments.InitForConvert(conversion_request.key());
  if (!immutable_converter.Convert(conversion_request, &segments, &lattice)) {
    return false;
  }

  mozc::OutputFileStream os(output);
  os << DumpNodes(lattice);
  return true;
}

std::unique_ptr<const DataManager> CreateDataManager(
    absl::string_view dictionary) {
  if (dictionary == "oss") {
    return std::make_unique<const oss::OssDataManager>();
  }
  if (dictionary == "mock") {
    return std::make_unique<const testing::MockDataManager>();
  }
  if (!dictionary.empty()) {
    std::cout << "ERROR: Unknown dictionary name: " << dictionary << std::endl;
  }

  return std::make_unique<const oss::OssDataManager>();
}
}  // namespace

int RunMain(int argc, char** argv) {
  InitMozc(argv[0], &argc, &argv);

  std::unique_ptr<const DataManager> data_manager =
      CreateDataManager(absl::GetFlag(FLAGS_dictionary));
  absl::StatusOr<std::unique_ptr<engine::Modules>> modules_opt =
      engine::Modules::Create(std::move(data_manager));
  if (!modules_opt.ok()) {
    std::cerr << "Failed to create modules for "
              << absl::GetFlag(FLAGS_dictionary) << std::endl;
    return 1;
  }
  std::unique_ptr<engine::Modules> modules = std::move(modules_opt.value());
  ImmutableConverter immutable_converter(*modules);

  std::string query = absl::GetFlag(FLAGS_query);
  std::string output = absl::GetFlag(FLAGS_output);
  if (!ExecCommand(immutable_converter, query, output)) {
    std::cerr << "ExecCommand() return false" << std::endl;
    return 1;
  }
  return 0;
}

}  // namespace converter
}  // namespace mozc

int main(int argc, char** argv) { return mozc::converter::RunMain(argc, argv); }
