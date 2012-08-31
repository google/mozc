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

#include "data_manager/android/android_data_manager.h"

#include "base/base.h"
#include "base/logging.h"
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
namespace android {

namespace {
// kLidGroup[] is defined in the following automatically generated header file.
#include "data_manager/android/pos_group_data.h"

class AndroidPosGroup : public PosGroup {
 public:
  AndroidPosGroup() : PosGroup(kLidGroup) {}
};
}  // namespace

const PosGroup *AndroidDataManager::GetPosGroup() const {
  return Singleton<AndroidPosGroup>::get();
}

namespace {
// Automatically generated header containing the definitions of
// kConnectionData_data and kConnectionData_size. We don't embed it when
// connection data is supplied from outside.
#ifndef MOZC_USE_SEPARATE_CONNECTION_DATA
#include "data_manager/android/embedded_connection_data.h"
#else
const char *kConnectionData_data = NULL;
const size_t kConnectionData_size = 0;
#endif  // MOZC_USE_SEPARATE_CONNECTION_DATA

char* g_connection_data_address = const_cast<char*>(kConnectionData_data);
int g_connection_data_size = kConnectionData_size;

// Use Thread Local Storage for Cache of the Connector.
const int kCacheSize = 256;
TLS_KEYWORD bool g_cache_initialized = false;
TLS_KEYWORD int g_cache_key[kCacheSize];
TLS_KEYWORD int g_cache_value[kCacheSize];

class AndroidConnector : public ConnectorBase {
 private:
  AndroidConnector() : ConnectorBase(g_connection_data_address,
                                     g_connection_data_size,
                                     &g_cache_initialized,
                                     g_cache_key,
                                     g_cache_value,
                                     kCacheSize) {
#ifdef MOZC_USE_SEPARATE_CONNECTION_DATA
    if (!g_connection_data_address || g_connection_data_size == 0) {
      LOG(FATAL) << "Connection data is not yet set.";
      CHECK(false);
    }
#endif
  }

  virtual ~AndroidConnector() {}
  friend class Singleton<AndroidConnector>;
};
}  // namespace

void AndroidDataManager::SetConnectionData(void *address, size_t size) {
  g_connection_data_address = reinterpret_cast<char*>(address);
  g_connection_data_size = size;
  DCHECK(g_connection_data_address);
  DCHECK_GT(g_connection_data_size, 0);
}

const ConnectorInterface *AndroidDataManager::GetConnector() const {
  return Singleton<AndroidConnector>::get();
}

namespace {
#ifdef MOZC_USE_SEPARATE_DICTIONARY
const char *kDictionaryData_data = NULL;
const size_t kDictionaryData_size = 0;
#else
// Automatically generated header containing the definitions of
// kDictionaryData_data[] and kDictionaryData_size.
#include "data_manager/android/embedded_dictionary_data.h"
#endif  // MOZC_USE_SEPARATE_DICTIONARY

char* g_dictionary_address = const_cast<char*>(kDictionaryData_data);
int g_dictionary_size = kDictionaryData_size;
}  // namespace

DictionaryInterface *AndroidDataManager::CreateSystemDictionary() const {
  return mozc::dictionary::SystemDictionary::CreateSystemDictionaryFromImage(
      g_dictionary_address, g_dictionary_size);
}

DictionaryInterface *AndroidDataManager::CreateValueDictionary() const {
  return mozc::dictionary::ValueDictionary::CreateValueDictionaryFromImage(
      *GetPOSMatcher(), g_dictionary_address, g_dictionary_size);
}

void AndroidDataManager::SetDictionaryData(void *address, size_t size) {
  g_dictionary_address = reinterpret_cast<char*>(address);
  g_dictionary_size = size;
  DCHECK(g_dictionary_address);
  DCHECK_GT(g_dictionary_size, 0);
}

namespace {
// Automatically generated headers containing data set for segmenter.
#include "data_manager/android/boundary_data.h"
#include "data_manager/android/segmenter_data.h"

class AndroidSegmenter : public SegmenterBase {
 private:
  AndroidSegmenter() : SegmenterBase(kCompressedLSize, kCompressedRSize,
                                     kCompressedLIDTable, kCompressedRIDTable,
                                     kSegmenterBitArrayData_size,
                                     kSegmenterBitArrayData_data,
                                     kBoundaryData) {}
  virtual ~AndroidSegmenter() {}
  friend class Singleton<AndroidSegmenter>;
};
}  // namespace

const SegmenterInterface *AndroidDataManager::GetSegmenter() const {
  return Singleton<AndroidSegmenter>::get();
}

namespace {
// The generated header defines kSuffixTokens[].
#include "data_manager/android/suffix_data.h"

class AndroidSuffixDictionary : public SuffixDictionary {
 public:
  AndroidSuffixDictionary() : SuffixDictionary(kSuffixTokens,
                                               arraysize(kSuffixTokens)) {}
  virtual ~AndroidSuffixDictionary() {}
};
}  // namespace

const DictionaryInterface *AndroidDataManager::GetSuffixDictionary() const {
  return Singleton<AndroidSuffixDictionary>::get();
}

namespace {
// Include kReadingCorrections.
#include "data_manager/android/reading_correction_data.h"
}  // namespace

void AndroidDataManager::GetReadingCorrectionData(
    const ReadingCorrectionItem **array,
    size_t *size) const {
  *array = kReadingCorrections;
  *size = arraysize(kReadingCorrections);
}

namespace {
// Include CollocationData::kExistenceData and
// CollocationData::kExistenceDataSize.
#include "data_manager/android/embedded_collocation_data.h"
// Include CollocationSuppressionData::kExistenceFilter_data and
// CollocationSuppressionData::kExistenceFilter_size.
#include "data_manager/android/embedded_collocation_suppression_data.h"
}  // namespace

void AndroidDataManager::GetCollocationData(const char **array,
                                            size_t *size) const {
  *array = CollocationData::kExistenceFilter_data;
  *size = CollocationData::kExistenceFilter_size;
}

void AndroidDataManager::GetCollocationSuppressionData(const char **array,
                                                       size_t *size) const {
  *array = CollocationSuppressionData::kExistenceFilter_data;
  *size = CollocationSuppressionData::kExistenceFilter_size;
}

namespace {
// Include kSuggestionFilterData_data and kSuggestionFilterData_size.
#include "data_manager/android/suggestion_filter_data.h"
}  // namespace

void AndroidDataManager::GetSuggestionFilterData(const char **data,
                                                 size_t *size) const {
  *data = kSuggestionFilterData_data;
  *size = kSuggestionFilterData_size;
}

}  // namespace android
}  // namespace mozc
