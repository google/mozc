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

#include "prediction/english_decoder.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "transliteration/transliteration.h"

namespace mozc::prediction {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::Token;
using ::testing::_;
using ::testing::StrEq;
using ::testing::WithParamInterface;

struct InvokeCallbackWithKeyValues {
  using Callback = DictionaryInterface::Callback;

  template <class T, class U>
  void operator()(T, U, Callback* callback) {
    for (const auto& [key, value] : kv_list) {
      if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE ||
          callback->OnActualKey(key, key, false) !=
              Callback::TRAVERSE_CONTINUE) {
        return;
      }
      const Token token(key, value, MockDictionary::kDefaultCost,
                        MockDictionary::kDefaultPosId,
                        MockDictionary::kDefaultPosId, token_attribute);
      if (callback->OnToken(key, key, token) != Callback::TRAVERSE_CONTINUE) {
        return;
      }
    }
  }

  std::vector<std::pair<absl::string_view, absl::string_view>> kv_list;
  Token::Attribute token_attribute = Token::NONE;
};

void GenerateKeyEvents(absl::string_view text,
                       std::vector<commands::KeyEvent>* keys) {
  keys->clear();
  for (const char32_t codepoint : Util::Utf8ToUtf32(text)) {
    commands::KeyEvent key;
    if (codepoint <= 0x7F) {
      key.set_key_code(codepoint);
    } else {
      key.set_key_code('?');
      *key.mutable_key_string() = Util::CodepointToUtf8(codepoint);
    }
    keys->push_back(key);
  }
}

void InsertInputSequence(absl::string_view text, composer::Composer* composer) {
  std::vector<commands::KeyEvent> keys;
  GenerateKeyEvents(text, &keys);
  for (size_t i = 0; i < keys.size(); ++i) {
    composer->InsertCharacterKeyEvent(keys[i]);
  }
}

class EnglishDecoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto mock_dict = std::make_unique<MockDictionary>();
    EXPECT_CALL(*mock_dict, LookupPredictive(StrEq("conv"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"converge", "converge"},
            {"converged", "converged"},
            {"convergent", "convergent"},
        }});
    EXPECT_CALL(*mock_dict, LookupPredictive(StrEq("con"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"contraction", "contraction"},
            {"control", "control"},
        }});
    EXPECT_CALL(*mock_dict, LookupPredictive(StrEq("hel"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues{{
            {"hello", "hello"},
        }});

    auto data_manager = std::make_unique<testing::MockDataManager>();
    ASSERT_OK_AND_ASSIGN(modules_, engine::ModulesPresetBuilder()
                                       .PresetDictionary(std::move(mock_dict))
                                       .Build(std::move(data_manager)));
    decoder_ = std::make_unique<EnglishDecoder>(*modules_);

    config::ConfigHandler::GetDefaultConfig(&config_);
    table_ = std::make_shared<composer::Table>();
    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_ = std::make_unique<composer::Composer>(table_, request_, config_);
  }

  ConversionRequest CreatePredictionRequest() {
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetRequestType(ConversionRequest::PREDICTION)
        .Build();
  }

  std::unique_ptr<engine::Modules> modules_;
  std::unique_ptr<EnglishDecoder> decoder_;
  config::Config config_;
  commands::Request request_;
  std::shared_ptr<composer::Table> table_;
  std::unique_ptr<composer::Composer> composer_;
};

TEST_F(EnglishDecoderTest, EmptyRequestReturnsEmpty) {
  ConversionRequest request;
  std::vector<Result> results = decoder_->Decode(request);
  EXPECT_TRUE(results.empty());
}

struct EnglishPredictionTestEntry {
  std::string name;
  transliteration::TransliterationType input_mode;
  std::string key;
  std::string expected_prefix;
  std::vector<std::string> expected_values;
};

class ParameterizedEnglishPredictionTest
    : public EnglishDecoderTest,
      public WithParamInterface<EnglishPredictionTestEntry> {};

TEST_P(ParameterizedEnglishPredictionTest, AggregateEnglishPrediction) {
  const EnglishPredictionTestEntry& entry = GetParam();

  composer_->Reset();
  composer_->SetInputMode(entry.input_mode);
  InsertInputSequence(entry.key, composer_.get());

  const ConversionRequest convreq = CreatePredictionRequest();
  std::vector<Result> results = decoder_->Decode(convreq);

  std::set<std::string> values;
  for (const auto& result : results) {
    EXPECT_TRUE(result.attributes & ENGLISH);
    EXPECT_TRUE(result.value.starts_with(entry.expected_prefix))
        << result.value << " doesn't start with " << entry.expected_prefix;
    values.insert(result.value);
  }
  for (const auto& expected_value : entry.expected_values) {
    EXPECT_TRUE(values.find(expected_value) != values.end())
        << expected_value << " isn't in the results";
  }
}

const std::vector<EnglishPredictionTestEntry>* kEnglishPredictionTestEntries =
    new std::vector<EnglishPredictionTestEntry>(
        {{"HALF_ASCII_lower_case",
          transliteration::HALF_ASCII,
          "conv",
          "conv",
          {"converge", "converged", "convergent"}},
         {"HALF_ASCII_upper_case",
          transliteration::HALF_ASCII,
          "CONV",
          "CONV",
          {"CONVERGE", "CONVERGED", "CONVERGENT"}},
         {"HALF_ASCII_capitalized",
          transliteration::HALF_ASCII,
          "Conv",
          "Conv",
          {"Converge", "Converged", "Convergent"}},
         {"FULL_ASCII_lower_case",
          transliteration::FULL_ASCII,
          "conv",
          "ｃｏｎｖ",
          {"ｃｏｎｖｅｒｇｅ", "ｃｏｎｖｅｒｇｅｄ", "ｃｏｎｖｅｒｇｅｎｔ"}},
         {"FULL_ASCII_upper_case",
          transliteration::FULL_ASCII,
          "CONV",
          "ＣＯＮＶ",
          {"ＣＯＮＶＥＲＧＥ", "ＣＯＮＶＥＲＧＥＤ", "ＣＯＮＶＥＲＧＥＮＴ"}},
         {"FULL_ASCII_capitalized",
          transliteration::FULL_ASCII,
          "Conv",
          "Ｃｏｎｖ",
          {"Ｃｏｎｖｅｒｇｅ", "Ｃｏｎｖｅｒｇｅｄ", "Ｃｏｎｖｅｒｇｅｎｔ"}}});

INSTANTIATE_TEST_SUITE_P(
    AggregateEnglishPredictioForInputMode, ParameterizedEnglishPredictionTest,
    ::testing::ValuesIn(*kEnglishPredictionTestEntries),
    [](const ::testing::TestParamInfo<
        ParameterizedEnglishPredictionTest::ParamType>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace mozc::prediction
