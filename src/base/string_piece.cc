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

#include "base/string_piece.h"

#include <algorithm>
#include <climits>
#include <iostream>
#include <string>

namespace mozc {

typedef StringPiece::size_type size_type;

ostream &operator<<(ostream &o, const StringPiece &piece) {
  o.write(piece.data(), static_cast<streamsize>(piece.size()));
  return o;
}

void StringPiece::CopyToString(string *target) const {
  target->assign(!empty() ? data() : "", size());
}

void StringPiece::AppendToString(string *target) const {
  if (!empty()) {
    target->append(data(), size());
  }
}

size_type StringPiece::copy(char *buf, size_type n, size_type pos) const {
  const size_type ret = min(length_ - pos, n);
  memcpy(buf, ptr_ + pos, ret);
  return ret;
}

size_type StringPiece::find(const StringPiece &s, size_type pos) const {
  if (pos > length_) {
    return npos;
  }
  if (s.length_ == 1) {
    return find(s.ptr_[0], pos);
  }
  const char *result = std::search(ptr_ + pos, ptr_ + length_,
                                   s.ptr_, s.ptr_ + s.length_);
  const size_type xpos = result - ptr_;
  return xpos + s.length_ <= length_ ? xpos : npos;
}

size_type StringPiece::find(char c, size_type pos) const {
  if (pos >= length_) {
    return npos;
  }

  const char *result = std::find(ptr_ + pos, ptr_ + length_, c);
  return result != ptr_ + length_ ? result - ptr_ : npos;
}

size_type StringPiece::rfind(const StringPiece &s, size_type pos) const {
  if (length_ < s.length_) {
    return npos;
  }

  if (s.empty()) {
    return min(length_, pos);
  }

  const char *last = ptr_ + min(length_ - s.length_, pos) + s.length_;
  const char *result = std::find_end(ptr_, last, s.ptr_, s.ptr_ + s.length_);
  return result != last ? result - ptr_ : npos;
}

size_type StringPiece::rfind(char c, size_type pos) const {
  if (length_ == 0) {
    return npos;
  }

  for (size_type i = min(pos, length_ - 1); ; --i) {
    if (ptr_[i] == c) {
      return i;
    }
    if (i == 0) {
      break;
    }
  }
  return npos;
}

// For each character in characters_wanted, sets the index corresponding
// to the ASCII code of that character to 1 in table.  This is used by
// the find_.*_of methods below to tell whether or not a character is in
// the lookup table in constant time.
// The argument `table' must be an array that is large enough to hold all
// the possible values of an unsigned char.  Thus it should be be declared
// as follows:
//   bool table[UCHAR_MAX + 1]
static inline void BuildLookupTable(const StringPiece &characters_wanted,
                                    bool* table) {
  const size_type length = characters_wanted.length();
  const char * const data = characters_wanted.data();
  for (size_type i = 0; i < length; ++i) {
    table[static_cast<unsigned char>(data[i])] = true;
  }
}

size_type StringPiece::find_first_of(const StringPiece &s,
                                     size_type pos) const {
  if (length_ == 0 || s.length_ == 0) {
    return npos;
  }

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) {
    return find_first_of(s.ptr_[0], pos);
  }

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (size_type i = pos; i < length_; ++i) {
    if (lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

size_type StringPiece::find_first_not_of(const StringPiece &s,
                                         size_type pos) const {
  if (length_ == 0) {
    return npos;
  }

  if (s.length_ == 0) {
    return 0;
  }

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) {
    return find_first_not_of(s.ptr_[0], pos);
  }

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (size_type i = pos; i < length_; ++i) {
    if (!lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
  }
  return npos;
}

size_type StringPiece::find_first_not_of(char c, size_type pos) const {
  for (; pos < length_; ++pos) {
    if (ptr_[pos] != c) {
      return pos;
    }
  }
  return npos;
}

size_type StringPiece::find_last_of(const StringPiece &s,
                                    size_type pos) const {
  if (length_ == 0 || s.length_ == 0) {
    return npos;
  }

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) {
    return find_last_of(s.ptr_[0], pos);
  }

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (size_type i = min(pos, length_ - 1); ; --i) {
    if (lookup[static_cast<unsigned char>(ptr_[i])]) {
      return i;
    }
    if (i == 0) {
      break;
    }
  }
  return npos;
}

size_type StringPiece::find_last_not_of(const StringPiece &s,
                                        size_type pos) const {
  if (length_ == 0) {
    return npos;
  }

  size_type i = min(pos, length_ - 1);
  if (s.length_ == 0) {
    return i;
  }

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.length_ == 1) {
    return find_last_not_of(s.ptr_[0], pos);
  }

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (; ; --i) {
    if (!lookup[static_cast<unsigned char>(ptr_[i])])
      return i;
    if (i == 0)
      break;
  }
  return npos;
}

size_type StringPiece::find_last_not_of(char c, size_type pos) const {
  if (length_ == 0) {
    return npos;
  }

  for (size_type i = min(pos, length_ - 1); ; --i) {
    if (ptr_[i] != c)
      return i;
    if (i == 0)
      break;
  }
  return npos;
}

StringPiece StringPiece::substr(size_type pos, size_type n) const {
  if (pos > length_) {
    pos = length_;
  }
  if (n > length_ - pos) {
    n = length_ - pos;
  }
  return StringPiece(ptr_ + pos, n);
}

const StringPiece::size_type StringPiece::npos = size_type(-1);

}  // namespace mozc

