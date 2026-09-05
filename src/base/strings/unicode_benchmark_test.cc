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
#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "base/strings/unicode.h"
#include "testing/benchmark.h"

namespace mozc {
namespace {

// Repeats phrase until the result is at least min_bytes long.
std::string MakeCorpus(const absl::string_view phrase, const size_t min_bytes) {
  std::string result;
  while (result.size() < min_bytes) {
    result += phrase;
  }
  return result;
}

constexpr absl::string_view kAsciiPhrase =
    "The quick brown fox jumps over the lazy dog. ";
// Greek letters are 2-byte in UTF-8 and can be entered with Japanese IMEs
// through symbol conversion (e.g. あるふぁ → α).
constexpr absl::string_view kGreekPhrase = "αβγδεζηθικλμνξοπρστυφχψω";
constexpr absl::string_view kKanjiPhrase = "日本語入力の変換精度と応答速度。";
constexpr absl::string_view kMixedPhrase =
    "Mozcは2010年5月10日に公開されました。"
    "明日は𰻞𰻞麺を食べます🍜。"
    "α線の飛程は空気中で約40μm×10³程度です。";

constexpr absl::string_view kEmojiPhrase = "😀🎉🚀🍣👍🗾🎌";

// Short: one phrase, resembles typical Mozc strings (tens of bytes).
// Long: ~8 KiB, shows asymptotic behavior (e.g. vectorization).
// Passing 1 to MakeCorpus appends the phrase exactly once.
constexpr size_t kOnePhrase = 1;
constexpr size_t kLongBytes = 8192;

void BM_CharsLen(benchmark::State& state, const absl::string_view phrase,
                 const size_t min_bytes) {
  const std::string corpus = MakeCorpus(phrase, min_bytes);
  const absl::string_view sv = corpus;
  for (auto _ : state) {
    benchmark::DoNotOptimize(sv);
    size_t len = strings::CharsLen(sv);
    benchmark::DoNotOptimize(len);
  }
  state.SetBytesProcessed(state.iterations() * corpus.size());
}

BENCHMARK_CAPTURE(BM_CharsLen, ascii_short, kAsciiPhrase, kOnePhrase);
BENCHMARK_CAPTURE(BM_CharsLen, ascii_long, kAsciiPhrase, kLongBytes);
BENCHMARK_CAPTURE(BM_CharsLen, greek_short, kGreekPhrase, kOnePhrase);
BENCHMARK_CAPTURE(BM_CharsLen, greek_long, kGreekPhrase, kLongBytes);
BENCHMARK_CAPTURE(BM_CharsLen, kanji_short, kKanjiPhrase, kOnePhrase);
BENCHMARK_CAPTURE(BM_CharsLen, kanji_long, kKanjiPhrase, kLongBytes);
BENCHMARK_CAPTURE(BM_CharsLen, emoji_short, kEmojiPhrase, kOnePhrase);
BENCHMARK_CAPTURE(BM_CharsLen, emoji_long, kEmojiPhrase, kLongBytes);
BENCHMARK_CAPTURE(BM_CharsLen, mixed_short, kMixedPhrase, kOnePhrase);
BENCHMARK_CAPTURE(BM_CharsLen, mixed_long, kMixedPhrase, kLongBytes);

}  // namespace
}  // namespace mozc
