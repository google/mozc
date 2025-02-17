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
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <numeric>
#include <ostream>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "base/japanese_util.h"
#include "base/singleton.h"
#include "base/stopwatch.h"
#include "base/util.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/random_keyevents_generator.h"

ABSL_FLAG(std::string, server_path, "", "specify server path");
ABSL_FLAG(std::string, log_path, "", "specify log output file path");

namespace mozc {
namespace {

struct Result {
  std::string test_name;
  std::vector<absl::Duration> operations_times;
};

class TestSentenceGenerator {
 public:
  absl::Span<const std::vector<commands::KeyEvent>> GetTestKeys() const {
    return keys_;
  }

  TestSentenceGenerator() {
    const absl::Span<const char *> sentences =
        session::RandomKeyEventsGenerator::GetTestSentences();
    CHECK(!sentences.empty());
    const size_t size = std::min<size_t>(200, sentences.size());

    for (size_t i = 0; i < size; ++i) {
      std::string output = japanese_util::HiraganaToRomanji(sentences[i]);
      std::vector<commands::KeyEvent> tmp;
      for (ConstChar32Iterator iter(output); !iter.Done(); iter.Next()) {
        const char32_t codepoint = iter.Get();
        if (codepoint >= 'a' && codepoint <= 'z') {
          commands::KeyEvent key;
          key.set_key_code(static_cast<int>(codepoint));
          tmp.push_back(key);
        }
      }
      if (!tmp.empty()) {
        keys_.push_back(tmp);
      }
    }
  }

 private:
  std::vector<std::vector<commands::KeyEvent>> keys_;
};

class TestScenarioInterface {
 public:
  virtual Result Run() = 0;

  TestScenarioInterface() {
    if (!absl::GetFlag(FLAGS_server_path).empty()) {
      client_.set_server_program(absl::GetFlag(FLAGS_server_path));
    }
    CHECK(client_.IsValidRunLevel()) << "IsValidRunLevel failed";
    CHECK(client_.EnsureSession()) << "EnsureSession failed";
    CHECK(client_.NoOperation()) << "Server is not responding";
  }

  virtual ~TestScenarioInterface() = default;

 protected:
  virtual void IMEOn() {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ON);
    client_.SendKey(key, &output_);
  }

  virtual void IMEOff() {
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::OFF);
    client_.SendKey(key, &output_);
  }

  virtual void ResetConfig() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    client_.SetConfig(config);
  }

  virtual void EnableSuggestion() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config.set_use_history_suggest(true);
    config.set_use_dictionary_suggest(true);
    client_.SetConfig(config);
  }

  virtual void DisableSuggestion() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config.set_use_history_suggest(false);
    config.set_use_dictionary_suggest(false);
    client_.SetConfig(config);
  }

  client::Client client_;
  commands::Output output_;
};

std::string GetBasicStats(absl::Span<const absl::Duration> times) {
  std::vector<uint64_t> temp;
  temp.resize(times.size());
  absl::c_transform(times, temp.begin(), absl::ToInt64Microseconds);
  absl::c_sort(temp);
  const uint64_t total = absl::c_accumulate(temp, 0);
  uint64_t mean = 0;
  uint64_t max = 0;
  uint64_t min = std::numeric_limits<uint64_t>::max();
  uint64_t median = 0;
  if (!temp.empty()) {
    min = temp.front();
    max = temp.back();
    mean = total / times.size();
    median = temp[temp.size() / 2];
  }

  uint64_t stddev = 0;
  if (times.size() >= 2) {
    double dsd = std::transform_reduce(
        temp.begin(), temp.end(), 0.0, std::plus<>(),
        [mean](uint64_t t) -> double { return (mean - t) * (mean - t); });
    stddev = static_cast<uint64_t>(sqrt(dsd / (temp.size() - 1)));
  }

  return absl::StrFormat("size=%d total=%d avg=%d max=%d min=%d st=%d med=%d",
                         times.size(), total, mean, max, min, stddev, median);
}

class PreeditCommon : public TestScenarioInterface {
 protected:
  virtual void RunTest(Result *result) {
    absl::Span<const std::vector<commands::KeyEvent>> keys =
        Singleton<TestSentenceGenerator>::get()->GetTestKeys();
    for (size_t i = 0; i < keys.size(); ++i) {
      for (int j = 0; j < keys[i].size(); ++j) {
        Stopwatch stopwatch;
        stopwatch.Start();
        client_.SendKey(keys[i][j], &output_);
        stopwatch.Stop();
        result->operations_times.push_back(stopwatch.GetElapsed());
      }
      commands::SessionCommand command;
      command.set_type(commands::SessionCommand::REVERT);
      client_.SendCommand(command, &output_);
    }
  }
};

class PreeditWithoutSuggestion : public PreeditCommon {
 public:
  Result Run() override {
    Result result;
    result.test_name = "preedit_without_suggestion";
    ResetConfig();
    IMEOn();
    DisableSuggestion();
    RunTest(&result);
    IMEOff();
    ResetConfig();
    return result;
  }
};

class PreeditWithSuggestion : public PreeditCommon {
 public:
  Result Run() override {
    Result result;
    result.test_name = "preedit_with_suggestion";
    ResetConfig();
    IMEOn();
    EnableSuggestion();
    RunTest(&result);
    IMEOff();
    ResetConfig();
    return result;
  }
};

enum PredictionRequestType { ONE_CHAR, TWO_CHARS };

void CreatePredictionKeys(PredictionRequestType type,
                          std::vector<std::string> *request_keys) {
  CHECK(request_keys);
  request_keys->clear();

  constexpr std::array<absl::string_view, 5> kVoels = {"a", "i", "u", "e", "o"};
  constexpr std::array<absl::string_view, 10> kConsonants = {
      "", "k", "s", "t", "n", "h", "m", "y", "r", "w"};

  std::vector<std::string> one_chars;
  for (absl::string_view c : kConsonants) {
    for (absl::string_view v : kVoels) {
      one_chars.push_back(absl::StrCat(c, v));
    }
  }

  std::vector<std::string> two_chars;
  for (absl::string_view c1 : one_chars) {
    for (absl::string_view c2 : one_chars) {
      two_chars.push_back(absl::StrCat(c1, c2));
    }
  }

  switch (type) {
    case ONE_CHAR:
      std::copy(one_chars.begin(), one_chars.end(),
                std::back_inserter(*request_keys));
      break;
    case TWO_CHARS:
      std::copy(two_chars.begin(), two_chars.end(),
                std::back_inserter(*request_keys));
      break;
    default:
      break;
  }

  CHECK(!request_keys->empty());
}

class PredictionCommon : public TestScenarioInterface {
 protected:
  void RunTest(PredictionRequestType type, Result *result) {
    IMEOn();
    ResetConfig();
    DisableSuggestion();
    std::vector<std::string> request_keys;
    CreatePredictionKeys(type, &request_keys);
    for (size_t i = 0; i < request_keys.size(); ++i) {
      const absl::string_view keys = request_keys[i];
      for (size_t j = 0; j < keys.size(); ++j) {
        commands::KeyEvent key;
        key.set_key_code(static_cast<int>(keys[j]));
        client_.SendKey(key, &output_);
      }
      commands::KeyEvent key;
      key.set_special_key(commands::KeyEvent::TAB);
      Stopwatch stopwatch;
      stopwatch.Start();
      client_.SendKey(key, &output_);
      stopwatch.Stop();
      result->operations_times.push_back(stopwatch.GetElapsed());

      commands::SessionCommand command;
      command.set_type(commands::SessionCommand::REVERT);
      client_.SendCommand(command, &output_);
    }
    IMEOff();
  }
};

class PredictionWithOneChar : public PredictionCommon {
 public:
  Result Run() override {
    Result result;
    result.test_name = "prediction_one_char";
    RunTest(ONE_CHAR, &result);
    return result;
  }
};

class PredictionWithTwoChars : public PredictionCommon {
 public:
  Result Run() override {
    Result result;
    result.test_name = "prediction_two_chars";
    RunTest(TWO_CHARS, &result);
    return result;
  }
};

class Conversion : public TestScenarioInterface {
 public:
  Result Run() override {
    Result result;
    result.test_name = "conversion";
    ResetConfig();
    DisableSuggestion();
    IMEOn();

    absl::Span<const std::vector<commands::KeyEvent>> keys =
        Singleton<TestSentenceGenerator>::get()->GetTestKeys();
    for (size_t i = 0; i < keys.size(); ++i) {
      for (int j = 0; j < keys[i].size(); ++j) {
        client_.SendKey(keys[i][j], &output_);
      }
      commands::KeyEvent key;
      key.set_special_key(commands::KeyEvent::SPACE);
      Stopwatch stopwatch;
      stopwatch.Start();
      client_.SendKey(key, &output_);
      stopwatch.Stop();
      result.operations_times.push_back(stopwatch.GetElapsed());

      commands::SessionCommand command;
      command.set_type(commands::SessionCommand::REVERT);
      client_.SendCommand(command, &output_);
    }

    IMEOff();
    ResetConfig();
    return result;
  }
};

void Run(std::ostream &os) {
  std::vector<std::unique_ptr<TestScenarioInterface>> tests;
  tests.push_back(std::make_unique<PreeditWithoutSuggestion>());
  tests.push_back(std::make_unique<PreeditWithSuggestion>());
  tests.push_back(std::make_unique<Conversion>());
  tests.push_back(std::make_unique<PredictionWithOneChar>());
  tests.push_back(std::make_unique<PredictionWithTwoChars>());

  std::vector<Result> results;
  results.reserve(tests.size());
  for (const auto &test : tests) {
    results.push_back(test->Run());
  }

  CHECK_EQ(results.size(), tests.size());

  // TODO(taku): generate histogram with ChartAPI
  for (const Result &result : results) {
    os << result.test_name << ": "
       << mozc::GetBasicStats(result.operations_times) << std::endl;
  }
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (!absl::GetFlag(FLAGS_log_path).empty()) {
    std::ofstream ofs = mozc::OutputFileStream(absl::GetFlag(FLAGS_log_path));
    mozc::Run(ofs);
  } else {
    mozc::Run(std::cout);
  }

  return 0;
}
