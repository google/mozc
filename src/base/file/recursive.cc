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
#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"

#ifdef _WIN32
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <windows.h>

#include "base/win32/scoped_com.h"
#include "base/win32/wide_char.h"
#else  // _WIN32
#include <fts.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/file_util.h"
#endif  // !_WIN32

#ifdef __APPLE__
#include <TargetConditionals.h>  // for TARGET_OS_OSX
#endif                           // __APPLE__

#undef StrCat

namespace mozc::file {

#ifdef _WIN32

namespace {

using ATL::CComPtr;

absl::StatusCode HResultToStatusCode(const HRESULT hr) {
  switch (hr) {
    case S_OK:
      return absl::StatusCode::kOk;
    case E_ABORT:
      return absl::StatusCode::kAborted;
    case E_ACCESSDENIED:
      return absl::StatusCode::kPermissionDenied;
    case E_OUTOFMEMORY:
      return absl::StatusCode::kResourceExhausted;
    default:
      if (HRESULT_CODE(hr) == ERROR_ACCESS_DENIED) {
        return absl::StatusCode::kPermissionDenied;
      }
      return absl::StatusCode::kInternal;
  }
}

absl::Status ComError(absl::string_view op, HRESULT hr) {
  return absl::Status(HResultToStatusCode(hr),
                      absl::StrCat(op, "failed: code = ", hr));
}

}  // namespace

absl::Status DeleteRecursively(const absl::string_view path) {
  ScopedCOMInitializer com_initializer;

  CComPtr<IShellItem> shell_item;
  if (HRESULT hr = SHCreateItemFromParsingName(
          win32::Utf8ToWide(path).c_str(), nullptr, IID_PPV_ARGS(&shell_item));
      FAILED(hr)) {
    if (HRESULT_CODE(hr) == ERROR_FILE_NOT_FOUND ||
        HRESULT_CODE(hr) == ERROR_PATH_NOT_FOUND) {
      // Return Ok if file is not found. Same behavior as the POSIX version.
      return absl::OkStatus();
    }
    return ComError("SHCreateItemFromParsingName", hr);
  }

  CComPtr<IFileOperation> file_operation;
  if (HRESULT hr = file_operation.CoCreateInstance(CLSID_FileOperation);
      FAILED(hr)) {
    return ComError("IFileOperation.CoCreateInstance", hr);
  }
  if (HRESULT hr = file_operation->SetOperationFlags(
          FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT);
      FAILED(hr)) {
    return ComError("IFileOperation.SetOperationFlags", hr);
  }
  if (HRESULT hr = file_operation->DeleteItem(shell_item, nullptr);
      FAILED(hr)) {
    return ComError("IFileOperation.DeleteItem", hr);
  }
  if (HRESULT hr = file_operation->PerformOperations(); FAILED(hr)) {
    return ComError("IFileOperation.PerformOperations", hr);
  }

  return absl::OkStatus();
}

#elif defined(__linux__) || (defined(TARGET_OS_OSX) && TARGET_OS_OSX)  // _WIN32

absl::Status DeleteRecursively(const absl::string_view path) {
  // fts is not POSIX, but it's available on both Linux and MacOS.
  std::string path_str(path);
  char *files[] = {path_str.data(), nullptr};
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
    absl::Status s;
    switch (ent->fts_info) {
      case FTS_DP:  // directory postorder
        s = FileUtil::RemoveDirectory(ent->fts_path);
        if (!s.ok() && !absl::IsNotFound(s)) {
          LOG(ERROR) << "Cannot remove directory " << ent->fts_path << ":" << s;
        }
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
        s = FileUtil::Unlink(ent->fts_path);
        if (!s.ok() && !absl::IsNotFound(s)) {
          LOG(ERROR) << "Cannot unlink " << ent->fts_path << ":" << s;
        }
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

#else  // __linux__ || TARGET_OS_OSX

#error "Recursive is not available on this platform."

#endif  // !(_WIN32 || __linux__ || TARGET_OS_OSX)

}  // namespace mozc::file
