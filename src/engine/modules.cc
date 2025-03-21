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

#include "engine/modules.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/connector.h"
#include "converter/segmenter.h"
#include "data_manager/data_manager.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary.h"
#include "dictionary/user_pos.h"
#include "prediction/single_kanji_prediction_aggregator.h"
#include "prediction/suggestion_filter.h"

using ::mozc::dictionary::DictionaryImpl;
using ::mozc::dictionary::PosGroup;
using ::mozc::dictionary::SuffixDictionary;
using ::mozc::dictionary::SystemDictionary;
using ::mozc::dictionary::UserDictionary;
using ::mozc::dictionary::UserPos;
using ::mozc::dictionary::ValueDictionary;

namespace mozc {
namespace engine {

// static
absl::StatusOr<std::unique_ptr<Modules>> Modules::Create(
    std::unique_ptr<const DataManager> data_manager) {
  return ModulesPresetBuilder().Build(std::move(data_manager));
}

absl::Status Modules::Init(std::unique_ptr<const DataManager> data_manager) {
#define RETURN_IF_NULL(ptr)                                                \
  do {                                                                     \
    if (!(ptr))                                                            \
      return absl::ResourceExhaustedError("modules.cc: " #ptr " is null"); \
  } while (false)

  DCHECK(data_manager) << "data_manager is null";
  RETURN_IF_NULL(data_manager);
  data_manager_ = std::move(data_manager);

  if (!pos_matcher_) {
    pos_matcher_ = std::make_unique<dictionary::PosMatcher>(
        data_manager_->GetPosMatcherData());
    RETURN_IF_NULL(pos_matcher_);
  }

  if (!user_dictionary_) {
    std::unique_ptr<UserPos> user_pos =
        UserPos::CreateFromDataManager(*data_manager_);
    RETURN_IF_NULL(user_pos);

    user_dictionary_ =
        std::make_unique<UserDictionary>(std::move(user_pos), *pos_matcher_);
    RETURN_IF_NULL(user_dictionary_);
  }

  if (!dictionary_) {
    absl::string_view dictionary_data =
        data_manager_->GetSystemDictionaryData();

    absl::StatusOr<std::unique_ptr<SystemDictionary>> sysdic =
        SystemDictionary::Builder(dictionary_data.data(),
                                  dictionary_data.size())
            .Build();
    if (!sysdic.ok()) {
      return std::move(sysdic).status();
    }
    auto value_dic = std::make_unique<ValueDictionary>(
        *pos_matcher_, &(*sysdic)->value_trie());
    RETURN_IF_NULL(user_dictionary_);
    RETURN_IF_NULL(pos_matcher_);
    dictionary_ = std::make_unique<DictionaryImpl>(
        *std::move(sysdic), std::move(value_dic), *user_dictionary_,
        *pos_matcher_);
    RETURN_IF_NULL(dictionary_);
  }

  if (!suffix_dictionary_) {
    absl::string_view suffix_key_array_data, suffix_value_array_data;
    absl::Span<const uint32_t> token_array;
    data_manager_->GetSuffixDictionaryData(
        &suffix_key_array_data, &suffix_value_array_data, &token_array);
    suffix_dictionary_ = std::make_unique<SuffixDictionary>(
        suffix_key_array_data, suffix_value_array_data, token_array);
    RETURN_IF_NULL(suffix_dictionary_);
  }

  auto status_or_connector = Connector::CreateFromDataManager(*data_manager_);
  if (!status_or_connector.ok()) {
    return std::move(status_or_connector).status();
  }
  connector_ = *std::move(status_or_connector);

  segmenter_ = Segmenter::CreateFromDataManager(*data_manager_);
  RETURN_IF_NULL(segmenter_);

  pos_group_ = std::make_unique<PosGroup>(data_manager_->GetPosGroupData());
  RETURN_IF_NULL(pos_group_);

  {
    absl::StatusOr<SuggestionFilter> status_or_suggestion_filter =
        SuggestionFilter::Create(data_manager_->GetSuggestionFilterData());
    if (!status_or_suggestion_filter.ok()) {
      return std::move(status_or_suggestion_filter).status();
    }
    suggestion_filter_ = *std::move(status_or_suggestion_filter);
  }

  if (!single_kanji_prediction_aggregator_) {
    single_kanji_prediction_aggregator_ =
        std::make_unique<prediction::SingleKanjiPredictionAggregator>(
            *data_manager_);
    RETURN_IF_NULL(single_kanji_prediction_aggregator_);
  }

  absl::string_view zero_query_token_array_data;
  absl::string_view zero_query_string_array_data;
  absl::string_view zero_query_number_token_array_data;
  absl::string_view zero_query_number_string_array_data;
  data_manager_->GetZeroQueryData(&zero_query_token_array_data,
                                  &zero_query_string_array_data,
                                  &zero_query_number_token_array_data,
                                  &zero_query_number_string_array_data);
  zero_query_dict_.Init(zero_query_token_array_data,
                        zero_query_string_array_data);
  zero_query_number_dict_.Init(zero_query_number_token_array_data,
                               zero_query_number_string_array_data);

  // All modules must not be non-null.
  RETURN_IF_NULL(pos_matcher_);
  RETURN_IF_NULL(segmenter_);
  RETURN_IF_NULL(user_dictionary_);
  RETURN_IF_NULL(suffix_dictionary_);
  RETURN_IF_NULL(pos_group_);
  RETURN_IF_NULL(single_kanji_prediction_aggregator_);

  return absl::Status();
#undef RETURN_IF_NULL
}

ModulesPresetBuilder::ModulesPresetBuilder()
    : modules_(std::make_unique<Modules>()) {}

ModulesPresetBuilder& ModulesPresetBuilder::PresetPosMatcher(
    std::unique_ptr<const dictionary::PosMatcher> pos_matcher) {
  DCHECK(modules_) << "Module is already initialized";
  modules_->pos_matcher_ = std::move(pos_matcher);
  return *this;
}

ModulesPresetBuilder& ModulesPresetBuilder::PresetUserDictionary(
    std::unique_ptr<dictionary::UserDictionaryInterface> user_dictionary) {
  DCHECK(modules_) << "Module is already initialized";
  modules_->user_dictionary_ = std::move(user_dictionary);
  return *this;
}

ModulesPresetBuilder& ModulesPresetBuilder::PresetSuffixDictionary(
    std::unique_ptr<dictionary::DictionaryInterface> suffix_dictionary) {
  DCHECK(modules_) << "Module is already initialized";
  modules_->suffix_dictionary_ = std::move(suffix_dictionary);
  return *this;
}

ModulesPresetBuilder& ModulesPresetBuilder::PresetDictionary(
    std::unique_ptr<dictionary::DictionaryInterface> dictionary) {
  DCHECK(modules_) << "Module is already initialized";
  modules_->dictionary_ = std::move(dictionary);
  return *this;
}

ModulesPresetBuilder&
ModulesPresetBuilder::PresetSingleKanjiPredictionAggregator(
    std::unique_ptr<const prediction::SingleKanjiPredictionAggregator>
        single_kanji_prediction_aggregator) {
  DCHECK(modules_) << "Module is already initialized";
  modules_->single_kanji_prediction_aggregator_ =
      std::move(single_kanji_prediction_aggregator);
  return *this;
}

absl::StatusOr<std::unique_ptr<Modules>> ModulesPresetBuilder::Build(
    std::unique_ptr<const DataManager> data_manager) {
  if (!modules_) {
    return absl::UnavailableError("Build() must not be called twice");
  }
  absl::Status status = modules_->Init(std::move(data_manager));
  if (!status.ok()) {
    return status;
  }
  return std::move(modules_);
}

}  // namespace engine
}  // namespace mozc
