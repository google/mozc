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

#include "base/process_mutex.h"

#include <cstdio>
#include <string>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/file_util.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/vlog.h"

#ifdef _WIN32
#include <wil/resource.h>
#include <windows.h>

#include "base/win32/wide_char.h"
#include "base/win32/win_sandbox.h"
#else  // _WIN32
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "absl/container/flat_hash_map.h"
#include "base/strings/zstring_view.h"
#endif  // !_WIN32

namespace mozc {
namespace {

std::string CreateProcessMutexFileName(const absl::string_view name) {
  absl::string_view hidden_prefix = TargetIsWindows() ? "" : ".";
  std::string basename = absl::StrCat(hidden_prefix, name, ".lock");
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), basename);
}
}  // namespace

ProcessMutex::ProcessMutex(absl::string_view name)
    : filename_(CreateProcessMutexFileName(name)) {}

ProcessMutex::~ProcessMutex() { UnLock(); }

bool ProcessMutex::Lock() { return LockAndWrite(""); }

bool ProcessMutex::LockAndWrite(absl::string_view message) {
  absl::MutexLock l(&mutex_);
  if (locked_) {
    MOZC_VLOG(1) << filename_ << " is already locked";
    return false;
  }
  locked_ = LockAndWriteInternal(message);
  return locked_;
}

// always return true at this moment.
bool ProcessMutex::UnLock() {
  absl::MutexLock l(&mutex_);
  if (!locked_) {
    MOZC_VLOG(1) << filename_ << " is not locked";
    return true;
  }
  UnLockInternal();
  locked_ = false;
  return true;
}

#ifdef _WIN32

bool ProcessMutex::LockAndWriteInternal(const absl::string_view message) {
  const std::wstring wfilename = win32::Utf8ToWide(filename_);
  constexpr DWORD kAttribute =
      FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_TEMPORARY |
      FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | FILE_FLAG_DELETE_ON_CLOSE;

  SECURITY_ATTRIBUTES serucity_attributes = {};
  if (!WinSandbox::MakeSecurityAttributes(WinSandbox::kSharableFileForRead,
                                          &serucity_attributes)) {
    return false;
  }
  handle_.reset(::CreateFileW(wfilename.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              &serucity_attributes, CREATE_ALWAYS, kAttribute,
                              nullptr));
  ::LocalFree(serucity_attributes.lpSecurityDescriptor);

  if (!handle_.is_valid()) {
    MOZC_VLOG(1) << "already locked";
    return false;
  }

  if (!message.empty()) {
    DWORD size = 0;
    if (!::WriteFile(handle_.get(), message.data(), message.size(), &size,
                     nullptr)) {
      const int last_error = ::GetLastError();
      LOG(ERROR) << "Cannot write message: " << message
                 << ", last_error:" << last_error;
      UnLockInternal();
      return false;
    }
  }

  return true;
}

void ProcessMutex::UnLockInternal() {
  handle_.reset();
  FileUtil::UnlinkOrLogError(filename_);
}

#else   // !_WIN32

namespace {
// Special workaround for the bad treatment of fcntl.
// Basically, fcntl provides "per-process" file-locking.
// When a process has multiple fds on the same file,
// file lock is released if one fd is closed. This is not
// an expected behavior for Mozc.
//
// Linux man says
// "As well as being removed by an explicit F_UNLCK, record locks are
// automatically released when the process terminates or
// if it closes any file descriptor referring to a file on which
// locks are held.  This is bad: it means that a process can lose
// the locks on a file like /etc/passwd or /etc/mtab when
// for some reason a library function decides to open,
// read and close it."
//
// FileLockerManager a wrapper class providing per-filename
// file locking implemented upon fcntl. Multiple threads on
//  the same process share one fd for file locking.
//
// We will be able to use flock() instead of fcntl since,
// flock() provides "per-fd" file locking. However, we don't
// use it because flock() doesn't work on NFS.
class FileLockManager {
 public:
  absl::StatusOr<int> Lock(const zstring_view filename) {
    absl::MutexLock l(&mutex_);

    if (filename.empty()) {
      return absl::InvalidArgumentError("filename is empty");
    }

    if (fdmap_.contains(filename)) {
      MOZC_VLOG(1) << filename << " is already locked by the same process";
      return absl::FailedPreconditionError("already locked");
    }

    chmod(filename.c_str(), 0600);  // write temporary
    const int fd = ::open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
      return absl::ErrnoToStatus(errno, "open() failed");
    }

    struct flock command;
    command.l_type = F_WRLCK;
    command.l_whence = SEEK_SET;
    command.l_start = 0;
    command.l_len = 0;

    const int result = ::fcntl(fd, F_SETLK, &command);
    if (result < 0) {  // failed
      ::close(fd);
      return absl::FailedPreconditionError(
          "Already locked. Another server is already running?");
    }

    fdmap_.emplace(filename.view(), fd);
    return fd;
  }

  absl::Status UnLock(const std::string &filename) {
    absl::MutexLock l(&mutex_);
    auto node = fdmap_.extract(filename);
    if (node.empty()) {
      return absl::FailedPreconditionError(
          absl::StrCat(filename, " is not locked"));
    }
    ::close(node.mapped());
    FileUtil::UnlinkOrLogError(filename);
    return absl::OkStatus();
  }

  FileLockManager() = default;

  ~FileLockManager() {
    for (const auto &[filename, fd] : fdmap_) {
      ::close(fd);
    }
  }

 private:
  absl::Mutex mutex_;
  absl::flat_hash_map<std::string, int> fdmap_;
};

}  // namespace

bool ProcessMutex::LockAndWriteInternal(const absl::string_view message) {
  absl::StatusOr<int> status_or_fd =
      Singleton<FileLockManager>::get()->Lock(filename_);
  if (!status_or_fd.ok()) {
    LOG(ERROR) << status_or_fd.status();
    return false;
  }
  if (!message.empty()) {
    if (write(*status_or_fd, message.data(), message.size()) !=
        message.size()) {
      LOG(ERROR) << "Cannot write message: " << message;
      UnLockInternal();
      return false;
    }
  }

  // changed it to read only mode.
  chmod(filename_.c_str(), 0400);
  return true;
}

void ProcessMutex::UnLockInternal() {
  if (absl::Status status =
          Singleton<FileLockManager>::get()->UnLock(filename_);
      !status.ok()) {
    LOG(WARNING) << status;
  }
}
#endif  // !_WIN32
}  // namespace mozc
