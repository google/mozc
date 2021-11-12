// Copyright 2010-2021, Google Inc.
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

#include "base/mutex.h"

#ifdef OS_WIN
#include <Windows.h>
#endif  // OS_WIN

#include <atomic>

namespace mozc {
namespace {

// State for once_t.
enum CallOnceState {
  ONCE_INIT = 0,
  ONCE_RUNNING = 1,
  ONCE_DONE = 2,
};

}  // namespace

void CallOnce(once_t *once, void (*func)()) {
  if (once == nullptr || func == nullptr) {
    return;
  }
  int expected_state = ONCE_INIT;
  if (once->compare_exchange_strong(expected_state, ONCE_RUNNING)) {
    (*func)();
    *once = ONCE_DONE;
    return;
  }
  // If the above compare_exchange_strong() returns false, it stores the value
  // of once to expected_state, which is ONCE_RUNNING or ONCE_DONE.
  if (expected_state == ONCE_DONE) {
    return;
  }
  // Here's the case where expected_state == ONCE_RUNNING, indicating that
  // another thread is calling func.  Wait for it to complete.
  while (*once == ONCE_RUNNING) {
#ifdef OS_WIN
    ::YieldProcessor();
#endif  // OS_WIN
  }     // Busy loop
}

void ResetOnce(once_t *once) {
  if (once) {
    *once = ONCE_INIT;
  }
}

}  // namespace mozc
