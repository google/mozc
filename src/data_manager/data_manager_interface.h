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

#ifndef MOZC_DATA_MANAGER_DATA_MANAGER_INTERFACE_H_
#define MOZC_DATA_MANAGER_DATA_MANAGER_INTERFACE_H_

#include <cstdint>
#include <string>

#include "base/port.h"
#include "absl/strings/string_view.h"

namespace mozc {

// Builds those objects that depend on a set of embedded data generated from
// files in data/dictionary, such as dictionary.txt, id.def, etc.
class DataManagerInterface {
 public:
  virtual ~DataManagerInterface() = default;

  // Returns data set for UserPos.
  virtual void GetUserPosData(absl::string_view *token_array_data,
                              absl::string_view *string_array_data) const = 0;

  // Returns a reference to PosMatcher class handling POS rules. Don't
  // delete the returned pointer, which is owned by the manager.
  virtual const uint16_t *GetPosMatcherData() const = 0;

  // Returns the address of an array of lid group.
  virtual const uint8_t *GetPosGroupData() const = 0;

  // Returns the address of connection data and its size.
  virtual void GetConnectorData(const char **data, size_t *size) const = 0;

  // Returns the addresses and their sizes necessary to create a segmenter.
  virtual void GetSegmenterData(size_t *l_num_elements, size_t *r_num_elements,
                                const uint16_t **l_table,
                                const uint16_t **r_table,
                                size_t *bitarray_num_bytes,
                                const char **bitarray_data,
                                const uint16_t **boundary_data) const = 0;

  // Returns the address of system dictionary data and its size.
  virtual void GetSystemDictionaryData(const char **data, int *size) const = 0;

  // Returns the array containing keys, values, and token (lid, rid, cost).
  virtual void GetSuffixDictionaryData(absl::string_view *key_array,
                                       absl::string_view *value_array,
                                       const uint32_t **token_array) const = 0;

  // Gets a reference to reading correction data array and its size.
  virtual void GetReadingCorrectionData(
      absl::string_view *value_array_data, absl::string_view *error_array_data,
      absl::string_view *correction_array_data) const = 0;

  // Gets the address of collocation data array and its size.
  virtual void GetCollocationData(const char **array, size_t *size) const = 0;

  // Gets the address of collocation suppression data array and its size.
  virtual void GetCollocationSuppressionData(const char **array,
                                             size_t *size) const = 0;

  // Gets an address of suggestion filter data array and its size.
  virtual void GetSuggestionFilterData(const char **data,
                                       size_t *size) const = 0;

  // Gets an address of symbol rewriter data array and its size.
  virtual void GetSymbolRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const = 0;

  // Gets an address of symbol rewriter data array and its size.
  virtual void GetEmoticonRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const = 0;

  // Gets EmojiRewriter data.
  virtual void GetEmojiRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const = 0;

  // Gets SingleKanjiRewriter data.
  virtual void GetSingleKanjiRewriterData(
      absl::string_view *token_array_data, absl::string_view *string_array_data,
      absl::string_view *variant_type_array_data,
      absl::string_view *variant_token_array_data,
      absl::string_view *variant_string_array_data,
      absl::string_view *noun_prefix_token_array_data,
      absl::string_view *noun_prefix_string_array_data) const = 0;

  // Gets A11yDescriptionRewriter data.
  virtual void GetA11yDescriptionRewriterData(
      absl::string_view *token_array_data,
      absl::string_view *string_array_data) const = 0;

#ifndef NO_USAGE_REWRITER
  // Gets the usage rewriter data.
  virtual void GetUsageRewriterData(
      absl::string_view *base_conjugation_suffix_data,
      absl::string_view *conjugation_suffix_data,
      absl::string_view *conjugation_suffix_index_data,
      absl::string_view *usage_items_data,
      absl::string_view *string_array_data) const = 0;
#endif  // NO_USAGE_REWRITER

  // Gets the address and size of a sorted array of counter suffix values.
  virtual void GetCounterSuffixSortedArray(const char **array,
                                           size_t *size) const = 0;

  // Gets the zero query prediction data.
  virtual void GetZeroQueryData(
      absl::string_view *zero_query_token_array_data,
      absl::string_view *zero_query_string_array_data,
      absl::string_view *zero_query_number_token_array_data,
      absl::string_view *zero_query_number_string_array_data) const = 0;

  // Gets the typing model binary data for the specified name.
  virtual absl::string_view GetTypingModel(const std::string &name) const = 0;

  // Gets the data version string.
  virtual absl::string_view GetDataVersion() const = 0;

 protected:
  DataManagerInterface() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DataManagerInterface);
};

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_DATA_MANAGER_INTERFACE_H_
