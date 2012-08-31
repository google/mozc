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

#ifndef MOZC_BASE_THREAD_H_
#define MOZC_BASE_THREAD_H_

#include "base/compiler_specific.h"
#include "base/port.h"
#include "base/scoped_ptr.h"

// Definition of TLS (Thread Local Storage) keyword.
#ifndef TLS_KEYWORD

#if defined(OS_WINDOWS) && defined(_MSC_VER)
// Use VC's keyword on Windows.
//
// On Windows XP, thread local has a limitation. Since thread local
// is now used inside converter and converter is a stand alone program,
// it's not a issue at this moment.
//
// http://msdn.microsoft.com/en-us/library/9w1sdazb.aspx
// Thread-local variables work for EXEs and DLLs that are statically
// linked to the main executable (directly or via another DLL) -
// i.e. the DLL has to load before the main executable code starts
// running.
#define TLS_KEYWORD __declspec(thread)
#define HAVE_TLS 1
#endif // OS_WINDOWS && _MSC_VER

// Andorid NDK and NaCl don't support TLS.
#if defined(OS_LINUX) && !defined(OS_ANDROID) && \
    !defined(__native_client__) && (defined(__GNUC__) || defined(__clang__))
// GCC and Clang support TLS.
#define TLS_KEYWORD __thread
#define HAVE_TLS 1
#endif  // OS_LINUX && !OS_ANDROID && (__GNUC__ || __clang__)


#if defined(OS_MACOSX) && MOZC_GCC_VERSION_GE(4, 5)
// GCC 4.5 and later can *emulate* TLS on Mac even though it is
// expensive operation.
#define TLS_KEYWORD __thread
#define HAVE_TLS 1
#endif  // OS_MACOSX && GCC 4.5 and later

#endif  // !TLS_KEYWORD

#ifndef TLS_KEYWORD
// If TLS is not available, define TLS_KEYWORD as a dummy keyword.
#define TLS_KEYWORD
#endif  // !TLS_KEYWORD

// Hereafter, we can use TLS_KEYWORD like a keyword.
#ifndef TLS_KEYWORD
#error TLS_KEYWORD should be defined here.
#endif  // !TLS_KEYWORD


namespace mozc {

struct ThreadInternalState;

class Thread {
 public:
  Thread();
  virtual ~Thread();

  virtual void Run() = 0;

  void Start();
  bool IsRunning() const;
  void SetJoinable(bool joinable);
  void Join();
  // This method is not encouraged especiialy in Windows.
  void Terminate();

 private:
  void Detach();

#ifndef OS_WINDOWS
  static void *WrapperForPOSIX(void *ptr);
#endif  // OS_WINDOWS

  scoped_ptr<ThreadInternalState> state_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

}  // namespace mozc

#endif  // MOZC_BASE_THREAD_H_
