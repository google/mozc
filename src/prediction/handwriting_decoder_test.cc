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

#include "prediction/handwriting_decoder.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "testing/gmock.h"
#include "absl/strings/string_view.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::Token;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Truly;

class MockRealtimeDecoder : public RealtimeDecoder {
 public:
  ~MockRealtimeDecoder() override = default;

  MOCK_METHOD(std::vector<Result>, Decode, (const ConversionRequest& request),
              (const, override));
  MOCK_METHOD(std::vector<Result>, ReverseDecode,
              (const ConversionRequest& request), (const, override));
};

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

bool FindResultByKeyValue(absl::Span<const Result> results,
                          const absl::string_view key,
                          const absl::string_view value) {
  for (const auto& result : results) {
    if (result.key == key && result.value == value && !result.removed) {
      return true;
    }
  }
  return false;
}

class HandwritingDecoderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config::ConfigHandler::GetDefaultConfig(&config_);
    table_ = std::make_shared<composer::Table>();
    table_->LoadFromFile("system://12keys-hiragana.tsv");
    composer_ = std::make_unique<composer::Composer>(table_, request_, config_);
    realtime_decoder_ = std::make_unique<MockRealtimeDecoder>();
  }

  void InitWithMockDictionary(std::unique_ptr<MockDictionary> mock_dict) {
    auto data_manager = std::make_unique<testing::MockDataManager>();
    ASSERT_OK_AND_ASSIGN(modules_, engine::ModulesPresetBuilder()
                                       .PresetDictionary(std::move(mock_dict))
                                       .Build(std::move(data_manager)));
    decoder_ =
        std::make_unique<HandwritingDecoder>(*modules_, *realtime_decoder_);
  }

  ConversionRequest CreatePredictionRequest(absl::string_view key) {
    return ConversionRequestBuilder()
        .SetComposer(*composer_)
        .SetRequest(request_)
        .SetConfig(config_)
        .SetKey(key)
        .SetRequestType(ConversionRequest::PREDICTION)
        .Build();
  }

  std::unique_ptr<engine::Modules> modules_;
  std::unique_ptr<MockRealtimeDecoder> realtime_decoder_;
  std::unique_ptr<HandwritingDecoder> decoder_;
  config::Config config_;
  commands::Request request_;
  std::shared_ptr<composer::Table> table_;
  std::unique_ptr<composer::Composer> composer_;
};

TEST_F(HandwritingDecoderTest, NonHandwritingRequestReturnsEmpty) {
  auto mock_dict = std::make_unique<MockDictionary>();
  InitWithMockDictionary(std::move(mock_dict));
  ConversionRequest request;
  EXPECT_TRUE(decoder_->Decode(request).empty());
}

TEST_F(HandwritingDecoderTest, Handwriting) {
  constexpr int kCostOffset = 3000;

  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(&request_);
  request_.mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  request_.mutable_decoder_experiment_params()
      ->set_handwriting_conversion_candidate_cost_offset(kCostOffset);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("かん字じ典");
    composition_event->set_probability(0.99);
    composition_event = command.add_composition_events();
    composition_event->set_composition_string("かlv字じ典");
    composition_event->set_probability(0.01);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  // reverse conversion
  {
    Result result;
    result.key = "かん字じ典";
    result.value = "かんじじてん";

    EXPECT_CALL(*realtime_decoder_,
                ReverseDecode(Truly([](const ConversionRequest& request) {
                  return request.request_type() ==
                             ConversionRequest::REVERSE_CONVERSION &&
                         request.key() == "かん字じ典";
                })))
        .WillOnce(Return(std::vector<Result>({result})));
  }

  auto mock_dict = std::make_unique<MockDictionary>();
  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupExact(StrEq("かんじじてん"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"かんじじてん", "漢字辞典"},
          {"かんじじてん", "漢字字典"},
          {"かんじじてん", "感じじてん"},
          {"かんじじてん", "幹事時点"},
          {"かんじじてん", "換字字典"},
          {"かんじじてん", "換字自転"},
          {"かんじじてん", "換字じてん"},
      }});

  InitWithMockDictionary(std::move(mock_dict));

  const ConversionRequest convreq = CreatePredictionRequest("かん字じ典");
  const std::vector<Result> results = decoder_->Decode(convreq);
  EXPECT_GE(results.size(), 5);

  // composition from handwriting output
  EXPECT_TRUE(FindResultByKeyValue(results, "かんじじてん", "かん字じ典"));
  EXPECT_TRUE(FindResultByKeyValue(results, "かlv字じ典", "かlv字じ典"));
  // look-up results
  EXPECT_TRUE(FindResultByKeyValue(results, "かんじじてん", "漢字辞典"));
  EXPECT_TRUE(FindResultByKeyValue(results, "かんじじてん", "漢字字典"));
  EXPECT_TRUE(FindResultByKeyValue(results, "かんじじてん", "換字字典"));

  for (const Result& result : results) {
    if (result.value == "かん字じ典") {
      // Top recognition result
      EXPECT_EQ(result.wcost, 0);
    } else if (result.key == "かんじじてん") {
      EXPECT_GE(result.wcost, kCostOffset);
    }
  }
}

TEST_F(HandwritingDecoderTest, HandwritingT13N) {
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(&request_);
  request_.mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("キた");
    composition_event->set_probability(0.99);
    composition_event = command.add_composition_events();
    composition_event->set_composition_string("もた");
    composition_event->set_probability(0.01);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  // reverse conversion
  {
    Result result;
    result.key = "きた";  // T13N key can be looked up
    result.value = "きた";

    EXPECT_CALL(*realtime_decoder_,
                ReverseDecode(Truly([](const ConversionRequest& request) {
                  return request.request_type() ==
                             ConversionRequest::REVERSE_CONVERSION &&
                         request.key() == "キた";
                })))
        .WillOnce(Return(std::vector<Result>({result})));
  }

  auto mock_dict = std::make_unique<MockDictionary>();
  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupExact(StrEq("きた"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"きた", "きた"},
          {"きた", "北"},
      }});

  InitWithMockDictionary(std::move(mock_dict));

  const ConversionRequest convreq = CreatePredictionRequest("キタ");
  const std::vector<Result> results = decoder_->Decode(convreq);

  EXPECT_GE(results.size(), 2);
  // composition from handwriting output
  EXPECT_TRUE(FindResultByKeyValue(results, "きた", "キた"));
  EXPECT_TRUE(FindResultByKeyValue(results, "もた", "もた"));
}

TEST_F(HandwritingDecoderTest, HandwritingNoHiragana) {
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(&request_);
  request_.mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("南");
    composition_event->set_probability(0.9);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  // reverse conversion will not be called
  EXPECT_CALL(*realtime_decoder_, ReverseDecode(_)).Times(0);

  auto mock_dict = std::make_unique<MockDictionary>();
  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(0);
  EXPECT_CALL(*mock_dict, LookupExact(_, _, _)).Times(0);

  InitWithMockDictionary(std::move(mock_dict));

  const ConversionRequest convreq = CreatePredictionRequest("南");
  const std::vector<Result> results = decoder_->Decode(convreq);
  EXPECT_GE(results.size(), 1);
  // composition from handwriting output
  EXPECT_TRUE(FindResultByKeyValue(results, "南", "南"));
}

TEST_F(HandwritingDecoderTest, HandwritingRealtime) {
  // Handwriting request
  request_test_util::FillMobileRequestForHandwriting(&request_);
  request_.mutable_decoder_experiment_params()
      ->set_max_composition_event_to_process(1);
  {
    commands::SessionCommand command;
    commands::SessionCommand::CompositionEvent* composition_event =
        command.add_composition_events();
    composition_event->set_composition_string("ばらが");
    composition_event->set_probability(0.9);
    composer_->Reset();
    composer_->SetCompositionsForHandwriting(command.composition_events());
  }

  // Reverse conversion
  {
    Result result;
    result.key = "ばらが";
    result.value = "ばらが";

    EXPECT_CALL(*realtime_decoder_,
                ReverseDecode(Truly([](const ConversionRequest& request) {
                  return request.request_type() ==
                             ConversionRequest::REVERSE_CONVERSION &&
                         request.key() == "ばらが";
                })))
        .WillOnce(Return(std::vector<Result>({result})));
  }

  auto mock_dict = std::make_unique<MockDictionary>();
  EXPECT_CALL(*mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_dict, LookupExact(StrEq("ばらが"), _, _))
      .WillRepeatedly(InvokeCallbackWithKeyValues{{
          {"ばらが", "薔薇が"},
      }});

  InitWithMockDictionary(std::move(mock_dict));

  const ConversionRequest convreq = CreatePredictionRequest("ばらが");
  const std::vector<Result> results = decoder_->Decode(convreq);
  EXPECT_GE(results.size(), 1);
  EXPECT_TRUE(FindResultByKeyValue(results, "ばらが", "薔薇が"));
}

}  // namespace
}  // namespace mozc::prediction
