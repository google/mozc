// Copyright 2010-2011, Google Inc.
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

#include <string>
#include <vector>
#include "base/base.h"

namespace mozc {
class UserPOS {
 public:
  // return posssible list of part-of-speech Mozc can handle
  static void GetPOSList(vector<string> *pos_list);

  // Return true if the given string is one of the POSes Mozc can handle.
  static bool IsValidPOS(const string &pos);

  // Return iid from Mozc POS.
  // If pos has inflection, this method only returns
  // the ids of base form
  static bool GetPOSIDs(const string &pos, uint16 *id);

  struct Token {
    string key;
    string value;
    uint16 id;
    int16  cost;
  };

  // CostType:
  // We don't allow user to set cost directly.
  enum CostType {
    DEFAULT,   // default cost.
    STRONG,    // cost is almost. 0
    WEAK,      // weak, close to 10000
    NEVER,     // this word is never shown
  };

  // Convert from key/value/pos pair to Token.
  // If pos has inflection, this function expands possible
  // inflections automatically.
  static bool GetTokens(const string &key,
                        const string &value,
                        const string &pos,
                        CostType cost_type,
                        vector<Token> *tokens);

  // This function is provided for test code to use mock version of
  // POS. There is an underlying helper class inside POS and behaivior
  // of POS can be customized by replacing the instance of the helper
  // class with another one.
  class UserPOSInterface;
  static void SetUserPOSInterface(const UserPOSInterface *impl);

  // Interface that defines interface of the helper class used by
  // POS. Default implementation is defined in the .cc file.
  class UserPOSInterface {
   public:
    virtual ~UserPOSInterface() {}
    virtual void GetPOSList(vector<string> *pos_list) const = 0;
    virtual bool IsValidPOS(const string &pos) const = 0;
    virtual bool GetPOSIDs(const string &pos, uint16 *id) const = 0;
    virtual bool GetTokens(const string &key,
                           const string &value,
                           const string &pos,
                           CostType cost_type,
                           vector<Token> *tokens) const = 0;
  };

 private:
  UserPOS() {}
  virtual ~UserPOS() {}
};
}  // mozc
#endif  // MOZC_DICTIONARY_USER_POS_H_
