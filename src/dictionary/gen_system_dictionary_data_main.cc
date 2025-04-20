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

// Generates system dictionary header file.
//
// gen_system_dictionary_data_main
//  --input="dictionary0.txt dictionary1.txt"
//  --output="output.h"
//  --make_header

#include <ios>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "data_manager/data_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/text_dictionary_loader.h"

ABSL_FLAG(std::string, input, "", "space separated input text files");
ABSL_FLAG(std::string, user_pos_manager_data, "", "user pos manager data");
ABSL_FLAG(std::string, output, "", "output binary file");

namespace mozc {
namespace {

// 10 dictionary files are passed to this program with --input flag.
// reading_correction.tsv is also passed to this program with --input flag
// in the same manner. This program checks the file name pattern and change
// the algorithm for handling dictionaries. Ideally, we want to use different
// flags for dictionary and reading correction, but due to the limitation
// of internal build system, it turned out that the description of the rules
// will become much complicated, if we use two flags.
constexpr absl::string_view kReadingCorrectionFile = "reading_correction.tsv";

// Converts space delimited text to CSV and returns {system_dictionary_input,
// reading_correction_input}.
std::pair<std::string, std::string> GetInputFileName(
    const absl::string_view input_file) {
  constexpr absl::string_view kDelimiter = ",";
  const std::vector<absl::string_view> fields =
      absl::StrSplit(input_file, ' ', absl::SkipWhitespace());
  std::vector<absl::string_view> system_dictionary_inputs,
      reading_correction_inputs;
  for (const absl::string_view &field : fields) {
    if (field.ends_with(kReadingCorrectionFile)) {
      reading_correction_inputs.push_back(field);
    } else {
      system_dictionary_inputs.push_back(field);
    }
  }
  return {absl::StrJoin(system_dictionary_inputs, kDelimiter),
          absl::StrJoin(reading_correction_inputs, kDelimiter)};
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  std::string system_dictionary_input, reading_correction_input;
  std::tie(system_dictionary_input, reading_correction_input) =
      mozc::GetInputFileName(absl::GetFlag(FLAGS_input));

  // User POS manager data for build tools has no magic number.
  constexpr absl::string_view kMagicNumber = "";
  absl::StatusOr<std::unique_ptr<const mozc::DataManager>> data_manager =
      mozc::DataManager::CreateUserPosManagerDataFromFile(
          absl::GetFlag(FLAGS_user_pos_manager_data), kMagicNumber);
  CHECK_OK(data_manager) << "Failed to initialize data manager from "
                         << absl::GetFlag(FLAGS_user_pos_manager_data);

  const mozc::dictionary::PosMatcher pos_matcher(
      data_manager.value()->GetPosMatcherData());

  mozc::dictionary::TextDictionaryLoader loader(pos_matcher);
  loader.Load(system_dictionary_input, reading_correction_input);

  mozc::dictionary::SystemDictionaryBuilder builder;
  builder.BuildFromTokens(loader.tokens());

  std::unique_ptr<std::ostream> output_stream(new mozc::OutputFileStream(
      absl::GetFlag(FLAGS_output), std::ios::out | std::ios::binary));
  builder.WriteToStream(absl::GetFlag(FLAGS_output), output_stream.get());

  return 0;
}
