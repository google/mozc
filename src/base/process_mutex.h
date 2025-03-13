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

#ifndef MOZC_BASE_PROCESS_MUTEX_H_
#define MOZC_BASE_PROCESS_MUTEX_H_

#include <string>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

#ifdef _WIN32
#include <wil/resource.h>
#endif  // _WIN32

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
// /* process named "foo" is never instantiated inside this block */
// foo.Unlock();

namespace mozc {

class ProcessMutex {
 public:
  explicit ProcessMutex(absl::string_view name);
  ProcessMutex(const ProcessMutex &) = delete;
  ProcessMutex &operator=(const ProcessMutex &) = delete;
  ~ProcessMutex();

  // return false if the process is already locked
  bool Lock();

  // Lock the mutex file and write some message to this file
  bool LockAndWrite(absl::string_view message);

  // always return true at this moment.
  bool UnLock();

  // return lock filename
  // filename: <user_profile>/.lock.<name>
  const std::string &lock_filename() const { return filename_; }

  void set_lock_filename(std::string filename) {
    filename_ = std::move(filename);
  }

  bool locked() const {
    absl::ReaderMutexLock l(&mutex_);
    return locked_;
  }

 private:
#ifdef _WIN32
  wil::unique_hfile handle_;
#endif  // _WIN32

  bool LockAndWriteInternal(absl::string_view message);
  void UnLockInternal();

  mutable absl::Mutex mutex_;

  // TODO(yukawa): Remove this flag as it can always be determined by other
  //     internal state.
  bool locked_ ABSL_GUARDED_BY(mutex_) = false;
  std::string filename_;
};

}  // namespace mozc

#endif  // MOZC_BASE_PROCESS_MUTEX_H_
