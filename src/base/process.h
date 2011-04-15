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

#ifndef MOZC_BASE_PROCESS_H_
#define MOZC_BASE_PROCESS_H_

#include <string>

#ifdef OS_WINDOWS
#include "base/port.h"
#include "base/scoped_handle.h"
#include "third_party/mozc/sandbox/restricted_token_utils.h"
#endif  // OS_WINDOWS

namespace mozc {
#ifdef OS_WINDOWS
typedef DWORD ProsessPriorityClass;
#else // OS_WINDOWS
// TODO(yukawa): MacOS and Linux definition
typedef int ProsessPriorityClass;
#endif
class Process {
 public:
  // Open the URL with the default browser.  If this function is not
  // supported on the OS or failed, false is returned.
  static bool OpenBrowser(const string &url);

  // Spawn a process specified by path using arg as options.
  // On Windows Vista the process is spawned as the same level as the parent
  // process.
  // Return true if process is successfully launched.
  // On Mac and Linux, if pid is specified, pid of child process is set.
  // On Windows, the pid parameter is ignored and the initial directory of the
  // new process is set to the system directory.
  // On Mac OSX, if the path does not specify the binary itself but
  // specifies an directory ending with ".app", an application is
  // spawned in the OSX way.
  static bool SpawnProcess(
      const string &path, const string& arg, size_t *pid = NULL);

  // A SpawnProcess wrapper to run an executable which is installed in
  // the Mozc server directory.
  static bool SpawnMozcProcess(
      const string &filename, const string &arg, size_t *pid = NULL);

  // Wait process |pid| until it terminates.
  // you can set timeout. if timeout is negative, wait forever.
  static bool WaitProcess(size_t pid, int timeout);

  // return true if a process having |pid| is still alive.
  // if the the current thread has no permission to get the status or
  // operation failed in system call, it returns |default_result|.
  // TODO(all):
  // Note that there is the case where the specified thread/process has already
  // been terminated but the same thread/process ID is assigned to new ones.
  // The caller might want to use another technique like comparing process
  // creation time if this kind of false-positive matters.
  static bool IsProcessAlive(size_t pid, bool default_result);

  // return true if a thread having |thread_id| is still alive.
  // if the current thread has no permission to get the status or
  // operation failed in system call, it returns |default_result|.
  // On Posix, it always returns |default_result| as thread_id is not supported.
  // TODO(all):
  // Note that there is the case where the specified thread/process has already
  // been terminated but the same thread/process ID is assigned to new ones.
  // The caller might want to use another technique like comparing process
  // creation time if this kind of false-positive matters.
  static bool IsThreadAlive(size_t thread_id, bool default_result);

#ifdef OS_WINDOWS
  // Spawn a process specified by path as the specified integriy level and job
  // level.
  // Return true if process is successfully launched.
  // if pid is specified, pid of child process is set.
  struct SecurityInfo {
    SecurityInfo();
    sandbox::TokenLevel primary_level;
    sandbox::TokenLevel impersonation_level;
    sandbox::JobLevel job_level;
    sandbox::IntegrityLevel integrity_level;
    bool allow_ui_operation;
    bool in_system_dir;
    uint32 creation_flags;
  };

  static bool SpawnProcessAs(const string &path,
                             const string &arg,
                             const SecurityInfo &info,
                             DWORD *pid);
#endif  // OS_WINDOWS

  // Change process mode to backgroud mode in which file I/O or network I/O of
  // the process is scheduled by the OS not to interference foreground tasks.
  // Return true if process successfully entered to the background mode.
  // It also returns previous mode via |previous_mode|, which should be passed
  // to EndBackgroundProcessMode;
  static bool BeginBackgroundProcessMode(
      ProsessPriorityClass *previous_mode);

  // Restore process mode to the previous mode.
  // Return true if process successfully returned to the previous mode.
  static bool EndBackgroundProcessMode(
      const ProsessPriorityClass *previous_mode);

  // launch error message dialog
  static bool LaunchErrorMessageDialog(const string &type);

 private:
  Process() {}
  virtual ~Process() {}
};
}  // namespace mozc

#endif  // MOZC_BASE_PROCESS_H_
