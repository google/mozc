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

#include "dictionary/user_dictionary_util.h"

#include <string.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "base/strings/japanese.h"
#include "base/strings/unicode.h"
#include "base/vlog.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace user_dictionary {
namespace {

// Maximum string length in UserDictionaryEntry's field
constexpr size_t kMaxStringSize = 300;
constexpr absl::string_view kInvalidChars = "\n\r\t";

bool InternalValidateNormalizedReading(const absl::string_view reading) {
  auto InRange = [](const char32_t c, const char32_t lo, const char32_t hi) {
    return lo <= c && c <= hi;
  };
  for (const char32_t c : Utf8AsChars32(reading)) {
    if (!InRange(c, 0x0021, 0x007E) &&  // Basic Latin (Ascii)
        !InRange(c, 0x3041, 0x3096) &&  // Hiragana
        !InRange(c, 0x309B, 0x309C) &&  // KATAKANA-HIRAGANA VOICED/SEMI-VOICED
                                        // SOUND MARK
        !InRange(c, 0x30FB, 0x30FC) &&  // Nakaten, Prolonged sound mark
        !InRange(c, 0x3001, 0x3002) &&  // Japanese punctuation marks
        !InRange(c, 0x300C, 0x300F) &&  // Japanese brackets
        !InRange(c, 0x301C, 0x301C)) {  // Japanese Wavedash
      LOG(INFO) << "Invalid character in reading.";
      return false;
    }
  }
  return true;
}

bool IsTooLongString(absl::string_view str) {
  return str.size() > kMaxStringSize;
}

bool ContainsInvalidChars(absl::string_view str) {
  return (str.find_first_of(kInvalidChars) != absl::string_view::npos ||
          !strings::IsValidUtf8(str));
}

}  // namespace

absl::Status ToStatus(ExtendedErrorCode code) {
  return absl::Status(static_cast<absl::StatusCode>(code), "Extended Error");
}

bool IsValidReading(const absl::string_view reading) {
  return InternalValidateNormalizedReading(NormalizeReading(reading));
}

std::string NormalizeReading(const absl::string_view input) {
  std::string tmp1 = japanese::FullWidthAsciiToHalfWidthAscii(input);
  std::string tmp2 = japanese::HalfWidthKatakanaToFullWidthKatakana(tmp1);
  return japanese::KatakanaToHiragana(tmp2);
}

absl::Status ValidateEntry(
    const user_dictionary::UserDictionary::Entry& entry) {
  // Validate reading.
  absl::string_view reading = entry.key();
  if (reading.empty()) {
    MOZC_VLOG(1) << "key is empty";
    return ToStatus(ExtendedErrorCode::READING_EMPTY);
  }
  if (IsTooLongString(reading)) {
    MOZC_VLOG(1) << "Too long key.";
    return ToStatus(ExtendedErrorCode::READING_TOO_LONG);
  }
  if (ContainsInvalidChars(reading)) {
    MOZC_VLOG(1) << "Invalid reading";
    return ToStatus(ExtendedErrorCode::READING_CONTAINS_INVALID_CHARACTER);
  }

  // Validate word.
  absl::string_view word = entry.value();
  if (word.empty()) {
    return ToStatus(ExtendedErrorCode::WORD_EMPTY);
  }
  if (IsTooLongString(word)) {
    MOZC_VLOG(1) << "Too long value.";
    return ToStatus(ExtendedErrorCode::WORD_TOO_LONG);
  }
  if (ContainsInvalidChars(word)) {
    MOZC_VLOG(1) << "Invalid character in value.";
    return ToStatus(ExtendedErrorCode::WORD_CONTAINS_INVALID_CHARACTER);
  }

  // Validate comment.
  absl::string_view comment = entry.comment();
  if (IsTooLongString(comment)) {
    MOZC_VLOG(1) << "Too long comment.";
    return ToStatus(ExtendedErrorCode::COMMENT_TOO_LONG);
  }
  if (ContainsInvalidChars(comment)) {
    MOZC_VLOG(1) << "Invalid character in comment.";
    return ToStatus(ExtendedErrorCode::COMMENT_CONTAINS_INVALID_CHARACTER);
  }

  // Validate pos.
  if (!entry.has_pos() ||
      !user_dictionary::UserDictionary::PosType_IsValid(entry.pos())) {
    MOZC_VLOG(1) << "Invalid POS";
    return ToStatus(ExtendedErrorCode::INVALID_POS_TYPE);
  }

  return absl::OkStatus();
}

// static
bool SanitizeEntry(user_dictionary::UserDictionary::Entry* entry) {
  bool modified = false;
  modified |= Sanitize(entry->mutable_key(), kMaxStringSize);
  modified |= Sanitize(entry->mutable_value(), kMaxStringSize);
  if (!user_dictionary::UserDictionary::PosType_IsValid(entry->pos())) {
    // Fallback to NOUN.
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    modified = true;
  }
  modified |= Sanitize(entry->mutable_comment(), kMaxStringSize);
  return modified;
}

// static
bool Sanitize(std::string* str, size_t max_size) {
  // First part: Remove invalid characters.
  const int n = absl::StrReplaceAll(
      {
          {"\t", ""},
          {"\n", ""},
          {"\r", ""},
      },
      str);

  // Second part: Truncate long strings.
  if (str->size() <= max_size) {
    return n > 0;
  }
  const Utf8AsChars chars(*str);
  size_t pos = 0;
  for (auto it = chars.begin(); pos + it.size() <= max_size; ++it) {
    pos += it.size();
  }
  str->erase(pos);
  return true;
}

absl::Status ValidateDictionaryName(const absl::string_view dictionary_name) {
  if (dictionary_name.empty()) {
    MOZC_VLOG(1) << "Empty dictionary name.";
    return ToStatus(ExtendedErrorCode::DICTIONARY_NAME_EMPTY);
  }
  if (IsTooLongString(dictionary_name)) {
    MOZC_VLOG(1) << "Too long dictionary name";
    return ToStatus(ExtendedErrorCode::DICTIONARY_NAME_TOO_LONG);
  }
  if (ContainsInvalidChars(dictionary_name)) {
    MOZC_VLOG(1) << "Invalid character in dictionary name: " << dictionary_name;
    return ToStatus(
        ExtendedErrorCode::DICTIONARY_NAME_CONTAINS_INVALID_CHARACTER);
  }

  return absl::OkStatus();
}

}  // namespace user_dictionary
}  // namespace mozc
