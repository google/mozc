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

#include "base/file_util.h"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
// Use <filesystem> for multi-platform compatibility.
#include <filesystem>  // NOLINT(build/c++17)
#include <ios>
#include <iterator>
#include <string>
// Use <system_error> for std::filesystem::equivalent.
#include <system_error>  // NOLINT(build/c++11)
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file_stream.h"
#include "base/mmap.h"
#include "base/port.h"
#include "base/strings/pfchar.h"

#ifdef _WIN32
#include <wil/resource.h>
#include <windows.h>

#include "base/util.h"
#include "base/win32/wide_char.h"

#else  // _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif  // _WIN32

namespace {

#ifdef _WIN32
constexpr char kFileDelimiter = '\\';
#else   // _WIN32
constexpr char kFileDelimiter = '/';
#endif  // _WIN32

}  // namespace

// Ad-hoc workaround against macro problem on Windows.
// On Windows, following macros, defined when you include <windows.h>,
// should be removed here because they affects the method name definition of
// Util class.
// TODO(yukawa): Use different method name if applicable.
#undef CreateDirectory
#undef RemoveDirectory
#undef CopyFile

namespace mozc {
namespace {

constinit static std::atomic<FileUtilInterface*> g_mock = nullptr;

}  // namespace

// Macro to reduce boilerplate for mock delegation.
// Each mockable method checks g_mock and delegates if non-null.
#define MAYBE_INVOKE_MOCK(method, ...)                                    \
  if (FileUtilInterface* mock = g_mock.load(std::memory_order_acquire)) { \
    return mock->method(__VA_ARGS__);                                     \
  }

#ifdef _WIN32
namespace {

// Converts the Win32 error code to absl::StatusCode.
absl::StatusCode Win32ErrorToStatusCode(DWORD error_code) {
  switch (error_code) {
    case ERROR_SUCCESS:
      return absl::StatusCode::kOk;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return absl::StatusCode::kNotFound;
    case ERROR_ACCESS_DENIED:
      return absl::StatusCode::kPermissionDenied;
    case ERROR_ALREADY_EXISTS:
      return absl::StatusCode::kAlreadyExists;
    default:
      return absl::StatusCode::kUnknown;
  }
}

// Converts the Win32 error code to absl::Status.
absl::Status Win32ErrorToStatus(DWORD error_code, absl::string_view message) {
  return absl::Status(Win32ErrorToStatusCode(error_code),
                      absl::StrCat(message, ": error_code=", error_code));
}

absl::StatusOr<DWORD> GetFileAttributes(const std::wstring& filename) {
  if (const DWORD attrs = ::GetFileAttributesW(filename.c_str());
      attrs != INVALID_FILE_ATTRIBUTES) {
    return attrs;
  }
  const DWORD error = ::GetLastError();
  return Win32ErrorToStatus(error, "GetFileAttributesW failed");
}

absl::Status SetFileAttributes(const std::wstring& filename, DWORD attrs) {
  if (::SetFileAttributesW(filename.c_str(), attrs)) {
    return absl::OkStatus();
  }
  const DWORD error = ::GetLastError();
  return Win32ErrorToStatus(
      error, absl::StrCat("SetFileAttributesW failed: attrs = ", attrs));
}

// Some high-level file APIs such as MoveFileEx simply fail if the target file
// has some special attribute like read-only. This method tries to strip system,
// hidden, and read-only attributes from |filename|.
// This function does nothing if |filename| does not exist.
absl::Status StripWritePreventingAttributesIfExists(
    absl::string_view filename) {
  if (absl::Status s = FileUtil::FileExists(filename); absl::IsNotFound(s)) {
    return absl::OkStatus();
  } else if (!s.ok()) {
    return s;
  }
  const pfstring pf_filename = to_pfstring(filename);
  constexpr DWORD kDropAttributes =
      FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY;
  absl::StatusOr<DWORD> attributes = GetFileAttributes(pf_filename);
  if (!attributes.ok()) {
    return std::move(attributes).status();
  }
  if (*attributes & kDropAttributes) {
    const DWORD attrs = *attributes & ~kDropAttributes;
    if (absl::Status s = SetFileAttributes(pf_filename, attrs); !s.ok()) {
      return absl::Status(
          s.code(),
          absl::StrFormat(
              "Cannot drop the write-preventing file attributes of %s: %s",
              filename, s.message()));
    }
  }
  return absl::OkStatus();
}

}  // namespace
#endif  // _WIN32

absl::Status FileUtil::CreateDirectory(absl::string_view path) {
  MAYBE_INVOKE_MOCK(CreateDirectory, path);

  const pfstring pf_path = to_pfstring(path);

#if !defined(_WIN32)
  // On Windows, this check is skipped to avoid freeze of the host application.
  // This platform dependent behavior is a temporary solution to avoid
  // freeze of the host application.
  // https://github.com/google/mozc/issues/1076
  //
  // If the path already exists, returns OkStatus and does nothing.
  if (const absl::Status status = DirectoryExists(path); status.ok()) {
    return absl::OkStatus();
  }
#endif  // !_WIN32

#if defined(_WIN32)
  if (pf_path.empty()) {
    return absl::InvalidArgumentError("Failed to convert to wstring");
  }
  if (!::CreateDirectoryW(pf_path.c_str(), nullptr)) {
    return Win32ErrorToStatus(::GetLastError(), "CreateDirectoryW failed");
  }
  return absl::OkStatus();
#else   // !_WIN32
  if (::mkdir(pf_path.c_str(), 0700) != 0) {
    const int err = errno;
    return absl::ErrnoToStatus(err, "mkdir failed");
  }
  return absl::OkStatus();
#endif  // _WIN32
}

absl::Status FileUtil::RemoveDirectory(absl::string_view dirname) {
  MAYBE_INVOKE_MOCK(RemoveDirectory, dirname);

  const pfstring pf_dirname = to_pfstring(dirname);

#ifdef _WIN32
  if (pf_dirname.empty()) {
    return absl::InvalidArgumentError("Failed to convert to wstring");
  }
  if (!::RemoveDirectoryW(pf_dirname.c_str())) {
    return Win32ErrorToStatus(::GetLastError(), "RemoveDirectoryW failed");
  }
  return absl::OkStatus();
#else   // !_WIN32
  if (::rmdir(pf_dirname.c_str()) != 0) {
    const int err = errno;
    return absl::ErrnoToStatus(err, "rmdir failed");
  }
  return absl::OkStatus();
#endif  // _WIN32
}

absl::Status FileUtil::RemoveDirectoryIfExists(absl::string_view dirname) {
  absl::Status s = FileExists(dirname);
  if (s.ok()) {
    return RemoveDirectory(dirname);
  }
  if (absl::IsNotFound(s)) {
    return absl::OkStatus();
  }
  return s;
}

absl::Status FileUtil::Unlink(absl::string_view filename) {
  MAYBE_INVOKE_MOCK(Unlink, filename);

  const pfstring pf_filename = to_pfstring(filename);

#ifdef _WIN32
  if (absl::Status s = StripWritePreventingAttributesIfExists(filename);
      !s.ok()) {
    return absl::UnknownError(absl::StrFormat(
        "StripWritePreventingAttributesIfExists failed: %s", s.ToString()));
  }
  if (pf_filename.empty()) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }
  if (!::DeleteFileW(pf_filename.c_str())) {
    const DWORD err = ::GetLastError();
    return absl::UnknownError(absl::StrFormat("DeleteFileW failed: %d", err));
  }
  return absl::OkStatus();
#else   // !_WIN32
  if (::unlink(pf_filename.c_str()) != 0) {
    const int err = errno;
    return absl::UnknownError(
        absl::StrFormat("unlink failed: errno = %d", err));
  }
  return absl::OkStatus();
#endif  // _WIN32
}

absl::Status FileUtil::UnlinkIfExists(absl::string_view filename) {
  absl::Status s = FileExists(filename);
  if (s.ok()) {
    return Unlink(filename);
  }
  if (absl::IsNotFound(s)) {
    return absl::OkStatus();
  }
  return s;
}

void FileUtil::UnlinkOrLogError(absl::string_view filename) {
  if (absl::Status s = Unlink(filename); !s.ok()) {
    LOG(ERROR) << "Cannot unlink " << filename << ": " << s;
  }
}

absl::Status FileUtil::FileExists(absl::string_view filename) {
  MAYBE_INVOKE_MOCK(FileExists, filename);

  const pfstring pf_filename = to_pfstring(filename);

#ifdef _WIN32
  if (pf_filename.empty()) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }
  return GetFileAttributes(pf_filename).status();
#else   // !_WIN32
  struct stat s;
  if (::stat(pf_filename.c_str(), &s) == 0) {
    return absl::OkStatus();
  }
  const int err = errno;
  return absl::ErrnoToStatus(err, absl::StrCat("Cannot stat ", filename));
#endif  // _WIN32
}

absl::Status FileUtil::DirectoryExists(absl::string_view dirname) {
  MAYBE_INVOKE_MOCK(DirectoryExists, dirname);

  const pfstring pf_dirname = to_pfstring(dirname);

#ifdef _WIN32
  if (pf_dirname.empty()) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }

  absl::StatusOr<DWORD> attrs = GetFileAttributes(pf_dirname);
  if (!attrs.ok()) {
    return std::move(attrs).status();
  }
  return (*attrs & FILE_ATTRIBUTE_DIRECTORY) ? absl::OkStatus()
                                             : absl::NotFoundError(dirname);
#else   // !_WIN32
  struct stat s;
  if (::stat(pf_dirname.c_str(), &s) == 0) {
    return S_ISDIR(s.st_mode)
               ? absl::OkStatus()
               : absl::NotFoundError("Path exists but it's not a directory");
  }
  const int err = errno;
  return absl::ErrnoToStatus(err, absl::StrCat("Cannot stat ", dirname));
#endif  // _WIN32
}

#ifdef _WIN32
bool FileUtil::HideFile(absl::string_view filename) {
  return HideFileWithExtraAttributes(filename, 0);
}

bool FileUtil::HideFileWithExtraAttributes(absl::string_view filename,
                                           DWORD extra_attributes) {
  if (absl::Status s = FileUtil::FileExists(filename); !s.ok()) {
    LOG(WARNING) << "File not exists: " << filename << ": " << s;
    return false;
  }

  const pfstring pf_filename = to_pfstring(filename);
  const absl::StatusOr<DWORD> original_attributes =
      GetFileAttributes(pf_filename);
  if (!original_attributes.ok()) {
    LOG(ERROR) << original_attributes.status();
    return false;
  }
  absl::Status s = SetFileAttributes(
      pf_filename, (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                    FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | *original_attributes |
                    extra_attributes) &
                       ~FILE_ATTRIBUTE_NORMAL);
  if (!s.ok()) {
    LOG(ERROR) << s;
  }
  return s.ok();
}
#endif  // _WIN32

absl::Status FileUtil::CopyFile(absl::string_view from, absl::string_view to) {
  MAYBE_INVOKE_MOCK(CopyFile, from, to);

#ifdef _WIN32
  const pfstring pf_from = to_pfstring(from);
  const pfstring pf_to = to_pfstring(to);

  if (pf_from.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Cannot convert to wstring: ", from));
  }
  if (pf_to.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Cannot convert to wstring: ", to));
  }
  if (absl::Status s = StripWritePreventingAttributesIfExists(to); !s.ok()) {
    return s;
  }
#endif  // _WIN32

  InputFileStream ifs(from, std::ios::binary);
  if (!ifs) {
    return absl::UnknownError(absl::StrCat("Can't open input file ", from));
  }

  OutputFileStream ofs(to, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    return absl::UnknownError(absl::StrCat("Can't open output file ", to));
  }

  // TODO(taku): we have to check disk quota in advance.
  if (!(ofs << ifs.rdbuf())) {
    return absl::UnknownError("Can't write data");
  }
  ifs.close();
  ofs.close();

#ifdef _WIN32
  absl::StatusOr<DWORD> attrs = GetFileAttributes(pf_from);
  if (attrs.ok()) {
    if (absl::Status s = SetFileAttributes(pf_to, *attrs); !s.ok()) {
      LOG(ERROR) << s;
    }
  } else {
    LOG(ERROR) << attrs.status();
  }
#endif  // _WIN32

  return absl::OkStatus();
}

absl::StatusOr<bool> FileUtil::IsEqualFile(absl::string_view filename1,
                                           absl::string_view filename2) {
  MAYBE_INVOKE_MOCK(IsEqualFile, filename1, filename2);

  absl::StatusOr<Mmap> mmap1 = Mmap::Map(filename1, Mmap::READ_ONLY);
  if (!mmap1.ok()) {
    return std::move(mmap1).status();
  }
  absl::StatusOr<Mmap> mmap2 = Mmap::Map(filename2, Mmap::READ_ONLY);
  if (!mmap2.ok()) {
    return std::move(mmap2).status();
  }
  return mmap1->span() == mmap2->span();
}

absl::StatusOr<bool> FileUtil::IsEquivalent(absl::string_view filename1,
                                            absl::string_view filename2) {
  MAYBE_INVOKE_MOCK(IsEquivalent, filename1, filename2);

  // If either of filename1 or filename2 does not exist, an error is returned.
  // Because filesystem::equivalent on some environments returns false instead,
  // that case is checked here to keep the consistency.
  if (FileExists(filename1).ok() != FileExists(filename2).ok()) {
    return absl::UnknownError("No such file or directory");
  }

  const std::filesystem::path src = to_pfstring(filename1);
  const std::filesystem::path dst = to_pfstring(filename2);

  std::error_code error_code;
  if (bool is_equiv = std::filesystem::equivalent(src, dst, error_code);
      !error_code) {
    return is_equiv;
  }
  return absl::UnknownError(
      absl::StrCat(error_code.value(), " ", error_code.message()));
}

absl::Status FileUtil::AtomicRename(absl::string_view from,
                                    absl::string_view to) {
  MAYBE_INVOKE_MOCK(AtomicRename, from, to);

  const pfstring pf_from = to_pfstring(from);
  const pfstring pf_to = to_pfstring(to);

#ifdef _WIN32

  const absl::StatusOr<DWORD> original_attributes = GetFileAttributes(pf_from);
  if (!original_attributes.ok()) {
    return absl::Status(
        original_attributes.status().code(),
        absl::StrFormat("GetFileAttributes failed: %s",
                        original_attributes.status().message()));
  }
  if (absl::Status s = StripWritePreventingAttributesIfExists(to); !s.ok()) {
    return absl::Status(
        s.code(),
        absl::StrFormat("StripWritePreventingAttributesIfExists failed: %s",
                        s.message()));
  }
  if (!::MoveFileExW(pf_from.c_str(), pf_to.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
    const DWORD move_file_ex_error = ::GetLastError();
    return Win32ErrorToStatus(move_file_ex_error, "MoveFileExW failed");
  }
  if (absl::Status s = SetFileAttributes(pf_to, *original_attributes);
      !s.ok()) {
    return absl::Status(
        s.code(),
        absl::StrFormat("SetFileAttributes failed: original_attrs: %d",
                        *original_attributes));
  }
  return absl::OkStatus();
#else   // !_WIN32
  // macOS: use rename(2), but rename(2) on macOS is not properly implemented,
  // atomic rename is POSIX spec though.
  // http://www.weirdnet.nl/apple/rename.html
  if (const int r = rename(pf_from.c_str(), pf_to.c_str()); r != 0) {
    const int err = errno;
    return absl::UnknownError(
        absl::StrFormat("errno(%d): %s", err, std::strerror(err)));
  }
  return absl::OkStatus();
#endif  // _WIN32
}

absl::Status FileUtil::CreateHardLink(absl::string_view from,
                                      absl::string_view to) {
  MAYBE_INVOKE_MOCK(CreateHardLink, from, to);

  const std::filesystem::path src = to_pfstring(from);
  const std::filesystem::path dst = to_pfstring(to);

  std::error_code error_code;
  std::filesystem::create_hard_link(src, dst, error_code);
  if (error_code) {
    return absl::UnknownError(
        absl::StrCat(error_code.message(), " (code=", error_code.value(), ")"));
  }
  return absl::OkStatus();
}

std::string FileUtil::JoinPath(
    const absl::Span<const absl::string_view> components) {
  std::string output;
  for (const absl::string_view component : components) {
    if (component.empty()) {
      continue;
    }
    if (!output.empty() && output.back() != kFileDelimiter) {
      output.append(1, kFileDelimiter);
    }
    absl::StrAppend(&output, component);
  }
  return output;
}

// TODO(taku): what happens if filename == '/foo/bar/../bar/..
std::string FileUtil::Dirname(absl::string_view filename) {
  const size_t p = filename.find_last_of(kFileDelimiter);
  if (p == absl::string_view::npos) {
    return "";
  }
  return std::string(filename.substr(0, p));
}

std::string FileUtil::Basename(absl::string_view filename) {
  const size_t p = filename.find_last_of(kFileDelimiter);
  if (p == absl::string_view::npos) {
    return std::string(filename);
  }
  return std::string(filename.substr(p + 1));
}

std::string FileUtil::NormalizeDirectorySeparator(absl::string_view path) {
  if constexpr (port::IsWindows()) {
    constexpr absl::string_view kFileDelimiterForUnix = "/";
    constexpr absl::string_view kFileDelimiterForWindows = "\\";
    return absl::StrReplaceAll(
        path, {{kFileDelimiterForUnix, kFileDelimiterForWindows}});
  }
  return std::string(path);
}

absl::StatusOr<FileTimeStamp> FileUtil::GetModificationTime(
    absl::string_view filename) {
  MAYBE_INVOKE_MOCK(GetModificationTime, filename);

  const pfstring pf_filename = to_pfstring(filename);

#if defined(_WIN32)
  if (pf_filename.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Utf8ToWide failed: ", filename));
  }
  WIN32_FILE_ATTRIBUTE_DATA info = {};
  if (!::GetFileAttributesEx(pf_filename.c_str(), GetFileExInfoStandard,
                             &info)) {
    const auto last_error = ::GetLastError();
    return Win32ErrorToStatus(
        last_error, absl::StrCat("GetFileAttributesEx(", filename, ") failed"));
  }
  return (static_cast<uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) +
         info.ftLastWriteTime.dwLowDateTime;
#else   // !_WIN32
  struct stat stat_info;
  if (::stat(pf_filename.c_str(), &stat_info)) {
    const int err = errno;
    return absl::ErrnoToStatus(err, absl::StrCat("stat failed: ", filename));
  }
  return stat_info.st_mtime;
#endif  // _WIN32
}

absl::StatusOr<std::string> FileUtil::ReadSymlink(absl::string_view filename) {
  MAYBE_INVOKE_MOCK(ReadSymlink, filename);

  const std::filesystem::path path = to_pfstring(filename);
  std::error_code error_code;
  const std::filesystem::path link_path =
      std::filesystem::read_symlink(path, error_code);
  if (error_code) {
    return absl::UnknownError(
        absl::StrCat(error_code.message(), " (code=", error_code.value(), ")"));
  }
  return link_path.string();
}

absl::StatusOr<std::string> FileUtil::GetContents(
    absl::string_view filename, std::ios_base::openmode mode) {
  InputFileStream ifs(filename, mode | std::ios::ate);
  if (ifs.fail()) {
    const int err = errno;
    return absl::ErrnoToStatus(err, absl::StrCat("Cannot open ", filename));
  }
  const ptrdiff_t size = ifs.tellg();
  if (size == -1) {
    const int err = errno;
    return absl::ErrnoToStatus(err, absl::StrCat("tellg failed: ", filename));
  }
  ifs.seekg(0, std::ios_base::beg);
  std::string content;
  if (mode & std::ios::binary) {
    content.resize(size);
    ifs.read(content.data(), size);
  } else {
    // In the text mode, the read size can be smaller than the file size as
    // "\r\n" can be translated to "\n" on Windows. Therefore, we just reserve a
    // buffer size and perform sequential read.
    content.reserve(size);
    content.assign(std::istreambuf_iterator<char>(ifs),
                   std::istreambuf_iterator<char>());
  }
  ifs.close();
  if (ifs.fail()) {
    const int err = errno;
    return absl::ErrnoToStatus(err, absl::StrCat("Cannot read ", filename,
                                                 " of size ", size, " bytes"));
  }
  return content;
}

absl::Status FileUtil::SetContents(absl::string_view filename,
                                   absl::string_view content,
                                   std::ios_base::openmode mode) {
  OutputFileStream ofs(filename, mode);
  if (ofs.fail()) {
    const int err = errno;
    return absl::ErrnoToStatus(err, absl::StrCat("Cannot open ", filename));
  }
  ofs << content;
  ofs.close();
  if (ofs.fail()) {
    const int err = errno;
    return absl::ErrnoToStatus(
        err,
        absl::StrCat("Cannot write ", content.size(), " bytes to ", filename));
  }
  return absl::OkStatus();
}

void FileUtil::SetMockForUnitTest(FileUtilInterface* mock) {
  g_mock.store(mock, std::memory_order_release);
}

#undef MAYBE_INVOKE_MOCK

}  // namespace mozc
