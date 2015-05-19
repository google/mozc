// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_DICTIONARY_USER_POS_H_
#define MOZC_DICTIONARY_USER_POS_H_

#include <map>
#include <string>
#include <vector>
#include "base/base.h"
#include "dictionary/user_pos_interface.h"

namespace mozc {

class UserPOS : public UserPOSInterface {
 public:
  struct ConjugationType {
    const char *key_suffix;
    const char *value_suffix;
    uint16 id;
  };

  struct POSToken {
    const char *pos;
    uint16 conjugation_size;
    const ConjugationType *conjugation_form;
  };

  // Initializes the user pos from the given POSToken array. The class doesn't
  // take the ownership of the array. The caller is responsible for deleting it.
  explicit UserPOS(const POSToken *pos_token_array);
  virtual ~UserPOS() {}

  // Implementation of UserPOSInterface.
  virtual void GetPOSList(vector<string> *pos_list) const;
  virtual bool IsValidPOS(const string &pos) const;
  virtual bool GetPOSIDs(const string &pos, uint16 *id) const;
  virtual bool GetTokens(const string &key, const string &value,
                         const string &pos, vector<Token> *tokens) const;

 private:
  const POSToken *pos_token_array_;
  map<string, const POSToken*> pos_map_;

  DISALLOW_COPY_AND_ASSIGN(UserPOS);
};

}  // mozc

#endif  // MOZC_DICTIONARY_USER_POS_H_
