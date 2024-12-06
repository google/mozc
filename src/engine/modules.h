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

#ifndef MOZC_ENGINE_MODULES_H_
#define MOZC_ENGINE_MODULES_H_

#include <memory>
#include <utility>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "converter/connector.h"
#include "converter/segmenter.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/single_kanji_prediction_aggregator.h"
#include "prediction/suggestion_filter.h"
#include "prediction/zero_query_dict.h"

namespace mozc {
namespace engine {

class Modules {
 public:
  Modules() = default;
  Modules(const Modules &) = delete;
  Modules &operator=(const Modules &) = delete;

  absl::Status Init(std::unique_ptr<const DataManagerInterface> data_manager);

  // Preset functions must be called before Init.
  void PresetPosMatcher(
      std::unique_ptr<const dictionary::PosMatcher> pos_matcher);
  void PresetSuppressionDictionary(
      std::unique_ptr<dictionary::SuppressionDictionary>
          suppression_dictionary);
  void PresetUserDictionary(
      std::unique_ptr<dictionary::UserDictionaryInterface> user_dictionary);
  void PresetSuffixDictionary(
      std::unique_ptr<dictionary::DictionaryInterface> suffix_dictionary);
  void PresetDictionary(
      std::unique_ptr<dictionary::DictionaryInterface> dictionary);
  void PresetSingleKanjiPredictionAggregator(
      std::unique_ptr<const prediction::SingleKanjiPredictionAggregator>
          single_kanji_prediction_aggregator);

  const DataManagerInterface &GetDataManager() const {
    // DataManager must be valid.
    DCHECK(data_manager_);
    return *data_manager_;
  }

  const dictionary::PosMatcher *GetPosMatcher() const {
    return pos_matcher_.get();
  }
  const dictionary::SuppressionDictionary *GetSuppressionDictionary() const {
    return suppression_dictionary_.get();
  }
  dictionary::SuppressionDictionary *GetMutableSuppressionDictionary() {
    return suppression_dictionary_.get();
  }
  const Connector &GetConnector() const { return connector_; }
  const Segmenter *GetSegmenter() const { return segmenter_.get(); }
  dictionary::UserDictionaryInterface *GetUserDictionary() const {
    return user_dictionary_.get();
  }
  const dictionary::DictionaryInterface *GetSuffixDictionary() const {
    return suffix_dictionary_.get();
  }
  const dictionary::DictionaryInterface *GetDictionary() const {
    return dictionary_.get();
  }
  const dictionary::PosGroup *GetPosGroup() const { return pos_group_.get(); }
  const SuggestionFilter &GetSuggestionFilter() const {
    return suggestion_filter_;
  }
  const prediction::SingleKanjiPredictionAggregator *
  GetSingleKanjiPredictionAggregator() const {
    return single_kanji_prediction_aggregator_.get();
  }
  const ZeroQueryDict &GetZeroQueryDict() const { return zero_query_dict_; }
  const ZeroQueryDict &GetZeroQueryNumberDict() const {
    return zero_query_number_dict_;
  }

  const engine::SupplementalModelInterface *GetSupplementalModel() const {
    return supplemental_model_;
  }

  engine::SupplementalModelInterface *GetMutableSupplementalModel() {
    return supplemental_model_;
  }

  void SetSupplementalModel(
      engine::SupplementalModelInterface *supplemental_model) {
    supplemental_model_ = supplemental_model;
  }

 private:
  bool initialized_ = false;
  std::unique_ptr<const DataManagerInterface> data_manager_;
  std::unique_ptr<const dictionary::PosMatcher> pos_matcher_;
  std::unique_ptr<dictionary::SuppressionDictionary> suppression_dictionary_;
  Connector connector_;
  std::unique_ptr<const Segmenter> segmenter_;
  std::unique_ptr<dictionary::UserDictionaryInterface> user_dictionary_;
  std::unique_ptr<dictionary::DictionaryInterface> suffix_dictionary_;
  std::unique_ptr<dictionary::DictionaryInterface> dictionary_;
  std::unique_ptr<const dictionary::PosGroup> pos_group_;
  SuggestionFilter suggestion_filter_;
  std::unique_ptr<const prediction::SingleKanjiPredictionAggregator>
      single_kanji_prediction_aggregator_;
  ZeroQueryDict zero_query_dict_;
  ZeroQueryDict zero_query_number_dict_;
  // The owner of supplemental_model_ is Engine.
  engine::SupplementalModelInterface *supplemental_model_ = nullptr;
};

}  // namespace engine
}  // namespace mozc

#endif  // MOZC_ENGINE_MODULES_H_
