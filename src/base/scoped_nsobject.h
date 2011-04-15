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

// scoped_nsobject supports RAII for Objective-C objects.
// The idea and code of scoped_nsobject originally comes from
// chromium base/scoped_nsobject.h.
// See http://src.chromium.org/viewvc/chrome/trunk/src/base/scoped_nsobject.h?revision=29399

#ifndef MOZC_BASE_SCOPED_NSOBJECT_H_
#define MOZC_BASE_SCOPED_NSOBJECT_H_

#import <Foundation/Foundation.h>

#include "base/port.h"

template<typename NST>
class scoped_nsobject {
 public:
  typedef NST* element_type;

  explicit scoped_nsobject(NST* object = nil)
      : object_(object) {
  }

  ~scoped_nsobject() {
    [object_ release];
  }

  void reset(NST* object = nil) {
    // We intentionally do not check that object != object_ as the caller must
    // already have an ownership claim over whatever it gives to scoped_nsobject
    // and scoped_cftyperef, whether it's in the constructor or in a call to
    // reset().  In either case, it relinquishes that claim and the scoper
    // assumes it.
    [object_ release];
    object_ = object;
  }

  bool operator==(NST* that) const {
    return object_ == that;
  }

  bool operator!=(NST* that) const {
    return object_ != that;
  }

  operator NST*() const {
    return object_;
  }

  NST* get() const {
    return object_;
  }

  void swap(scoped_nsobject& that) {
    NST* temp = that.object_;
    that.object_ = object_;
    object_ = temp;
  }

  // scoped_nsobject<>::release() is like scoped_ptr<>::release.  It is NOT
  // a wrapper for [object_ release].  To force a scoped_nsobject<> object to
  // call [object_ release], use scoped_nsobject<>::reset().
  NST* release() {
    NST* temp = object_;
    object_ = nil;
    return temp;
  }

 private:
  NST* object_;

  DISALLOW_COPY_AND_ASSIGN(scoped_nsobject);
};


#endif  // MOZC_BASE_SCOPED_NSOBJECT_H_
