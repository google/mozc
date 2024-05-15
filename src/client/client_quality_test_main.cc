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

#include <cstdint>
#include <ios>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "evaluation/scorer.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "base/japanese_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "client/client.h"
#include "protocol/commands.pb.h"

ABSL_FLAG(std::string, server_path, "", "specify server path");
ABSL_FLAG(std::string, log_path, "", "specify log output file path");
ABSL_FLAG(int32_t, max_case_for_source, 500,
          "specify max test case number for each test sources");

namespace mozc {
namespace {

bool IsValidSourceSentence(const absl::string_view str) {
  // TODO(noriyukit) Treat alphabets by changing to Eisu-mode
  if (Util::ContainsScriptType(str, Util::ALPHABET)) {
    LOG(WARNING) << "contains ALPHABET: " << str;
    return false;
  }

  // Source should not contain kanji
  if (Util::ContainsScriptType(str, Util::KANJI)) {
    LOG(WARNING) << "contains KANJI: " << str;
    return false;
  }

  // Source should not contain katakana
  std::string tmp = absl::StrReplaceAll(str, {{"ー", ""}, {"・", ""}});
  if (Util::ContainsScriptType(tmp, Util::KATAKANA)) {
    LOG(WARNING) << "contain KATAKANA: " << str;
    return false;
  }

  return true;
}

std::optional<std::vector<commands::KeyEvent>> GenerateKeySequenceFrom(
    const absl::string_view hiragana_sentence) {
  std::vector<commands::KeyEvent> keys;

  std::string input;
  {
    std::string tmp = japanese_util::HiraganaToRomanji(hiragana_sentence);
    input = japanese_util::FullWidthToHalfWidth(tmp);
  }

  for (ConstChar32Iterator iter(input); !iter.Done(); iter.Next()) {
    const char32_t codepoint = iter.Get();

    // TODO(noriyukit) Improve key sequence generation; currently, a few
    // codepoint codes, like FF5E and 300E, cannot be handled.
    commands::KeyEvent key;
    if (codepoint >= 0x20 && codepoint <= 0x7F) {
      key.set_key_code(static_cast<int>(codepoint));
    } else if (codepoint == 0x3001 || codepoint == 0xFF64) {
      key.set_key_code(0x002C);  // Full-width comma -> Half-width comma
    } else if (codepoint == 0x3002 || codepoint == 0xFF0E ||
               codepoint == 0xFF61) {
      key.set_key_code(0x002E);  // Full-width period -> Half-width period
    } else if (codepoint == 0x2212 || codepoint == 0x2015) {
      key.set_key_code(0x002D);  // "−" -> "-"
    } else if (codepoint == 0x300C || codepoint == 0xff62) {
      key.set_key_code(0x005B);  // "「" -> "["
    } else if (codepoint == 0x300D || codepoint == 0xff63) {
      key.set_key_code(0x005D);  // "」" -> "]"
    } else if (codepoint == 0x30FB || codepoint == 0xFF65) {
      key.set_key_code(0x002F);  // "・" -> "/"  "･" -> "/"
    } else {
      LOG(WARNING) << "Unexpected character: " << std::hex
                   << static_cast<uint32_t>(codepoint) << ": in " << input
                   << " (" << hiragana_sentence << ")";
      return std::nullopt;
    }
    keys.push_back(std::move(key));
  }

  // Conversion key
  {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::SPACE);
    keys.push_back(std::move(key));
  }
  return keys;
}

std::optional<std::string> GetPreedit(const commands::Output &output) {
  if (!output.has_preedit()) {
    LOG(WARNING) << "No result";
    return std::nullopt;
  }

  return absl::StrJoin(output.preedit().segment(), "");
}

std::optional<double> CalculateBleu(client::Client &client,
                                    const absl::string_view hiragana_sentence,
                                    const absl::string_view expected_result) {
  // Prepare key events
  std::optional<std::vector<commands::KeyEvent>> keys =
      GenerateKeySequenceFrom(hiragana_sentence);
  if (!keys.has_value()) {
    LOG(WARNING) << "Failed to generated key events from: "
                 << hiragana_sentence;
    return std::nullopt;
  }

  // Must send ON first
  commands::Output output;
  {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ON);
    client.SendKey(key, &output);
  }

  // Send keys
  for (const commands::KeyEvent &key : *keys) {
    client.SendKey(key, &output);
  }
  MOZC_VLOG(2) << "Server response: " << output;

  // Calculate score
  std::string expected_normalized =
      Scorer::NormalizeForEvaluate(expected_result);

  std::string preedit_normalized;
  if (std::optional<std::string> preedit = GetPreedit(output);
      preedit.has_value() && !preedit->empty()) {
    preedit_normalized = Scorer::NormalizeForEvaluate(*preedit);
  } else {
    LOG(WARNING) << "Could not get output";
    return std::nullopt;
  }

  double score = Scorer::BLEUScore({expected_normalized}, preedit_normalized);

  MOZC_VLOG(1) << hiragana_sentence << std::endl
               << "   score: " << score << std::endl
               << " preedit: " << preedit_normalized << std::endl
               << "expected: " << expected_normalized;

  // Revert session to prevent server from learning this conversion
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::REVERT);
  client.SendCommand(command, &output);

  return score;
}

double CalculateMean(absl::Span<const double> scores) {
  CHECK(!scores.empty());
  const double sum = std::accumulate(scores.begin(), scores.end(), 0.0);
  return sum / static_cast<double>(scores.size());
}

}  // namespace
}  // namespace mozc

namespace {

struct TestCase {
  const char *source;
  const char *expected_result;
  const char *hiragana_sentence;
};

// Test data automatically generated by gen_client_quality_test_data.py
// constexpr TestCase kTestCases[] is defined.
#include "client/client_quality_test_data.inc"

}  // namespace

int main(int argc, char *argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);

  mozc::client::Client client;
  if (!absl::GetFlag(FLAGS_server_path).empty()) {
    client.set_server_program(absl::GetFlag(FLAGS_server_path));
  }

  CHECK(client.IsValidRunLevel()) << "IsValidRunLevel failed";
  CHECK(client.EnsureSession()) << "EnsureSession failed";
  CHECK(client.NoOperation()) << "Server is not respoinding";

  absl::flat_hash_map<std::string, std::vector<double>> score_map;

  for (const TestCase &test_case : kTestCases) {
    const absl::string_view source = test_case.source;
    const absl::string_view hiragana_sentence = test_case.hiragana_sentence;
    const absl::string_view expected_result = test_case.expected_result;

    if (score_map[source].size() >= absl::GetFlag(FLAGS_max_case_for_source)) {
      continue;
    }

    MOZC_VLOG(1) << "Processing " << hiragana_sentence;
    if (!mozc::IsValidSourceSentence(hiragana_sentence)) {
      LOG(WARNING) << "Invalid test case: " << std::endl
                   << "    source: " << source << std::endl
                   << "  hiragana: " << hiragana_sentence << std::endl
                   << "  expected: " << expected_result;
      continue;
    }

    std::optional<double> score =
        mozc::CalculateBleu(client, hiragana_sentence, expected_result);
    if (!score.has_value()) {
      LOG(WARNING) << "Failed to calculate BLEU score: " << std::endl
                   << "    source: " << source << std::endl
                   << "  hiragana: " << hiragana_sentence << std::endl
                   << "  expected: " << expected_result;
      continue;
    }
    score_map[source].push_back(*score);
  }

  std::unique_ptr<std::ostream> ofs;
  if (!absl::GetFlag(FLAGS_log_path).empty()) {
    ofs =
        std::make_unique<mozc::OutputFileStream>(absl::GetFlag(FLAGS_log_path));
    std::cout.rdbuf(ofs->rdbuf());
  }

  // Average the scores
  for (const auto &[source, scores] : score_map) {
    std::cout << source << " : " << mozc::CalculateMean(scores) << std::endl;
  }

  return 0;
}
