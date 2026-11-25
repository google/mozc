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

#include <memory>
#include <string>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/strings/unicode.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/system/system_dictionary.h"
#include "testing/benchmark.h"
#include "testing/mozctest.h"

ABSL_FLAG(std::string, dictionary, "", "Input dictionary to be tested");

namespace mozc {
namespace dictionary {
namespace {

std::string GetDictionaryPath() {
  if (std::string path = absl::GetFlag(FLAGS_dictionary); !path.empty()) {
    return path;
  }
  return testing::GetSourceFileOrDie(
      {"data_manager", "oss", "dictionary.data"});
}

std::unique_ptr<SystemDictionary> LoadDictionary(
    absl::string_view file, SystemDictionary::Options options,
    std::string* image) {
  absl::StatusOr<std::string> contents =
      FileUtil::GetContents(std::string(file));
  CHECK_OK(contents);
  *image = *contents;
  return SystemDictionary::Builder(image->data(), image->size())
      .SetOptions(SystemDictionary::ENABLE_REVERSE_LOOKUP_INDEX)
      .Build()
      .value();
}

}  // namespace

void BM_incremental(benchmark::State& state) {
  const std::string dictionary_path = GetDictionaryPath();
  std::string image;
  std::unique_ptr<SystemDictionary> dictionary =
      LoadDictionary(dictionary_path, SystemDictionary::NONE, &image);
  const std::string key =
      "とうきょうとっきょきょかきょくきょくちょうしつで"
      "とうきょうとっきょきょかきょくきょくちょうと"
      "とうきょうとっきょきょかきょくもときょくちょうと"
      "とうきょうとっきょきょかきょくふくきょくちょうが"
      "あった";
  bool checked = false;
  for (auto s : state) {
    int input_pos = 0;
    const char* str = key.c_str();
    while (input_pos < key.size()) {
      // users' input is appended 1 character by 1 character.
      input_pos += strings::OneCharLen(&str[input_pos]);
      std::string current_input(str, input_pos);

      // look up prefixes as the converter do.
      int start_pos = 0;
      while (start_pos < current_input.size()) {
        CollectTokenCallback callback;
        dictionary->LookupPrefix(
            absl::string_view(&str[start_pos], input_pos - start_pos),
            &callback);
        benchmark::DoNotOptimize(checked |= !callback.tokens().empty());
        start_pos += strings::OneCharLen(&str[start_pos]);
      }
    }
  }
}

void BM_initialize(benchmark::State& state) {
  const std::string dictionary_path = GetDictionaryPath();
  for (auto s : state) {
    std::string image;
    std::unique_ptr<SystemDictionary> dictionary =
        LoadDictionary(dictionary_path, SystemDictionary::NONE, &image);
  }
}

void BM_initialize_with_index(benchmark::State& state) {
  const std::string dictionary_path = GetDictionaryPath();
  for (auto s : state) {
    std::string image;
    std::unique_ptr<SystemDictionary> dictionary = LoadDictionary(
        dictionary_path, SystemDictionary::ENABLE_REVERSE_LOOKUP_INDEX, &image);
  }
}

void BM_reverse(benchmark::State& state) {
  const std::string dictionary_path = GetDictionaryPath();
  std::string image;
  std::unique_ptr<SystemDictionary> dictionary =
      LoadDictionary(dictionary_path, SystemDictionary::NONE, &image);

  const std::string key =
      "東京特許許可局局長室で東京特許許可局局長と"
      "東京特許許可局元局長と東京特許許可局副局長が会った";

  bool checked = false;
  for (auto s : state) {
    const char* str = key.c_str();
    while (*str) {
      CollectTokenCallback callback;
      dictionary->LookupReverse(str, &callback);
      benchmark::DoNotOptimize(checked |= !callback.tokens().empty());
      str += strings::OneCharLen(str);
    }
  }
}

BENCHMARK(BM_incremental);
BENCHMARK(BM_initialize);
BENCHMARK(BM_initialize_with_index);
BENCHMARK(BM_reverse);

}  // namespace dictionary
}  // namespace mozc
