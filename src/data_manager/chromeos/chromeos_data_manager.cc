// Copyright 2010-2013, Google Inc.
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

#include "data_manager/chromeos/chromeos_data_manager.h"
#include "data_manager/chromeos/chromeos_data_manager_factory.h"

#include "base/base.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "converter/boundary_struct.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary_token.h"
#include "rewriter/correction_rewriter.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter_data_structs.h"
#endif  // NO_USAGE_REWRITER

namespace mozc {
namespace chromeos {

const DataManagerInterface *CreateDataManager() {
  return new ChromeOsDataManager();
}

void DeleteDataManager(const DataManagerInterface *data_manager) {
  delete data_manager;
}

namespace {
// kLidGroup[] is defined in the following automatically generated header file.
#include "data_manager/chromeos/pos_group_data.h"
}  // namespace

const uint8 *ChromeOsDataManager::GetPosGroupData() const {
  DCHECK(kLidGroup != NULL);
  return kLidGroup;
}

namespace {
// Automatically generated header containing the definitions of
// kConnectionData_data and kConnectionData_size.
#include "data_manager/chromeos/embedded_connection_data.h"
}  // namespace

void ChromeOsDataManager::GetConnectorData(const char **data,
                                           size_t *size) const {
  *data = kConnectionData_data;
  *size = kConnectionData_size;
}

namespace {
// Automatically generated header containing the definitions of
// kDictionaryData_data[] and kDictionaryData_size.
#include "data_manager/chromeos/embedded_dictionary_data.h"
}  // namespace

void ChromeOsDataManager::GetSystemDictionaryData(
    const char **data, int *size) const {
  *data = kDictionaryData_data;
  *size = kDictionaryData_size;
}

namespace {
// Automatically generated headers containing data set for segmenter.
#include "data_manager/chromeos/boundary_data.h"
#include "data_manager/chromeos/segmenter_data.h"
}  // namespace

void ChromeOsDataManager::GetSegmenterData(
    size_t *l_num_elements, size_t *r_num_elements,
    const uint16 **l_table, const uint16 **r_table,
    size_t *bitarray_num_bytes, const char **bitarray_data,
    const BoundaryData **boundary_data) const {
  *l_num_elements = kCompressedLSize;
  *r_num_elements = kCompressedRSize;
  *l_table = kCompressedLIDTable;
  *r_table = kCompressedRIDTable;
  *bitarray_num_bytes = kSegmenterBitArrayData_size;
  *bitarray_data = kSegmenterBitArrayData_data;
  *boundary_data = kBoundaryData;
}

namespace {
// The generated header defines kSuffixTokens[].
#include "data_manager/chromeos/suffix_data.h"
}  // namespace

void ChromeOsDataManager::GetSuffixDictionaryData(const SuffixToken **tokens,
                                                  size_t *size) const {
  *tokens = kSuffixTokens;
  *size = arraysize(kSuffixTokens);
}

namespace {
// Include kReadingCorrections.
#include "data_manager/chromeos/reading_correction_data.h"
}  // namespace

void ChromeOsDataManager::GetReadingCorrectionData(
    const ReadingCorrectionItem **array,
    size_t *size) const {
  *array = kReadingCorrections;
  *size = arraysize(kReadingCorrections);
}

namespace {
// Include CollocationData::kExistenceData and
// CollocationData::kExistenceFilter_size.
#include "data_manager/chromeos/embedded_collocation_data.h"
// Include CollocationSuppressionData::kExistenceFilter_data and
// CollocationSuppressionData::kExistenceFilter_size.
#include "data_manager/chromeos/embedded_collocation_suppression_data.h"
}  // namespace

void ChromeOsDataManager::GetCollocationData(const char **array,
                                             size_t *size) const {
  *array = CollocationData::kExistenceFilter_data;
  *size = CollocationData::kExistenceFilter_size;
}

void ChromeOsDataManager::GetCollocationSuppressionData(const char **array,
                                                        size_t *size) const {
  *array = CollocationSuppressionData::kExistenceFilter_data;
  *size = CollocationSuppressionData::kExistenceFilter_size;
}

namespace {
// Include kSuggestionFilterData_data and kSuggestionFilterData_size.
#include "data_manager/chromeos/suggestion_filter_data.h"
}  // namespace

void ChromeOsDataManager::GetSuggestionFilterData(const char **data,
                                                  size_t *size) const {
  *data = kSuggestionFilterData_data;
  *size = kSuggestionFilterData_size;
}

namespace {
// Include kSymbolData_token_data and kSymbolData_token_size.
#include "data_manager/chromeos/symbol_rewriter_data.h"
}  // namespace

void ChromeOsDataManager::GetSymbolRewriterData(
    const EmbeddedDictionary::Token **data,
    size_t *size) const {
  *data = kSymbolData_token_data;
  *size = kSymbolData_token_size;
}

#ifndef NO_USAGE_REWRITER
namespace {
#include "rewriter/usage_rewriter_data.h"
}  // namespace

void ChromeOsDataManager::GetUsageRewriterData(
    const ConjugationSuffix **base_conjugation_suffix,
    const ConjugationSuffix **conjugation_suffix_data,
    const int **conjugation_suffix_data_index,
    const UsageDictItem **usage_data_value) const {
  *base_conjugation_suffix = kBaseConjugationSuffix;
  *conjugation_suffix_data = kConjugationSuffixData;
  *conjugation_suffix_data_index = kConjugationSuffixDataIndex;
  *usage_data_value = kUsageData_value;
}
#endif  // NO_USAGE_REWRITER

}  // namespace chromeos
}  // namespace mozc
