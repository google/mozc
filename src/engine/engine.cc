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
#include "converter/immutable_converter.h"
#include "data_manager/data_manager_interface.h"
#include "engine/data_loader.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "engine/user_data_manager_interface.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"
#include "protocol/engine_builder.pb.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

using ::mozc::prediction::PredictorInterface;

class UserDataManager final : public UserDataManagerInterface {
 public:
  UserDataManager(PredictorInterface *predictor, RewriterInterface *rewriter)
      : predictor_(predictor), rewriter_(rewriter) {}

  UserDataManager(const UserDataManager &) = delete;
  UserDataManager &operator=(const UserDataManager &) = delete;

  bool Sync() override;
  bool Reload() override;
  bool ClearUserHistory() override;
  bool ClearUserPrediction() override;
  bool ClearUnusedUserPrediction() override;
  bool Wait() override;

 private:
  PredictorInterface *predictor_;
  RewriterInterface *rewriter_;
};

bool UserDataManager::Sync() {
  // TODO(noriyukit): In the current implementation, if rewriter_->Sync() fails,
  // predictor_->Sync() is never called. Check if we should call
  // predictor_->Sync() or not.
  return rewriter_->Sync() && predictor_->Sync();
}

bool UserDataManager::Reload() {
  // TODO(noriyukit): The same TODO as Sync().
  return rewriter_->Reload() && predictor_->Reload();
}

bool UserDataManager::ClearUserHistory() {
  rewriter_->Clear();
  return true;
}

bool UserDataManager::ClearUserPrediction() {
  predictor_->ClearAllHistory();
  return true;
}

bool UserDataManager::ClearUnusedUserPrediction() {
  predictor_->ClearUnusedHistory();
  return true;
}

bool UserDataManager::Wait() { return predictor_->Wait(); }

}  // namespace

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

Engine::Engine()
    : loader_(std::make_unique<DataLoader>()),
      modules_(std::make_unique<engine::Modules>()) {}

absl::Status Engine::ReloadModules(std::unique_ptr<engine::Modules> modules,
                                   bool is_mobile) {
  ReloadAndWait();
  return Init(std::move(modules), is_mobile);
}

absl::Status Engine::Init(std::unique_ptr<engine::Modules> modules,
                          bool is_mobile) {
#define RETURN_IF_NULL(ptr)                                               \
  do {                                                                    \
    if (!(ptr))                                                           \
      return absl::ResourceExhaustedError("engine.cc: " #ptr " is null"); \
  } while (false)

  RETURN_IF_NULL(modules);

  // Keeps the previous supplemental_model if exists.
  const engine::SupplementalModelInterface *supplemental_model =
      modules_->GetSupplementalModel();

  modules_ = std::move(modules);
  modules_->SetSupplementalModel(supplemental_model);

  immutable_converter_ = std::make_unique<ImmutableConverter>(*modules_);
  RETURN_IF_NULL(immutable_converter_);

  // Since predictor and rewriter require a pointer to a converter instance,
  // allocate it first without initialization. It is initialized at the end of
  // this method.
  // TODO(noriyukit): This circular dependency is a bad design as careful
  // handling is necessary to avoid infinite loop. Find more beautiful design
  // and fix it!
  converter_ = std::make_unique<Converter>(*modules_, *immutable_converter_);
  RETURN_IF_NULL(converter_);

  std::unique_ptr<PredictorInterface> predictor;
  {
    // Create a predictor with three sub-predictors, dictionary predictor, user
    // history predictor, and extra predictor.
    auto dictionary_predictor =
        std::make_unique<prediction::DictionaryPredictor>(
            *modules_, converter_.get(), immutable_converter_.get());
    RETURN_IF_NULL(dictionary_predictor);

    const bool enable_content_word_learning = is_mobile;
    auto user_history_predictor =
        std::make_unique<prediction::UserHistoryPredictor>(
            *modules_, enable_content_word_learning);
    RETURN_IF_NULL(user_history_predictor);

    if (is_mobile) {
      predictor = prediction::MobilePredictor::CreateMobilePredictor(
          std::move(dictionary_predictor), std::move(user_history_predictor),
          converter_.get());
    } else {
      predictor = prediction::DefaultPredictor::CreateDefaultPredictor(
          std::move(dictionary_predictor), std::move(user_history_predictor),
          converter_.get());
    }
    RETURN_IF_NULL(predictor);
  }
  predictor_ = predictor.get();  // Keep the reference

  auto rewriter = std::make_unique<Rewriter>(*modules_, *converter_);
  RETURN_IF_NULL(rewriter);
  rewriter_ = rewriter.get();  // Keep the reference

  converter_->Init(std::move(predictor), std::move(rewriter));

  user_data_manager_ = std::make_unique<UserDataManager>(predictor_, rewriter_);

  initialized_ = true;
  return absl::Status();

#undef RETURN_IF_NULL
}

bool Engine::Reload() {
  if (!modules_->GetUserDictionary()) {
    return true;
  }
  MOZC_VLOG(1) << "Reloading user dictionary";
  bool result_dictionary = modules_->GetUserDictionary()->Reload();
  MOZC_VLOG(1) << "Reloading UserDataManager";
  bool result_user_data = GetUserDataManager()->Reload();
  return result_dictionary && result_user_data;
}

bool Engine::Sync() {
  GetUserDataManager()->Sync();
  if (!modules_->GetUserDictionary()) {
    return true;
  }
  return modules_->GetUserDictionary()->Sync();
}

bool Engine::Wait() {
  if (modules_->GetUserDictionary()) {
    modules_->GetUserDictionary()->WaitForReloader();
  }
  return GetUserDataManager()->Wait();
}

bool Engine::ReloadAndWait() {
  if (!Reload()) {
    return false;
  }
  return Wait();
}

bool Engine::MaybeReloadEngine(EngineReloadResponse *response) {
  if (response == nullptr || !loader_) {
    LOG(ERROR) << "response or loader_ is null";
    return false;
  }

  // In the while loop, tries to reload the new data. If the new data is broken,
  // tries it again as a next round of this while loop.
  while (true) {
    if (!loader_->StartNewDataBuildTask()) {
      // No new build process is running or ready.
      return false;
    }

    std::unique_ptr<DataLoader::Response> loader_response =
        loader_->MaybeMoveDataLoaderResponse();
    if (!loader_response) {
      // No new data is available. The build process is still running.
      return false;
    }

    *response = loader_response->response;
    LOG(INFO) << "New data is ready (install_location="
              << response->request().install_location() << ")";

    if (!loader_response->modules ||
        response->status() != EngineReloadResponse::RELOAD_READY) {
      // The loader_response does not contain a valid result.

      // This request id causes a critical error.
      LOG(ERROR) << "Failure in loading response: " << *response;

      // Unregisters the invalid ID and continues to rebuild a new data loader.
      loader_->ReportLoadFailure(loader_response->id);
      continue;
    }

    if (user_data_manager_) {
      user_data_manager_->Wait();
    }

    // Reloads DataManager.
    const bool is_mobile =
        response->request().engine_type() == EngineReloadRequest::MOBILE;
    absl::Status reload_status =
        ReloadModules(std::move(loader_response->modules), is_mobile);
    if (!reload_status.ok()) {
      LOG(ERROR) << reload_status;

      // Unregisters the invalid ID and continues to rebuild a new data loader.
      loader_->ReportLoadFailure(loader_response->id);
      continue;
    }

    loader_->ReportLoadSuccess(loader_response->id);
    response->set_status(EngineReloadResponse::RELOADED);
    return true;
  }
  ABSL_UNREACHABLE();
}

bool Engine::SendEngineReloadRequest(const EngineReloadRequest &request) {
  if (!loader_) {
    return false;
  }
  loader_->RegisterRequest(request);
  loader_->StartNewDataBuildTask();
  return true;
}

}  // namespace mozc
