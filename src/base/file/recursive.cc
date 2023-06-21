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

#include "base/file/recursive.h"

#include <string>

#include "base/logging.h"
#include "base/strings/zstring_view.h"
#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"

#ifdef _WIN32
#include <wil/filesystem.h>
#include <windows.h>

#include "base/win32/hresult.h"
#include "base/win32/wide_char.h"
#include "absl/strings/str_cat.h"
#else  // _WIN32
#include <fts.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>

#include "base/file_util.h"
#include "absl/base/attributes.h"
#endif  // !_WIN32

#ifdef __APPLE__
#include <TargetConditionals.h>  // for TARGET_OS_OSX
#endif                           // __APPLE__

#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#include <ftw.h>
#endif  // __ANDROID__ || TARGET_OS_IPHONE

#undef StrCat

namespace mozc::file {

#ifdef _WIN32

absl::Status DeleteRecursively(const zstring_view path) {
  const win32::HResult hr(wil::RemoveDirectoryRecursiveNoThrow(
      win32::Utf8ToWide(path).c_str(),
      wil::RemoveDirectoryOptions::RemoveReadOnly));
  if (hr.Succeeded()) {
    return absl::OkStatus();
  }
  if (HRESULT_CODE(hr) == ERROR_FILE_NOT_FOUND ||
      HRESULT_CODE(hr) == ERROR_PATH_NOT_FOUND) {
    // Return Ok if file is not found. Same behavior as the POSIX version.
    return absl::OkStatus();
  }
  if (HRESULT_CODE(hr) == ERROR_ACCESS_DENIED) {
    return absl::PermissionDeniedError(hr.ToString());
  }
  return absl::UnknownError(hr.ToString());
}

#else  // _WIN32

namespace {
// TODO(b/271087668): msan false positive in fts functions.
ABSL_ATTRIBUTE_NO_SANITIZE_MEMORY void RemoveDirectoryOrLog(const char *path) {
  const absl::Status s = FileUtil::RemoveDirectory(path);
  if (!s.ok() && !absl::IsNotFound(s)) {
    LOG(ERROR) << "Cannot remove directory " << path << ":" << s;
  }
}

// TODO(b/271087668): msan false positive in fts functions.
ABSL_ATTRIBUTE_NO_SANITIZE_MEMORY void UnlinkFileOrLog(const char *path) {
  const absl::Status s = FileUtil::Unlink(path);
  if (!s.ok() && !absl::IsNotFound(s)) {
    LOG(ERROR) << "Cannot unlink " << path << ":" << s;
  }
}
}  // namespace

#if (defined(__linux__) && !defined(__ANDROID__)) || \
    (defined(TARGET_OS_OSX) && TARGET_OS_OSX)

absl::Status DeleteRecursively(const zstring_view path) {
  // fts is not POSIX, but it's available on both Linux and MacOS.
  std::string path_str =
      to_string(path);  // fts_open takes a non-const pointer.
  char *const files[] = {path_str.data(), nullptr};
  // - FTS_NOCHDIR: don't change current directory.
  // - FTS_NOSTAT: don't stat(2).
  // - FTS_PHYSICAL: don't follow symlinks.
  // - FTS_XDEV: don't descend into a different device.
  FTS *ftsp = fts_open(
      files, FTS_NOCHDIR | FTS_NOSTAT | FTS_PHYSICAL | FTS_XDEV, nullptr);
  if (ftsp == nullptr) {
    return absl::ErrnoToStatus(errno, "fts_open failed");
  }
  absl::Cleanup fts_closer = [ftsp]() {
    if (fts_close(ftsp) < 0) {
      LOG(ERROR) << "fts_close failed : errno = " << errno;
    }
  };

  while (FTSENT *ent = fts_read(ftsp)) {
    switch (ent->fts_info) {
      case FTS_DP:  // directory postorder
        RemoveDirectoryOrLog(ent->fts_path);
        break;
      case FTS_F:  // regular file
        [[fallthrough]];
      case FTS_NSOK:  // no stat requested (FTS_NOSTAT)
        [[fallthrough]];
      case FTS_SL:  // symlink
        [[fallthrough]];
      case FTS_SLNONE:  // broken symlink
        [[fallthrough]];
      case FTS_DEFAULT:  // others
        UnlinkFileOrLog(ent->fts_path);
        break;
      default:
        break;
    }
  }
  if (errno) {
    return absl::ErrnoToStatus(errno, "fts_read failed");
  }
  return absl::OkStatus();
}

#else  // (__linux__ && !__ANDROID__) || TARGET_OS_OSX

namespace {
int FtwDelete(const char *path, const struct stat *sb, int typeflag,
              struct FTW *ftwbuf) {
  switch (typeflag) {
    case FTW_DP:  // directory postorder
      [[fallthrough]];
    case FTW_DNR:  // directory which can't be read. will try anyways.
      RemoveDirectoryOrLog(path);
      break;
    case FTW_F:  // regular file
      [[fallthrough]];
    case FTW_NS:  // stat failure (no execute permission)
      [[fallthrough]];
    case FTW_SL:  // symlink
      [[fallthrough]];
    case FTW_SLN:  // broken symlink
      UnlinkFileOrLog(path);
      break;
    default:
      break;
  }
  return 0;
}
}  // namespace

absl::Status DeleteRecursively(const zstring_view path) {
  // ntfw() is a POSIX interface but it's not recommended on (at least) macOS.
  // This is a fallback implementation.
  constexpr int kOpenFdLimit = 100;
  if (nftw(path.c_str(), FtwDelete, kOpenFdLimit,
           FTW_DEPTH | FTW_PHYS | FTW_MOUNT) < 0) {
    return absl::ErrnoToStatus(errno, "nftw failed");
  }
  return absl::OkStatus();
}

#endif  // !(__linux__ && !__ANDROID__) || TARGET_OS_OSX)
#endif  // !_WIN32

}  // namespace mozc::file
