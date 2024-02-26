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

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/logging.h"
#include "base/protobuf/message.h"
#include "base/vlog.h"
#include "converter/converter.h"
#include "converter/immutable_converter.h"
#include "data_manager/data_manager_interface.h"
#include "engine/data_loader.h"
#include "engine/modules.h"
#include "engine/spellchecker_interface.h"
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
  bool ClearUserPredictionEntry(absl::string_view key,
                                absl::string_view value) override;
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

bool UserDataManager::ClearUserPredictionEntry(const absl::string_view key,
                                               const absl::string_view value) {
  return predictor_->ClearHistoryEntry(key, value);
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

Engine::Engine()
    : loader_(std::make_unique<DataLoader>()),
      modules_(std::make_unique<engine::Modules>()) {}

absl::Status Engine::ReloadModules(std::unique_ptr<engine::Modules> modules,
                                   bool is_mobile) {
  ReloadAndWait();
  return Init(std::move(modules), is_mobile);
}

absl::Status Engine::Init(
    std::unique_ptr<engine::Modules> modules, bool is_mobile) {
#define RETURN_IF_NULL(ptr)                                               \
  do {                                                                    \
    if (!(ptr))                                                           \
      return absl::ResourceExhaustedError("engine.cc: " #ptr " is null"); \
  } while (false)

  RETURN_IF_NULL(modules);

  // Keeps the previous spellchecker if exists.
  const engine::SpellcheckerInterface *spellchecker =
      modules_->GetSpellchecker();

  modules_ = std::move(modules);
  modules_->SetSpellchecker(spellchecker);

  immutable_converter_ = std::make_unique<ImmutableConverter>(*modules_);
  RETURN_IF_NULL(immutable_converter_);

  // Since predictor and rewriter require a pointer to a converter instance,
  // allocate it first without initialization. It is initialized at the end of
  // this method.
  // TODO(noriyukit): This circular dependency is a bad design as careful
  // handling is necessary to avoid infinite loop. Find more beautiful design
  // and fix it!
  converter_ = std::make_unique<Converter>();
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

  converter_->Init(*modules_, std::move(predictor), std::move(rewriter),
                   immutable_converter_.get());

  user_data_manager_ = std::make_unique<UserDataManager>(predictor_, rewriter_);

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

bool Engine::ReloadAndWait() {
  if (!Reload()) {
    return false;
  }
  if (modules_->GetUserDictionary()) {
    modules_->GetUserDictionary()->WaitForReloader();
  }
  return GetUserDataManager()->Wait();
}

bool Engine::MaybeReloadEngine(EngineReloadResponse *response) {
  if (!loader_ || response == nullptr) {
    return false;
  }

  // Maybe build new engine if new request is received.
  // EngineBuilder::Build just returns a future object so
  // client needs to replace the new engine when the future is the ready to use.
  if (!engine_response_future_ && current_data_id_ != latest_data_id_ &&
      latest_data_id_ != 0) {
    engine_response_future_ = loader_->Build(latest_data_id_);
    // Wait the engine if the no new engine is loaded so far.
    if (current_data_id_ == 0 || always_wait_for_engine_response_future_) {
      engine_response_future_->Wait();
    }
  }

  if (!engine_response_future_ || !engine_response_future_->Ready()) {
    // Response is not ready to reload engine_.
    return false;
  }

  // Replaces the engine when the new engine is ready to use.
  mozc::DataLoader::Response &&engine_response =
      std::move(*engine_response_future_).Get();
  engine_response_future_.reset();
  *response = engine_response.response;

  if (!engine_response.modules ||
      engine_response.response.status() != EngineReloadResponse::RELOAD_READY) {
    // The engine_response does not contain a valid result.

    // This engine id causes a critical error, so rollback the id.
    LOG(ERROR) << "Failure in engine loading: "
               << protobuf::Utf8Format(engine_response.response);
    const uint64_t rollback_id =
        loader_->UnregisterRequest(engine_response.id);
    // Update latest_engine_id_ if latest_engine_id_ == engine_response.id.
    // Otherwise, latest_engine_id_ may already be updated by the new request.
    latest_data_id_.compare_exchange_strong(engine_response.id, rollback_id);

    return false;
  }

  if (user_data_manager_) {
    user_data_manager_->Wait();
  }

  // Reloads DataManager.
  const bool is_mobile = engine_response.response.request().engine_type() ==
                         EngineReloadRequest::MOBILE;
  absl::Status reload_status =
      ReloadModules(std::move(engine_response.modules), is_mobile);
  if (!reload_status.ok()) {
    LOG(ERROR) << reload_status;
    return false;
  }

  current_data_id_ = engine_response.id;
  response->set_status(EngineReloadResponse::RELOADED);
  return true;
}

bool Engine::SendEngineReloadRequest(const EngineReloadRequest &request) {
  if (!loader_) {
    return false;
  }
  latest_data_id_ = loader_->RegisterRequest(request);
  return true;
}

}  // namespace mozc
