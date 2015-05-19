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

#ifndef MOZC_DATA_MANAGER_OSS_OSS_DATA_MANAGER_H_
#define MOZC_DATA_MANAGER_OSS_OSS_DATA_MANAGER_H_

#include "base/port.h"
#include "base/singleton.h"
#include "data_manager/oss/oss_user_pos_manager.h"

namespace mozc {
namespace oss {

class OssDataManager : public OssUserPosManager {
 public:
  OssDataManager() {}
  virtual ~OssDataManager() {}

#ifdef MOZC_USE_SEPARATE_COLLOCATION_DATA
  static void SetCollocationData(void *address, size_t size);
#endif  // MOZC_USE_SEPARATE_COLLOCATION_DATA
#ifdef MOZC_USE_SEPARATE_CONNECTION_DATA
  static void SetConnectionData(void *address, size_t size);
#endif  // MOZC_USE_SEPARATE_CONNECTION_DATA
#ifdef MOZC_USE_SEPARATE_DICTIONARY
  static void SetDictionaryData(void *address, size_t size);
#endif  // MOZC_USE_SEPARATE_DICTIONARY

  virtual const uint8 *GetPosGroupData() const;
  virtual void GetConnectorData(const char **data, size_t *size) const;
  virtual void GetSegmenterData(
      size_t *l_num_elements, size_t *r_num_elements,
      const uint16 **l_table, const uint16 **r_table,
      size_t *bitarray_num_bytes, const char **bitarray_data,
      const BoundaryData **boundary_data) const;
  virtual void GetSystemDictionaryData(const char **data, int *size) const;
  virtual void GetSuffixDictionaryData(const SuffixToken **tokens,
                                       size_t *size) const;
  virtual void GetReadingCorrectionData(const ReadingCorrectionItem **array,
                                        size_t *size) const;
  virtual void GetCollocationData(const char **array, size_t *size) const;
  virtual void GetCollocationSuppressionData(const char **array,
                                             size_t *size) const;
  virtual void GetSuggestionFilterData(const char **data, size_t *size) const;
  virtual void GetSymbolRewriterData(const EmbeddedDictionary::Token **data,
                                     size_t *size) const;
#ifndef NO_USAGE_REWRITER
  virtual void GetUsageRewriterData(
      const ConjugationSuffix **base_conjugation_suffix,
      const ConjugationSuffix **conjugation_suffix_data,
      const int **conjugation_suffix_data_index,
      const UsageDictItem **usage_data_value) const;
#endif  // NO_USAGE_REWRITER
  virtual void GetCounterSuffixSortedArray(const CounterSuffixEntry **array,
                                           size_t *size) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(OssDataManager);
};

}  // namespace oss
}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_OSS_OSS_DATA_MANAGER_H_
