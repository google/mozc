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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/logging.h"
#include "converter/connector.h"
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
#include "prediction/suggestion_filter.h"

using ::mozc::dictionary::DictionaryImpl;
using ::mozc::dictionary::PosGroup;
using ::mozc::dictionary::SuffixDictionary;
using ::mozc::dictionary::SuppressionDictionary;
using ::mozc::dictionary::SystemDictionary;
using ::mozc::dictionary::UserDictionary;
using ::mozc::dictionary::UserPos;
using ::mozc::dictionary::ValueDictionary;

namespace mozc {
namespace engine {

absl::Status Modules::Init(const DataManagerInterface *data_manager) {
#define RETURN_IF_NULL(ptr)                                                 \
  do {                                                                      \
    if (!(ptr))                                                             \
      return absl::ResourceExhaustedError("modules.cc: " #ptr " is null"); \
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

  return absl::Status();

#undef RETURN_IF_NULL
}

}  // namespace engine
}  // namespace mozc
