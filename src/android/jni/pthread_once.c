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

// pthread_once(3C) of Android 2.1 and 2.2 have a bug that it's not recursively-
// callable (nor reentrant).  It hangs when it's recursively called.
// This module provides a replacement of pthread_once() and supposed to be
// linked prior to the default one.
// Once we get rid of Android 2.1 and 2.2, we no longer need this hack.

#ifdef OS_ANDROID

#include <pthread.h>
#include <stddef.h>

static pthread_mutex_t once_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

// This implementation doesn't support thread-cancelation nor fork during
// the execution of init_routine().
int pthread_once(pthread_once_t  *once_control, void (*init_routine)(void)) {
  // pthread_once always returns 0.

  if (once_control == NULL || init_routine == NULL) {
    return 0;
  }

  // Double-checked locking is not a good solution in general, but it works
  // in this case in C.
  if (*once_control != PTHREAD_ONCE_INIT) {
    return 0;
  }

  pthread_mutex_lock(&once_lock);

  if (*once_control == PTHREAD_ONCE_INIT) {
    init_routine();
    *once_control = PTHREAD_ONCE_INIT + 1;
  }

  pthread_mutex_unlock(&once_lock);

  return 0;
}

#endif
