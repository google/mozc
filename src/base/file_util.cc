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

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <ios>
#include <iterator>
#include <string>
#include <system_error>
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
#include "base/singleton.h"
#include "base/strings/zstring_view.h"

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

// Ad-hoc workadound against macro problem on Windows.
// On Windows, following macros, defined when you include <windows.h>,
// should be removed here because they affects the method name definition of
// Util class.
// TODO(yukawa): Use different method name if applicable.
#undef CreateDirectory
#undef RemoveDirectory
#undef CopyFile

namespace mozc {
namespace {
class FileUtilImpl : public FileUtilInterface {
 public:
  FileUtilImpl() = default;
  ~FileUtilImpl() override = default;

  absl::Status CreateDirectory(zstring_view path) const override;
  absl::Status RemoveDirectory(zstring_view dirname) const override;
  absl::Status Unlink(zstring_view filename) const override;
  absl::Status FileExists(zstring_view filename) const override;
  absl::Status DirectoryExists(zstring_view dirname) const override;
  absl::Status CopyFile(zstring_view from, zstring_view to) const override;
  absl::StatusOr<bool> IsEqualFile(zstring_view filename1,
                                   zstring_view filename2) const override;
  absl::StatusOr<bool> IsEquivalent(zstring_view filename1,
                                    zstring_view filename2) const override;
  absl::Status AtomicRename(zstring_view from, zstring_view to) const override;
  absl::Status CreateHardLink(zstring_view from, zstring_view to) override;
  absl::StatusOr<FileTimeStamp> GetModificationTime(
      zstring_view filename) const override;
};

using FileUtilSingleton = SingletonMockable<FileUtilInterface, FileUtilImpl>;

}  // namespace

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

absl::StatusOr<DWORD> GetFileAttributes(const std::wstring &filename) {
  if (const DWORD attrs = ::GetFileAttributesW(filename.c_str());
      attrs != INVALID_FILE_ATTRIBUTES) {
    return attrs;
  }
  const DWORD error = ::GetLastError();
  return Win32ErrorToStatus(error, "GetFileAttributesW failed");
}

absl::Status SetFileAttributes(const std::wstring &filename, DWORD attrs) {
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
absl::Status StripWritePreventingAttributesIfExists(zstring_view filename) {
  if (absl::Status s = FileUtil::FileExists(filename); absl::IsNotFound(s)) {
    return absl::OkStatus();
  } else if (!s.ok()) {
    return s;
  }
  const std::wstring wide_filename = win32::Utf8ToWide(filename);
  constexpr DWORD kDropAttributes =
      FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY;
  absl::StatusOr<DWORD> attributes = GetFileAttributes(wide_filename);
  if (!attributes.ok()) {
    return std::move(attributes).status();
  }
  if (*attributes & kDropAttributes) {
    const DWORD attrs = *attributes & ~kDropAttributes;
    if (absl::Status s = SetFileAttributes(wide_filename, attrs); !s.ok()) {
      return absl::Status(
          s.code(),
          absl::StrFormat(
              "Cannot drop the write-preventing file attributes of %s: %s",
              filename.view(), s.message()));
    }
  }
  return absl::OkStatus();
}

}  // namespace
#endif  // _WIN32

absl::Status FileUtil::CreateDirectory(zstring_view path) {
  return FileUtilSingleton::Get()->CreateDirectory(path);
}

absl::Status FileUtilImpl::CreateDirectory(zstring_view path) const {
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
  const std::wstring wide = win32::Utf8ToWide(path);
  if (wide.empty()) {
    return absl::InvalidArgumentError("Failed to convert to wstring");
  }
  if (!::CreateDirectoryW(wide.c_str(), nullptr)) {
    return Win32ErrorToStatus(::GetLastError(), "CreateDirectoryW failed");
  }
  return absl::OkStatus();
#else   // !_WIN32
  if (::mkdir(path.c_str(), 0700) != 0) {
    const int err = errno;
    return absl::ErrnoToStatus(err, "mkdir failed");
  }
  return absl::OkStatus();
#endif  // _WIN32
}

absl::Status FileUtil::RemoveDirectory(zstring_view dirname) {
  return FileUtilSingleton::Get()->RemoveDirectory(dirname);
}

absl::Status FileUtilImpl::RemoveDirectory(zstring_view dirname) const {
#ifdef _WIN32
  const std::wstring wide = win32::Utf8ToWide(dirname);
  if (wide.empty()) {
    return absl::InvalidArgumentError("Failed to convert to wstring");
  }
  if (!::RemoveDirectoryW(wide.c_str())) {
    return Win32ErrorToStatus(::GetLastError(), "RemoveDirectoryW failed");
  }
  return absl::OkStatus();
#else   // !_WIN32
  if (::rmdir(dirname.c_str()) != 0) {
    const int err = errno;
    return absl::ErrnoToStatus(err, "rmdir failed");
  }
  return absl::OkStatus();
#endif  // _WIN32
}

absl::Status FileUtil::RemoveDirectoryIfExists(zstring_view dirname) {
  absl::Status s = FileExists(dirname);
  if (s.ok()) {
    return RemoveDirectory(dirname);
  }
  if (absl::IsNotFound(s)) {
    return absl::OkStatus();
  }
  return s;
}

absl::Status FileUtil::Unlink(zstring_view filename) {
  return FileUtilSingleton::Get()->Unlink(filename);
}

absl::Status FileUtilImpl::Unlink(zstring_view filename) const {
#ifdef _WIN32
  if (absl::Status s = StripWritePreventingAttributesIfExists(filename);
      !s.ok()) {
    return absl::UnknownError(absl::StrFormat(
        "StripWritePreventingAttributesIfExists failed: %s", s.ToString()));
  }
  const std::wstring wide = win32::Utf8ToWide(filename);
  if (wide.empty()) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }
  if (!::DeleteFileW(wide.c_str())) {
    const DWORD err = ::GetLastError();
    return absl::UnknownError(absl::StrFormat("DeleteFileW failed: %d", err));
  }
  return absl::OkStatus();
#else   // !_WIN32
  if (::unlink(filename.c_str()) != 0) {
    const int err = errno;
    return absl::UnknownError(
        absl::StrFormat("unlink failed: errno = %d", err));
  }
  return absl::OkStatus();
#endif  // _WIN32
}

absl::Status FileUtil::UnlinkIfExists(zstring_view filename) {
  absl::Status s = FileExists(filename);
  if (s.ok()) {
    return Unlink(filename);
  }
  if (absl::IsNotFound(s)) {
    return absl::OkStatus();
  }
  return s;
}

void FileUtil::UnlinkOrLogError(zstring_view filename) {
  if (absl::Status s = Unlink(filename); !s.ok()) {
    LOG(ERROR) << "Cannot unlink " << filename << ": " << s;
  }
}

absl::Status FileUtil::FileExists(zstring_view filename) {
  return FileUtilSingleton::Get()->FileExists(filename);
}

absl::Status FileUtilImpl::FileExists(zstring_view filename) const {
#ifdef _WIN32
  const std::wstring wide = win32::Utf8ToWide(filename);
  if (wide.empty()) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }
  return GetFileAttributes(wide).status();
#else   // !_WIN32
  struct stat s;
  if (::stat(filename.c_str(), &s) == 0) {
    return absl::OkStatus();
  }
  const int err = errno;
  return absl::ErrnoToStatus(err, absl::StrCat("Cannot stat ", filename));
#endif  // _WIN32
}

absl::Status FileUtil::DirectoryExists(zstring_view dirname) {
  return FileUtilSingleton::Get()->DirectoryExists(dirname);
}

absl::Status FileUtilImpl::DirectoryExists(zstring_view dirname) const {
#ifdef _WIN32
  const std::wstring wide = win32::Utf8ToWide(dirname);
  if (wide.empty()) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }

  absl::StatusOr<DWORD> attrs = GetFileAttributes(wide);
  if (!attrs.ok()) {
    return std::move(attrs).status();
  }
  return (*attrs & FILE_ATTRIBUTE_DIRECTORY) ? absl::OkStatus()
                                             : absl::NotFoundError(dirname);
#else   // !_WIN32
  struct stat s;
  if (::stat(dirname.c_str(), &s) == 0) {
    return S_ISDIR(s.st_mode)
               ? absl::OkStatus()
               : absl::NotFoundError("Path exists but it's not a directory");
  }
  const int err = errno;
  return absl::ErrnoToStatus(err, absl::StrCat("Cannot stat ", dirname));
#endif  // _WIN32
}

#ifdef _WIN32
bool FileUtil::HideFile(zstring_view filename) {
  return HideFileWithExtraAttributes(filename, 0);
}

bool FileUtil::HideFileWithExtraAttributes(zstring_view filename,
                                           DWORD extra_attributes) {
  if (absl::Status s = FileUtil::FileExists(filename); !s.ok()) {
    LOG(WARNING) << "File not exists: " << filename << ": " << s;
    return false;
  }

  const std::wstring wfilename = win32::Utf8ToWide(filename);
  const absl::StatusOr<DWORD> original_attributes =
      GetFileAttributes(wfilename);
  if (!original_attributes.ok()) {
    LOG(ERROR) << original_attributes.status();
    return false;
  }
  absl::Status s = SetFileAttributes(
      wfilename, (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                  FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | *original_attributes |
                  extra_attributes) &
                     ~FILE_ATTRIBUTE_NORMAL);
  if (!s.ok()) {
    LOG(ERROR) << s;
  }
  return s.ok();
}
#endif  // _WIN32

absl::Status FileUtil::CopyFile(zstring_view from, zstring_view to) {
  return FileUtilSingleton::Get()->CopyFile(from, to);
}

absl::Status FileUtilImpl::CopyFile(zstring_view from, zstring_view to) const {
#ifdef _WIN32
  const std::wstring wfrom = win32::Utf8ToWide(from);
  if (wfrom.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Cannot convert to wstring: ", from));
  }
  const std::wstring wto = win32::Utf8ToWide(to);
  if (wto.empty()) {
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
  absl::StatusOr<DWORD> attrs = GetFileAttributes(wfrom);
  if (attrs.ok()) {
    if (absl::Status s = SetFileAttributes(wto, *attrs); !s.ok()) {
      LOG(ERROR) << s;
    }
  } else {
    LOG(ERROR) << attrs.status();
  }
#endif  // _WIN32

  return absl::OkStatus();
}

absl::StatusOr<bool> FileUtil::IsEqualFile(zstring_view filename1,
                                           zstring_view filename2) {
  return FileUtilSingleton::Get()->IsEqualFile(filename1, filename2);
}

absl::StatusOr<bool> FileUtilImpl::IsEqualFile(zstring_view filename1,
                                               zstring_view filename2) const {
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

absl::StatusOr<bool> FileUtil::IsEquivalent(zstring_view filename1,
                                            zstring_view filename2) {
  return FileUtilSingleton::Get()->IsEquivalent(filename1, filename2);
}

absl::StatusOr<bool> FileUtilImpl::IsEquivalent(zstring_view filename1,
                                                zstring_view filename2) const {
  // If either of filename1 or filename2 does not exist, an error is returned.
  // Because filesystem::equivalent on some environments returns false instead,
  // that case is checked here to keep the consistency.
  if (FileExists(filename1).ok() != FileExists(filename2).ok()) {
    return absl::UnknownError("No such file or directory");
  }

#ifdef __APPLE__
  return absl::UnimplementedError(
      "std::filesystem is only available on macOS 10.15, iOS 13.0, or later");
#else   // __APPLE__

  // u8path is deprecated in C++20. The current target is C++17.
  const std::filesystem::path src = std::filesystem::u8path(filename1.c_str());
  const std::filesystem::path dst = std::filesystem::u8path(filename2.c_str());

  std::error_code error_code;
  if (bool is_equiv = std::filesystem::equivalent(src, dst, error_code);
      !error_code) {
    return is_equiv;
  }
  return absl::UnknownError(
      absl::StrCat(error_code.value(), " ", error_code.message()));
#endif  // __APPLE__
}

absl::Status FileUtil::AtomicRename(zstring_view from, zstring_view to) {
  return FileUtilSingleton::Get()->AtomicRename(from, to);
}

absl::Status FileUtilImpl::AtomicRename(zstring_view from,
                                        zstring_view to) const {
#ifdef _WIN32
  const std::wstring fromw = win32::Utf8ToWide(from);
  const std::wstring tow = win32::Utf8ToWide(to);

  const absl::StatusOr<DWORD> original_attributes = GetFileAttributes(fromw);
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
  if (!::MoveFileExW(fromw.c_str(), tow.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
    const DWORD move_file_ex_error = ::GetLastError();
    return Win32ErrorToStatus(move_file_ex_error, "MoveFileExW failed");
  }
  if (absl::Status s = SetFileAttributes(tow, *original_attributes); !s.ok()) {
    return absl::Status(
        s.code(),
        absl::StrFormat("SetFileAttributes failed: original_attrs: %d",
                        *original_attributes));
  }
  return absl::OkStatus();
#else   // !_WIN32
  // Mac OSX: use rename(2), but rename(2) on Mac OSX
  // is not properly implemented, atomic rename is POSIX spec though.
  // http://www.weirdnet.nl/apple/rename.html
  if (const int r = rename(from.c_str(), to.c_str()); r != 0) {
    const int err = errno;
    return absl::UnknownError(
        absl::StrFormat("errno(%d): %s", err, std::strerror(err)));
  }
  return absl::OkStatus();
#endif  // _WIN32
}

absl::Status FileUtil::CreateHardLink(zstring_view from, zstring_view to) {
  return FileUtilSingleton::Get()->CreateHardLink(from, to);
}

absl::Status FileUtilImpl::CreateHardLink(zstring_view from, zstring_view to) {
#ifdef __APPLE__
  return absl::UnimplementedError(
      "std::filesystem is only available on macOS 10.15, iOS 13.0, or later.");
#else   // __APPLE__

  // u8path is deprecated in C++20. The current target is C++17.
  const std::filesystem::path src = std::filesystem::u8path(from.c_str());
  const std::filesystem::path dst = std::filesystem::u8path(to.c_str());

  std::error_code error_code;
  std::filesystem::create_hard_link(src, dst, error_code);
  if (error_code) {
    return absl::UnknownError(
        absl::StrCat(error_code.message(), " (code=", error_code.value(), ")"));
  }
  return absl::OkStatus();
#endif  // __APPLE__
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
std::string FileUtil::Dirname(zstring_view filename) {
  const std::string filename_str(filename.c_str());
  const std::string::size_type p = filename_str.find_last_of(kFileDelimiter);
  if (p == std::string::npos) {
    return "";
  }
  return filename_str.substr(0, p);
}

std::string FileUtil::Basename(zstring_view filename) {
  std::string filename_str(filename.c_str());
  const std::string::size_type p = filename_str.find_last_of(kFileDelimiter);
  if (p == std::string::npos) {
    return filename_str;
  }
  return filename_str.substr(p + 1, filename_str.size() - p);
}

std::string FileUtil::NormalizeDirectorySeparator(zstring_view path) {
  if constexpr (TargetIsWindows()) {
    constexpr absl::string_view kFileDelimiterForUnix = "/";
    constexpr absl::string_view kFileDelimiterForWindows = "\\";
    return absl::StrReplaceAll(
        path, {{kFileDelimiterForUnix, kFileDelimiterForWindows}});
  }
  return std::string(path.view());
}

absl::StatusOr<FileTimeStamp> FileUtil::GetModificationTime(
    zstring_view filename) {
  return FileUtilSingleton::Get()->GetModificationTime(filename);
}

absl::StatusOr<FileTimeStamp> FileUtilImpl::GetModificationTime(
    zstring_view filename) const {
#if defined(_WIN32)
  const std::wstring wide = win32::Utf8ToWide(filename);
  if (wide.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Utf8ToWide failed: ", filename));
  }
  WIN32_FILE_ATTRIBUTE_DATA info = {};
  if (!::GetFileAttributesEx(wide.c_str(), GetFileExInfoStandard, &info)) {
    const auto last_error = ::GetLastError();
    return Win32ErrorToStatus(
        last_error, absl::StrCat("GetFileAttributesEx(", filename, ") failed"));
  }
  return (static_cast<uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32) +
         info.ftLastWriteTime.dwLowDateTime;
#else   // !_WIN32
  struct stat stat_info;
  if (::stat(filename.c_str(), &stat_info)) {
    const int err = errno;
    return absl::ErrnoToStatus(err, absl::StrCat("stat failed: ", filename));
  }
  return stat_info.st_mtime;
#endif  // _WIN32
}

absl::StatusOr<std::string> FileUtil::GetContents(
    zstring_view filename, std::ios_base::openmode mode) {
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

absl::Status FileUtil::SetContents(zstring_view filename,
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

void FileUtil::SetMockForUnitTest(FileUtilInterface *mock) {
  FileUtilSingleton::SetMock(mock);
}

}  // namespace mozc
