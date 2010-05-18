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

#include "dictionary/user_dictionary_util.h"

#include <string.h>
#include <algorithm>
#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "dictionary/user_pos.h"

namespace mozc {

namespace {
// Maximum string length in UserDictionaryEntry's field
const size_t kMaxKeySize            =   300;
const size_t kMaxValueSize          =   300;
const size_t kMaxPOSSize            =   300;
const size_t kMaxCommentSize        =   300;
const char kInvalidChars[]= "\n\r\t";
const char kUserDictionaryFile[] = "user://user_dictionary.db";
}

// TODO(keni): Write unit tests for this function.
bool UserDictionaryUtil::IsValidEntry(
    const UserDictionaryStorage::UserDictionaryEntry &entry) {
  if (entry.key().empty()) {
    VLOG(1) << "key is empty";
    return false;
  }
  if (entry.key().find_first_of(kInvalidChars) != string::npos) {
    VLOG(1) << "Invalid character in key.";
    return false;
  }
  if (entry.key().size() > kMaxKeySize) {
    VLOG(1) << "Too long key.";
    return false;
  }
  if (entry.value().find_first_of(kInvalidChars) != string::npos) {
    VLOG(1) << "Invalid character in value.";
    return false;
  }
  if (entry.value().size() > kMaxValueSize) {
    VLOG(1) << "Too long value.";
    return false;
  }
  if (entry.pos().find_first_of(kInvalidChars) != string::npos) {
    VLOG(1) << "Invalid character in POS.";
    return false;
  }
  if (entry.pos().size() > kMaxPOSSize) {
    VLOG(1) << "Too long POS.";
    return false;
  }
  if (entry.comment().find_first_of(kInvalidChars) != string::npos) {
    VLOG(1) << "Invalid character in comment.";
    return false;
  }
  if (entry.comment().size() > kMaxCommentSize) {
    VLOG(1) << "Too long comment.";
    return false;
  }
  if (!UserDictionaryUtil::IsValidReading(entry.key())) {
    VLOG(1) << "Invalid reading";
    return false;
  }
  if (!UserPOS::IsValidPOS(entry.pos())) {
    VLOG(1) << "Invalid POS";
    return false;
  }

  return true;
}

namespace {

#define INRANGE(w, a, b) ((w) >= (a) && (w) <= (b))

bool InternalValidateNormalizedReading(const string &normalized_reading) {
  const char *begin = normalized_reading.c_str();
  const char *end = begin + normalized_reading.size();
  size_t mblen = 0;
  while (begin < end) {
    const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);
    if (INRANGE(w, 0x0021, 0x007E) ||  // Basic Latin (Ascii)
        INRANGE(w, 0x3041, 0x3096) ||  // Hiragana
        INRANGE(w, 0x309B, 0x309C) ||  // KATAKANA-HIRAGANA VOICED/SEMI-VOICED
                                       // SOUND MARK
        INRANGE(w, 0x30FB, 0x30FC) ||  // Nakaten, Prolonged sound mark
        INRANGE(w, 0x3001, 0x3002) ||  // Japanese punctuation marks
        INRANGE(w, 0x300C, 0x300F) ||  // Japanese brackets
        INRANGE(w, 0x301C, 0x301C)) {  // Japanese Wavedash
      begin += mblen;
    } else {
      LOG(INFO) << "Invalid character in reading.";
      return false;
    }
  }
  return true;
}

#undef INRANGE

}  // namespace

bool UserDictionaryUtil::IsValidReading(const string &reading) {
  string normalized;
  NormalizeReading(reading, &normalized);
  return InternalValidateNormalizedReading(normalized);
}

void UserDictionaryUtil::NormalizeReading(const string &input, string *output) {
  output->clear();
  string tmp1, tmp2;
  Util::FullWidthAsciiToHalfWidthAscii(input, &tmp1);
  Util::HalfWidthKatakanaToFullWidthKatakana(tmp1, &tmp2);
  Util::KatakanaToHiragana(tmp2, output);
}

string UserDictionaryUtil::GetUserDictionaryFileName() {
  return ConfigFileStream::GetFileName(kUserDictionaryFile);
}

// static
bool UserDictionaryUtil::SanitizeEntry(
    UserDictionaryStorage::UserDictionaryEntry *entry) {
  bool modified = false;
  modified |= Sanitize(entry->mutable_key(), kMaxKeySize);
  modified |= Sanitize(entry->mutable_value(), kMaxValueSize);
  modified |= Sanitize(entry->mutable_pos(), kMaxPOSSize);
  modified |= Sanitize(entry->mutable_comment(), kMaxCommentSize);
  return modified;
}

// static
bool UserDictionaryUtil::Sanitize(string *str, size_t max_size) {
  // First part: Remove invalid characters.
  {
    const size_t original_size = str->size();
    string::iterator begin = str->begin();
    string::iterator end = str->end();
    end = remove(begin, end, '\t');
    end = remove(begin, end, '\n');
    end = remove(begin, end, '\r');

    if (end - begin <= max_size) {
      if (end - begin == original_size) {
        return false;
      } else {
        str->erase(end - begin);
        return true;
      }
    }
  }

  // Second part: Truncate long strings.
  {
    const char *begin = str->data();
    const char *p = begin;
    const char *end = begin + str->size();
    while (p < end) {
      const size_t len = Util::OneCharLen(p);
      if ((p + len - begin) > max_size) {
        str->erase(p - begin);
        return true;
      }
      p += len;
    }
    LOG(FATAL) <<
        "There should be a bug in implementation of the function.";
  }

  return true;
}
}  // namespace mozc
