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
#include <memory>
#include <string>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "composer/key_parser.h"
#include "composer/table.h"
#include "config/character_form_manager.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/benchmark.h"

using ::mozc::commands::Request;
using ::mozc::config::CharacterFormManager;
using ::mozc::config::Config;

namespace mozc {
namespace composer {
namespace {

constexpr absl::string_view kVowels[] = {
    "a", "e", "i", "o", "u",
};

constexpr absl::string_view kConsonants[] = {
    "k", "s", "t", "n", "h", "m", "y", "r", "w",
};

bool InsertKey(const absl::string_view key_string, Composer* composer) {
  commands::KeyEvent key;
  if (!KeyParser::ParseKey(key_string, &key)) {
    return false;
  }
  return composer->InsertCharacterKeyEvent(key);
}

class ComposerBenchmark {
 public:
  ComposerBenchmark() { SetUp(); }
  ~ComposerBenchmark() { TearDown(); }

  ComposerBenchmark(const ComposerBenchmark&) = delete;
  ComposerBenchmark& operator=(const ComposerBenchmark&) = delete;

  void Initialize() {
    config_->set_preedit_method(Config::ROMAN);
    config_->set_auto_switch_composition_mode(false);

    table_->InitializeWithRequestAndConfig(*request_, *config_);
  }

  void ResetComposer() { composer_->Reset(); }

  void InsertRandomKeys(int len) {
    for (int i = 0; i < len; i++) {
      InsertKey(kConsonants[absl::Uniform(bitgen_, 0u, std::size(kConsonants))],
                composer_.get());
      InsertKey(kVowels[absl::Uniform(bitgen_, 0u, std::size(kVowels))],
                composer_.get());
    }
  }

  std::string GetStringForPreedit() { return composer_->GetStringForPreedit(); }

 private:
  void SetUp() {
    table_ = std::make_shared<Table>();
    config_ = std::make_unique<Config>();
    request_ = std::make_unique<Request>();
    composer_ = std::make_unique<Composer>(table_, *request_, *config_);
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
  }

  void TearDown() {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    composer_.reset();
    request_.reset();
    config_.reset();
    table_.reset();
  }

  std::unique_ptr<Composer> composer_;
  std::shared_ptr<Table> table_;
  std::unique_ptr<Request> request_;
  std::unique_ptr<Config> config_;
  absl::BitGen bitgen_;
};

void BM_Composer(benchmark::State& state) {
  ComposerBenchmark bm;
  bm.Initialize();

  for (auto s : state) {
    bm.ResetComposer();
    bm.InsertRandomKeys(10);

    [[maybe_unused]] std::string output = bm.GetStringForPreedit();
  }
}
BENCHMARK(BM_Composer);

void BM_Table_Initialize_ToggleFlick(benchmark::State& state) {
  Request request;
  request.set_special_romanji_table(Request::TOGGLE_FLICK_TO_HIRAGANA);
  Config config;
  for (auto s : state) {
    Table table;
    bool ok = table.InitializeWithRequestAndConfig(request, config);
    benchmark::DoNotOptimize(ok);
    benchmark::DoNotOptimize(table);
  }
}
BENCHMARK(BM_Table_Initialize_ToggleFlick);

void BM_Table_LookUpPrefix_ToggleFlick(benchmark::State& state) {
  Request request;
  request.set_special_romanji_table(Request::TOGGLE_FLICK_TO_HIRAGANA);
  Config config;
  Table table;
  table.InitializeWithRequestAndConfig(request, config);

  constexpr absl::string_view kRawQueries[] = {
      "1",      "_",      "2",      "a",      "5",        "\t1",      "\t2",
      "{?}あ1", "{?}い1", "{?}か*", "{?}は*", "{?}あ{!}", "{*}あ{<}", "{*}き2",
      "{*}し*", "{?}た4", "{?}ら9", "{*}ん0", "z",        "{?}ん#",
  };
  std::vector<std::string> queries;
  queries.reserve(std::size(kRawQueries));
  for (const absl::string_view q : kRawQueries) {
    queries.push_back(table.ParseSpecialKey(q));
  }

  for (auto s : state) {
    for (const std::string& q : queries) {
      size_t key_length = 0;
      bool fixed = false;
      const Entry* entry = table.LookUpPrefix(q, &key_length, &fixed);
      benchmark::DoNotOptimize(entry);
      benchmark::DoNotOptimize(key_length);
      benchmark::DoNotOptimize(fixed);
    }
  }
}
BENCHMARK(BM_Table_LookUpPrefix_ToggleFlick);

}  // namespace
}  // namespace composer
}  // namespace mozc
