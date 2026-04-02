// Copyright 2010-2021, Google Inc.
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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace user_dictionary {

// Extended error code stored in absl::Status. We use the absl's canonical
// error code (absl::StatusCode) for general resource management. Extended
// Error code is mainly used for the dictionary management. When
// absl::IsUnknown(status) is true, we can access the extended error code
// via status.raw_code(). Otherwise, canonical error code is used.
enum ExtendedErrorCode {
  OK = 0,

  // Migrated from UserDictionaryCommandStatus::Status.
  UNKNOWN_ERROR = 100,

  FILE_NOT_FOUND,
  INVALID_FILE_FORMAT,

  // Note: currently if we receive this error status,
  // the file is actually saved.
  FILE_SIZE_LIMIT_EXCEEDED,
  DICTIONARY_SIZE_LIMIT_EXCEEDED,
  ENTRY_SIZE_LIMIT_EXCEEDED,

  UNKNOWN_DICTIONARY_ID,
  ENTRY_INDEX_OUT_OF_RANGE,

  // Errors for dictionary names.
  DICTIONARY_NAME_EMPTY,
  DICTIONARY_NAME_TOO_LONG,
  DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER,
  DICTIONARY_NAME_DUPLICATED,

  // Errors for entry data.
  READING_EMPTY,
  READING_TOO_LONG,
  READING_CONTAINS_INVALID_CHARACTER,
  WORD_EMPTY,
  WORD_TOO_LONG,
  WORD_CONTAINS_INVALID_CHARACTER,
  INVALID_POS_TYPE,
  COMMENT_TOO_LONG,
  COMMENT_CONTAINS_INVALID_CHARACTER,

  // Errors for importing.
  IMPORT_TOO_MANY_WORDS,
  IMPORT_NOT_SUPPORTED,
  IMPORT_INVALID_ENTRIES,
  IMPORT_FATAL,
  IMPORT_UNKNOWN_ERROR
};

absl::Status ToStatus(ExtendedErrorCode code);

// Returns true if all characters in the given string is a legitimate
// character for reading.
bool IsValidReading(absl::string_view reading);

// Performs various kinds of character normalization such as
// katakana-> hiragana and full-width ascii -> half width
// ascii. Identity of reading of a word should be defined by the
// output of this function.
std::string NormalizeReading(absl::string_view input);

// Returns the error status of the validity for the given entry.
// The validation process is as follows:
// - Checks the reading
//   - if it isn't empty
//   - if it doesn't exceed the max length
//   - if it doesn't contain invalid character
// - Checks the word
//   - if it isn't empty
//   - if it doesn't exceed the max length
//   - if it doesn't contain invalid character
// - Checks the comment
//   - if it isn't exceed the max length
//   - if it doesn't contain invalid character
// - Checks if a valid pos type is set.
absl::Status ValidateEntry(const user_dictionary::UserDictionary::Entry& entry);

// Sanitizes a dictionary entry so that it's acceptable to the
// class. A user of the class may want this function to make sure an
// error want happen before calling AddEntry() and other
// methods. Return true if the entry is changed.
bool SanitizeEntry(user_dictionary::UserDictionary::Entry* entry);

// Helper function for SanitizeEntry
// "max_size" is the maximum allowed size of str. If str size exceeds
// "max_size", remaining part is truncated by this function.
bool Sanitize(std::string* str, size_t max_size);

// Returns the error status of the validity for the given dictionary name.
absl::Status ValidateDictionaryName(absl::string_view dictionary_name);

}  // namespace user_dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_UTIL_H_
