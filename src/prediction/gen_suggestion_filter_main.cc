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
#include <ios>
#include <memory>
#include <string>
#include <vector>

#include "base/codegen_bytearray_stream.h"
#include "base/file_stream.h"
#include "base/hash.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/multifile.h"
#include "base/util.h"
#include "storage/existence_filter.h"
#include "absl/flags/flag.h"

ABSL_FLAG(std::string, input, "", "per-line suggestion filter list");
ABSL_FLAG(std::string, output, "", "output bloom filter");
ABSL_FLAG(bool, header, true, "make header file instead of raw bloom filter");
ABSL_FLAG(std::string, name, "SuggestionFilterData",
          "name for variable name in the header file");
ABSL_FLAG(std::string, safe_list_files, "",
          "Comma separated files that contain safe word list. If specified, "
          "retries filter generation with different parameters until these "
          "words will not be filtered.");

namespace {
using mozc::storage::ExistenceFilter;

void ReadHashList(const std::string &name, std::vector<uint64_t> *words) {
  std::string line;
  mozc::InputFileStream input(name.c_str());
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    mozc::Util::LowerString(&line);
    words->push_back(mozc::Hash::Fingerprint(line));
  }
}

void ReadSafeWords(const std::string &safe_list_files,
                   std::vector<std::string> *safe_word_list) {
  if (safe_list_files.empty()) {
    return;
  }
  mozc::InputMultiFile input(safe_list_files.c_str());
  std::string line;
  while (input.ReadLine(&line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::string lower_value = line;
    mozc::Util::LowerString(&lower_value);
    safe_word_list->push_back(lower_value);
  }
}

constexpr size_t kMinimumFilterBytes = 100 * 1000;

std::unique_ptr<ExistenceFilter> GetFilter(
    const size_t num_bytes, const std::vector<uint64_t> &hash_list) {
  LOG(INFO) << "num_bytes: " << num_bytes;

  std::unique_ptr<ExistenceFilter> filter(
      ExistenceFilter::CreateOptimal(num_bytes, hash_list.size()));
  for (size_t i = 0; i < hash_list.size(); ++i) {
    filter->Insert(hash_list[i]);
  }
  return filter;
}

bool TestFilter(const ExistenceFilter &filter,
                const std::vector<std::string> &safe_word_list) {
  for (const std::string &word : safe_word_list) {
    if (filter.Exists(mozc::Hash::Fingerprint(word))) {
      LOG(WARNING) << "Safe word, " << word
                   << " is determined as bad suggestion.";
      return false;
    }
  }
  return true;
}
}  // namespace

// read per-line word list and generate
// bloom filter in raw byte array or header file format
int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if ((absl::GetFlag(FLAGS_input).empty() ||
       absl::GetFlag(FLAGS_output).empty()) &&
      argc > 2) {
    absl::SetFlag(&FLAGS_input, argv[1]);
    absl::SetFlag(&FLAGS_output, argv[2]);
  }

  std::vector<uint64_t> hash_list;
  ReadHashList(absl::GetFlag(FLAGS_input), &hash_list);

  LOG(INFO) << hash_list.size() << " words found";

  static constexpr float kErrorRate = 0.00001;
  const size_t num_bytes =
      std::max(ExistenceFilter::MinFilterSizeInBytesForErrorRate(
                   kErrorRate, hash_list.size()),
               kMinimumFilterBytes);

  std::vector<std::string> safe_word_list;
  ReadSafeWords(absl::GetFlag(FLAGS_safe_list_files), &safe_word_list);

  std::unique_ptr<ExistenceFilter> filter;
  constexpr int kNumRetryMax = 10;
  constexpr int kSizeOffset = 8;
  // Prevent filtering of common words by false positive.
  for (int i = 0; i < kNumRetryMax; ++i) {
    filter = GetFilter(num_bytes + i * kSizeOffset, hash_list);
    if (TestFilter(*filter, safe_word_list)) {
      break;
    }
    if (i == kNumRetryMax - 1) {
      LOG(FATAL) << "Gave up retrying suggestion filter generation.";
    }
  }

  LOG(INFO) << "writing bloomfilter: " << absl::GetFlag(FLAGS_output);
  char *buf = nullptr;
  size_t size = 0;
  filter->Write(&buf, &size);

  if (absl::GetFlag(FLAGS_header)) {
    mozc::OutputFileStream ofs(absl::GetFlag(FLAGS_output).c_str());
    mozc::CodeGenByteArrayOutputStream codegen_stream(
        &ofs, mozc::codegenstream::NOT_OWN_STREAM);
    codegen_stream.OpenVarDef(absl::GetFlag(FLAGS_name));
    codegen_stream.write(buf, size);
    codegen_stream.CloseVarDef();
  } else {
    mozc::OutputFileStream ofs(
        absl::GetFlag(FLAGS_output).c_str(),
        std::ios::out | std::ios::trunc | std::ios::binary);
    ofs.write(buf, size);
  }

  delete[] buf;

  return 0;
}
