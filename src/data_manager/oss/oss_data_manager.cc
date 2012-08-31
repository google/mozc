// Copyright 2010-2012, Google Inc.
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

#include "base/base.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "converter/boundary_struct.h"
#include "converter/connector_base.h"
#include "converter/segmenter_base.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "rewriter/correction_rewriter.h"

namespace mozc {
namespace oss {

namespace {
// kLidGroup[] is defined in the following automatically generated header file.
#include "data_manager/oss/pos_group_data.h"

class OssPosGroup : public PosGroup {
 public:
  OssPosGroup() : PosGroup(kLidGroup) {}
};
}  // namespace

const PosGroup *OssDataManager::GetPosGroup() const {
  return Singleton<OssPosGroup>::get();
}

namespace {
// Automatically generated header containing the definitions of
// kConnectionData_data and kConnectionData_size.
#include "data_manager/oss/embedded_connection_data.h"

// Use Thread Local Storage for Cache of the Connector.
const int kCacheSize = 1024;
TLS_KEYWORD bool g_cache_initialized = false;
TLS_KEYWORD int g_cache_key[kCacheSize];
TLS_KEYWORD int g_cache_value[kCacheSize];

class OssConnector : public ConnectorBase {
 private:
  OssConnector() : ConnectorBase(kConnectionData_data,
                                 kConnectionData_size,
                                 &g_cache_initialized,
                                 g_cache_key,
                                 g_cache_value,
                                 kCacheSize) {}
  virtual ~OssConnector() {}
  friend class Singleton<OssConnector>;
};
}  // namespace

const ConnectorInterface *OssDataManager::GetConnector() const {
  return Singleton<OssConnector>::get();
}

namespace {
// Automatically generated header containing the definitions of
// kDictionaryData_data[] and kDictionaryData_size.
#include "data_manager/oss/embedded_dictionary_data.h"
}  // namespace

DictionaryInterface *OssDataManager::CreateSystemDictionary() const {
  return mozc::dictionary::SystemDictionary::CreateSystemDictionaryFromImage(
      kDictionaryData_data, kDictionaryData_size);
}

DictionaryInterface *OssDataManager::CreateValueDictionary() const {
  return mozc::dictionary::ValueDictionary::CreateValueDictionaryFromImage(
      *GetPOSMatcher(), kDictionaryData_data, kDictionaryData_size);
}

namespace {
// Automatically generated headers containing data set for segmenter.
#include "data_manager/oss/boundary_data.h"
#include "data_manager/oss/segmenter_data.h"

class OssSegmenter : public SegmenterBase {
 private:
  OssSegmenter() : SegmenterBase(kCompressedLSize, kCompressedRSize,
                                 kCompressedLIDTable, kCompressedRIDTable,
                                 kSegmenterBitArrayData_size,
                                 kSegmenterBitArrayData_data,
                                 kBoundaryData) {}
  virtual ~OssSegmenter() {}
  friend class Singleton<OssSegmenter>;
};
}  // namespace

const SegmenterInterface *OssDataManager::GetSegmenter() const {
  return Singleton<OssSegmenter>::get();
}

namespace {
// The generated header defines kSuffixTokens[].
#include "data_manager/oss/suffix_data.h"

class OssSuffixDictionary : public SuffixDictionary {
 public:
  OssSuffixDictionary() : SuffixDictionary(kSuffixTokens,
                                           arraysize(kSuffixTokens)) {}
  virtual ~OssSuffixDictionary() {}
};
}  // namespace

const DictionaryInterface *OssDataManager::GetSuffixDictionary() const {
  return Singleton<OssSuffixDictionary>::get();
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
// Include CollocationData::kExistenceFilter_data and
// CollocationData::kExistenceFilter_size.
#include "data_manager/oss/embedded_collocation_data.h"
// Include CollocationSuppressionData::kExistenceFilter_data and
// CollocationSuppressionData::kExistenceFilter_size.
#include "data_manager/oss/embedded_collocation_suppression_data.h"
}  // namespace

void OssDataManager::GetCollocationData(const char **array,
                                        size_t *size) const {
  *array = CollocationData::kExistenceFilter_data;
  *size = CollocationData::kExistenceFilter_size;
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

}  // namespace oss
}  // namespace mozc
