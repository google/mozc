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

#ifndef MOZC_DATA_MANAGER_PACKED_SYSTEM_DICTIONARY_DATA_PACKER_H_
#define MOZC_DATA_MANAGER_PACKED_SYSTEM_DICTIONARY_DATA_PACKER_H_

#include <memory>

#include "base/port.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/user_pos.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/embedded_dictionary.h"

namespace mozc {

#ifndef NO_USAGE_REWRITER
struct ConjugationSuffix;
struct UsageDictItem;
#endif  // NO_USAGE_REWRITER

namespace packed {

class SystemDictionaryData;

class SystemDictionaryDataPacker {
 public:
  explicit SystemDictionaryDataPacker(const string &product_version);
  ~SystemDictionaryDataPacker();
  void SetPosTokens(
      const dictionary::UserPOS::POSToken *pos_token_data,
      size_t token_count);
  void SetPosMatcherData(
      const uint16 *rule_id_table,
      size_t rule_id_table_count,
      const dictionary::POSMatcher::Range *const *range_tables,
      size_t range_tables_count);
  void SetReadingCorretions(
      const ReadingCorrectionItem *reading_corrections,
      size_t reading_corrections_count);
  void SetSuggestionFilterData(
      const void *suggestion_filter_data,
      size_t suggestion_filter_data_size);
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
  void SetMozcData(const string &data, const string &magic);

  bool Output(const string &file_path, bool use_gzip);
  bool OutputHeader(const string &file_path, bool use_gzip);

 private:
  std::unique_ptr<SystemDictionaryData> system_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionaryDataPacker);
};

}  // namespace packed
}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_PACKED_SYSTEM_DICTIONARY_DATA_PACKER_H_
