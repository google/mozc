// Copyright 2010-2018, Google Inc.
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
#include "base/string_piece.h"
#include "data_manager/data_manager_interface.h"

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

  static string StatusCodeToString(Status code);

  DataManager();
  ~DataManager() override;

  // Parses |array| and extracts byte blocks of data set.  The |array| must
  // outlive this instance.  The second version specifies a custom magic number
  // to expect (e.g., mock data set has a different magic number).
  Status InitFromArray(StringPiece array);
  Status InitFromArray(StringPiece array, StringPiece magic);

  // The same as above InitFromArray() but the data is loaded using mmap, which
  // is owned in this instance.
  Status InitFromFile(const string &path);
  Status InitFromFile(const string &path, StringPiece magic);

  // The same as above InitFromArray() but only parses data set for user pos
  // manager.  For mozc runtime modules, use InitFromArray() because this method
  // is only for build tools, e.g., rewriter/dictionary_generator.cc (some build
  // tools depend on user pos data to create outputs, so we need to handle
  // partial data set).
  Status InitUserPosManagerDataFromArray(StringPiece array, StringPiece magic);
  Status InitUserPosManagerDataFromFile(const string &path, StringPiece magic);

  // Implementation of DataManagerInterface.
  const uint16 *GetPOSMatcherData() const override;
  void GetUserPOSData(StringPiece *token_array_data,
                      StringPiece *string_array_data) const override;
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
  void GetSuffixDictionaryData(StringPiece *key_array_data,
                               StringPiece *value_array_data,
                               const uint32 **token_array) const override;
  void GetReadingCorrectionData(
      StringPiece *value_array_data, StringPiece *error_array_data,
      StringPiece *correction_array_data) const override;
  void GetSymbolRewriterData(StringPiece *token_array_data,
                             StringPiece *string_array_data) const override;
  void GetEmoticonRewriterData(StringPiece *token_array_data,
                               StringPiece *string_array_data) const override;
  void GetEmojiRewriterData(StringPiece *token_array_data,
                            StringPiece *string_array_data) const override;
  void GetSingleKanjiRewriterData(
      StringPiece *token_array_data,
      StringPiece *string_array_data,
      StringPiece *variant_type_array_data,
      StringPiece *variant_token_array_data,
      StringPiece *variant_string_array_data,
      StringPiece *noun_prefix_token_array_data,
      StringPiece *noun_prefix_string_array_data) const override;
  void GetZeroQueryData(
      StringPiece *zero_query_token_array_data,
      StringPiece *zero_query_string_array_data,
      StringPiece *zero_query_number_token_array_data,
      StringPiece *zero_query_number_string_array_data) const override;

#ifndef NO_USAGE_REWRITER
  void GetUsageRewriterData(
      StringPiece *base_conjugation_suffix_data,
      StringPiece *conjugation_suffix_data,
      StringPiece *conjugation_index_data,
      StringPiece *usage_items_data,
      StringPiece *string_array_data) const override;
#endif  // NO_USAGE_REWRITER

  StringPiece GetTypingModel(const string &name) const override;
  StringPiece GetDataVersion() const override;

 private:
  Status InitFromReader(const DataSetReader &reader);

  Mmap mmap_;
  StringPiece pos_matcher_data_;
  StringPiece user_pos_token_array_data_;
  StringPiece user_pos_string_array_data_;
  StringPiece connection_data_;
  StringPiece dictionary_data_;
  StringPiece suggestion_filter_data_;
  StringPiece collocation_data_;
  StringPiece collocation_suppression_data_;
  StringPiece pos_group_data_;
  StringPiece boundary_data_;
  size_t segmenter_compressed_lsize_;
  size_t segmenter_compressed_rsize_;
  StringPiece segmenter_ltable_;
  StringPiece segmenter_rtable_;
  StringPiece segmenter_bitarray_;
  StringPiece counter_suffix_data_;
  StringPiece suffix_key_array_data_;
  StringPiece suffix_value_array_data_;
  StringPiece suffix_token_array_data_;
  StringPiece reading_correction_value_array_data_;
  StringPiece reading_correction_error_array_data_;
  StringPiece reading_correction_correction_array_data_;
  StringPiece symbol_token_array_data_;
  StringPiece symbol_string_array_data_;
  StringPiece emoticon_token_array_data_;
  StringPiece emoticon_string_array_data_;
  StringPiece emoji_token_array_data_;
  StringPiece emoji_string_array_data_;
  StringPiece single_kanji_token_array_data_;
  StringPiece single_kanji_string_array_data_;
  StringPiece single_kanji_variant_type_data_;
  StringPiece single_kanji_variant_token_array_data_;
  StringPiece single_kanji_variant_string_array_data_;
  StringPiece single_kanji_noun_prefix_token_array_data_;
  StringPiece single_kanji_noun_prefix_string_array_data_;
  StringPiece zero_query_token_array_data_;
  StringPiece zero_query_string_array_data_;
  StringPiece zero_query_number_token_array_data_;
  StringPiece zero_query_number_string_array_data_;
  StringPiece usage_base_conjugation_suffix_data_;
  StringPiece usage_conjugation_suffix_data_;
  StringPiece usage_conjugation_index_data_;
  StringPiece usage_items_data_;
  StringPiece usage_string_array_data_;
  std::vector<std::pair<string, StringPiece>> typing_model_data_;
  StringPiece data_version_;

  DISALLOW_COPY_AND_ASSIGN(DataManager);
};

// Print helper for DataManager::Status.  Logging, e.g., CHECK_EQ(), requires
// arguments to be printable.
std::ostream &operator<<(std::ostream &os, DataManager::Status status);

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_DATA_MANAGER_H_
