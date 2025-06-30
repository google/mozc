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
#include "data_manager/data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/suggestion_filter.h"
#include "prediction/zero_query_dict.h"

namespace mozc {
namespace engine {

class Modules {
 public:
  Modules(const Modules &) = delete;
  Modules &operator=(const Modules &) = delete;

  // Modules must be initialized via Create() method to
  // keep Modules as immutable as possible.
  static absl::StatusOr<std::unique_ptr<Modules>> Create(
      std::unique_ptr<const DataManager> data_manager);

  const DataManager &GetDataManager() const {
    // DataManager must be valid.
    DCHECK(data_manager_);
    return *data_manager_;
  }

  const dictionary::PosMatcher &GetPosMatcher() const {
    DCHECK(pos_matcher_);
    return *pos_matcher_;
  }

  const Connector &GetConnector() const { return connector_; }

  const Segmenter &GetSegmenter() const {
    DCHECK(segmenter_);
    return *segmenter_;
  }

  dictionary::UserDictionaryInterface &GetUserDictionary() const {
    DCHECK(user_dictionary_);
    return *user_dictionary_;
  }

  const dictionary::DictionaryInterface &GetSuffixDictionary() const {
    DCHECK(suffix_dictionary_);
    return *suffix_dictionary_;
  }

  const dictionary::DictionaryInterface &GetDictionary() const {
    DCHECK(dictionary_);
    return *dictionary_;
  }

  const dictionary::PosGroup &GetPosGroup() const {
    DCHECK(pos_group_);
    return *pos_group_;
  }

  const SuggestionFilter &GetSuggestionFilter() const {
    return suggestion_filter_;
  }

  const dictionary::SingleKanjiDictionary &GetSingleKanjiDictionary() const {
    DCHECK(single_kanji_dictionary_);
    return *single_kanji_dictionary_;
  }

  const ZeroQueryDict &GetZeroQueryDict() const { return zero_query_dict_; }
  const ZeroQueryDict &GetZeroQueryNumberDict() const {
    return zero_query_number_dict_;
  }

  engine::SupplementalModelInterface &GetSupplementalModel() const {
    DCHECK(supplemental_model_);
    return *supplemental_model_;
  }

 private:
  friend class ModulesPresetBuilder;
  // For the constructor.
  friend std::unique_ptr<Modules> std::make_unique<Modules>();

  Modules() = default;

  absl::Status Init(std::unique_ptr<const DataManager> data_manager);

  std::unique_ptr<const DataManager> data_manager_;
  std::unique_ptr<const dictionary::PosMatcher> pos_matcher_;
  Connector connector_;
  std::unique_ptr<const Segmenter> segmenter_;
  std::unique_ptr<dictionary::UserDictionaryInterface> user_dictionary_;
  std::unique_ptr<dictionary::DictionaryInterface> suffix_dictionary_;
  std::unique_ptr<dictionary::DictionaryInterface> dictionary_;
  std::unique_ptr<const dictionary::PosGroup> pos_group_;
  SuggestionFilter suggestion_filter_;
  std::unique_ptr<const dictionary::SingleKanjiDictionary>
      single_kanji_dictionary_;
  ZeroQueryDict zero_query_dict_;
  ZeroQueryDict zero_query_number_dict_;

  // `supplemental_model_` is a class variable and initialized by
  // a static singleton object. However, it can also be set to a different value
  // by a PresetBuilder. Since singleton object cannot be deallocated,
  // `supplemental_model_` is managed using a shared_ptr.
  std::shared_ptr<engine::SupplementalModelInterface> supplemental_model_;
};

class ModulesPresetBuilder {
 public:
  ModulesPresetBuilder();

  // Preset functions must be called before Build().
  ModulesPresetBuilder &PresetPosMatcher(
      std::unique_ptr<const dictionary::PosMatcher> pos_matcher);
  ModulesPresetBuilder &PresetUserDictionary(
      std::unique_ptr<dictionary::UserDictionaryInterface> user_dictionary);
  ModulesPresetBuilder &PresetSuffixDictionary(
      std::unique_ptr<dictionary::DictionaryInterface> suffix_dictionary);
  ModulesPresetBuilder &PresetDictionary(
      std::unique_ptr<dictionary::DictionaryInterface> dictionary);
  ModulesPresetBuilder &PresetSingleKanjiDictionary(
      std::unique_ptr<const dictionary::SingleKanjiDictionary>
          single_kanji_dictionary);
  ModulesPresetBuilder &PresetSupplementalModel(
      std::unique_ptr<engine::SupplementalModelInterface> supplemental_model);
  absl::StatusOr<std::unique_ptr<Modules>> Build(
      std::unique_ptr<const DataManager> data_manager);

 private:
  std::unique_ptr<Modules> modules_;
};

}  // namespace engine
}  // namespace mozc

#endif  // MOZC_ENGINE_MODULES_H_
