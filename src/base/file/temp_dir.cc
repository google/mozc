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

#include "base/file/temp_dir.h"

#include <cerrno>
#include <optional>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/environ.h"
#include "base/file/recursive.h"
#include "base/file_util.h"

#ifdef _WIN32
#include <windows.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/win32/wide_char.h"
#else  // _WIN32
#include <unistd.h>
#endif  // _WIN32

namespace mozc {
namespace {

bool TryTempDirectory(const std::string &dir) {
  return FileUtil::DirectoryExists(dir).ok();
}

std::optional<std::string> TryTempEnv(const char *envname) {
  std::string env = Environ::GetEnv(envname);
  if (env.empty()) {
    return std::nullopt;
  }
  if (TryTempDirectory(env)) {
    return env;
  }
  return std::nullopt;
}

}  // namespace

TempFile::~TempFile() {
  if (keep_) {
    return;
  }
  const absl::Status s = FileUtil::UnlinkIfExists(path_);
  if (!s.ok()) {
    LOG(WARNING) << "Failed to remove temporary file (" << path_ << ") :" << s;
  }
}

namespace {
#ifdef _WIN32

DWORD GetTempPath(const absl::Span<wchar_t> buf) {
  // Use GetTempPath2 if it's available (Windows 10 Build 20348 or later).
  // DWORD GetTempPath2W([in]  DWORD  BufferLength, [out] LPWSTR Buffer));
  using Func = DWORD(WINAPI *)(DWORD, LPWSTR);
  Func get_temp_path_2_w = reinterpret_cast<Func>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTempPath2W"));
  if (get_temp_path_2_w) {
    return get_temp_path_2_w(buf.size(), buf.data());
  }
  return ::GetTempPathW(buf.size(), buf.data());
}

UINT GetTempFileNameW(const absl::string_view path, const wchar_t *prefix,
                      wchar_t *output) {
  return ::GetTempFileNameW(win32::Utf8ToWide(path).c_str(), prefix, 0, output);
}

#endif  // _WIN32
}  // namespace

TempDirectory::~TempDirectory() {
  if (keep_) {
    return;
  }
  const absl::Status s = file::DeleteRecursively(path_);
  if (!s.ok()) {
    LOG(WARNING) << "Failed to remove temporary directory (" << path_
                 << ") :" << s;
  }
}

TempDirectory TempDirectory::Default() {
  // Try TEST_TMPDIR first. This is set by Blaze.
  if (std::optional<std::string> tmpdir = TryTempEnv("TEST_TMPDIR"); tmpdir) {
    return TempDirectory(*std::move(tmpdir));
  }

#ifdef _WIN32

  // On Win32, GetTempPath/2 is the recommended method.
  wchar_t wtmp[MAX_PATH + 1];
  if (GetTempPath(absl::MakeSpan(wtmp)) != 0) {
    std::string tmp = win32::WideToUtf8(wtmp);
    if (TryTempDirectory(tmp)) {
      return TempDirectory(std::move(tmp));
    }
  }

#else  // _WIN32

  if (std::optional<std::string> tmpdir = TryTempEnv("TMPDIR"); tmpdir) {
    return TempDirectory(*std::move(tmpdir));
  }
  if (TryTempDirectory("/tmp")) {
    return TempDirectory("/tmp");
  }

#ifdef __ANDROID__
  // TODO(b/269706946): Android doesn't have /tmp. gtest seems to use /sdcard
  // instead, but ideally it should be obtained by getCacheDir().
  // https://developer.android.com/reference/android/content/ContextWrapper.html#getCacheDir()
  // TempDirectory is only used by native tools and unit tests, so we don't
  // need to worry about this for now.
  if (TryTempDirectory("/sdcard")) {
    return TempDirectory("/sdcard");
  }
#endif  // __ANDROID__

  // We ran out of options. Return the current directory as the best
  // alternative.
  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) != nullptr && TryTempDirectory(cwd)) {
    return TempDirectory(cwd);
  }

#endif  // !_WIN32

  return TempDirectory("");
}

absl::StatusOr<TempFile> TempDirectory::CreateTempFile() const {
#ifdef _WIN32
  // On Win32, GetTempFileNameW does what mkstemp does.
  // The SDK reference say the maximum possible length is MAX_PATH, which is
  // shorter than what's required for GetTempPath/2. Weird.
  wchar_t wtemp_file[MAX_PATH];
  // GetTempFileW accepts up to three prefix characters.
  if (GetTempFileNameW(path_, L"mzc", wtemp_file) == 0) {
    return absl::FailedPreconditionError(
        absl::StrCat("GetTempFileNameW failed (temp path too long), error = ",
                     GetLastError()));
  }
  return TempFile(win32::WideToUtf8(wtemp_file));
#else   // _WIN32
  std::string temp_file = FileUtil::JoinPath(path_, "mozc-XXXXXX");  // six X's
  int fd = mkstemp(temp_file.data());
  if (fd < 0) {
    return absl::ErrnoToStatus(errno, "mkstemp failed");
  }
  if (close(fd) != 0) {
    return absl::ErrnoToStatus(errno, "close failed");
  }
  return TempFile(std::move(temp_file));
#endif  // !_WIN32
}

absl::StatusOr<TempDirectory> TempDirectory::CreateTempDirectory() const {
#ifdef _WIN32
  // Win32 doesn't have a GetTempFileName for directory. We'll try it a few
  // times and return a successful one to avoid race.
  int retries = 3;
  while (retries-- > 0) {
    wchar_t new_dir_path[MAX_PATH];
    // Use a different prefix to avoid conflict.
    if (GetTempFileNameW(path_, L"mzd", new_dir_path) == 0) {
      // GetTempFileName only fails when the buffer is not long enough.
      return absl::FailedPreconditionError(
          absl::StrCat("GetTempFileNameW failed (temp path too long), error = ",
                       GetLastError()));
    }
    // Delete the file first.
    if (!DeleteFileW(new_dir_path)) {
      return absl::InternalError(absl::StrCat(
          "DeleteFileW failed inside CreateTempDirectory, error = ",
          GetLastError()));
    }
    // There's a chance someone creates a file or directory with the same
    // name. We'll retry in that case.
    if (CreateDirectoryW(new_dir_path, nullptr) != 0) {
      // Success.
      return TempDirectory(win32::WideToUtf8(new_dir_path), false);
    }
    const DWORD err = GetLastError();
    if (err != ERROR_ALREADY_EXISTS) {
      // The only possible error here is ERROR_PATH_NOT_FOUND.
      return absl::InternalError(absl::StrCat(
          "CreateDirectoryW failed inside CreateTempDirectory, error = ", err));
    }
  }
  return absl::UnavailableError(
      absl::StrCat("Can't create a temporary directory in ", path_));
#else   // _WIN32
  std::string new_dir = FileUtil::JoinPath(path_, "mozc-XXXXXX");  // six X's
  if (mkdtemp(new_dir.data()) == nullptr) {
    return absl::ErrnoToStatus(errno, "mkdtemp failed");
  }
  return TempDirectory(std::move(new_dir), false);
#endif  // !_WIN32
}

}  // namespace mozc
