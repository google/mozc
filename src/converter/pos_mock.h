// Copyright 2010, Google Inc.
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

#ifndef MOZC_CONVERTER_POS_MOCK_H_
#define MOZC_CONVERTER_POS_MOCK_H_

#include "converter/pos.h"

namespace mozc {

// This class is a mock class for writing unit tests of a class that
// depends on POS. It accepts only two values for part-of-speech:
// "noun" as words without inflection and "verb" as words with
// inflection.
class POSMockHandler : public POS::POSHandlerInterface {
 public:
  POSMockHandler() {}
  virtual ~POSMockHandler() {}

  // This method returns true if the given pos is "noun" or "verb".
  virtual bool IsValidPOS(const string &pos) const;

  // Given a verb, this method expands it to three different forms,
  // i.e. base form (the word itself), "-ed" form and "-ing" form. For
  // example, if the given word is "play", the method returns "play",
  // "played" and "playing". When a noun is passed, it returns only
  // base form. The method set lid and rid of the word as following:
  //
  //  POS              | lid | rid
  // ------------------+-----+-----
  //  noun             | 100 | 100
  //  verb (base form) | 200 | 200
  //  verb (-ed form)  | 210 | 210
  //  verb (-ing form) | 220 | 220
  virtual bool GetTokens(const string &key,
                         const string &value,
                         const string &pos,
                         POS::CostType cost_type,
                         vector<POS::Token> *tokens) const;

  // These functions are currently not used in test code. They do nothing.
  virtual uint16 number_id() const { return 0; }
  virtual bool IsNumber(uint16 id) const { return false; }
  virtual bool IsZipcode(uint16 id) const { return false; }
  virtual bool IsFunctional(uint16 id) const { return false; }
  virtual uint16 unknown_id() const { return 0; }
  virtual uint16 first_name_id() const { return 0; }
  virtual uint16 last_name_id() const { return 0; }
  virtual void GetPOSList(vector<string> *pos_list) const {}
  virtual bool GetPOSIDs(const string &pos, uint16 *id) const {
    return false;
  }
  virtual bool GetArabicNumberPOSIDs(uint32 number,
                                     vector<pair<uint16, uint16> > *ids) const {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(POSMockHandler);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_POS_MOCK_H_
