// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_DATA_MANAGER_DATA_MANAGER_H_
#define MOZC_DATA_MANAGER_DATA_MANAGER_H_

#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "base/mmap.h"
#include "base/port.h"
#include "data_manager/data_manager_interface.h"
#include "absl/strings/string_view.h"

namespace mozc {

class DataSetReader;  // Forward-declare this as it is used privately.

// This data manager parses a data set file image and extracts each data
// (dictionary, LM, etc.).
// TODO(noriyukit): Migrate all the embedded data managers, such as
// oss/oss_data_manager.h, to this one.
class DataManager : public DataManagerInterface {
 public:
  // Return status for initialization.
  enum class Status {
    OK = 0,
    ENGINE_VERSION_MISMATCH = 1,
    DATA_MISSING = 2,
    DATA_BROKEN = 3,
    MMAP_FAILURE = 4,
    UNKNOWN = 5,
  };

  static std::string StatusCodeToString(Status code);

  DataManager();
  ~DataManager() override;

  // Parses |array| and extracts byte blocks of data set.  The |array| must
  // outlive this instance.  The second version specifies a custom magic number
  // to expect (e.g., mock data set has a different magic number).
  Status InitFromArray(absl::string_view array);
  Status InitFromArray(absl::string_view array, absl::string_view magic);

  // The same as above InitFromArray() but the data is loaded using mmap, which
  // is owned in this instance.
  Status InitFromFile(const std::string &path);
  Status InitFromFile(const std::string &path, absl::string_view magic);

  // The same as above InitFromArray() but only parses data set for user pos
  // manager.  For mozc runtime modules, use InitFromArray() because this method
  // is only for build tools, e.g., rewriter/dictionary_generator.cc (some build
  // tools depend on user pos data to create outputs, so we need to handle
  // partial data set).
  Status InitUserPosManagerDataFromArray(absl::string_view array,
                                         absl::string_view magic);
  Status InitUserPosManagerDataFromFile(const std::string &path,
                                        absl::string_view magic);

  // Implementation of DataManagerInterface.
  const uint16 *GetPOSMatcherData() const override;
  void GetUserPOSData(absl::string_view *token_array_data,
                      absl::string_view *string_array_data) const override;
  void GetConnectorData(const char **data, size_t *size) const override;
  void GetSystemDictionaryData(const char **data, int *size) const override;
  void GetCollocationData(const char **array, size_t *size) const override;
  void GetCollocationSuppressionData(const char **array,
                                     size_t *size) const override;
  void GetSuggestionFilterData(const char **data, size_t *size) const override;
  const uint8 *GetPosGroupData() const override;
  void GetSegmenterData(size_t *l_num_elements, size_t *r_num_elements,
                        const uint16 **l_table, const uint16 **r_table,
                        size_t *bitarray_num_bytes, const char **bitarray_data,
                        const uint16 **boundary_data) const override;
  void GetCounterSuffixSortedArray(const char **array,
                                   size_t *size) const override;
  void GetSuffixDictionaryData(absl::string_view *key_array_data,
                               absl::string_view *value_array_data,
                               const uint32 **token_array) const override;
  void GetReadingCorrectionData(
      absl::string_view *value_array_data, absl::string_view *error_array_data,
      absl::string_view *correction_array_data) const override;
  void GetSymbolRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const override;
  void GetEmoticonRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const override;
  void GetEmojiRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const override;
  void GetSingleKanjiRewriterData(
      absl::string_view *token_array_data, absl::string_view *string_array_data,
      absl::string_view *variant_type_array_data,
      absl::string_view *variant_token_array_data,
      absl::string_view *variant_string_array_data,
      absl::string_view *noun_prefix_token_array_data,
      absl::string_view *noun_prefix_string_array_data) const override;
  void GetZeroQueryData(
      absl::string_view *zero_query_token_array_data,
      absl::string_view *zero_query_string_array_data,
      absl::string_view *zero_query_number_token_array_data,
      absl::string_view *zero_query_number_string_array_data) const override;

#ifndef NO_USAGE_REWRITER
  void GetUsageRewriterData(
      absl::string_view *base_conjugation_suffix_data,
      absl::string_view *conjugation_suffix_data,
      absl::string_view *conjugation_index_data,
      absl::string_view *usage_items_data,
      absl::string_view *string_array_data) const override;
#endif  // NO_USAGE_REWRITER

  absl::string_view GetTypingModel(const std::string &name) const override;
  absl::string_view GetDataVersion() const override;

 private:
  Status InitFromReader(const DataSetReader &reader);

  Mmap mmap_;
  absl::string_view pos_matcher_data_;
  absl::string_view user_pos_token_array_data_;
  absl::string_view user_pos_string_array_data_;
  absl::string_view connection_data_;
  absl::string_view dictionary_data_;
  absl::string_view suggestion_filter_data_;
  absl::string_view collocation_data_;
  absl::string_view collocation_suppression_data_;
  absl::string_view pos_group_data_;
  absl::string_view boundary_data_;
  size_t segmenter_compressed_lsize_;
  size_t segmenter_compressed_rsize_;
  absl::string_view segmenter_ltable_;
  absl::string_view segmenter_rtable_;
  absl::string_view segmenter_bitarray_;
  absl::string_view counter_suffix_data_;
  absl::string_view suffix_key_array_data_;
  absl::string_view suffix_value_array_data_;
  absl::string_view suffix_token_array_data_;
  absl::string_view reading_correction_value_array_data_;
  absl::string_view reading_correction_error_array_data_;
  absl::string_view reading_correction_correction_array_data_;
  absl::string_view symbol_token_array_data_;
  absl::string_view symbol_string_array_data_;
  absl::string_view emoticon_token_array_data_;
  absl::string_view emoticon_string_array_data_;
  absl::string_view emoji_token_array_data_;
  absl::string_view emoji_string_array_data_;
  absl::string_view single_kanji_token_array_data_;
  absl::string_view single_kanji_string_array_data_;
  absl::string_view single_kanji_variant_type_data_;
  absl::string_view single_kanji_variant_token_array_data_;
  absl::string_view single_kanji_variant_string_array_data_;
  absl::string_view single_kanji_noun_prefix_token_array_data_;
  absl::string_view single_kanji_noun_prefix_string_array_data_;
  absl::string_view zero_query_token_array_data_;
  absl::string_view zero_query_string_array_data_;
  absl::string_view zero_query_number_token_array_data_;
  absl::string_view zero_query_number_string_array_data_;
  absl::string_view usage_base_conjugation_suffix_data_;
  absl::string_view usage_conjugation_suffix_data_;
  absl::string_view usage_conjugation_index_data_;
  absl::string_view usage_items_data_;
  absl::string_view usage_string_array_data_;
  std::vector<std::pair<std::string, absl::string_view>> typing_model_data_;
  absl::string_view data_version_;

  DISALLOW_COPY_AND_ASSIGN(DataManager);
};

// Print helper for DataManager::Status.  Logging, e.g., CHECK_EQ(), requires
// arguments to be printable.
std::ostream &operator<<(std::ostream &os, DataManager::Status status);

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_DATA_MANAGER_H_
