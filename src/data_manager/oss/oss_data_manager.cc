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

#include "data_manager/oss/oss_data_manager.h"

#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"
#include "converter/boundary_struct.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary_token.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/counter_suffix.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter_data_structs.h"
#endif  // NO_USAGE_REWRITER

namespace mozc {
namespace oss {

namespace {
// kLidGroup[] is defined in the following automatically generated header file.
#include "data_manager/oss/pos_group_data.h"
}  // namespace

const uint8 *OssDataManager::GetPosGroupData() const {
  DCHECK(kLidGroup != NULL);
  return kLidGroup;
}

namespace {
#ifdef MOZC_USE_SEPARATE_CONNECTION_DATA
const char *kConnectionData_data = NULL;
const size_t kConnectionData_size = 0;
#else  // MOZC_USE_SEPARATE_CONNECTION_DATA
// Automatically generated header containing the definitions of
// kConnectionData_data and kConnectionData_size. We don't embed it when
// connection data is supplied from outside.
#include "data_manager/oss/embedded_connection_data.h"
#endif  // MOZC_USE_SEPARATE_CONNECTION_DATA

char *g_connection_data_address = const_cast<char *>(kConnectionData_data);
int g_connection_data_size = kConnectionData_size;
}  // namespace

#ifdef MOZC_USE_SEPARATE_CONNECTION_DATA
void OssDataManager::SetConnectionData(void *address, size_t size) {
  g_connection_data_address = reinterpret_cast<char *>(address);
  g_connection_data_size = size;
  DCHECK(g_connection_data_address);
  DCHECK_GT(g_connection_data_size, 0);
}
#endif  // MOZC_USE_SEPARATE_CONNECTION_DATA

void OssDataManager::GetConnectorData(const char **data, size_t *size) const {
#ifdef MOZC_USE_SEPARATE_CONNECTION_DATA
    if (!g_connection_data_address || g_connection_data_size == 0) {
      LOG(FATAL) << "Connection data is not yet set.";
      CHECK(false);
    }
#endif
  *data = g_connection_data_address;
  *size = g_connection_data_size;
}

namespace {
#ifdef MOZC_USE_SEPARATE_DICTIONARY
const char *kDictionaryData_data = NULL;
const size_t kDictionaryData_size = 0;
#else  // MOZC_USE_SEPARATE_DICTIONARY
// Automatically generated header containing the definitions of
// kDictionaryData_data[] and kDictionaryData_size.
#include "data_manager/oss/embedded_dictionary_data.h"
#endif  // MOZC_USE_SEPARATE_DICTIONARY

char *g_dictionary_address = const_cast<char *>(kDictionaryData_data);
int g_dictionary_size = kDictionaryData_size;
}  // namespace

void OssDataManager::GetSystemDictionaryData(
    const char **data, int *size) const {
  *data = g_dictionary_address;
  *size = g_dictionary_size;
}

#ifdef MOZC_USE_SEPARATE_DICTIONARY
void OssDataManager::SetDictionaryData(void *address, size_t size) {
  g_dictionary_address = reinterpret_cast<char *>(address);
  g_dictionary_size = size;
  DCHECK(g_dictionary_address);
  DCHECK_GT(g_dictionary_size, 0);
}
#endif  // MOZC_USE_SEPARATE_DICTIONARY

namespace {
// Automatically generated headers containing data set for segmenter.
#include "data_manager/oss/boundary_data.h"
#include "data_manager/oss/segmenter_data.h"
}  // namespace

void OssDataManager::GetSegmenterData(
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
#include "data_manager/oss/suffix_data.h"
}  // namespace

void OssDataManager::GetSuffixDictionaryData(const SuffixToken **tokens,
                                             size_t *size) const {
  *tokens = kSuffixTokens;
  *size = arraysize(kSuffixTokens);
}

namespace {
// Include kReadingCorrections.
#include "data_manager/oss/reading_correction_data.h"
}  // namespace

void OssDataManager::GetReadingCorrectionData(
    const ReadingCorrectionItem **array,
    size_t *size) const {
  *array = kReadingCorrections;
  *size = arraysize(kReadingCorrections);
}

namespace {
#ifdef MOZC_USE_SEPARATE_COLLOCATION_DATA
namespace CollocationData {
const char *kExistenceFilter_data = NULL;
const size_t kExistenceFilter_size = 0;
}  // namespace CollocationData
#else  // MOZC_USE_SEPARATE_COLLOCATION_DATA
// Include CollocationData::kExistenceFilter_data and
// CollocationData::kExistenceFilter_size.
#include "data_manager/oss/embedded_collocation_data.h"
#endif  // MOZC_USE_SEPARATE_COLLOCATION_DATA

char *g_collocation_data_address =
    const_cast<char *>(CollocationData::kExistenceFilter_data);
int g_collocation_data_size = CollocationData::kExistenceFilter_size;

// Include CollocationSuppressionData::kExistenceFilter_data and
// CollocationSuppressionData::kExistenceFilter_size.
#include "data_manager/oss/embedded_collocation_suppression_data.h"
}  // namespace

#ifdef MOZC_USE_SEPARATE_COLLOCATION_DATA
void OssDataManager::SetCollocationData(void *address, size_t size) {
  g_collocation_data_address = reinterpret_cast<char *>(address);
  g_collocation_data_size = size;
  DCHECK(g_collocation_data_address);
  DCHECK_GT(g_collocation_data_size, 0);
}
#endif  // MOZC_USE_SEPARATE_COLLOCATION_DATA

void OssDataManager::GetCollocationData(const char **array,
                                        size_t *size) const {
#ifdef MOZC_USE_SEPARATE_COLLOCATION_DATA
    if (!g_collocation_data_address || g_collocation_data_size == 0) {
      LOG(FATAL) << "Collocation data is not yet set.";
    }
#endif  // MOZC_USE_SEPARATE_COLLOCATION_DATA
  *array = g_collocation_data_address;
  *size = g_collocation_data_size;
}

void OssDataManager::GetCollocationSuppressionData(const char **array,
                                                   size_t *size) const {
  *array = CollocationSuppressionData::kExistenceFilter_data;
  *size = CollocationSuppressionData::kExistenceFilter_size;
}

namespace {
// Include kSuggestionFilterData_data and kSuggestionFilterData_size.
#include "data_manager/oss/suggestion_filter_data.h"
}  // namespace

void OssDataManager::GetSuggestionFilterData(const char **data,
                                             size_t *size) const {
  *data = kSuggestionFilterData_data;
  *size = kSuggestionFilterData_size;
}

namespace {
// Include kSymbolData_token_data and kSymbolData_token_size.
#include "data_manager/oss/symbol_rewriter_data.h"
}  // namespace

void OssDataManager::GetSymbolRewriterData(
    const EmbeddedDictionary::Token **data,
    size_t *size) const {
  *data = kSymbolData_token_data;
  *size = kSymbolData_token_size;
}

#ifndef NO_USAGE_REWRITER
namespace {
#include "rewriter/usage_rewriter_data.h"
}  // namespace

void OssDataManager::GetUsageRewriterData(
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

namespace {
#include "data_manager/oss/counter_suffix_data.h"
}  // namespace

void OssDataManager::GetCounterSuffixSortedArray(
    const CounterSuffixEntry **array, size_t *size) const {
  *array = kCounterSuffixes;
  *size = arraysize(kCounterSuffixes);
}

}  // namespace oss
}  // namespace mozc
