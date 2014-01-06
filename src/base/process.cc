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

#include "base/process.h"

#ifdef OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#else
#include <string.h>
#include <sys/stat.h>
#include <cerrno>
#endif  // WINDOWS

#ifdef OS_MACOSX
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>  // for posix_spawn().
#include "base/mac_process.h"
#endif  // OS_MACOSX

#ifdef OS_LINUX
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>  // for posix_spawn().
#include <sys/types.h>
#endif

#include <cstdlib>
#include <vector>

#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "base/util.h"

#ifdef OS_MACOSX
// We do not use the global environ variable because it is unavailable
// in Mac Framework/dynamic libraries.  Instead call _NSGetEnviron().
// See the "PROGRAMMING" section of http://goo.gl/4Hq0D for the
// detailed information.
#include <crt_externs.h>
static char **environ = *_NSGetEnviron();
#elif !defined(OS_WIN)
// Defined somewhere in libc. We can't pass NULL as the 6th argument of
// posix_spawn() since Qt applications use (at least) DISPLAY and QT_IM_MODULE
// environment variables.
extern char **environ;
#endif

namespace mozc {

namespace {
#ifdef OS_WIN
// ShellExecute to execute file in system dir.
// Since Windows does not allow rename or delete a directory which
// is set to the working directory by existing processes, we should
// avoid unexpected directory locking by background processes.
// System dir is expected to be more appropriate than tha directory
// where the executable exist, because installer can rename the
// executable to another directory and delete the application directory.
bool ShellExecuteInSystemDir(const wchar_t *verb,
                             const wchar_t *file,
                             const wchar_t *parameters,
                             INT show_command) {
  const int result =
      reinterpret_cast<int>(::ShellExecuteW(0, verb, file, parameters,
                                            SystemUtil::GetSystemDir(),
                                            show_command));
  LOG_IF(ERROR, result <= 32)
      << "ShellExecute failed."
      << ", error:" << result
      << ", verb: " << verb
      << ", file: " << file
      << ", parameters: " << parameters;
  return result > 32;
}
#endif  // OS_WIN
}  // namespace

bool Process::OpenBrowser(const string &url) {
  // url must start with http:// or https:// or file://
  if (url.find("http://") != 0 &&
      url.find("https://") != 0 &&
      url.find("file://") != 0) {
    return false;
  }

#ifdef OS_WIN
  wstring wurl;
  Util::UTF8ToWide(url.c_str(), &wurl);
  return ShellExecuteInSystemDir(L"open", wurl.c_str(), NULL, SW_SHOW);
#endif

#ifdef OS_LINUX
  static const char kBrowserCommand[] = "/usr/bin/xdg-open";
  // xdg-open which uses kfmclient or gnome-open internally works both on KDE
  // and GNOME environments.
  return SpawnProcess(kBrowserCommand, url);
#endif  // LINUX

#ifdef OS_MACOSX
  return MacProcess::OpenBrowserForMac(url);
#endif  // OS_MACOSX
  return false;
}

bool Process::SpawnProcess(const string &path,
                           const string& arg, size_t *pid) {
#ifdef OS_WIN
  wstring wpath;
  Util::UTF8ToWide(path.c_str(), &wpath);
  wpath = L"\"" + wpath + L"\"";
  if (!arg.empty()) {
    wstring warg;
    Util::UTF8ToWide(arg.c_str(), &warg);
    wpath += L" ";
    wpath += warg;
  }

  // The |lpCommandLine| parameter of CreateProcessW should be writable
  // so that we create a scoped_ptr<wchar_t[]> here.
  scoped_ptr<wchar_t[]> wpath2(new wchar_t[wpath.size() + 1]);
  if (0 != wcscpy_s(wpath2.get(), wpath.size() + 1, wpath.c_str())) {
    return false;
  }

  STARTUPINFOW si = { 0 };
  si.cb = sizeof(si);
  si.dwFlags = STARTF_FORCEOFFFEEDBACK;
  PROCESS_INFORMATION pi = { 0 };

  // If both |lpApplicationName| and |lpCommandLine| are non-NULL,
  // the argument array of the process will be shifted.
  // http://support.microsoft.com/kb/175986
  const bool create_process_succeeded = ::CreateProcessW(
      NULL, wpath2.get(), NULL, NULL, FALSE,
      CREATE_DEFAULT_ERROR_MODE, NULL,
      // NOTE: Working directory will be locked by the system.
      // We use system directory to spawn process so that users will not
      // to be aware of any undeletable directory. (http://b/2017482)
      SystemUtil::GetSystemDir(),
      &si, &pi) != FALSE;

  if (create_process_succeeded) {
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
  } else {
    LOG(ERROR) << "Create process failed: " << ::GetLastError();
  }

  return create_process_succeeded;
#else

  vector<string> arg_tmp;
  Util::SplitStringUsing(arg, " ", &arg_tmp);
  scoped_ptr<const char * []> argv(new const char *[arg_tmp.size() + 2]);
  argv[0] = path.c_str();
  for (size_t i = 0; i < arg_tmp.size(); ++i) {
    argv[i + 1] = arg_tmp[i].c_str();
  }
  argv[arg_tmp.size() + 1] = NULL;

  struct stat statbuf;
  if (::stat(path.c_str(), &statbuf) != 0) {
    LOG(ERROR) << "Can't stat " << path << ": " << strerror(errno);
    return false;
  }
#ifdef OS_MACOSX
  // Check if the "path" is an application or not.  If it's an
  // application, do a special process using mac_process.mm.
  if (S_ISDIR(statbuf.st_mode) && path.size() > 4 &&
      path.substr(path.size() - 4) == ".app") {
    // In mac launchApplication cannot accept any arguments.
    return MacProcess::OpenApplication(path);
  }
#endif

#ifdef OS_LINUX
  // Do not call posix_spawn() for obviously bad path.
  if (!S_ISREG(statbuf.st_mode)) {
    LOG(ERROR) << "Not a regular file: " << path;
    return false;
  }
  if ((statbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0) {
    LOG(ERROR) << "Not a executable file: " << path;
    return false;
  }

  // We don't want to execute setuid/gid binaries for security reasons.
  if ((statbuf.st_mode & (S_ISUID | S_ISGID)) != 0) {
    LOG(ERROR) << "Can't execute setuid or setgid binaries: " << path;
    return false;
  }

  // Use simple heap corruption checker in child processes. This setenv() does
  // not affect current process behavior since Glibc never check the variable
  // after main().
  // (www.gnu.org/software/libc/manual/html_node/Heap-Consistency-Checking.html)
  const int kOverwrite = 0;  // Do not overwrite.
  ::setenv("MALLOC_CHECK_", "2", kOverwrite);
#endif  // OS_LINUX
  pid_t tmp_pid = 0;

  // Spawn new process.
  // NOTE:
  // posix_spawn returns 0 even if kMozcServer doesn't exist, because
  // the return value of posix_spawn is basically determined
  // by the return value of fork().
  const int result = ::posix_spawn(
      &tmp_pid, path.c_str(), NULL, NULL, const_cast<char* const*>(argv.get()),
      environ);
  if (result == 0) {
    VLOG(1) << "posix_spawn: child pid is " << tmp_pid;
  } else {
    LOG(ERROR) << "posix_spawn failed: " << strerror(result);
  }

  if (pid != NULL) {
    *pid = tmp_pid;
  }
  return result == 0;
#endif  // OS_WIN
}

bool Process::SpawnMozcProcess(
    const string &filename, const string &arg, size_t *pid) {
  return Process::SpawnProcess(
      FileUtil::JoinPath(SystemUtil::GetServerDirectory(), filename),
      arg, pid);
}

bool Process::WaitProcess(size_t pid, int timeout) {
  if (pid == 0) {
    LOG(WARNING) << "pid is 0. ignored";
    return true;
  }

  if (timeout == 0) {
    LOG(ERROR) << "timeout is 0";
    return false;
  }

#ifdef OS_WIN
  DWORD processe_id = static_cast<DWORD>(pid);
  ScopedHandle handle(::OpenProcess(SYNCHRONIZE, FALSE, processe_id));
  if (handle.get() == NULL) {
    LOG(ERROR) << "Cannot get handle id";
    return true;
  }

  if (timeout < 0) {
    timeout = INFINITE;
  }

  const DWORD result = ::WaitForSingleObject(handle.get(), timeout);
  if (result == WAIT_TIMEOUT) {
    LOG(ERROR) << pid << " didn't terminate within " << timeout << " msec";
    return false;
  }

  return true;
#else
  pid_t processe_id = static_cast<pid_t>(pid);
  const int kPollingDuration = 250;
  int left_time = timeout < 0 ? 1 : timeout;
  while (left_time > 0) {
    Util::Sleep(kPollingDuration);
    if (::kill(processe_id, 0) != 0) {
      if (errno == EPERM) {
        return false;   // access defined
      }
      return true;   // process not found
    }
    if (timeout > 0) {
      left_time -= kPollingDuration;
    }
  }

  LOG(ERROR) << pid << " didn't terminate within " << timeout << " msec";
  return false;
#endif
}

bool Process::IsProcessAlive(size_t pid, bool default_result) {
  if (pid == 0) {
    return default_result;
  }

#ifdef OS_WIN
  {
    ScopedHandle handle(::OpenProcess(SYNCHRONIZE, FALSE,
                                      static_cast<DWORD>(pid)));
    if (NULL == handle.get()) {
      const DWORD error = ::GetLastError();
      if (error == ERROR_ACCESS_DENIED) {
        LOG(ERROR) << "OpenProcess failed: " << error;
        return default_result;  // unknown
      }
      return false;  // not running
    }

    if (WAIT_TIMEOUT == ::WaitForSingleObject(handle.get(), 0)) {
      return true;  // running
    }
  }  // release handle

  return default_result;  // unknown

#else  // OS_WIN
  const int kSig = 0;
  if (::kill(static_cast<pid_t>(pid), kSig) == -1) {
    if (errno == EPERM || errno == EINVAL) {
      // permission denied or invalid signal.
      return default_result;
    }
    return false;
  }

  return true;
#endif  // OS_WIN
}

bool Process::IsThreadAlive(size_t thread_id, bool default_result) {
#ifdef OS_WIN
  if (thread_id == 0) {
    return default_result;
  }

  {
    ScopedHandle handle(::OpenThread(SYNCHRONIZE, FALSE,
                                     static_cast<DWORD>(thread_id)));
    if (NULL == handle.get()) {
      const DWORD error = ::GetLastError();
      if (error == ERROR_ACCESS_DENIED) {
        LOG(ERROR) << "OpenThread failed: " << error;
        return default_result;  // unknown
      }
      return false;  // not running
    }

    if (WAIT_TIMEOUT == ::WaitForSingleObject(handle.get(), 0)) {
      return true;  // running
    }
  }  // release handle

  return default_result;  // unknown

#else   // OS_WIN
  // On Linux/Mac, there is no way to check the status of thread id from
  // other process.
  return default_result;
#endif  // OS_WNIDOWS
}

bool Process::LaunchErrorMessageDialog(const string &error_type) {
#ifdef OS_MACOSX
  if (!MacProcess::LaunchErrorMessageDialog(error_type)) {
    LOG(ERROR) << "cannot error message dialog";
    return false;
  }
#endif  // OS_MACOSX

#ifdef OS_WIN
  const string arg = "--mode=error_message_dialog --error_type=" + error_type;
  size_t pid = 0;
  if (!Process::SpawnProcess(SystemUtil::GetToolPath(), arg, &pid)) {
    LOG(ERROR) << "cannot launch " << kMozcTool;
    return false;
  }
#endif  // OS_WIN

#ifdef OS_LINUX
  const char kMozcTool[] = "mozc_tool";
  const string arg = "--mode=error_message_dialog --error_type=" + error_type;
  size_t pid = 0;
  if (!Process::SpawnProcess(SystemUtil::GetToolPath(), arg, &pid)) {
    LOG(ERROR) << "cannot launch " << kMozcTool;
    return false;
  }
#endif  // OS_LINUX

  return true;
}
}  // namespace mozc
