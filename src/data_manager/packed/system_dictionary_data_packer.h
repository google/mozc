// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_DATA_MANAGER_PACKED_SYSTEM_DICTIONARY_DATA_PACKER_H_
#define MOZC_DATA_MANAGER_PACKED_SYSTEM_DICTIONARY_DATA_PACKER_H_

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "converter/boundary_struct.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary_token.h"
#include "dictionary/user_pos.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/embedded_dictionary.h"

namespace mozc {

#ifndef NO_USAGE_REWRITER
struct ConjugationSuffix;
struct UsageDictItem;
#endif  // NO_USAGE_REWRITER
struct CounterSuffixEntry;

namespace packed {

class SystemDictionaryData;

class SystemDictionaryDataPacker {
 public:
  explicit SystemDictionaryDataPacker(const string &product_version);
  ~SystemDictionaryDataPacker();
  void SetPosTokens(
      const UserPOS::POSToken *pos_token_data,
      size_t token_count);
  void SetPosMatcherData(
      const uint16 *rule_id_table,
      size_t rule_id_table_count,
      const POSMatcher::Range *const *range_tables,
      size_t range_tables_count);
  void SetLidGroupData(
      const void *lid_group_data,
      size_t lid_group_data_size);
  void SetBoundaryData(
      const BoundaryData *boundary_data,
      size_t boundary_data_count);
  void SetSuffixTokens(
      const SuffixToken *suffix_tokens,
      size_t suffix_tokens_count);
  void SetReadingCorretions(
      const ReadingCorrectionItem *reading_corrections,
      size_t reading_corrections_count);
  void SetSegmenterData(
      size_t compressed_l_size,
      size_t compressed_r_size,
      const uint16 *compressed_lid_table,
      size_t compressed_lid_table_size,
      const uint16 *compressed_rid_table,
      size_t compressed_rid_table_size,
      const char *segmenter_bit_array_data,
      size_t segmenter_bit_array_data_size);
  void SetSuggestionFilterData(
      const void *suggestion_filter_data,
      size_t suggestion_filter_data_size);
  void SetConnectionData(
      const void *connection_data,
      size_t connection_data_size);
  void SetDictionaryData(
      const void *dictionary_data,
      size_t dictionary_data_size);
  void SetCollocationData(
      const void *collocation_data,
      size_t collocation_data_size);
  void SetCollocationSuppressionData(
      const void *collocation_suppression_data,
      size_t collocation_suppression_data_size);
  void SetSymbolRewriterData(
      const mozc::EmbeddedDictionary::Token *token_data,
      size_t token_size);
#ifndef NO_USAGE_REWRITER
  void SetUsageRewriterData(
      int conjugation_num,
      const ConjugationSuffix *base_conjugation_suffix,
      const ConjugationSuffix *conjugation_suffix_data,
      const int *conjugation_suffix_data_index,
      size_t usage_data_size,
      const UsageDictItem *usage_data_value);
#endif  // NO_USAGE_REWRITER
  void SetCounterSuffixSortedArray(
      const CounterSuffixEntry *suffix_array, size_t size);

  bool Output(const string &file_path, bool use_gzip);
  bool OutputHeader(const string &file_path, bool use_gzip);

 private:
  scoped_ptr<SystemDictionaryData> system_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionaryDataPacker);
};

}  // namespace packed
}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_PACKED_SYSTEM_DICTIONARY_DATA_PACKER_H_
