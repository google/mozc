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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/logging.h"
#include "base/vlog.h"
#include "converter/converter.h"
#include "converter/immutable_converter.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/user_dictionary.h"
#include "engine/modules.h"
#include "engine/user_data_manager_interface.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

using ::mozc::prediction::PredictorInterface;

class UserDataManagerImpl final : public UserDataManagerInterface {
 public:
  UserDataManagerImpl(PredictorInterface *predictor,
                      RewriterInterface *rewriter)
      : predictor_(predictor), rewriter_(rewriter) {}

  UserDataManagerImpl(const UserDataManagerImpl &) = delete;
  UserDataManagerImpl &operator=(const UserDataManagerImpl &) = delete;

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

bool UserDataManagerImpl::Sync() {
  // TODO(noriyukit): In the current implementation, if rewriter_->Sync() fails,
  // predictor_->Sync() is never called. Check if we should call
  // predictor_->Sync() or not.
  return rewriter_->Sync() && predictor_->Sync();
}

bool UserDataManagerImpl::Reload() {
  // TODO(noriyukit): The same TODO as Sync().
  return rewriter_->Reload() && predictor_->Reload();
}

bool UserDataManagerImpl::ClearUserHistory() {
  rewriter_->Clear();
  return true;
}

bool UserDataManagerImpl::ClearUserPrediction() {
  predictor_->ClearAllHistory();
  return true;
}

bool UserDataManagerImpl::ClearUnusedUserPrediction() {
  predictor_->ClearUnusedHistory();
  return true;
}

bool UserDataManagerImpl::ClearUserPredictionEntry(
    const absl::string_view key, const absl::string_view value) {
  return predictor_->ClearHistoryEntry(key, value);
}

bool UserDataManagerImpl::Wait() { return predictor_->Wait(); }

}  // namespace

absl::StatusOr<std::unique_ptr<Engine>> Engine::CreateDesktopEngine(
    std::unique_ptr<const DataManagerInterface> data_manager) {
  auto engine = std::make_unique<Engine>();
  constexpr bool is_mobile = false;
  auto status = engine->Init(std::move(data_manager), is_mobile);
  if (!status.ok()) {
    return status;
  }
  return engine;
}

absl::StatusOr<std::unique_ptr<Engine>> Engine::CreateMobileEngine(
    std::unique_ptr<const DataManagerInterface> data_manager) {
  auto engine = std::make_unique<Engine>();
  constexpr bool is_mobile = true;
  auto status = engine->Init(std::move(data_manager), is_mobile);
  if (!status.ok()) {
    return status;
  }
  return engine;
}

// Since the composite predictor class differs on desktop and mobile, Init()
// takes a function pointer to create an instance of predictor class.
absl::Status Engine::Init(
    std::unique_ptr<const DataManagerInterface> data_manager, bool is_mobile) {
#define RETURN_IF_NULL(ptr)                                                 \
  do {                                                                      \
    if (!(ptr))                                                             \
      return absl::ResourceExhaustedError("engine.cc: " #ptr " is null"); \
  } while (false)

  absl::Status modules_init_status = modules_.Init(data_manager.get());
  if (!modules_init_status.ok()) {
    return modules_init_status;
  }

  immutable_converter_ = std::make_unique<ImmutableConverterImpl>(modules_);
  RETURN_IF_NULL(immutable_converter_);

  // Since predictor and rewriter require a pointer to a converter instance,
  // allocate it first without initialization. It is initialized at the end of
  // this method.
  // TODO(noriyukit): This circular dependency is a bad design as careful
  // handling is necessary to avoid infinite loop. Find more beautiful design
  // and fix it!
  converter_ = std::make_unique<ConverterImpl>();
  RETURN_IF_NULL(converter_);

  std::unique_ptr<PredictorInterface> predictor;
  {
    // Create a predictor with three sub-predictors, dictionary predictor, user
    // history predictor, and extra predictor.
    auto dictionary_predictor =
        std::make_unique<prediction::DictionaryPredictor>(
            *data_manager, converter_.get(), immutable_converter_.get(),
            modules_);
    RETURN_IF_NULL(dictionary_predictor);

    const bool enable_content_word_learning = is_mobile;
    auto user_history_predictor =
        std::make_unique<prediction::UserHistoryPredictor>(
            modules_.GetDictionary(), modules_.GetPosMatcher(),
            modules_.GetSuppressionDictionary(), enable_content_word_learning);
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

  auto rewriter = std::make_unique<RewriterImpl>(
      converter_.get(), data_manager.get(), modules_.GetPosGroup(),
      modules_.GetDictionary());
  RETURN_IF_NULL(rewriter);
  rewriter_ = rewriter.get();  // Keep the reference

  converter_->Init(modules_.GetPosMatcher(),
                   modules_.GetSuppressionDictionary(), std::move(predictor),
                   std::move(rewriter), immutable_converter_.get());

  user_data_manager_ =
      std::make_unique<UserDataManagerImpl>(predictor_, rewriter_);

  data_manager_ = std::move(data_manager);

  return absl::Status();

#undef RETURN_IF_NULL
}

bool Engine::Reload() {
  if (!modules_.GetUserDictionary()) {
    return true;
  }
  MOZC_VLOG(1) << "Reloading user dictionary";
  bool result_dictionary = modules_.GetUserDictionary()->Reload();
  MOZC_VLOG(1) << "Reloading UserDataManager";
  bool result_user_data = GetUserDataManager()->Reload();
  return result_dictionary && result_user_data;
}

bool Engine::ReloadAndWait() {
  if (!Reload()) {
    return false;
  }
  if (modules_.GetUserDictionary()) {
    modules_.GetUserDictionary()->WaitForReloader();
  }
  return GetUserDataManager()->Wait();
}

}  // namespace mozc
