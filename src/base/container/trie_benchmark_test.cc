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

#include <cstddef>
#include <iterator>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/container/trie.h"
#include "testing/benchmark.h"

namespace mozc {
namespace {

Trie<int> MakeSamplePreeditTrie() {
  constexpr absl::string_view kKeys[] = {
      "a",    "i",    "u",    "e",    "o",    "ka",   "ki",   "ku",   "ke",
      "ko",   "kya",  "kyu",  "kyo",  "sa",   "si",   "su",   "se",   "so",
      "shi",  "sha",  "shu",  "sho",  "ta",   "ti",   "tu",   "te",   "to",
      "chi",  "tsu",  "cha",  "chu",  "cho",  "na",   "ni",   "nu",   "ne",
      "no",   "nn",   "nya",  "nyu",  "nyo",  "ha",   "hi",   "hu",   "he",
      "ho",   "fu",   "hya",  "hyu",  "hyo",  "ma",   "mi",   "mu",   "me",
      "mo",   "ya",   "yu",   "yo",   "ra",   "ri",   "ru",   "re",   "ro",
      "wa",   "wo",   "xtu",  "ltu",  "か゛", "き゛", "く゛", "け゛", "こ゛",
      "さ゛", "し゛", "す゛", "せ゛", "そ゛", "た゛", "ち゛", "つ゛", "て゛",
      "と゛", "は゛", "ひ゛", "ふ゛", "へ゛", "ほ゛", "は゜", "ひ゜", "ふ゜",
      "へ゜", "ほ゜", "う゛", "?あ1", "?あ2", "?あ3", "?か1", "?か2", "?か*",
      "?は1", "?は*", "*き2", "*し*", "?ん#",
  };
  Trie<int> trie;
  for (size_t i = 0; i < std::size(kKeys); ++i) {
    trie.AddEntry(kKeys[i], static_cast<int>(i));
  }
  return trie;
}

void BM_Trie_LookUpPrefix(benchmark::State& state) {
  const Trie<int> trie = MakeSamplePreeditTrie();
  constexpr absl::string_view kQueries[] = {
      "a",    "ka",   "shi",  "tsu",  "kya",  "nn",   "xtu",
      "k",    "sh",   "ky",   "kk",   "ltu",  "?あ1", "?か*",
      "*し*", "か゛", "は゜", "う゛", "?ん#", "xyz",
  };
  for (auto _ : state) {
    for (const absl::string_view q : kQueries) {
      int value = 0;
      size_t key_length = 0;
      bool fixed = false;
      bool found = trie.LookUpPrefix(q, &value, &key_length, &fixed);
      benchmark::DoNotOptimize(found);
      benchmark::DoNotOptimize(value);
      benchmark::DoNotOptimize(key_length);
      benchmark::DoNotOptimize(fixed);
    }
  }
}
BENCHMARK(BM_Trie_LookUpPrefix);

void BM_Trie_LookUpPredictiveAll(benchmark::State& state) {
  const Trie<int> trie = MakeSamplePreeditTrie();
  constexpr absl::string_view kPrefixes[] = {"k", "s", "t", "?あ", "?か", "は"};
  std::vector<int> results;
  results.reserve(32);
  for (auto _ : state) {
    for (const absl::string_view p : kPrefixes) {
      results.clear();
      trie.LookUpPredictiveAll(p, &results);
      benchmark::DoNotOptimize(results);
    }
  }
}
BENCHMARK(BM_Trie_LookUpPredictiveAll);

void BM_Trie_LongestMatchAndAdd(benchmark::State& state) {
  constexpr absl::string_view kCandidates[] = {
      "ありがとう",
      "ありがとうございます",
      "ありがとうございました",
      "ありがと",
      "有り難う",
      "有難う",
      "よろしく",
      "よろしくお願いします",
      "宜しくお願いします",
      "よろしくね",
      "わたし",
      "私の",
      "私の名前は",
      "わたしのなまえ",
      "これ",
      "これは",
      "これはいったい",
      "東京",
      "東京都",
      "東京都港区",
  };
  for (auto _ : state) {
    Trie<bool> filter;
    int kept = 0;
    for (const absl::string_view cand : kCandidates) {
      bool dummy = false;
      size_t key_len = 0;
      if (!filter.LongestMatch(cand, &dummy, &key_len)) {
        filter.AddEntry(cand, true);
        ++kept;
      }
    }
    benchmark::DoNotOptimize(kept);
    benchmark::DoNotOptimize(filter);
  }
}
BENCHMARK(BM_Trie_LongestMatchAndAdd);

}  // namespace
}  // namespace mozc
