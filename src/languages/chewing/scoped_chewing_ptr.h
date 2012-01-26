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

// Scoped pointer used for chewing.

#ifndef MOZC_LANGUAGES_CHEWING_SCOPED_CHEWING_PTR_H_
#define MOZC_LANGUAGES_CHEWING_SCOPED_CHEWING_PTR_H_

#include <chewingio.h>

#include "base/port.h"

template<typename PTR>
class scoped_chewing_ptr {
 public:
  typedef PTR* element_type;

  explicit scoped_chewing_ptr(PTR* object = NULL)
      : object_(object) {
  }

  ~scoped_chewing_ptr() {
    if (object_) {
      ::chewing_free(object_);
    }
  }

  void reset(PTR* object = NULL) {
    if (object_) {
      ::chewing_free(object_);
    }
    object_ = object;
  }

  bool operator==(PTR* that) const {
    return object_ == that;
  }

  bool operator!=(PTR* that) const {
    return object_ != that;
  }

  operator PTR*() const {
    return object_;
  }

  PTR* get() const {
    return object_;
  }

 private:
  PTR* object_;

  DISALLOW_COPY_AND_ASSIGN(scoped_chewing_ptr);
};


#endif  // MOZC_LANGUAGES_CHEWING_SCOPED_CHEWING_PTR_H_
