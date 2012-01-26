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

#ifndef MOZC_IPC_PROCESS_MUTEX_H_
#define MOZC_IPC_PROCESS_MUTEX_H_

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include <string>

// Process-wide named mutex class:
// Make a simple process-wide named mutex using file locking.
// Useful to prevent a process from being instantiated twice or more.
//
// Example:
// ProcessMutex foo("foo");
// if (!foo.Lock()) {
//   process foo is already running
//   exit(-1);
// }
//
// foo.Lock();
// /* process named "foo" is never instatiated inside this block */
// foo.Unlock();

namespace mozc {

class ProcessMutex {
 public:
  explicit ProcessMutex(const char *name);
  virtual ~ProcessMutex();

  // return false if the process is already locked
  bool Lock();

  // Lock the mutex file and write some message to this file
  bool LockAndWrite(const string &message);

  // always return true at this moment.
  bool UnLock();

  // return lock filename
  // filename: <user_profile>/.lock.<name>
  const string &lock_filename() const {
    return filename_;
  }

  void set_lock_filename(const string &filename) {
    filename_ = filename;
  }

#ifdef OS_WINDOWS
  HANDLE handle_;
#endif

  bool locked_;
  string filename_;
};
}  // namespace mozc
#endif  // MOZC_IPC_PROCESS_MUTEX_H_
