// Copyright 2010-2016, Google Inc.
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

#include "base/port.h"
#include "base/string_piece.h"
#include "data_manager/data_manager_interface.h"

namespace mozc {

// This data manager parses a data set file image and extracts each data
// (dictionary, LM, etc.).
// TODO(noriyukit): Migrate all the embedded data managers, such as
// oss/oss_data_manager.h, to this one.
class DataManager : public DataManagerInterface {
 public:
  DataManager();
  ~DataManager() override;

  bool InitFromArray(StringPiece array, StringPiece magic);

  // The following interfaces are implemented.
  void GetConnectorData(const char **data, size_t *size) const override;
  void GetSystemDictionaryData(const char **data, int *size) const override;
  void GetCollocationData(const char **array, size_t *size) const override;
  void GetCollocationSuppressionData(const char **array,
                                     size_t *size) const override;
  void GetSuggestionFilterData(const char **data, size_t *size) const override;
  const uint8 *GetPosGroupData() const override;

  // The following interfaces are not yet implemented.
  // TODO(noriyukit): Implements all the interfaces by migrating embedded C++
  // structures to a data set file.
  const dictionary::UserPOS::POSToken *GetUserPOSData() const override;
  const dictionary::POSMatcher *GetPOSMatcher() const override;
  void GetSegmenterData(size_t *l_num_elements, size_t *r_num_elements,
                        const uint16 **l_table, const uint16 **r_table,
                        size_t *bitarray_num_bytes, const char **bitarray_data,
                        const uint16 **boundary_data) const override;

  void GetSuffixDictionaryData(const dictionary::SuffixToken **data,
                               size_t *size) const override;
  void GetReadingCorrectionData(const ReadingCorrectionItem **array,
                                size_t *size) const override;
  void GetSymbolRewriterData(const EmbeddedDictionary::Token **data,
                             size_t *size) const override;
#ifndef NO_USAGE_REWRITER
  void GetUsageRewriterData(
      const ConjugationSuffix **base_conjugation_suffix,
      const ConjugationSuffix **conjugation_suffix_data,
      const int **conjugation_suffix_data_index,
      const UsageDictItem **usage_data_value) const override;
#endif  // NO_USAGE_REWRITER
  void GetCounterSuffixSortedArray(const CounterSuffixEntry **array,
                                   size_t *size) const override;

 private:
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

  DISALLOW_COPY_AND_ASSIGN(DataManager);
};

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_DATA_MANAGER_H_
