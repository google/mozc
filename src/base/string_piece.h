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

// A string-like object that points to a sized piece of memory.

#ifndef MOZC_BASE_STRING_PIECE_H_
#define MOZC_BASE_STRING_PIECE_H_

#include <algorithm>
#include <cstring>
#include <iosfwd>
#include <string>

namespace mozc {


class StringPiece {
 public:
  typedef size_t size_type;

  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "string" wherever a "StringPiece" is expected.
  StringPiece() : ptr_(NULL), length_(0) {}
  StringPiece(const char *str)    // NOLINT
      : ptr_(str), length_((str != NULL) ? strlen(str) : 0) {}
  StringPiece(const string &str)  // NOLINT
      : ptr_(str.data()), length_(str.size()) {}
  StringPiece(const char *offset, size_type len)
      : ptr_(offset), length_(len) {}

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.
  const char *data() const { return ptr_; }
  size_type size() const { return length_; }
  size_type length() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() { ptr_ = NULL; length_ = 0; }
  void set(const char *data, size_type len) { ptr_ = data; length_ = len; }
  void set(const char *str) {
    ptr_ = str;
    length_ = (str != NULL) ? strlen(str) : 0;
  }
  void set(const void *data, size_type len) {
    ptr_ = reinterpret_cast<const char *>(data);
    length_ = len;
  }

  char operator[](size_type i) const { return ptr_[i]; }

  void remove_prefix(size_type n) {
    ptr_ += n;
    length_ -= n;
  }

  void remove_suffix(size_type n) {
    length_ -= n;
  }

  // returns {-1, 0, 1}
  int compare(StringPiece x) const {
    const size_type min_size = length_ < x.length_ ? length_ : x.length_;
    const int r = memcmp(ptr_, x.ptr_, min_size);
    if (r < 0) {
      return -1;
    }
    if (r > 0) {
      return 1;
    }
    if (length_ < x.length_) {
      return -1;
    }
    if (length_ > x.length_) {
      return 1;
    }
    return 0;
  }

  string as_string() const {
    // string doesn't like to take a NULL pointer even with a 0 size.
    return string(!empty() ? data() : "", size());
  }

  void CopyToString(string *target) const;
  void AppendToString(string *target) const;

  bool starts_with(StringPiece x) const {
    return (length_ >= x.length_) && (memcmp(ptr_, x.ptr_, x.length_) == 0);
  }

  bool ends_with(StringPiece x) const {
    return ((length_ >= x.length_) &&
            (memcmp(ptr_ + (length_ - x.length_), x.ptr_, x.length_) == 0));
  }

  // standard STL container boilerplate
  typedef char value_type;
  typedef const char *pointer;
  typedef const char &reference;
  typedef const char &const_reference;
  typedef ptrdiff_t difference_type;
  static const size_type npos;
  typedef const char *const_iterator;
  typedef const char *iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  iterator begin() const { return ptr_; }
  iterator end() const { return ptr_ + length_; }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(ptr_ + length_);
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(ptr_);
  }

  size_type max_size() const { return length_; }
  size_type capacity() const { return length_; }

  size_type copy(char *buf, size_type n, size_type pos = 0) const;

  size_type find(const StringPiece &s, size_type pos = 0) const;
  size_type find(char c, size_type pos = 0) const;
  size_type rfind(const StringPiece &s, size_type pos = npos) const;
  size_type rfind(char c, size_type pos = npos) const;

  size_type find_first_of(const StringPiece &s, size_type pos = 0) const;
  size_type find_first_of(char c, size_type pos = 0) const {
    return find(c, pos);
  }
  size_type find_first_not_of(const StringPiece &s, size_type pos = 0) const;
  size_type find_first_not_of(char c, size_type pos = 0) const;
  size_type find_last_of(const StringPiece &s, size_type pos = npos) const;
  size_type find_last_of(char c, size_type pos = npos) const {
    return rfind(c, pos);
  }
  size_type find_last_not_of(const StringPiece &s, size_type pos = npos) const;
  size_type find_last_not_of(char c, size_type pos = npos) const;

  StringPiece substr(size_type pos, size_type n = npos) const;

  friend bool operator==(const StringPiece &x, const StringPiece &y) {
    if (x.size() != y.size()) {
      return false;
    }
    return memcmp(x.data(), y.data(), x.size()) == 0;
  }

  friend bool operator!=(const StringPiece &x, const StringPiece &y) {
    return !(x == y);
  }

  friend bool operator<(const StringPiece &x, const StringPiece &y) {
    const int min_size = x.size() < y.size() ? x.size() : y.size();
    const int r = memcmp(x.data(), y.data(), min_size);
    return (r < 0) || (r == 0 && x.size() < y.size());
  }

  friend bool operator>(const StringPiece &x, const StringPiece &y) {
    return y < x;
  }

  friend bool operator<=(const StringPiece &x, const StringPiece &y) {
    return !(x > y);
  }

  friend bool operator>=(const StringPiece &x, const StringPiece &y) {
    return !(x < y);
  }

 private:
  const char *ptr_;
  size_type length_;
};

// allow StringPiece to be logged (needed for unit testing).
extern ostream &operator<<(ostream &o, const StringPiece &piece);


}  // namespace mozc

#endif  // MOZC_BASE_STRING_PIECE_H_
