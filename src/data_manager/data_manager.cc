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

#include "data_manager/data_manager.h"

#include "base/logging.h"
#include "data_manager/dataset_reader.h"

namespace mozc {

DataManager::DataManager() = default;
DataManager::~DataManager() = default;

bool DataManager::InitFromArray(StringPiece array, StringPiece magic) {
  DataSetReader reader;
  if (!reader.Init(array, magic)) {
    LOG(ERROR) << "Binary data of size " << array.size() << " is broken";
    return false;
  }
  if (!reader.Get("conn", &connection_data_)) {
    LOG(ERROR) << "Cannot find a connection data";
    return false;
  }
  if (!reader.Get("dict", &dictionary_data_)) {
    LOG(ERROR) << "Cannot find a dictionary data";
    return false;
  }
  if (!reader.Get("sugg", &suggestion_filter_data_)) {
    LOG(ERROR) << "Cannot find a suggestion filter data";
    return false;
  }
  if (!reader.Get("coll", &collocation_data_)) {
    LOG(ERROR) << "Cannot find a collocation data";
    return false;
  }
  if (!reader.Get("cols", &collocation_suppression_data_)) {
    LOG(ERROR) << "Cannot find a collocation suprression";
    return false;
  }
  if (!reader.Get("posg", &pos_group_data_)) {
    LOG(ERROR) << "Cannot find a POS group data";
    return false;
  }
  return true;
}

void DataManager::GetConnectorData(const char **data, size_t *size) const {
  *data = connection_data_.data();
  *size = connection_data_.size();
}

void DataManager::GetSystemDictionaryData(const char **data, int *size) const {
  *data = dictionary_data_.data();
  *size = dictionary_data_.size();
}

void DataManager::GetCollocationData(const char **array, size_t *size) const {
  *array = collocation_data_.data();
  *size = collocation_data_.size();
}

void DataManager::GetCollocationSuppressionData(const char **array,
                                                size_t *size) const {
  *array = collocation_suppression_data_.data();
  *size = collocation_suppression_data_.size();
}

void DataManager::GetSuggestionFilterData(const char **data,
                                          size_t *size) const {
  *data = suggestion_filter_data_.data();
  *size = suggestion_filter_data_.size();
}

const dictionary::UserPOS::POSToken *DataManager::GetUserPOSData() const {
  LOG(FATAL) << "Not implemented";
  return nullptr;
}

const dictionary::POSMatcher *DataManager::GetPOSMatcher() const {
  LOG(FATAL) << "Not implemented";
  return nullptr;
}

const uint8 *DataManager::GetPosGroupData() const {
  return reinterpret_cast<const uint8 *>(pos_group_data_.data());
}

void DataManager::GetSegmenterData(
    size_t *l_num_elements, size_t *r_num_elements, const uint16 **l_table,
    const uint16 **r_table, size_t *bitarray_num_bytes,
    const char **bitarray_data, const BoundaryData **boundary_data) const {
  LOG(FATAL) << "Not implemented";
}

void DataManager::GetSuffixDictionaryData(const dictionary::SuffixToken **data,
                                          size_t *size) const {
  LOG(FATAL) << "Not implemented";
}

void DataManager::GetReadingCorrectionData(const ReadingCorrectionItem **array,
                                           size_t *size) const {
  LOG(FATAL) << "Not implemented";
}

void DataManager::GetSymbolRewriterData(const EmbeddedDictionary::Token **data,
                                        size_t *size) const {
  LOG(FATAL) << "Not implemented";
}

void DataManager::GetCounterSuffixSortedArray(const CounterSuffixEntry **array,
                                              size_t *size) const {
  LOG(FATAL) << "Not implemented";
}

#ifndef NO_USAGE_REWRITER
void DataManager::GetUsageRewriterData(
    const ConjugationSuffix **base_conjugation_suffix,
    const ConjugationSuffix **conjugation_suffix_data,
    const int **conjugation_suffix_data_index,
    const UsageDictItem **usage_data_value) const {
  LOG(FATAL) << "Not implemented";
}
#endif  // NO_USAGE_REWRITER

}  // namespace mozc
