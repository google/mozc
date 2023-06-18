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

#include "base/logging.h"
#include "converter/connector.h"
#include "converter/converter.h"
#include "converter/immutable_converter.h"
#include "converter/segmenter.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary.h"
#include "dictionary/user_pos.h"
#include "engine/user_data_manager_interface.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/rescorer_interface.h"
#include "prediction/suggestion_filter.h"
#include "prediction/user_history_predictor.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using ::mozc::dictionary::DictionaryImpl;
using ::mozc::dictionary::PosGroup;
using ::mozc::dictionary::SuffixDictionary;
using ::mozc::dictionary::SuppressionDictionary;
using ::mozc::dictionary::SystemDictionary;
using ::mozc::dictionary::UserDictionary;
using ::mozc::dictionary::UserPos;
using ::mozc::dictionary::ValueDictionary;
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

std::unique_ptr<prediction::RescorerInterface> MaybeCreateRescorer(
    const DataManagerInterface &data_manager) {
  return nullptr;
}

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
      return absl::ResourceExhaustedError("engigine.cc: " #ptr " is null"); \
  } while (false)

  RETURN_IF_NULL(data_manager);

  suppression_dictionary_ = std::make_unique<SuppressionDictionary>();
  RETURN_IF_NULL(suppression_dictionary_);

  pos_matcher_ = std::make_unique<dictionary::PosMatcher>(
      data_manager->GetPosMatcherData());
  RETURN_IF_NULL(pos_matcher_);

  std::unique_ptr<UserPos> user_pos =
      UserPos::CreateFromDataManager(*data_manager);
  RETURN_IF_NULL(user_pos);

  user_dictionary_ = std::make_unique<UserDictionary>(
      std::move(user_pos), *pos_matcher_, suppression_dictionary_.get());
  RETURN_IF_NULL(user_dictionary_);

  const char *dictionary_data = nullptr;
  int dictionary_size = 0;
  data_manager->GetSystemDictionaryData(&dictionary_data, &dictionary_size);

  absl::StatusOr<std::unique_ptr<SystemDictionary>> sysdic =
      SystemDictionary::Builder(dictionary_data, dictionary_size).Build();
  if (!sysdic.ok()) {
    return std::move(sysdic).status();
  }
  auto value_dic = std::make_unique<ValueDictionary>(*pos_matcher_,
                                                     &(*sysdic)->value_trie());
  dictionary_ = std::make_unique<DictionaryImpl>(
      *std::move(sysdic), std::move(value_dic), user_dictionary_.get(),
      suppression_dictionary_.get(), pos_matcher_.get());
  RETURN_IF_NULL(dictionary_);

  absl::string_view suffix_key_array_data, suffix_value_array_data;
  const uint32_t *token_array = nullptr;
  data_manager->GetSuffixDictionaryData(&suffix_key_array_data,
                                        &suffix_value_array_data, &token_array);
  suffix_dictionary_ = std::make_unique<SuffixDictionary>(
      suffix_key_array_data, suffix_value_array_data, token_array);
  RETURN_IF_NULL(suffix_dictionary_);

  auto status_or_connector = Connector::CreateFromDataManager(*data_manager);
  if (!status_or_connector.ok()) {
    return std::move(status_or_connector).status();
  }
  connector_ = *std::move(status_or_connector);

  segmenter_ = Segmenter::CreateFromDataManager(*data_manager);
  RETURN_IF_NULL(segmenter_);

  pos_group_ = std::make_unique<PosGroup>(data_manager->GetPosGroupData());
  RETURN_IF_NULL(pos_group_);

  {
    absl::StatusOr<SuggestionFilter> status_or_suggestion_filter =
        SuggestionFilter::Create(data_manager->GetSuggestionFilterData());
    if (!status_or_suggestion_filter.ok()) {
      return std::move(status_or_suggestion_filter).status();
    }
    suggestion_filter_ = *std::move(status_or_suggestion_filter);
  }

  immutable_converter_ = std::make_unique<ImmutableConverterImpl>(
      dictionary_.get(), suffix_dictionary_.get(),
      suppression_dictionary_.get(), connector_, segmenter_.get(),
      pos_matcher_.get(), pos_group_.get(), suggestion_filter_);
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
    rescorer_ = MaybeCreateRescorer(*data_manager);
    auto dictionary_predictor =
        std::make_unique<prediction::DictionaryPredictor>(
            *data_manager, converter_.get(), immutable_converter_.get(),
            dictionary_.get(), suffix_dictionary_.get(), connector_,
            segmenter_.get(), pos_matcher_.get(), suggestion_filter_,
            rescorer_.get());
    RETURN_IF_NULL(dictionary_predictor);

    const bool enable_content_word_learning = is_mobile;
    auto user_history_predictor =
        std::make_unique<prediction::UserHistoryPredictor>(
            dictionary_.get(), pos_matcher_.get(),
            suppression_dictionary_.get(), enable_content_word_learning);
    RETURN_IF_NULL(user_history_predictor);

    if (is_mobile) {
      predictor = prediction::MobilePredictor::CreateMobilePredictor(
          std::move(dictionary_predictor), std::move(user_history_predictor));
    } else {
      predictor = prediction::DefaultPredictor::CreateDefaultPredictor(
          std::move(dictionary_predictor), std::move(user_history_predictor));
    }
    RETURN_IF_NULL(predictor);
  }
  predictor_ = predictor.get();  // Keep the reference

  auto rewriter =
      std::make_unique<RewriterImpl>(converter_.get(), data_manager.get(),
                                     pos_group_.get(), dictionary_.get());
  RETURN_IF_NULL(rewriter);
  rewriter_ = rewriter.get();  // Keep the reference

  converter_->Init(pos_matcher_.get(), suppression_dictionary_.get(),
                   std::move(predictor), std::move(rewriter),
                   immutable_converter_.get());

  user_data_manager_ =
      std::make_unique<UserDataManagerImpl>(predictor_, rewriter_);

  data_manager_ = std::move(data_manager);

  return absl::Status();

#undef RETURN_IF_NULL
}

bool Engine::Reload() {
  if (!user_dictionary_) {
    return true;
  }
  VLOG(1) << "Reloading user dictionary";
  bool result_dictionary = user_dictionary_->Reload();
  VLOG(1) << "Reloading UserDataManager";
  bool result_user_data = GetUserDataManager()->Reload();
  return result_dictionary && result_user_data;
}

}  // namespace mozc
