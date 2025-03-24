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

#ifndef MOZC_DATA_MANAGER_DATA_MANAGER_H_
#define MOZC_DATA_MANAGER_DATA_MANAGER_H_

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/mmap.h"

namespace mozc {

class DataSetReader;  // Forward-declare this as it is used privately.

// This data manager parses a data set file image and extracts each data
// (dictionary, LM, etc.).
// TODO(noriyukit): Migrate all the embedded data managers, such as
// oss/oss_data_manager.h, to this one.
class DataManager {
 public:
  static absl::string_view GetDataSetMagicNumber(absl::string_view type);

  // Creates an instance of *const* DataManager from a data set file or returns
  // error status on failure.
  using DMStatusOr = absl::StatusOr<std::unique_ptr<const DataManager>>;

  static DMStatusOr CreateFromFile(const std::string &path);
  static DMStatusOr CreateFromFile(const std::string &path,
                                   absl::string_view magic);

  static DMStatusOr CreateFromArray(absl::string_view array);
  static DMStatusOr CreateFromArray(absl::string_view array,
                                    absl::string_view magic);
  static DMStatusOr CreateFromArray(absl::string_view array,
                                    size_t magic_length);

  static DMStatusOr CreateUserPosManagerDataFromArray(absl::string_view array,
                                                      absl::string_view magic);
  static DMStatusOr CreateUserPosManagerDataFromFile(const std::string &path,
                                                     absl::string_view magic);

  DataManager(const DataManager &) = delete;
  DataManager &operator=(const DataManager &) = delete;
  virtual ~DataManager() = default;

  virtual std::optional<std::string> GetFilename() const { return filename_; }
  virtual absl::Span<const uint16_t> GetPosMatcherData() const;
  virtual void GetUserPosData(absl::string_view *token_array_data,
                              absl::string_view *string_array_data) const;
  virtual absl::string_view GetConnectorData() const;
  virtual absl::string_view GetSystemDictionaryData() const;
  virtual absl::Span<const uint32_t> GetCollocationData() const;
  virtual absl::Span<const uint32_t> GetCollocationSuppressionData() const;
  virtual absl::Span<const uint32_t> GetSuggestionFilterData() const;
  virtual absl::Span<const uint8_t> GetPosGroupData() const;
  virtual void GetSegmenterData(
      size_t *l_num_elements, size_t *r_num_elements,
      absl::Span<const uint16_t> *l_table, absl::Span<const uint16_t> *r_table,
      absl::Span<const char> *bitarray_data,
      absl::Span<const uint16_t> *boundary_data) const;
  absl::string_view GetCounterSuffixSortedArray() const;
  virtual void GetSuffixDictionaryData(
      absl::string_view *key_array, absl::string_view *value_array,
      absl::Span<const uint32_t> *token_array) const;
  virtual void GetReadingCorrectionData(
      absl::string_view *value_array_data, absl::string_view *error_array_data,
      absl::string_view *correction_array_data) const;
  virtual void GetSymbolRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const;
  virtual void GetEmoticonRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const;
  virtual void GetEmojiRewriterData(absl::string_view *token_array_data,
                                    absl::string_view *string_array_data) const;
  virtual void GetSingleKanjiRewriterData(
      absl::string_view *token_array_data, absl::string_view *string_array_data,
      absl::string_view *variant_type_array_data,
      absl::string_view *variant_token_array_data,
      absl::string_view *variant_string_array_data,
      absl::string_view *noun_prefix_token_array_data,
      absl::string_view *noun_prefix_string_array_data) const;
  virtual void GetA11yDescriptionRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const;
  virtual void GetZeroQueryData(
      absl::string_view *zero_query_token_array_data,
      absl::string_view *zero_query_string_array_data,
      absl::string_view *zero_query_number_token_array_data,
      absl::string_view *zero_query_number_string_array_data) const;

#ifndef NO_USAGE_REWRITER
  virtual void GetUsageRewriterData(
      absl::string_view *base_conjugation_suffix_data,
      absl::string_view *conjugation_suffix_data,
      absl::string_view *conjugation_index_data,
      absl::string_view *usage_items_data,
      absl::string_view *string_array_data) const;
#endif  // NO_USAGE_REWRITER

  virtual absl::string_view GetDataVersion() const;

  virtual std::optional<std::pair<size_t, size_t>> GetOffsetAndSize(
      absl::string_view name) const;

 protected:
  DataManager() = default;
  friend std::unique_ptr<DataManager> std::make_unique<DataManager>();

  // Parses |array| and extracts byte blocks of data set.  The |array| must
  // outlive this instance.  The second version specifies a custom magic number
  // to expect (e.g., mock data set has a different magic number).
  // The third version specifies the length of the magic number in bytes.

  absl::Status InitFromArray(absl::string_view array);
  absl::Status InitFromArray(absl::string_view array, absl::string_view magic);
  absl::Status InitFromArray(absl::string_view array, size_t magic_length);

  // The same as above InitFromArray() but the data is loaded using mmap, which
  // is owned in this instance.
  absl::Status InitFromFile(const std::string &path);
  absl::Status InitFromFile(const std::string &path, absl::string_view magic);

  // The same as above InitFromArray() but only parses data set for user pos
  // manager.  For mozc runtime modules, use InitFromArray() because this method
  // is only for build tools, e.g., rewriter/dictionary_generator.cc (some build
  // tools depend on user pos data to create outputs, so we need to handle
  // partial data set).
  absl::Status InitUserPosManagerDataFromArray(absl::string_view array,
                                               absl::string_view magic);
  absl::Status InitUserPosManagerDataFromFile(const std::string &path,
                                              absl::string_view magic);

 private:
  absl::Status InitFromReader(const DataSetReader &reader);

  std::optional<std::string> filename_ = std::nullopt;
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
  absl::string_view a11y_description_token_array_data_;
  absl::string_view a11y_description_string_array_data_;
  absl::string_view zero_query_token_array_data_;
  absl::string_view zero_query_string_array_data_;
  absl::string_view zero_query_number_token_array_data_;
  absl::string_view zero_query_number_string_array_data_;
  absl::string_view usage_base_conjugation_suffix_data_;
  absl::string_view usage_conjugation_suffix_data_;
  absl::string_view usage_conjugation_index_data_;
  absl::string_view usage_items_data_;
  absl::string_view usage_string_array_data_;
  absl::string_view data_version_;
  absl::flat_hash_map<std::string, std::pair<size_t, size_t>> offset_and_size_;
};

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_DATA_MANAGER_H_
