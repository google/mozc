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

#ifndef IME_MOZC_DATA_MANAGER_ANDROID_ANDROID_USER_POS_MANAGER_H_
#define IME_MOZC_DATA_MANAGER_ANDROID_ANDROID_USER_POS_MANAGER_H_

#include "base/base.h"
#include "data_manager/data_manager_interface.h"

namespace mozc {

class POSMatcher;
class PosGroup;

namespace android {

class AndroidUserPosManager : public DataManagerInterface {
 public:
  AndroidUserPosManager() {}
  virtual ~AndroidUserPosManager() {}

  static AndroidUserPosManager *GetUserPosManager();

  // Partially implement the interface because some binary only reqiures the
  // folloiwng embedded data.
  virtual const UserPOSInterface *GetUserPOS() const;
  virtual const POSMatcher *GetPOSMatcher() const;

  // The following are implemented in AndroidDataManager.
  virtual const PosGroup *GetPosGroup() const { return NULL; }
  virtual const ConnectorInterface *GetConnector() const { return NULL; }
  virtual const SegmenterInterface *GetSegmenter() const { return NULL; }
  virtual DictionaryInterface *CreateSystemDictionary() const { return NULL; }
  virtual DictionaryInterface *CreateValueDictionary() const { return NULL; }
  virtual const DictionaryInterface *GetSuffixDictionary() const {
    return NULL;
  }
  virtual void GetReadingCorrectionData(const ReadingCorrectionItem **array,
                                        size_t *size) const {}
  virtual void GetCollocationData(const char **array, size_t *size) const {}
  virtual void GetCollocationSuppressionData(const char **array,
                                             size_t *size) const {}
  virtual void GetSuggestionFilterData(const char **data, size_t *size) const {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidUserPosManager);
};

}  // namespace android
}  // namespace mozc

#endif  // IME_MOZC_DATA_MANAGER_ANDROID_ANDROID_USER_POS_MANAGER_H_
