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

#ifdef OS_WIN
#include <KtmW32.h>
#include <Windows.h>
#else  // OS_WIN
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // OS_WIN

#include <filesystem>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/mutex.h"
#include "base/scoped_handle.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/win_util.h"
#include "absl/status/status.h"
#ifdef OS_WIN
#include "absl/status/statusor.h"
#endif  // OS_WIN

namespace {

#ifdef OS_WIN
constexpr char kFileDelimiter = '\\';
#else   // OS_WIN
constexpr char kFileDelimiter = '/';
#endif  // OS_WIN

}  // namespace

// Ad-hoc workadound against macro problem on Windows.
// On Windows, following macros, defined when you include <Windows.h>,
// should be removed here because they affects the method name definition of
// Util class.
// TODO(yukawa): Use different method name if applicable.
#ifdef CreateDirectory
#undef CreateDirectory
#endif  // CreateDirectory
#ifdef RemoveDirectory
#undef RemoveDirectory
#endif  // RemoveDirectory
#ifdef CopyFile
#undef CopyFile
#endif  // CopyFile

namespace mozc {
namespace {
class FileUtilImpl : public FileUtilInterface {
 public:
  FileUtilImpl() = default;
  ~FileUtilImpl() override = default;

  bool CreateDirectory(const std::string &path) const override;
  bool RemoveDirectory(const std::string &dirname) const override;
  absl::Status Unlink(const std::string &filename) const override;
  absl::Status FileExists(const std::string &filename) const override;
  absl::Status DirectoryExists(const std::string &dirname) const override;
  bool CopyFile(const std::string &from, const std::string &to) const override;
  bool IsEqualFile(const std::string &filename1,
                   const std::string &filename2) const override;
  bool IsEquivalent(const std::string &filename1,
                    const std::string &filename2) const override;
  absl::Status AtomicRename(const std::string &from,
                            const std::string &to) const override;
  bool CreateHardLink(const std::string &from, const std::string &to) override;
  bool GetModificationTime(const std::string &filename,
                           FileTimeStamp *modified_at) const override;
};

using FileUtilSingleton = SingletonMockable<FileUtilInterface, FileUtilImpl>;

}  // namespace

#ifdef OS_WIN
namespace {

absl::StatusOr<DWORD> GetFileAttributes(const std::wstring &filename) {
  if (const DWORD attrs = ::GetFileAttributesW(filename.c_str());
      attrs != INVALID_FILE_ATTRIBUTES) {
    return attrs;
  }
  const DWORD error = ::GetLastError();
  return WinUtil::ErrorToCanonicalStatus(error, "GetFileAttributesW failed");
}

// Some high-level file APIs such as MoveFileEx simply fail if the target file
// has some special attribute like read-only. This method tries to strip system,
// hidden, and read-only attributes from |filename|.
// This function does nothing if |filename| does not exist.
absl::Status StripWritePreventingAttributesIfExists(
    const std::string &filename) {
  if (absl::Status s = FileUtil::FileExists(filename); absl::IsNotFound(s)) {
    return absl::OkStatus();
  } else if (!s.ok()) {
    return s;
  }
  std::wstring wide_filename;
  Util::Utf8ToWide(filename, &wide_filename);
  constexpr DWORD kDropAttributes =
      FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY;
  absl::StatusOr<DWORD> attributes = GetFileAttributes(wide_filename);
  if (!attributes.ok()) {
    return std::move(attributes).status();
  }
  if (*attributes & kDropAttributes) {
    if (!::SetFileAttributesW(wide_filename.c_str(),
                              *attributes & ~kDropAttributes)) {
      const DWORD error = ::GetLastError();
      return WinUtil::ErrorToCanonicalStatus(
          error,
          absl::StrFormat("Cannot drop the write-preventing file attributes of "
                          "%s, whose attrs is %d",
                          filename, *attributes));
    }
  }
  return absl::OkStatus();
}

}  // namespace
#endif  // OS_WIN

bool FileUtil::CreateDirectory(const std::string &path) {
  return FileUtilSingleton::Get()->CreateDirectory(path);
}

bool FileUtilImpl::CreateDirectory(const std::string &path) const {
#if defined(OS_WIN)
  std::wstring wide;
  return (Util::Utf8ToWide(path, &wide) > 0 &&
          ::CreateDirectoryW(wide.c_str(), nullptr) != 0);
#else   // !OS_WIN
  return ::mkdir(path.c_str(), 0700) == 0;
#endif  // OS_WIN
}

bool FileUtil::RemoveDirectory(const std::string &dirname) {
  return FileUtilSingleton::Get()->RemoveDirectory(dirname);
}

bool FileUtilImpl::RemoveDirectory(const std::string &dirname) const {
#ifdef OS_WIN
  std::wstring wide;
  return (Util::Utf8ToWide(dirname, &wide) > 0 &&
          ::RemoveDirectoryW(wide.c_str()) != 0);
#else   // !OS_WIN
  return ::rmdir(dirname.c_str()) == 0;
#endif  // OS_WIN
}

absl::Status FileUtil::Unlink(const std::string &filename) {
  return FileUtilSingleton::Get()->Unlink(filename);
}

absl::Status FileUtilImpl::Unlink(const std::string &filename) const {
#ifdef OS_WIN
  if (absl::Status s = StripWritePreventingAttributesIfExists(filename);
      !s.ok()) {
    return absl::UnknownError(absl::StrFormat(
        "StripWritePreventingAttributesIfExists failed: %s", s.ToString()));
  }
  std::wstring wide;
  if (Util::Utf8ToWide(filename, &wide) <= 0) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }
  if (!::DeleteFileW(wide.c_str())) {
    const DWORD err = ::GetLastError();
    return absl::UnknownError(absl::StrFormat("DeleteFileW failed: %d", err));
  }
  return absl::OkStatus();
#else   // !OS_WIN
  if (::unlink(filename.c_str()) != 0) {
    const int err = errno;
    return absl::UnknownError(
        absl::StrFormat("unlink failed: errno = %d", err));
  }
  return absl::OkStatus();
#endif  // OS_WIN
}

absl::Status FileUtil::UnlinkIfExists(const std::string &filename) {
  absl::Status s = FileExists(filename);
  if (s.ok()) {
    return Unlink(filename);
  }
  if (absl::IsNotFound(s)) {
    return absl::OkStatus();
  }
  return s;
}

void FileUtil::UnlinkOrLogError(const std::string &filename) {
  if (absl::Status s = Unlink(filename); !s.ok()) {
    LOG(ERROR) << "Cannot unlink " << filename << ": " << s;
  }
}

absl::Status FileUtil::FileExists(const std::string &filename) {
  return FileUtilSingleton::Get()->FileExists(filename);
}

absl::Status FileUtilImpl::FileExists(const std::string &filename) const {
#ifdef OS_WIN
  std::wstring wide;
  if (Util::Utf8ToWide(filename, &wide) <= 0) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }
  return GetFileAttributes(wide).status();
#else   // !OS_WIN
  struct stat s;
  if (::stat(filename.c_str(), &s) == 0) {
    return absl::OkStatus();
  }
  const int err = errno;
  return Util::ErrnoToCanonicalStatus(err,
                                      absl::StrCat("Cannot stat ", filename));
#endif  // OS_WIN
}

absl::Status FileUtil::DirectoryExists(const std::string &dirname) {
  return FileUtilSingleton::Get()->DirectoryExists(dirname);
}

absl::Status FileUtilImpl::DirectoryExists(const std::string &dirname) const {
#ifdef OS_WIN
  std::wstring wide;
  if (Util::Utf8ToWide(dirname, &wide) <= 0) {
    return absl::InvalidArgumentError("Utf8ToWide failed");
  }

  absl::StatusOr<DWORD> attrs = GetFileAttributes(wide);
  if (!attrs.ok()) {
    return std::move(attrs).status();
  }
  return (*attrs & FILE_ATTRIBUTE_DIRECTORY) ? absl::OkStatus()
                                             : absl::NotFoundError(dirname);
#else   // !OS_WIN
  struct stat s;
  if (::stat(dirname.c_str(), &s) == 0) {
    return S_ISDIR(s.st_mode)
               ? absl::OkStatus()
               : absl::NotFoundError("Path exists but it's not a directory");
  }
  const int err = errno;
  return Util::ErrnoToCanonicalStatus(err,
                                      absl::StrCat("Cannot stat ", dirname));
#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {

absl::Status TransactionalMoveFile(const std::wstring &from,
                                   const std::wstring &to) {
  constexpr DWORD kTimeout = 5000;  // 5 sec.
  ScopedHandle handle(
      ::CreateTransaction(nullptr, 0, 0, 0, 0, kTimeout, nullptr));
  if (handle.get() == 0) {
    const DWORD create_transaction_error = ::GetLastError();
    return absl::UnknownError(absl::StrFormat("CreateTransaction failed: %d",
                                              create_transaction_error));
  }

  WIN32_FILE_ATTRIBUTE_DATA file_attribute_data = {};
  if (!::GetFileAttributesTransactedW(from.c_str(), GetFileExInfoStandard,
                                      &file_attribute_data, handle.get())) {
    const DWORD get_file_attributes_error = ::GetLastError();
    return absl::UnknownError(absl::StrFormat(
        "GetFileAttributesTransactedW failed: %d", get_file_attributes_error));
  }

  if (!::MoveFileTransactedW(from.c_str(), to.c_str(), nullptr, nullptr,
                             MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING,
                             handle.get())) {
    const DWORD move_file_transacted_error = ::GetLastError();
    return absl::UnknownError(absl::StrFormat("MoveFileTransactedW failed: %d",
                                              move_file_transacted_error));
  }

  if (!::SetFileAttributesTransactedW(
          to.c_str(), file_attribute_data.dwFileAttributes, handle.get())) {
    const DWORD set_file_attributes_error = ::GetLastError();
    return absl::UnknownError(absl::StrFormat(
        "SetFileAttributesTransactedW failed: %d", set_file_attributes_error));
  }

  if (!::CommitTransaction(handle.get())) {
    const DWORD commit_transaction_error = ::GetLastError();
    return absl::UnknownError(absl::StrFormat("CommitTransaction failed: %d",
                                              commit_transaction_error));
  }

  return absl::OkStatus();
}

}  // namespace

bool FileUtil::HideFile(const std::string &filename) {
  return HideFileWithExtraAttributes(filename, 0);
}

bool FileUtil::HideFileWithExtraAttributes(const std::string &filename,
                                           DWORD extra_attributes) {
  if (absl::Status s = FileUtil::FileExists(filename); !s.ok()) {
    LOG(WARNING) << "File not exists: " << filename << ": " << s;
    return false;
  }

  std::wstring wfilename;
  Util::Utf8ToWide(filename, &wfilename);

  const absl::StatusOr<DWORD> original_attributes =
      GetFileAttributes(wfilename);
  if (!original_attributes.ok()) {
    LOG(ERROR) << original_attributes.status();
    return false;
  }
  const auto result = ::SetFileAttributesW(
      wfilename.c_str(), (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                          FILE_ATTRIBUTE_NOT_CONTENT_INDEXED |
                          *original_attributes | extra_attributes) &
                             ~FILE_ATTRIBUTE_NORMAL);
  return result != 0;
}
#endif  // OS_WIN

bool FileUtil::CopyFile(const std::string &from, const std::string &to) {
  return FileUtilSingleton::Get()->CopyFile(from, to);
}

bool FileUtilImpl::CopyFile(const std::string &from,
                            const std::string &to) const {
  InputFileStream ifs(from.c_str(), std::ios::binary);
  if (!ifs) {
    LOG(ERROR) << "Can't open input file. " << from;
    return false;
  }

#ifdef OS_WIN
  std::wstring wto;
  Util::Utf8ToWide(to, &wto);
  StripWritePreventingAttributesIfExists(to);
#endif  // OS_WIN

  OutputFileStream ofs(to.c_str(), std::ios::binary | std::ios::trunc);
  if (!ofs) {
    LOG(ERROR) << "Can't open output file. " << to;
    return false;
  }

  // TODO(taku): we have to check disk quota in advance.
  if (!(ofs << ifs.rdbuf())) {
    LOG(ERROR) << "Can't write data.";
    return false;
  }
  ifs.close();
  ofs.close();

#ifdef OS_WIN
  std::wstring wfrom;
  Util::Utf8ToWide(from, &wfrom);
  absl::StatusOr<DWORD> attrs = GetFileAttributes(wfrom);
  if (attrs.ok()) {
    ::SetFileAttributesW(wto.c_str(), *attrs);
  } else {
    LOG(ERROR) << attrs.status();
  }
#endif  // OS_WIN

  return true;
}

bool FileUtil::IsEqualFile(const std::string &filename1,
                           const std::string &filename2) {
  return FileUtilSingleton::Get()->IsEqualFile(filename1, filename2);
}

bool FileUtilImpl::IsEqualFile(const std::string &filename1,
                               const std::string &filename2) const {
  Mmap mmap1, mmap2;

  if (!mmap1.Open(filename1.c_str(), "r")) {
    LOG(ERROR) << "Cannot open: " << filename1;
    return false;
  }

  if (!mmap2.Open(filename2.c_str(), "r")) {
    LOG(ERROR) << "Cannot open: " << filename2;
    return false;
  }

  if (mmap1.size() != mmap2.size()) {
    return false;
  }

  return memcmp(mmap1.begin(), mmap2.begin(), mmap1.size()) == 0;
}

bool FileUtil::IsEquivalent(const std::string &filename1,
                            const std::string &filename2) {
  return FileUtilSingleton::Get()->IsEquivalent(filename1, filename2);
}

bool FileUtilImpl::IsEquivalent(const std::string &filename1,
                                const std::string &filename2) const {
#ifdef __APPLE__
  // std::filesystem is only available on macOS 10.15, iOS 13.0, or later.
  return false;
#else   // __APPLE__

  // u8path is deprecated in C++20. The current target is C++17.
  const std::filesystem::path src = std::filesystem::u8path(filename1);
  const std::filesystem::path dst = std::filesystem::u8path(filename2);

  std::error_code error_code;
  return std::filesystem::equivalent(src, dst, error_code);
#endif  // __APPLE__
}

absl::Status FileUtil::AtomicRename(const std::string &from,
                                    const std::string &to) {
  return FileUtilSingleton::Get()->AtomicRename(from, to);
}

absl::Status FileUtilImpl::AtomicRename(const std::string &from,
                                        const std::string &to) const {
#ifdef OS_WIN
  std::wstring fromw, tow;
  Util::Utf8ToWide(from, &fromw);
  Util::Utf8ToWide(to, &tow);

  absl::Status s = TransactionalMoveFile(fromw, tow);
  if (s.ok()) {
    return absl::OkStatus();
  }
  LOG(WARNING) << "TransactionalMoveFile failed: from: " << from
               << ", to: " << to << ", status: " << s;

  const absl::StatusOr<DWORD> original_attributes = GetFileAttributes(fromw);
  if (!original_attributes.ok()) {
    return absl::UnknownError(absl::StrFormat(
        "GetFileAttributes failed: %s; Status of TransactionalMoveFile: %s",
        original_attributes.status().ToString(), s.ToString()));
  }
  if (absl::Status s = StripWritePreventingAttributesIfExists(to); !s.ok()) {
    return absl::UnknownError(absl::StrFormat(
        "StripWritePreventingAttributesIfExists failed: %s", s.ToString()));
  }
  if (!::MoveFileExW(fromw.c_str(), tow.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
    const DWORD move_file_ex_error = ::GetLastError();
    return absl::UnknownError(absl::StrFormat(
        "MoveFileExW failed: %d; Status of TransactionalMoveFile: %s",
        move_file_ex_error, s.ToString()));
  }
  if (!::SetFileAttributesW(tow.c_str(), *original_attributes)) {
    const DWORD set_file_attributes_error = ::GetLastError();
    return absl::UnknownError(absl::StrFormat(
        "SetFileAttributesW failed: %d, original_attributes: %d; Status of "
        "TransactionalMoveFile: %s",
        set_file_attributes_error, *original_attributes, s.ToString()));
  }

  return absl::OkStatus();
#else   // !OS_WIN
  // Mac OSX: use rename(2), but rename(2) on Mac OSX
  // is not properly implemented, atomic rename is POSIX spec though.
  // http://www.weirdnet.nl/apple/rename.html
  if (const int r = rename(from.c_str(), to.c_str()); r != 0) {
    const int err = errno;
    return absl::UnknownError(
        absl::StrFormat("errno(%d): %s", err, std::strerror(err)));
  }
  return absl::OkStatus();
#endif  // OS_WIN
}

bool FileUtil::CreateHardLink(const std::string &from, const std::string &to) {
  return FileUtilSingleton::Get()->CreateHardLink(from, to);
}

bool FileUtilImpl::CreateHardLink(const std::string &from,
                                  const std::string &to) {
#ifdef __APPLE__
  // std::filesystem is only available on macOS 10.15, iOS 13.0, or later.
  return false;
#else   // __APPLE__

  // u8path is deprecated in C++20. The current target is C++17.
  const std::filesystem::path src = std::filesystem::u8path(from);
  const std::filesystem::path dst = std::filesystem::u8path(to);

  std::error_code error_code;
  std::filesystem::create_hard_link(src, dst, error_code);
  return !error_code;
#endif  // __APPLE__
}

std::string FileUtil::JoinPath(
    const std::vector<absl::string_view> &components) {
  std::string output;
  JoinPath(components, &output);
  return output;
}

void FileUtil::JoinPath(const std::vector<absl::string_view> &components,
                        std::string *output) {
  output->clear();
  for (size_t i = 0; i < components.size(); ++i) {
    if (components[i].empty()) {
      continue;
    }
    if (!output->empty() && output->back() != kFileDelimiter) {
      output->append(1, kFileDelimiter);
    }
    output->append(components[i].data(), components[i].size());
  }
}

// TODO(taku): what happens if filename == '/foo/bar/../bar/..
std::string FileUtil::Dirname(const std::string &filename) {
  const std::string::size_type p = filename.find_last_of(kFileDelimiter);
  if (p == std::string::npos) {
    return "";
  }
  return filename.substr(0, p);
}

std::string FileUtil::Basename(const std::string &filename) {
  const std::string::size_type p = filename.find_last_of(kFileDelimiter);
  if (p == std::string::npos) {
    return filename;
  }
  return filename.substr(p + 1, filename.size() - p);
}

std::string FileUtil::NormalizeDirectorySeparator(const std::string &path) {
#ifdef OS_WIN
  constexpr char kFileDelimiterForUnix = '/';
  constexpr char kFileDelimiterForWindows = '\\';
  std::string normalized;
  Util::StringReplace(path, std::string(1, kFileDelimiterForUnix),
                      std::string(1, kFileDelimiterForWindows), true,
                      &normalized);
  return normalized;
#else   // OS_WIN
  return path;
#endif  // OS_WIN
}

bool FileUtil::GetModificationTime(const std::string &filename,
                                   FileTimeStamp *modified_at) {
  return FileUtilSingleton::Get()->GetModificationTime(filename, modified_at);
}

bool FileUtilImpl::GetModificationTime(const std::string &filename,
                                       FileTimeStamp *modified_at) const {
#if defined(OS_WIN)
  std::wstring wide;
  if (!Util::Utf8ToWide(filename, &wide)) {
    return false;
  }
  WIN32_FILE_ATTRIBUTE_DATA info = {};
  if (!::GetFileAttributesEx(wide.c_str(), GetFileExInfoStandard, &info)) {
    const auto last_error = ::GetLastError();
    LOG(ERROR) << "GetFileAttributesEx(" << filename
               << ") failed. error=" << last_error;
    return false;
  }
  *modified_at =
      (static_cast<uint64>(info.ftLastWriteTime.dwHighDateTime) << 32) +
      info.ftLastWriteTime.dwLowDateTime;
  return true;
#else   // !OS_WIN
  struct stat stat_info;
  if (::stat(filename.c_str(), &stat_info)) {
    return false;
  }
  *modified_at = stat_info.st_mtime;
  return true;
#endif  // OS_WIN
}

void FileUtil::SetMockForUnitTest(FileUtilInterface *mock) {
  FileUtilSingleton::SetMock(mock);
}

}  // namespace mozc
