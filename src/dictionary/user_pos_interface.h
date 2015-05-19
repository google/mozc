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

#ifndef MOZC_DICTIONARY_USER_POS_INTERFACE_H_
#define MOZC_DICTIONARY_USER_POS_INTERFACE_H_

#include <string>
#include <vector>
#include "base/base.h"

namespace mozc {

// Interface that defines interface of the helper class used by
// POS. Default implementation is defined in the .cc file.
class UserPOSInterface {
 public:
  struct Token {
    string key;
    string value;
    uint16 id;
    int16  cost;
  };

  // Sets posssible list of part-of-speech Mozc can handle
  virtual void GetPOSList(vector<string> *pos_list) const = 0;

  // Returns true if the given string is one of the POSes Mozc can handle.
  virtual bool IsValidPOS(const string &pos) const = 0;

  // Returns iid from Mozc POS. If the pos has inflection, this method only
  // returns the ids of base form.
  virtual bool GetPOSIDs(const string &pos, uint16 *id) const = 0;

  // Converts the given tuple (key, value, pos) to Token.  If the pos has
  // inflection, this function expands possible inflections automatically.
  virtual bool GetTokens(const string &key,
                         const string &value,
                         const string &pos,
                         vector<Token> *tokens) const = 0;

 protected:
  UserPOSInterface() {}
  virtual ~UserPOSInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UserPOSInterface);
};

}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_POS_INTERFACE_H_
