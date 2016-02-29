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
#include "dictionary/pos_matcher.h"

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
  return manager_.GetPosGroupData();
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

void OssDataManager::GetSegmenterData(
    size_t *l_num_elements, size_t *r_num_elements,
    const uint16 **l_table, const uint16 **r_table,
    size_t *bitarray_num_bytes, const char **bitarray_data,
    const uint16 **boundary_data) const {
  manager_.GetSegmenterData(l_num_elements, r_num_elements,
                            l_table, r_table, bitarray_num_bytes,
                            bitarray_data, boundary_data);
}

void OssDataManager::GetSuffixDictionaryData(StringPiece *key_array,
                                             StringPiece *value_array,
                                             const uint32 **token_array) const {
  manager_.GetSuffixDictionaryData(key_array, value_array, token_array);
}

void OssDataManager::GetReadingCorrectionData(
    StringPiece *value_array_data, StringPiece *error_array_data,
    StringPiece *correction_array_data) const {
  manager_.GetReadingCorrectionData(value_array_data, error_array_data,
                                    correction_array_data);
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

void OssDataManager::GetSymbolRewriterData(
    StringPiece *token_array_data, StringPiece *string_array_data) const {
  manager_.GetSymbolRewriterData(token_array_data, string_array_data);
}

#ifndef NO_USAGE_REWRITER
void OssDataManager::GetUsageRewriterData(
    StringPiece *base_conjugation_suffix_data,
    StringPiece *conjugation_suffix_data,
    StringPiece *conjugation_suffix_index_data,
    StringPiece *usage_items_data,
    StringPiece *string_array_data) const {
  manager_.GetUsageRewriterData(base_conjugation_suffix_data,
                                conjugation_suffix_data,
                                conjugation_suffix_index_data,
                                usage_items_data,
                                string_array_data);
}
#endif  // NO_USAGE_REWRITER

void OssDataManager::GetCounterSuffixSortedArray(
    const char **array, size_t *size) const {
  manager_.GetCounterSuffixSortedArray(array, size);
}

}  // namespace oss
}  // namespace mozc
