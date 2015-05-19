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

#include "data_manager/testing/mock_data_manager.h"

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
namespace testing {

namespace {
// kLidGroup[] is defined in the following automatically generated header file.
#include "data_manager/testing/pos_group_data.h"

class MockPosGroup : public PosGroup {
 public:
  MockPosGroup() : PosGroup(kLidGroup) {}
};
}  // namespace

const PosGroup *MockDataManager::GetPosGroup() const {
  return Singleton<MockPosGroup>::get();
}

namespace {
// Automatically generated header containing the definitions of
// kConnectionData_data and kConnectionData_size. We don't embed it when
// connection data is supplied from outside.
#include "data_manager/testing/embedded_connection_data.h"

#ifdef OS_ANDROID
const int kCacheSize = 256;
#else
const int kCacheSize = 1024;
#endif

// Use Thread Local Storage for Cache of the Connector.
TLS_KEYWORD bool g_cache_initialized = false;
TLS_KEYWORD int g_cache_key[kCacheSize];
TLS_KEYWORD int g_cache_value[kCacheSize];

class MockConnector : public ConnectorBase {
 private:
  MockConnector() : ConnectorBase(kConnectionData_data,
                                  kConnectionData_size,
                                  &g_cache_initialized,
                                  g_cache_key,
                                  g_cache_value,
                                  kCacheSize) {}
  virtual ~MockConnector() {}
  friend class Singleton<MockConnector>;
};
}  // namespace

const ConnectorInterface *MockDataManager::GetConnector() const {
  return Singleton<MockConnector>::get();
}

namespace {
// Automatically generated header containing the definitions of
// kDictionaryData_data[] and kDictionaryData_size.
#include "data_manager/testing/embedded_dictionary_data.h"
}  // namespace

DictionaryInterface *MockDataManager::CreateSystemDictionary() const {
  return mozc::dictionary::SystemDictionary::CreateSystemDictionaryFromImage(
      kDictionaryData_data, kDictionaryData_size);
}

DictionaryInterface *MockDataManager::CreateValueDictionary() const {
  return mozc::dictionary::ValueDictionary::CreateValueDictionaryFromImage(
      *GetPOSMatcher(), kDictionaryData_data, kDictionaryData_size);
}

namespace {
// Automatically generated headers containing data set for segmenter.
#include "data_manager/testing/boundary_data.h"
#include "data_manager/testing/segmenter_data.h"

class MockSegmenter : public SegmenterBase {
 private:
  MockSegmenter() : SegmenterBase(kCompressedLSize, kCompressedRSize,
                                  kCompressedLIDTable, kCompressedRIDTable,
                                  kSegmenterBitArrayData_size,
                                  kSegmenterBitArrayData_data,
                                  kBoundaryData) {}
  virtual ~MockSegmenter() {}
  friend class Singleton<MockSegmenter>;
};
}  // namespace

const SegmenterInterface *MockDataManager::GetSegmenter() const {
  return Singleton<MockSegmenter>::get();
}

namespace {
// The generated header defines kSuffixTokens[].
#include "data_manager/testing/suffix_data.h"

class MockSuffixDictionary : public SuffixDictionary {
 public:
  MockSuffixDictionary() : SuffixDictionary(kSuffixTokens,
                                            arraysize(kSuffixTokens)) {}
  virtual ~MockSuffixDictionary() {}
};
}  // namespace

const DictionaryInterface *MockDataManager::GetSuffixDictionary() const {
  return Singleton<MockSuffixDictionary>::get();
}

namespace {
// Include kReadingCorrections.
#include "data_manager/testing/reading_correction_data.h"
}  // namespace

void MockDataManager::GetReadingCorrectionData(
    const ReadingCorrectionItem **array,
    size_t *size) const {
  *array = kReadingCorrections;
  *size = arraysize(kReadingCorrections);
}

namespace {
// Include CollocationData::kExistenceFilter_data and
// CollocationData::kExistenceFilter_size.
#include "data_manager/testing/embedded_collocation_data.h"
// Include CollocationSuppressionData::kExistenceFilter_data and
// CollocationSuppressionData::kExistenceFilter_size.
#include "data_manager/testing/embedded_collocation_suppression_data.h"
}  // namespace

void MockDataManager::GetCollocationData(const char **array,
                                         size_t *size) const {
  *array = CollocationData::kExistenceFilter_data;
  *size = CollocationData::kExistenceFilter_size;
}

void MockDataManager::GetCollocationSuppressionData(const char **array,
                                                    size_t *size) const {
  *array = CollocationSuppressionData::kExistenceFilter_data;
  *size = CollocationSuppressionData::kExistenceFilter_size;
}

namespace {
// Include kSuggestionFilterData_data and kSuggestionFilterData_size.
#include "data_manager/testing/suggestion_filter_data.h"
}  // namespace

void MockDataManager::GetSuggestionFilterData(const char **data,
                                              size_t *size) const {
  *data = kSuggestionFilterData_data;
  *size = kSuggestionFilterData_size;
}

}  // namespace testing
}  // namespace mozc
