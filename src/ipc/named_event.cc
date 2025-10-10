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

#include "ipc/named_event.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/const.h"
#include "base/hash.h"
#include "base/system_util.h"
#include "base/vlog.h"

#ifdef _WIN32
#include <sddl.h>
#include <wil/resource.h>
#include <windows.h>

#include "base/win32/wide_char.h"
#include "base/win32/win_sandbox.h"
#else  // _WIN32
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#endif  // _WIN32

namespace mozc {
namespace {

#ifndef _WIN32
const pid_t kInvalidPid = 1;  // We can safely use 1 as 1 is reserved for init.

// Returns true if the process is alive.
bool IsProcessAlive(pid_t pid) {
  if (pid == kInvalidPid) {
    return true;  // return dummy value.
  }
  constexpr int kSig = 0;
  // As the signal number is 0, no signal is sent, but error checking is
  // still performed.
  return ::kill(pid, kSig) == 0;
}
#endif  // !_WIN32
}  // namespace

std::string NamedEventUtil::GetEventPath(const char* name) {
  name = (name == nullptr) ? "nullptr" : name;
  std::string event_name = absl::StrCat(
      kEventPathPrefix, SystemUtil::GetUserSidAsString(), ".", name);
#ifdef _WIN32
  return event_name;
#else   // _WIN32
  // To maximize mozc portability, (especially on BSD including OSX),
  // makes the length of path name shorter than 14byte.
  // Please see the following man page for detail:
  // http://www.freebsd.org/cgi/man.cgi?query=sem_open&manpath=FreeBSD+7.0-RELEASE
  // "This implementation places strict requirements on the value of name: it
  //  must begin with a slash (`/'), contain no other slash characters, and be
  //  equal to or less than 13 characters in length not including the
  //  terminating null character."
  constexpr size_t kEventPathLength = 13;
  std::string buf = absl::StrFormat("/%x", CityFingerprint(event_name));
  buf.erase(std::min(kEventPathLength, buf.size()));
  return buf;
#endif  // _WIN32
}

#ifdef _WIN32
NamedEventListener::NamedEventListener(const char* name)
    : is_owner_(false), handle_(nullptr) {
  std::wstring event_path =
      win32::Utf8ToWide(NamedEventUtil::GetEventPath(name));

  handle_ = ::OpenEventW(EVENT_ALL_ACCESS, false, event_path.c_str());

  if (handle_ == nullptr) {
    wil::unique_hlocal_security_descriptor security_descriptor =
        WinSandbox::MakeSecurityDescriptor(WinSandbox::kSharableEvent);
    if (!security_descriptor) {
      LOG(ERROR) << "Cannot make SecurityDescriptor";
      return;
    }
    SECURITY_ATTRIBUTES security_attributes = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = security_descriptor.get(),
        .bInheritHandle = FALSE,
    };
    handle_ =
        ::CreateEventW(&security_attributes, true, false, event_path.c_str());
    if (handle_ == nullptr) {
      LOG(ERROR) << "CreateEvent() failed: " << ::GetLastError();
      return;
    }

    is_owner_ = true;
  }

  MOZC_VLOG(1) << "NamedEventListener " << name << " is created";
}

NamedEventListener::~NamedEventListener() {
  if (nullptr != handle_) {
    ::CloseHandle(handle_);
  }
  handle_ = nullptr;
}

bool NamedEventListener::IsAvailable() const { return (handle_ != nullptr); }

bool NamedEventListener::IsOwner() const {
  return (IsAvailable() && is_owner_);
}

bool NamedEventListener::Wait(absl::Duration msec) {
  if (!IsAvailable()) {
    LOG(ERROR) << "NamedEventListener is not available";
    return false;
  }

  const DWORD result = ::WaitForSingleObject(
      handle_,
      msec < absl::ZeroDuration() ? INFINITE : absl::ToInt64Milliseconds(msec));
  if (result == WAIT_TIMEOUT) {
    LOG(WARNING) << "NamedEvent timeout " << ::GetLastError();
    return false;
  }

  return true;
}

int NamedEventListener::WaitEventOrProcess(absl::Duration msec, size_t pid) {
  if (!IsAvailable()) {
    return TIMEOUT;
  }

  HANDLE handle = ::OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
  if (nullptr == handle) {
    LOG(ERROR) << "OpenProcess: failed() " << ::GetLastError() << " " << pid;
    if (::GetLastError() == ERROR_INVALID_PARAMETER) {
      LOG(ERROR) << "No such process found: " << pid;
      return PROCESS_SIGNALED;
    }
  }

  HANDLE handles[2] = {handle_, handle};

  const DWORD handles_size = (handle == nullptr) ? 1 : 2;

  const DWORD ret = ::WaitForMultipleObjects(
      handles_size, handles, FALSE,
      msec < absl::ZeroDuration() ? INFINITE : absl::ToInt64Milliseconds(msec));
  int result = TIMEOUT;
  switch (ret) {
    case WAIT_OBJECT_0:
    case WAIT_ABANDONED_0:
      result = EVENT_SIGNALED;
      break;
    case WAIT_OBJECT_0 + 1:
    case WAIT_ABANDONED_0 + 1:
      result = PROCESS_SIGNALED;
      break;
    case WAIT_TIMEOUT:
      LOG(WARNING) << "NamedEvent timeout " << ::GetLastError();
      result = TIMEOUT;
      break;
    default:
      result = TIMEOUT;
      break;
  }

  if (nullptr != handle) {
    ::CloseHandle(handle);
  }

  return result;
}

NamedEventNotifier::NamedEventNotifier(const char* name) : handle_(nullptr) {
  std::wstring event_path =
      win32::Utf8ToWide(NamedEventUtil::GetEventPath(name));
  handle_ = ::OpenEventW(EVENT_MODIFY_STATE, false, event_path.c_str());
  if (handle_ == nullptr) {
    LOG(ERROR) << "Cannot open Event name: " << name;
    return;
  }
}

NamedEventNotifier::~NamedEventNotifier() {
  if (nullptr != handle_) {
    ::CloseHandle(handle_);
  }
  handle_ = nullptr;
}

bool NamedEventNotifier::IsAvailable() const { return handle_ != nullptr; }

bool NamedEventNotifier::Notify() {
  if (!IsAvailable()) {
    LOG(ERROR) << "NamedEventListener is not available";
    return false;
  }

  if (0 == ::SetEvent(handle_)) {
    LOG(ERROR) << "SetEvent() failed: " << ::GetLastError();
    return false;
  }

  return true;
}

#else   // _WIN32

NamedEventListener::NamedEventListener(const char* name)
    : is_owner_(false), sem_(SEM_FAILED) {
  key_filename_ = NamedEventUtil::GetEventPath(name);

  sem_ = ::sem_open(key_filename_.c_str(), O_CREAT | O_EXCL, 0600, 0);

  if (sem_ == SEM_FAILED && errno == EEXIST) {
    sem_ = ::sem_open(key_filename_.c_str(), O_CREAT, 0600, 0);
  } else {
    is_owner_ = true;
  }

  if (sem_ == SEM_FAILED) {
    LOG(ERROR) << "sem_open() failed " << key_filename_ << " "
               << ::strerror(errno);
    return;
  }

  MOZC_VLOG(1) << "NamedEventNotifier " << name << " is created";
}

NamedEventListener::~NamedEventListener() {
  if (IsAvailable()) {
    ::sem_close(sem_);
    ::sem_unlink(key_filename_.c_str());
  }
  sem_ = SEM_FAILED;
}

bool NamedEventListener::IsAvailable() const { return sem_ != SEM_FAILED; }

bool NamedEventListener::IsOwner() const {
  return (IsAvailable() && is_owner_);
}

bool NamedEventListener::Wait(absl::Duration msec) {
  return WaitEventOrProcess(msec, kInvalidPid /* don't check pid */) ==
         NamedEventListener::EVENT_SIGNALED;
}

int NamedEventListener::WaitEventOrProcess(absl::Duration msec, size_t pid) {
  if (!IsAvailable()) {
    return NamedEventListener::TIMEOUT;
  }

  const bool infinite = msec < absl::ZeroDuration() ? true : false;
  constexpr absl::Duration kWait = absl::Milliseconds(200);

  while (infinite || msec > absl::ZeroDuration()) {
    absl::SleepFor(kWait);

    if (!IsProcessAlive(pid)) {
      return NamedEventListener::PROCESS_SIGNALED;
    }

    if (-1 == ::sem_trywait(sem_)) {
      if (errno != EAGAIN) {
        LOG(ERROR) << "sem_trywait failed: " << ::strerror(errno);
        return EVENT_SIGNALED;
      }
    } else {
      // raise other events recursively.
      if (-1 == ::sem_post(sem_)) {
        LOG(ERROR) << "sem_post failed: " << ::strerror(errno);
      }
      return EVENT_SIGNALED;
    }

    msec -= kWait;
  }

  // timeout.
  return NamedEventListener::TIMEOUT;
}

NamedEventNotifier::NamedEventNotifier(const char* name) : sem_(SEM_FAILED) {
  const std::string key_filename = NamedEventUtil::GetEventPath(name);
  sem_ = ::sem_open(key_filename.c_str(), 0);
  if (sem_ == SEM_FAILED) {
    LOG(ERROR) << "sem_open(" << key_filename
               << ") failed: " << ::strerror(errno);
  }
}

NamedEventNotifier::~NamedEventNotifier() {
  if (IsAvailable()) {
    ::sem_close(sem_);
  }
  sem_ = SEM_FAILED;
}

bool NamedEventNotifier::IsAvailable() const { return sem_ != SEM_FAILED; }

bool NamedEventNotifier::Notify() {
  if (!IsAvailable()) {
    LOG(ERROR) << "NamedEventNotifier is not available";
    return false;
  }

  if (-1 == ::sem_post(sem_)) {
    LOG(ERROR) << "semop failed: " << ::strerror(errno);
    return false;
  }

  return true;
}
#endif  // _WIN32
}  // namespace mozc
