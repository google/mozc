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

#ifndef MOZC_DATA_MANAGER_OSS_OSS_DATA_MANAGER_H_
#define MOZC_DATA_MANAGER_OSS_OSS_DATA_MANAGER_H_

#include "base/base.h"
#include "base/singleton.h"
#include "data_manager/oss/oss_user_pos_manager.h"

namespace mozc {

class ConnectorInterface;
class DictionaryInterface;
class DictionaryInterface;
class PosGroup;
class SegmenterInterface;

namespace oss {

class OssDataManager : public OssUserPosManager {
 public:
  OssDataManager() {}
  virtual ~OssDataManager() {}

  virtual const PosGroup *GetPosGroup() const;
  virtual const ConnectorInterface *GetConnector() const;
  virtual const SegmenterInterface *GetSegmenter() const;
  virtual DictionaryInterface *CreateSystemDictionary() const;
  virtual DictionaryInterface *CreateValueDictionary() const;
  virtual const DictionaryInterface *GetSuffixDictionary() const;
  virtual void GetReadingCorrectionData(const ReadingCorrectionItem **array,
                                        size_t *size) const;
  virtual void GetCollocationData(const char **array, size_t *size) const;
  virtual void GetCollocationSuppressionData(const char **array,
                                             size_t *size) const;
  virtual void GetSuggestionFilterData(const char **data, size_t *size) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(OssDataManager);
};

}  // namespace oss
}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_OSS_OSS_DATA_MANAGER_H_
