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
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>  // NOLINT
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "base/japanese_util.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/stopwatch.h"
#include "base/util.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/random_keyevents_generator.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

ABSL_FLAG(std::string, server_path, "", "specify server path");
ABSL_FLAG(std::string, log_path, "", "specify log output file path");

namespace mozc {
namespace {

struct Result {
  std::string test_name;
  std::vector<uint32_t> operations_times;
};

class TestSentenceGenerator {
 public:
  const std::vector<std::vector<commands::KeyEvent> > &GetTestKeys() const {
    return keys_;
  }

  TestSentenceGenerator() {
    const absl::Span<const char *> sentences =
        session::RandomKeyEventsGenerator::GetTestSentences();
    CHECK(!sentences.empty());
    const size_t size = std::min<size_t>(200, sentences.size());

    for (size_t i = 0; i < size; ++i) {
      std::string output;
      japanese_util::HiraganaToRomanji(sentences[i], &output);
      std::vector<commands::KeyEvent> tmp;
      for (ConstChar32Iterator iter(output); !iter.Done(); iter.Next()) {
        const char32_t ucs4 = iter.Get();
        if (ucs4 >= 'a' && ucs4 <= 'z') {
          commands::KeyEvent key;
          key.set_key_code(static_cast<int>(ucs4));
          tmp.push_back(key);
        }
      }
      if (!tmp.empty()) {
        keys_.push_back(tmp);
      }
    }
  }

 private:
  std::vector<std::vector<commands::KeyEvent> > keys_;
};

class TestScenarioInterface {
 public:
  virtual void Run(Result *result) = 0;

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

std::string GetBasicStats(const std::vector<uint32_t> times) {
  uint32_t total_time = 0;
  uint32_t avg_time = 0;
  uint32_t max_time = 0;
  uint32_t min_time = 0;
  uint32_t sd_time = 0;  // Standard Deviation
  uint32_t med_time = 0;

  min_time = INT_MAX;
  max_time = 0;
  for (size_t i = 0; i < times.size(); ++i) {
    total_time += times[i];
    min_time = std::min(times[i], min_time);
    max_time = std::max(times[i], max_time);
  }

  avg_time = static_cast<uint32_t>(1.0 * total_time / times.size());

  if (times.size() >= 2) {
    double dsd_time = 0;
    for (size_t i = 0; i < times.size(); ++i) {
      dsd_time += (avg_time - times[i]) * (avg_time - times[i]);
    }
    dsd_time = sqrt(dsd_time / (times.size() - 1));
    sd_time = static_cast<uint32_t>(dsd_time);
  }

  if (!times.empty()) {
    std::vector<uint32_t> tmp(times);
    std::sort(tmp.begin(), tmp.end());
    med_time = tmp[tmp.size() / 2];
  }

  return absl::StrFormat("size=%d total=%d avg=%d max=%d min=%d st=%d med=%d",
                         static_cast<int>(times.size()), total_time, avg_time,
                         max_time, min_time, sd_time, med_time);
}

class PreeditCommon : public TestScenarioInterface {
 protected:
  virtual void RunTest(Result *result) {
    const std::vector<std::vector<commands::KeyEvent> > &keys =
        Singleton<TestSentenceGenerator>::get()->GetTestKeys();
    for (size_t i = 0; i < keys.size(); ++i) {
      for (int j = 0; j < keys[i].size(); ++j) {
        Stopwatch stopwatch;
        stopwatch.Start();
        client_.SendKey(keys[i][j], &output_);
        stopwatch.Stop();
        result->operations_times.push_back(stopwatch.GetElapsedMicroseconds());
      }
      commands::SessionCommand command;
      command.set_type(commands::SessionCommand::REVERT);
      client_.SendCommand(command, &output_);
    }
  }
};

class PreeditWithoutSuggestion : public PreeditCommon {
 public:
  void Run(Result *result) override {
    result->test_name = "preedit_without_suggestion";
    ResetConfig();
    IMEOn();
    DisableSuggestion();
    RunTest(result);
    IMEOff();
    ResetConfig();
  }
};

class PreeditWithSuggestion : public PreeditCommon {
 public:
  void Run(Result *result) override {
    result->test_name = "preedit_with_suggestion";
    ResetConfig();
    IMEOn();
    EnableSuggestion();
    RunTest(result);
    IMEOff();
    ResetConfig();
  }
};

enum PredictionRequestType { ONE_CHAR, TWO_CHARS };

void CreatePredictionKeys(PredictionRequestType type,
                          std::vector<std::string> *request_keys) {
  CHECK(request_keys);
  request_keys->clear();

  const char *kVoels[] = {"a", "i", "u", "e", "o"};
  const char *kConsonant[] = {"k", "s", "t", "n", "h", "m", "y", "r", "w"};
  std::vector<std::string> one_chars;
  for (size_t i = 0; i < std::size(kVoels); ++i) {
    one_chars.push_back(kVoels[i]);
  }

  for (size_t i = 0; i < std::size(kConsonant); ++i) {
    for (size_t j = 0; j < std::size(kVoels); ++j) {
      one_chars.push_back(std::string(kConsonant[i]) + std::string(kVoels[j]));
    }
  }

  std::vector<std::string> two_chars;
  for (size_t i = 0; i < one_chars.size(); ++i) {
    for (size_t j = 0; j < one_chars.size(); ++j) {
      two_chars.push_back(one_chars[i] + one_chars[j]);
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
      result->operations_times.push_back(stopwatch.GetElapsedMicroseconds());

      commands::SessionCommand command;
      command.set_type(commands::SessionCommand::REVERT);
      client_.SendCommand(command, &output_);
    }
    IMEOff();
  }
};

class PredictionWithOneChar : public PredictionCommon {
 public:
  void Run(Result *result) override {
    result->test_name = "prediction_one_char";
    RunTest(ONE_CHAR, result);
  }
};

class PredictionWithTwoChars : public PredictionCommon {
 public:
  void Run(Result *result) override {
    result->test_name = "prediction_two_chars";
    RunTest(TWO_CHARS, result);
  }
};

class Conversion : public TestScenarioInterface {
 public:
  void Run(Result *result) override {
    result->test_name = "conversion";
    ResetConfig();
    DisableSuggestion();
    IMEOn();

    const std::vector<std::vector<commands::KeyEvent> > &keys =
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
      result->operations_times.push_back(stopwatch.GetElapsedMicroseconds());

      commands::SessionCommand command;
      command.set_type(commands::SessionCommand::REVERT);
      client_.SendCommand(command, &output_);
    }

    IMEOff();
    ResetConfig();
  }
};
}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  std::vector<mozc::TestScenarioInterface *> tests;
  std::vector<mozc::Result *> results;

  tests.push_back(new mozc::PreeditWithoutSuggestion);
  tests.push_back(new mozc::PreeditWithSuggestion);
  tests.push_back(new mozc::Conversion);
  tests.push_back(new mozc::PredictionWithOneChar);
  tests.push_back(new mozc::PredictionWithTwoChars);

  for (size_t i = 0; i < tests.size(); ++i) {
    mozc::Result *result = new mozc::Result;
    tests[i]->Run(result);
    results.push_back(result);
  }

  CHECK_EQ(results.size(), tests.size());

  std::ostream *ofs = &std::cout;
  if (!absl::GetFlag(FLAGS_log_path).empty()) {
    ofs = new mozc::OutputFileStream(absl::GetFlag(FLAGS_log_path));
  }

  // TODO(taku): generate histogram with ChartAPI
  for (size_t i = 0; i < tests.size(); ++i) {
    (*ofs) << results[i]->test_name << ": "
           << mozc::GetBasicStats(results[i]->operations_times) << std::endl;
    delete tests[i];
    delete results[i];
  }
  if (ofs != &std::cout) {
    delete ofs;
  }

  return 0;
}
