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

#include "data_manager/oss/oss_data_manager.h"

#include "base/embedded_file.h"
#include "base/logging.h"
#include "base/port.h"
#include "converter/boundary_struct.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary_token.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/counter_suffix.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter_data_structs.h"
#endif  // NO_USAGE_REWRITER

using mozc::dictionary::SuffixToken;

namespace mozc {
namespace oss {
namespace {

const char *g_mozc_data_address = nullptr;
size_t g_mozc_data_size = 0;

#ifdef MOZC_USE_SEPARATE_DATASET
const EmbeddedFile kOssMozcDataSet = {nullptr, 0};
#else
// kOssMozcDataSet is embedded.
#include "data_manager/oss/mozc_imy.h"
#endif  // MOZC_USE_SEPARATE_DATASET

#ifndef MOZC_DATASET_MAGIC_NUMBER
#error "MOZC_DATASET_MAGIC_NUMBER is not defined by build system"
#endif  // MOZC_DATASET_MAGIC_NUMBER

const char kMagicNumber[] = MOZC_DATASET_MAGIC_NUMBER;

// kLidGroup[] is defined in the following automatically generated header file.
#include "data_manager/oss/pos_group_data.h"

}  // namespace

OssDataManager::OssDataManager() {
  const StringPiece magic(kMagicNumber, arraysize(kMagicNumber) - 1);
  if (g_mozc_data_address != nullptr) {
    const StringPiece data(g_mozc_data_address, g_mozc_data_size);
    CHECK(manager_.InitFromArray(data, magic))
        << "Image set by SetMozcDataSet() is broken";
    return;
  }
#ifdef MOZC_USE_SEPARATE_DATASET
  LOG(FATAL)
      << "When MOZC_USE_SEPARATE_DATASET build flag is defined, "
      << "OssDataManager::SetMozcDataSet() must be called before "
      << "instantiation of OssDataManager instances.";
#endif  // MOZC_USE_SEPARATE_DATASET
  CHECK(manager_.InitFromArray(LoadEmbeddedFile(kOssMozcDataSet), magic))
      << "Embedded mozc_imy.h for OSS is broken";
}

OssDataManager::~OssDataManager() = default;

const uint8 *OssDataManager::GetPosGroupData() const {
  DCHECK(kLidGroup != NULL);
  return kLidGroup;
}

// Both pointers can be nullptr when the DataManager is reset on testing.
void OssDataManager::SetMozcDataSet(void *address, size_t size) {
  g_mozc_data_address = reinterpret_cast<char *>(address);
  g_mozc_data_size = size;
}

void OssDataManager::GetConnectorData(const char **data, size_t *size) const {
  manager_.GetConnectorData(data, size);
}

void OssDataManager::GetSystemDictionaryData(
    const char **data, int *size) const {
  manager_.GetSystemDictionaryData(data, size);
}

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

void OssDataManager::GetCollocationData(const char **array,
                                        size_t *size) const {
  manager_.GetCollocationData(array, size);
}

void OssDataManager::GetCollocationSuppressionData(const char **array,
                                                   size_t *size) const {
  manager_.GetCollocationSuppressionData(array, size);
}

void OssDataManager::GetSuggestionFilterData(const char **data,
                                             size_t *size) const {
  manager_.GetSuggestionFilterData(data, size);
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
