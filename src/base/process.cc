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

#include "base/process.h"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "base/strings/zstring_view.h"
#include "base/system_util.h"
#include "base/vlog.h"

#ifdef _WIN32
#include <wil/resource.h>
#include <windows.h>

#include "base/const.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_util.h"
#else  // _WIN32
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>  // for posix_spawn().
#include <string.h>
#include <sys/stat.h>

#include <cerrno>
#endif  // !_WIN32

#ifdef __linux
#include <sys/types.h>
#endif  // __linux__

#ifdef __APPLE__
#include <crt_externs.h>

#include "base/mac/mac_process.h"
#endif  // __APPLE__

#ifdef __APPLE__
// We do not use the global environ variable because it is unavailable
// in Mac Framework/dynamic libraries.  Instead call _NSGetEnviron().
// See the "PROGRAMMING" section of http://goo.gl/4Hq0D for the
// detailed information.
static char **environ = *_NSGetEnviron();
#elif !defined(_WIN32)
// Defined somewhere in libc. We can't pass nullptr as the 6th argument of
// posix_spawn() since Qt applications use (at least) DISPLAY and QT_IM_MODULE
// environment variables.
extern char **environ;
#endif  // !__APPLE__ && !_WIN32

namespace mozc {

bool Process::OpenBrowser(zstring_view url) {
  // url must start with http:// or https:// or file://
  if (!absl::StartsWith(url, "http://") && !absl::StartsWith(url, "https://") &&
      !absl::StartsWith(url, "file://")) {
    return false;
  }

#ifdef _WIN32
  return WinUtil::ShellExecuteInSystemDir(
      L"open", win32::Utf8ToWide(url).c_str(), nullptr);
#endif  // _WIN32

#ifdef __linux__

#ifndef MOZC_BROWSER_COMMAND
  // xdg-open which uses kfmclient or gnome-open internally works both on KDE
  // and GNOME environments.
#define MOZC_BROWSER_COMMAND "/usr/bin/xdg-open"
#endif  // MOZC_BROWSER_COMMAND

  return SpawnProcess(MOZC_BROWSER_COMMAND, url);
#endif  // __linux__

#ifdef __APPLE__
  return MacProcess::OpenBrowserForMac(url);
#endif  // __APPLE__
  return false;
}

bool Process::SpawnProcess(zstring_view path, zstring_view arg, size_t *pid) {
#ifdef _WIN32
  std::wstring wpath = win32::Utf8ToWide(path);
  wpath = L"\"" + wpath + L"\"";
  if (!arg.empty()) {
    std::wstring warg = win32::Utf8ToWide(arg);
    wpath += L" ";
    wpath += warg;
  }

  // The |lpCommandLine| parameter of CreateProcessW should be writable
  // so that we create a std::unique_ptr<wchar_t[]> here.
  std::unique_ptr<wchar_t[]> wpath2(new wchar_t[wpath.size() + 1]);
  if (0 != wcscpy_s(wpath2.get(), wpath.size() + 1, wpath.c_str())) {
    return false;
  }

  STARTUPINFOW si = {0};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_FORCEOFFFEEDBACK;
  PROCESS_INFORMATION pi = {0};

  // If both |lpApplicationName| and |lpCommandLine| are non-nullptr,
  // the argument array of the process will be shifted.
  // http://support.microsoft.com/kb/175986
  const bool create_process_succeeded =
      ::CreateProcessW(
          nullptr, wpath2.get(), nullptr, nullptr, FALSE,
          CREATE_DEFAULT_ERROR_MODE, nullptr,
          // NOTE: Working directory will be locked by the system.
          // We use system directory to spawn process so that users will not
          // to be aware of any undeletable directory. (http://b/2017482)
          SystemUtil::GetSystemDir(), &si, &pi) != FALSE;

  if (create_process_succeeded) {
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
  } else {
    LOG(ERROR) << "Create process failed: " << ::GetLastError();
  }

  return create_process_succeeded;
#elif defined(__wasm__) || defined(__ANDROID__)
  // Spawning processes is not supported in WASM or Android.
  return false;
#else  // __wasm__ || __ANDROID__

  const std::vector<std::string> arg_tmp =
      absl::StrSplit(arg.view(), ' ', absl::SkipEmpty());
  auto argv = std::make_unique<const char *[]>(arg_tmp.size() + 2);
  argv[0] = path.c_str();
  for (size_t i = 0; i < arg_tmp.size(); ++i) {
    argv[i + 1] = arg_tmp[i].c_str();
  }
  argv[arg_tmp.size() + 1] = nullptr;

  struct stat statbuf;
  if (::stat(path.c_str(), &statbuf) != 0) {
    LOG(ERROR) << "Can't stat " << path << ": " << strerror(errno);
    return false;
  }
#ifdef __APPLE__
  // Check if the "path" is an application or not.  If it's an
  // application, do a special process using mac_process.mm.
  if (S_ISDIR(statbuf.st_mode) && path.size() > 4 &&
      absl::EndsWith(path, ".app")) {
    // In mac launchApplication cannot accept any arguments.
    return MacProcess::OpenApplication(path);
  }
#endif  // __APPLE__

#ifdef __linux__
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
  constexpr int kOverwrite = 0;  // Do not overwrite.
  ::setenv("MALLOC_CHECK_", "2", kOverwrite);
#endif  // __linux__
  pid_t tmp_pid = 0;

  // Spawn new process.
  // NOTE:
  // posix_spawn returns 0 even if kMozcServer doesn't exist, because
  // the return value of posix_spawn is basically determined
  // by the return value of fork().
  const int result =
      ::posix_spawn(&tmp_pid, path.c_str(), nullptr, nullptr,
                    const_cast<char *const *>(argv.get()), environ);
  if (result == 0) {
    MOZC_VLOG(1) << "posix_spawn: child pid is " << tmp_pid;
  } else {
    LOG(ERROR) << "posix_spawn failed: " << strerror(result);
  }

  if (pid != nullptr) {
    *pid = tmp_pid;
  }
  return result == 0;
#endif  // !_WIN32
}

bool Process::SpawnMozcProcess(zstring_view filename, zstring_view arg,
                               size_t *pid) {
  return Process::SpawnProcess(
      FileUtil::JoinPath(SystemUtil::GetServerDirectory(), filename), arg, pid);
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

#ifdef _WIN32
  DWORD processe_id = static_cast<DWORD>(pid);
  wil::unique_process_handle handle(
      ::OpenProcess(SYNCHRONIZE, FALSE, processe_id));
  if (!handle) {
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
#elif defined(__wasm__)
  // Process handling is not supported in WASM.
  return false;
#else   // __wasm__
  pid_t processe_id = static_cast<pid_t>(pid);
  constexpr int kPollingDuration = 250;
  int left_time = timeout < 0 ? 1 : timeout;
  while (left_time > 0) {
    absl::SleepFor(absl::Milliseconds(kPollingDuration));
    if (::kill(processe_id, 0) != 0) {
      if (errno == EPERM) {
        return false;  // access defined
      }
      return true;  // process not found
    }
    if (timeout > 0) {
      left_time -= kPollingDuration;
    }
  }

  LOG(ERROR) << pid << " didn't terminate within " << timeout << " msec";
  return false;
#endif  // __wasm__
}

bool Process::IsProcessAlive(size_t pid, bool default_result) {
  if (pid == 0) {
    return default_result;
  }

#ifdef _WIN32
  {
    wil::unique_process_handle handle(
        ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid)));
    if (!handle) {
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
#elif defined(__wasm__)
  // Process handling is not supported in WASM.
  return false;
#else   // _WIN32
  constexpr int kSig = 0;
  if (::kill(static_cast<pid_t>(pid), kSig) == -1) {
    if (errno == EPERM || errno == EINVAL) {
      // permission denied or invalid signal.
      return default_result;
    }
    return false;
  }

  return true;
#endif  // _WIN32
}

bool Process::IsThreadAlive(size_t thread_id, bool default_result) {
#ifdef _WIN32
  if (thread_id == 0) {
    return default_result;
  }

  {
    wil::unique_process_handle handle(
        ::OpenThread(SYNCHRONIZE, FALSE, static_cast<DWORD>(thread_id)));
    if (!handle) {
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

#else   // _WIN32
  // On Linux/Mac, there is no way to check the status of thread id from
  // other process.
  return default_result;
#endif  // _WIN32
}

bool Process::LaunchErrorMessageDialog(zstring_view error_type) {
#ifdef __APPLE__
  if (!MacProcess::LaunchErrorMessageDialog(error_type)) {
    LOG(ERROR) << "cannot error message dialog";
    return false;
  }
#endif  // __APPLE__

#ifdef _WIN32
  const std::string arg =
      absl::StrCat("--mode=error_message_dialog --error_type=", error_type);
  size_t pid = 0;
  if (!Process::SpawnProcess(SystemUtil::GetToolPath(), arg, &pid)) {
    LOG(ERROR) << "cannot launch " << kMozcTool;
    return false;
  }
#endif  // _WIN32

#if defined(__linux__) && !defined(__ANDROID__)
  constexpr char kMozcTool[] = "mozc_tool";
  const std::string arg =
      absl::StrCat("--mode=error_message_dialog --error_type=", error_type);
  size_t pid = 0;
  if (!Process::SpawnProcess(SystemUtil::GetToolPath(), arg, &pid)) {
    LOG(ERROR) << "cannot launch " << kMozcTool;
    return false;
  }
#endif  // __linux__ && !__ANDROID__

  return true;
}
}  // namespace mozc
