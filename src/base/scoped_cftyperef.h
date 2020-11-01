// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_BASE_SCOPED_CFTYPEREF_H_
#define MOZC_BASE_SCOPED_CFTYPEREF_H_

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>

template <typename T>
class scoped_cftyperef {
 public:
  typedef T element_type;
  explicit scoped_cftyperef(T p = nullptr, bool do_retain = false) : ptr_(p) {
    if (do_retain && p != nullptr) {
      CFRetain(ptr_);
    }
  }

  ~scoped_cftyperef() {
    if (ptr_ != nullptr) {
      CFRelease(ptr_);
    }
  }

  void reset(T p = nullptr) {
    if (ptr_ != nullptr) {
      CFRelease(ptr_);
    }
    ptr_ = p;
  }

  T get() const { return ptr_; }

  bool Verify(CFTypeID type_id) {
    return ptr_ != nullptr &&
           CFGetTypeID(reinterpret_cast<CFTypeRef>(ptr_)) == type_id;
  }

 private:
  T ptr_;
  scoped_cftyperef(scoped_cftyperef const &);
  scoped_cftyperef &operator=(scoped_cftyperef const &);
  typedef scoped_cftyperef<T> this_type;
};

#endif  // __APPLE__
#endif  // MOZC_BASE_SCOPED_CFTYPEREF_H_
