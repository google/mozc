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

#ifdef OS_WINDOWS
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef OS_MACOSX
#include <libkern/OSAtomic.h>
#endif

#include "base/base.h"
#include "base/mutex.h"

namespace mozc {

// Wrapper for Windows InterlockedCompareExchange
namespace {
#ifdef OS_LINUX
// TODO(taku):
// Linux doesn't provide InterlockedCompareExchange-like function.
// we have to double-check it
inline int InterlockedCompareExchange(volatile int *target,
                                      int new_value,
                                      int old_value) {
#if defined(__GNUC__) && \
        (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)) && \
        !defined(__arm__)
  // Use GCC's extention (Note: ARM GCC doesn't have this function.)
  // http://gcc.gnu.org/onlinedocs/gcc-4.1.2/gcc/Atomic-Builtins.html
  return __sync_val_compare_and_swap(target, old_value, new_value);
#else
#ifdef __i486__
  // Use cmpxchgl: http://0xcc.net/blog/archives/000128.html
  int result;
  asm volatile ("lock; cmpxchgl %1, %2"
                : "=a" (result)
                : "r" (new_value), "m" (*(target)), "0"(old_value)
                : "memory", "cc");
  return result;
#else  // __i486__
  // TODO(yusukes): Write a thread-safe implementation for ChromeOS for ARM.

  // not thread safe
  if (*target == old_value) {
    *target = new_value;
    return old_value;
  }
  return *target;
#endif  // __i486__
#endif  // __GNUC__
}
#endif  // OS_LINUX

// Use OSAtomicCompareAndSwapInt on Mac OSX
// http://developer.apple.com/iphone/library/documentation/
// system/conceptual/manpages_iphoneos/man3/OSAtomicCompareAndSwapInt.3.html
// TODO(taku): should we use OSAtomicCompareAndSwapIntBarrier?
#ifdef OS_MACOSX
inline int InterlockedCompareExchange(volatile int *target,
                                      int new_value,
                                      int old_value) {
  return OSAtomicCompareAndSwapInt(old_value, new_value, target)
      ? old_value : *target;
}
#endif  // OX_MACOSX
}  // namespace

void CallOnce(once_t *once, void (*func)()) {
  if (once == NULL || func == NULL) {
    return;
  }

  if (once->state != ONCE_INIT) {
    return;
  }

  // change the counter in atomic
  if (0 == InterlockedCompareExchange(&(once->counter), 1, 0)) {
    // call func
    (*func)();
    // change the status to be ONCE_DONE in atomic
    // Maybe we won't use it, but in order to make memory barrier,
    // we use InterlockedCompareExchange just in case.
    InterlockedCompareExchange(&(once->state), ONCE_DONE, ONCE_INIT);
  } else {
    while (once->state == ONCE_INIT) {
#ifdef OS_WINDOWS
      YieldProcessor();
#endif
    }  // busy loop
  }
}

void ResetOnce(once_t *once) {
  InterlockedCompareExchange(&(once->state), ONCE_INIT, ONCE_DONE);
  InterlockedCompareExchange(&(once->counter), 0, 1);
}
}  // namespace mozc
