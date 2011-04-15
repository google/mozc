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

// UserDicUtil provides various utility functions related to the user
// dictionary.

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_UTIL_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_UTIL_H_

#include <string>
#include <vector>
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"

namespace mozc {

class UserDictionaryUtil {
 public:
  // Return true if all characters in the given string is a legitimate
  // character for reading.
  static bool IsValidReading(const string &reading);

  // Perform varirous kinds of character normalization such as
  // katakana-> hiragana and full-width ascii -> half width
  // ascii. Identity of reading of a word should be defined by the
  // output of this function.
  static void NormalizeReading(const string &input, string *output);

  // Return true if all fields of the given data is properly set and
  // have a legitimate value. It checks for an empty string, an
  // invalid character and so on. If the function returns false, we
  // shouldn't accept the data being passed into the dictionary.
  static bool IsValidEntry(const UserDictionaryStorage::UserDictionaryEntry
                           &entry);

  // Sanitize a dictionary entry so that it's acceptable to the
  // class. A user of the class may want this function to make sure an
  // error want happen before calling AddEntry() and other
  // methods. Return true if the entry is changed.
  static bool SanitizeEntry(UserDictionaryStorage::UserDictionaryEntry *entry);

  // Helper function for SanitizeEntry
  // "max_size" is the maximum allowed size of str. If str size exceeds
  // "max_size", remaining part is truncated by this function.
  static bool Sanitize(string *str, size_t max_size);

  // return the file name of UserDictionary
  static string GetUserDictionaryFileName();

 private:
  UserDictionaryUtil() {}
  ~UserDictionaryUtil() {}
};
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_UTIL_H_
