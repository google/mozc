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

#include "dictionary/dictionary_impl.h"

#include "base/singleton.h"
#include "data_manager/user_dictionary_manager.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary.h"

namespace mozc {
namespace {

#ifdef MOZC_USE_SEPARATE_DICTIONARY
const char *kDictionaryData_data = NULL;
const size_t kDictionaryData_size = 0;
#else
#include "dictionary/embedded_dictionary_data.h"
#endif  // MOZC_USE_SEPARATE_DICTIONARY

DictionaryInterface *g_dictionary = NULL;

char* g_dictionary_address = const_cast<char*>(kDictionaryData_data);
int g_dictionary_size = kDictionaryData_size;

class MozcDictionaryImpl : public DictionaryImpl {
 private:
  MozcDictionaryImpl() : DictionaryImpl(
      SystemDictionary::CreateSystemDictionaryFromImage(g_dictionary_address,
                                                        g_dictionary_size),
      ValueDictionary::CreateValueDictionaryFromImage(
          *UserDictionaryManager::GetUserDictionaryManager()->GetPOSMatcher(),
          g_dictionary_address,
          g_dictionary_size),
      UserDictionaryManager::GetUserDictionaryManager()->GetUserDictionary(),
      Singleton<SuppressionDictionary>::get(),
      UserDictionaryManager::GetUserDictionaryManager()->GetPOSMatcher()) {}
  virtual ~MozcDictionaryImpl() {}

  friend class Singleton<MozcDictionaryImpl>;
};
}  // namespace

DictionaryInterface *DictionaryFactory::GetDictionary() {
  if (g_dictionary == NULL) {
    if (!g_dictionary_address || g_dictionary_size == 0) {
      LOG(FATAL) << "Dictionary address/size is/are not set yet.";
      CHECK(false);
    }
    return Singleton<MozcDictionaryImpl>::get();
  } else {
    return g_dictionary;
  }
}

void DictionaryFactory::SetDictionary(DictionaryInterface *dictionary) {
  g_dictionary = dictionary;
}

void DictionaryFactory::SetDictionaryData(void *address, size_t size) {
  g_dictionary_address = reinterpret_cast<char*>(address);
  g_dictionary_size = size;
}

}  // namespace mozc
