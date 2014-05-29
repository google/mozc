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

#ifndef MOZC_WIN32_IME_IME_SCOPED_CONTEXT_H_
#define MOZC_WIN32_IME_IME_SCOPED_CONTEXT_H_

#include <Windows.h>

#include "base/port.h"
#include "win32/base/immdev.h"

namespace mozc {
namespace win32 {

template <class T>
class ScopedHIMC {
 public:
  explicit ScopedHIMC(HIMC himc) : himc_(nullptr), pointer_(nullptr) {
    pointer_ = static_cast<T*>(::ImmLockIMC(himc));
    if (pointer_ == nullptr) {
      return;
    }
    himc_ = himc;
  }

  ~ScopedHIMC() {
    if (himc_ != nullptr) {
      ::ImmUnlockIMC(himc_);
      himc_ = nullptr;
    }
    pointer_ = nullptr;
  }

  const T *get() const {
    return pointer_;
  }

  T *get() {
    return pointer_;
  }

  const T *operator->() const {
    return pointer_;
  }

  T *operator->() {
    return pointer_;
  }

 private:
  HIMC himc_;
  T *pointer_;
  DISALLOW_COPY_AND_ASSIGN(ScopedHIMC);
};

template <class T>
class ScopedHIMCC {
 public:
  explicit ScopedHIMCC(HIMCC himcc) : himcc_(himcc), pointer_(nullptr) {
    pointer_ = static_cast<T*>(::ImmLockIMCC(himcc_));
    if (pointer_ == nullptr) {
      himcc_ = nullptr;
    }
  }

  ~ScopedHIMCC() {
    if (himcc_ != nullptr) {
      ::ImmUnlockIMCC(himcc_);
    }
    pointer_ = nullptr;
  }

  const T *get() const {
    return pointer_;
  }

  T *get() {
    return pointer_;
  }

  const T *operator->() const {
    return pointer_;
  }

  T *operator->() {
    return pointer_;
  }

 private:
  T *pointer_;
  HIMCC himcc_;
  DISALLOW_COPY_AND_ASSIGN(ScopedHIMCC);
};

}  // namespace mozc
}  // namespace win32

#endif  // MOZC_WIN32_IME_IME_SCOPED_CONTEXT_H_
