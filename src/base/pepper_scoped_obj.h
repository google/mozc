// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_BASE_PEPPER_SCOPED_OBJ_H_
#define MOZC_BASE_PEPPER_SCOPED_OBJ_H_

#include <ppapi/utility/completion_callback_factory.h>

#include "base/port.h"
#include "base/unnamed_event.h"

namespace mozc {

// Some Pepper classes have to be deleted in the NaCl main thread.
// scoped_main_thread_destructed_object template guarantees that the object will
// be deleted in the NaCl main thread.
template <class C>
class scoped_main_thread_destructed_object {
 public:
  typedef C element_type;

  explicit scoped_main_thread_destructed_object(C *p = NULL) : ptr_(p) {
    cc_factory_.Initialize(this);
  }

  ~scoped_main_thread_destructed_object() {
    if (ptr_ != NULL) {
      DeleteObject();
    }
  }

  void reset(C *p = NULL) {
    if (p != ptr_) {
      if (ptr_ != NULL) {
        DeleteObject();
      }
      ptr_ = p;
    }
  }

  C &operator*() const {
    assert(ptr_ != NULL);
    return *ptr_;
  }

  C *operator->() const  {
    assert(ptr_ != NULL);
    return ptr_;
  }

  C *get() const {
    return ptr_;
  }

 private:
  void DeleteObject() {
    if (!pp::Module::Get()->core()->IsMainThread()) {
      pp::Module::Get()->core()->CallOnMainThread(
          0,
          cc_factory_.NewCallback(
              &scoped_main_thread_destructed_object::DeleteObjectImpl));
    } else {
      DeleteObjectImpl(PP_OK);
    }
    event_.Wait(-1);
  }

  void DeleteObjectImpl(int32_t result) {
    delete ptr_;
    event_.Notify();
  }

  C *ptr_;
  UnnamedEvent event_;
  pp::CompletionCallbackFactory<scoped_main_thread_destructed_object>
      cc_factory_;
  DISALLOW_COPY_AND_ASSIGN(scoped_main_thread_destructed_object);
};

}  // namespace mozc

#endif  // MOZC_BASE_PEPPER_SCOPED_OBJ_H_
