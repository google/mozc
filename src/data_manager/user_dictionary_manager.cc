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

#include "data_manager/user_dictionary_manager.h"

#include "base/base.h"
#include "base/init.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_dictionary.h"
#include "dictionary/user_pos.h"

namespace mozc {
namespace {

// TODO(noriyukit): It's clearer to reload the user dictionary not by
// REGISTER_MODULE_RELOADER().
void ReloadUserDictionary() {
  VLOG(1) << "Reloading user dictionary";
  UserDictionaryManager *manager =
      UserDictionaryManager::GetUserDictionaryManager();
  manager->GetUserDictionary()->Reload();
}

// ReloadUserDictionary() is called by SessionHandler::Reload()
REGISTER_MODULE_RELOADER(reload_user_dictionary,
                         ReloadUserDictionary());

}  // namespace

UserDictionaryManager *UserDictionaryManager::GetUserDictionaryManager() {
  return Singleton<UserDictionaryManager>::get();
}

UserDictionaryManager::UserDictionaryManager() : user_dictionary_(NULL) {}

UserDictionaryManager::~UserDictionaryManager() {
  delete user_dictionary_;
  user_dictionary_ = NULL;
}

UserDictionary *UserDictionaryManager::GetUserDictionary() {
#ifdef __native_client__
  return NULL;
#else  // __native_client__
  if (user_dictionary_ == NULL) {
    scoped_lock l(&mutex_);
    if (user_dictionary_ == NULL) {
      // TODO(noriyukit): POSMatcher depends on embedded data set and should be
      // factoried by DataManager.
      // TODO(noriyukit): SuppressionDictionary should be managed by engine
      // class. This is a part of refactoring and will be fixed in future.
      user_dictionary_ = new UserDictionary(
          GetUserPOS(),
          GetPOSMatcher(),
          Singleton<SuppressionDictionary>::get());
    }
  }
  DCHECK(user_dictionary_);
  return user_dictionary_;
#endif  // __native_client__
}

}  // namespace mozc
