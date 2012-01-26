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

#ifndef MOZC_CHROME_SKK_SKK_UTIL_H_
#define MOZC_CHROME_SKK_SKK_UTIL_H_

#include <string>
#include <vector>

#include "dictionary/system/system_dictionary.h"
#include "third_party/jsoncpp/json.h"

namespace mozc {

namespace chrome {
namespace skk {

extern const char *kStatusDebug;
extern const char *kStatusError;
extern const char *kStatusOK;

extern const char *kMethodLookup;

extern const char *kMessageBodyField;
extern const char *kMessageIdField;
extern const char *kMessageMethodField;
extern const char *kMessageStatusField;

extern const char *kMessageBaseField;
extern const char *kMessageStemField;

extern const char *kMessageCandidatesField;
extern const char *kMessagePredictionsField;
extern const char *kMessageMessageField;

}  // namespace skk
}  // namespace chrome

class SkkUtil {
 public:
  static bool IsSupportedMethod(const string &method);
  static void RemoveDuplicateEntry(vector<string> *candidates);
  static bool ValidateMessage(const Json::Value &json_message,
                              string *error_message);
  static void LookupEntry(mozc::SystemDictionary *dictionary,
                          const string &reading,
                          vector<string> *candidates,
                          vector<string> *predictions);
};

}  // namespace mozc

#endif  // MOZC_CHROME_SKK_SKK_UTIL_H_
