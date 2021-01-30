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
#include <memory>
#include <string>

#include "base/codegen_bytearray_stream.h"
#include "base/file_stream.h"
#include "base/flags.h"
#include "base/hash.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/util.h"
#include "storage/existence_filter.h"

DEFINE_string(input, "", "per-line suggestion filter list");
DEFINE_string(output, "", "output bloom filter");
DEFINE_bool(header, true, "make header file instead of raw bloom filter");
DEFINE_string(name, "SuggestionFilterData",
              "name for variable name in the header file");

namespace {
void ReadWords(const std::string &name, std::vector<uint64> *words) {
  std::string line;
  mozc::InputFileStream input(name.c_str());
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::string lower_value = line;
    mozc::Util::LowerString(&lower_value);
    words->push_back(mozc::Hash::Fingerprint(lower_value));
  }
}

const size_t kMinimumFilterBytes = 100 * 1000;
}  // namespace

using mozc::storage::ExistenceFilter;

// read per-line word list and generate
// bloom filter in raw byte array or header file format
int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if ((mozc::GetFlag(FLAGS_input).empty() ||
       mozc::GetFlag(FLAGS_output).empty()) && argc > 2) {
    mozc::SetFlag(&FLAGS_input, argv[1]);
    mozc::SetFlag(&FLAGS_output, argv[2]);
  }

  std::vector<uint64> words;

  ReadWords(mozc::GetFlag(FLAGS_input), &words);

  LOG(INFO) << words.size() << " words found";

  static const float kErrorRate = 0.00001;
  const size_t num_bytes =
      std::max(ExistenceFilter::MinFilterSizeInBytesForErrorRate(kErrorRate,
                                                                 words.size()),
               kMinimumFilterBytes);

  LOG(INFO) << "num_bytes: " << num_bytes;

  std::unique_ptr<ExistenceFilter> filter(
      ExistenceFilter::CreateOptimal(num_bytes, words.size()));
  for (size_t i = 0; i < words.size(); ++i) {
    filter->Insert(words[i]);
  }

  char *buf = nullptr;
  size_t size = 0;

  LOG(INFO) << "writing bloomfilter: " << mozc::GetFlag(FLAGS_output);
  filter->Write(&buf, &size);

  if (mozc::GetFlag(FLAGS_header)) {
    mozc::OutputFileStream ofs(mozc::GetFlag(FLAGS_output).c_str());
    mozc::CodeGenByteArrayOutputStream codegen_stream(
        &ofs, mozc::codegenstream::NOT_OWN_STREAM);
    codegen_stream.OpenVarDef(mozc::GetFlag(FLAGS_name));
    codegen_stream.write(buf, size);
    codegen_stream.CloseVarDef();
  } else {
    mozc::OutputFileStream ofs(
        mozc::GetFlag(FLAGS_output).c_str(),
        std::ios::out | std::ios::trunc | std::ios::binary);
    ofs.write(buf, size);
  }

  delete[] buf;

  return 0;
}
