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
#include <utility>
#include <vector>

#include "testing/base/public/benchmark.h"
#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/file/temp_dir.h"
#include "base/system_util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/converter.h"
#include "converter/converter_interface.h"
#include "converter/immutable_decoder.h"
#include "engine/android_engine_factory.h"
#include "engine/engine.h"
#include "prediction/realtime_decoder.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "testing/mozctest.h"

namespace mozc {
namespace converter {
namespace {

class ImmutableDecoderBenchmarkFixture {
 public:
  ImmutableDecoderBenchmarkFixture()
      : temp_user_profile_dir_(testing::MakeTempDirectoryOrDie()) {
    SystemUtil::SetUserProfileDirectory(temp_user_profile_dir_.path());
    auto engine_or = AndroidEngineFactory::Create();
    CHECK_OK(engine_or.status());
    engine_ = std::move(engine_or).value();
    const ConverterInterface* converter = engine_->GetConverter().get();
    const converter::Converter* concrete_converter =
        dynamic_cast<const converter::Converter*>(converter);
    CHECK(concrete_converter != nullptr);
    realtime_decoder_ = std::make_unique<prediction::RealtimeDecoder>(
        concrete_converter->immutable_converter(), *converter);
    immutable_decoder_ =
        std::make_unique<ImmutableDecoder>(engine_->GetModulesForTesting());
  }

  const prediction::RealtimeDecoder& realtime_decoder() const {
    return *realtime_decoder_;
  }
  const ImmutableDecoder& immutable_decoder() const {
    return *immutable_decoder_;
  }

 private:
  TempDirectory temp_user_profile_dir_;
  std::unique_ptr<Engine> engine_;
  std::unique_ptr<prediction::RealtimeDecoder> realtime_decoder_;
  std::unique_ptr<ImmutableDecoder> immutable_decoder_;
};

constexpr absl::string_view kSuggestionTestData[] = {
    "よ",
    "よろし",
    "よろしく",
    "よろしくおねがい",
    "よろしくおねがいしま",
    "わ",
    "わたし",
    "わたしの",
    "わたしのなまえ",
    "わたしのなまえは",
    "こ",
    "これ",
    "これは",
    "これはいったい",
    "あ",
    "あり",
    "ありが",
    "ありがとう",
    "ありがとうございま",
};

std::vector<ConversionRequest> CreateRequests(
    std::shared_ptr<const composer::Table> table,
    const commands::Request& request, const config::Config& config,
    ConversionRequest::RequestType request_type) {
  std::vector<ConversionRequest> requests;
  for (const absl::string_view input : kSuggestionTestData) {
    auto composer =
        std::make_unique<composer::Composer>(table, request, config);
    composer->InsertCharacter(std::string(input));
    ConversionRequest::Options options = {
        .request_type = request_type,
        .max_conversion_candidates_size = 10,
        .create_partial_candidates =
            (request_type == ConversionRequest::PREDICTION),
    };
    requests.emplace_back(ConversionRequestBuilder()
                              .SetComposer(*composer)
                              .SetRequestView(request)
                              .SetConfigView(config)
                              .SetOptions(std::move(options))
                              .Build());
  }
  return requests;
}

void BM_RealtimeDecoder_Decode(benchmark::State& state) {
  ImmutableDecoderBenchmarkFixture fixture;
  commands::Request request;
  request_test_util::FillMobileRequest(&request);
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  auto table = std::make_shared<const composer::Table>();

  std::vector<ConversionRequest> requests =
      CreateRequests(table, request, config, ConversionRequest::PREDICTION);

  for (auto s : state) {
    for (const auto& req : requests) {
      benchmark::DoNotOptimize(fixture.realtime_decoder().Decode(req));
    }
  }
}
BENCHMARK(BM_RealtimeDecoder_Decode);

void BM_ImmutableDecoder_Decode(benchmark::State& state) {
  ImmutableDecoderBenchmarkFixture fixture;
  commands::Request request;
  request_test_util::FillMobileRequest(&request);
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  auto table = std::make_shared<const composer::Table>();

  std::vector<ConversionRequest> requests =
      CreateRequests(table, request, config, ConversionRequest::PREDICTION);

  for (auto s : state) {
    for (const auto& req : requests) {
      benchmark::DoNotOptimize(fixture.immutable_decoder().Decode(
          req.key(), req.options(), req.history_result()));
    }
  }
}
BENCHMARK(BM_ImmutableDecoder_Decode);

}  // namespace
}  // namespace converter
}  // namespace mozc
