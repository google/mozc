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

#include "engine/engine.h"

#include <memory>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/vlog.h"
#include "converter/converter.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "data_manager/data_manager_interface.h"
#include "engine/data_loader.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"
#include "protocol/engine_builder.pb.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"


namespace mozc {

absl::StatusOr<std::unique_ptr<Engine>> Engine::CreateDesktopEngine(
    std::unique_ptr<const DataManagerInterface> data_manager) {
  constexpr bool kIsMobile = false;

  auto modules = std::make_unique<engine::Modules>();
  absl::Status modules_status = modules->Init(std::move(data_manager));
  if (!modules_status.ok()) {
    return modules_status;
  }

  return CreateEngine(std::move(modules), kIsMobile);
}

absl::StatusOr<std::unique_ptr<Engine>> Engine::CreateMobileEngine(
    std::unique_ptr<const DataManagerInterface> data_manager) {
  constexpr bool kIsMobile = true;

  auto modules = std::make_unique<engine::Modules>();
  absl::Status modules_status = modules->Init(std::move(data_manager));
  if (!modules_status.ok()) {
    return modules_status;
  }

  return CreateEngine(std::move(modules), kIsMobile);
}

absl::StatusOr<std::unique_ptr<Engine>> Engine::CreateEngine(
    std::unique_ptr<engine::Modules> modules, bool is_mobile) {
  // Since Engine() is a private function, std::make_unique does not work.
  auto engine = absl::WrapUnique(new Engine());
  absl::Status engine_status = engine->Init(std::move(modules), is_mobile);
  if (!engine_status.ok()) {
    return engine_status;
  }
  return engine;
}

std::unique_ptr<Engine> Engine::CreateEngine() {
  return absl::WrapUnique(new Engine());
}

absl::Status Engine::ReloadModules(std::unique_ptr<engine::Modules> modules,
                                   bool is_mobile) {
  ReloadAndWait();
  return Init(std::move(modules), is_mobile);
}

absl::Status Engine::Init(std::unique_ptr<engine::Modules> modules,
                          bool is_mobile) {

  auto immutable_converter_factory = [](const engine::Modules &modules) {
    return std::make_unique<ImmutableConverter>(modules);
  };

  auto predictor_factory =
      [is_mobile](const engine::Modules &modules,
                  const ConverterInterface *converter,
                  const ImmutableConverterInterface *immutable_converter) {
        auto dictionary_predictor =
            std::make_unique<prediction::DictionaryPredictor>(
                modules, converter, immutable_converter);

        const bool enable_content_word_learning = is_mobile;
        auto user_history_predictor =
            std::make_unique<prediction::UserHistoryPredictor>(
                modules, enable_content_word_learning);

        return is_mobile ? prediction::MobilePredictor::CreateMobilePredictor(
                               std::move(dictionary_predictor),
                               std::move(user_history_predictor), converter)
                         : prediction::DefaultPredictor::CreateDefaultPredictor(
                               std::move(dictionary_predictor),
                               std::move(user_history_predictor), converter);
      };

  auto rewriter_factory = [](const engine::Modules &modules,
                             const ConverterInterface *converter) {
    return std::make_unique<Rewriter>(modules, *converter);
  };

  auto converter = std::make_unique<Converter>(
      std::move(modules), immutable_converter_factory, predictor_factory,
      rewriter_factory);

  if (!converter) {
    return absl::ResourceExhaustedError("engine.cc: converter_ is null");
  }

  converter_ = std::move(converter);

  return absl::OkStatus();
}

bool Engine::Reload() { return converter_ && converter_->Reload(); }

bool Engine::Sync() { return converter_ && converter_->Sync(); }

bool Engine::Wait() { return converter_ && converter_->Wait(); }

bool Engine::ReloadAndWait() { return Reload() && Wait(); }

bool Engine::ClearUserHistory() {
  if (converter_) {
    converter_->rewriter()->Clear();
  }
  return true;
}

bool Engine::ClearUserPrediction() {
  return converter_ && converter_->predictor()->ClearAllHistory();
}

bool Engine::ClearUnusedUserPrediction() {
  return converter_ && converter_->predictor()->ClearUnusedHistory();
}

bool Engine::MaybeReloadEngine(EngineReloadResponse *response) {
  if (!converter_ || always_wait_for_testing_) {
    loader_.Wait();
  }

  if (loader_.IsRunning() || !loader_response_) {
    return false;
  }

  const bool is_mobile = loader_response_->response.request().engine_type() ==
                         EngineReloadRequest::MOBILE;
  *response = std::move(loader_response_->response);

  const absl::Status reload_status =
      ReloadModules(std::move(loader_response_->modules), is_mobile);
  if (reload_status.ok()) {
    response->set_status(EngineReloadResponse::RELOADED);
  }
  loader_response_.reset();

  return reload_status.ok();
}

bool Engine::SendEngineReloadRequest(const EngineReloadRequest &request) {
  return loader_.StartNewDataBuildTask(
      request, [this](std::unique_ptr<DataLoader::Response> response) {
        loader_response_ = std::move(response);
        return absl::OkStatus();
      });
}

bool Engine::SendSupplementalModelReloadRequest(
    const EngineReloadRequest &request) {
  if (converter_ && converter_->modules()->GetSupplementalModel()) {
    converter_->modules()->GetMutableSupplementalModel()->LoadAsync(request);
  }
  return true;
}

}  // namespace mozc
