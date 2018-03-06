// Copyright 2010-2018, Google Inc.
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
#include <Windows.h>
#include <KtmW32.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // OS_WIN

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/mutex.h"
#ifdef MOZC_USE_PEPPER_FILE_IO
#include "base/pepper_file_util.h"
#endif  // MOZC_USE_PEPPER_FILE_IO
#include "base/scoped_handle.h"
#include "base/util.h"
#include "base/win_util.h"

namespace {

#ifdef OS_WIN
const char kFileDelimiter = '\\';
#else
const char kFileDelimiter = '/';
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

#ifdef OS_WIN
namespace {

// Some high-level file APIs such as MoveFileEx simply fail if the target file
// has some special attribute like read-only. This method tries to strip system,
// hidden, and read-only attributes from |filename|.
// This function does nothing if |filename| does not exist.
void StripWritePreventingAttributesIfExists(const string &filename) {
  if (!FileUtil::FileExists(filename)) {
    return;
  }
  std::wstring wide_filename;
  Util::UTF8ToWide(filename, &wide_filename);
  const DWORD kDropAttributes =
      FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY;
  const DWORD attributes = ::GetFileAttributesW(wide_filename.c_str());
  if (attributes & kDropAttributes) {
    ::SetFileAttributesW(wide_filename.c_str(), attributes & ~kDropAttributes);
  }
}

}  // namespace
#endif  // OS_WIN

bool FileUtil::CreateDirectory(const string &path) {
#if defined(OS_WIN)
  std::wstring wide;
  return (Util::UTF8ToWide(path, &wide) > 0 &&
          ::CreateDirectoryW(wide.c_str(), nullptr) != 0);
#elif defined(OS_NACL)  // OS_WIN
  return PepperFileUtil::CreateDirectory(path);
#else  // OS_WIN or OS_NACL
  return ::mkdir(path.c_str(), 0700) == 0;
#endif  // OS_WIN or OS_NACL
}

bool FileUtil::RemoveDirectory(const string &dirname) {
#ifdef OS_WIN
  std::wstring wide;
  return (Util::UTF8ToWide(dirname, &wide) > 0 &&
          ::RemoveDirectoryW(wide.c_str()) != 0);
#elif defined(OS_NACL)  // OS_WIN
  return PepperFileUtil::Delete(dirname);
#else  // OS_WIN or OS_NACL
  return ::rmdir(dirname.c_str()) == 0;
#endif  // OS_WIN or OS_NACL
}

bool FileUtil::Unlink(const string &filename) {
#ifdef OS_WIN
  StripWritePreventingAttributesIfExists(filename);
  std::wstring wide;
  return (Util::UTF8ToWide(filename, &wide) > 0 &&
          ::DeleteFileW(wide.c_str()) != 0);
#elif defined(MOZC_USE_PEPPER_FILE_IO)
  return PepperFileUtil::Delete(filename);
#else  // !OS_WIN && !MOZC_USE_PEPPER_FILE_IO
  return ::unlink(filename.c_str()) == 0;
#endif  // OS_WIN
}

bool FileUtil::FileExists(const string &filename) {
#ifdef OS_WIN
  std::wstring wide;
  return (Util::UTF8ToWide(filename, &wide) > 0 &&
          ::GetFileAttributesW(wide.c_str()) != -1);
#elif defined(MOZC_USE_PEPPER_FILE_IO)
  return PepperFileUtil::FileExists(filename);
#else  // !OS_WIN && !MOZC_USE_PEPPER_FILE_IO
  struct stat s;
  return ::stat(filename.c_str(), &s) == 0;
#endif  // OS_WIN
}

bool FileUtil::DirectoryExists(const string &dirname) {
#ifdef OS_WIN
  std::wstring wide;
  if (Util::UTF8ToWide(dirname, &wide) <= 0) {
    return false;
  }

  const DWORD attribute = ::GetFileAttributesW(wide.c_str());
  return ((attribute != -1) &&
          (attribute & FILE_ATTRIBUTE_DIRECTORY));
#elif defined(MOZC_USE_PEPPER_FILE_IO)
  return PepperFileUtil::DirectoryExists(dirname);
#else  // !OS_WIN && !MOZC_USE_PEPPER_FILE_IO
  struct stat s;
  return (::stat(dirname.c_str(), &s) == 0 && S_ISDIR(s.st_mode));
#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {

bool TransactionalMoveFile(const std::wstring &from, const std::wstring &to) {
  const DWORD kTimeout = 5000;  // 5 sec.
  ScopedHandle handle(::CreateTransaction(
      nullptr, 0, 0, 0, 0, kTimeout, nullptr));
  const DWORD create_transaction_error = ::GetLastError();
  if (handle.get() == 0) {
    LOG(ERROR) << "CreateTransaction failed: " << create_transaction_error;
    return false;
  }

  WIN32_FILE_ATTRIBUTE_DATA file_attribute_data = {};
  if (!::GetFileAttributesTransactedW(from.c_str(), GetFileExInfoStandard,
                                      &file_attribute_data, handle.get())) {
    const DWORD get_file_attributes_error = ::GetLastError();
    LOG(ERROR) << "GetFileAttributesTransactedW failed: "
               << get_file_attributes_error;
    return false;
  }

  if (!::MoveFileTransactedW(from.c_str(), to.c_str(), nullptr, nullptr,
                             MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING,
                             handle.get())) {
    const DWORD move_file_transacted_error = ::GetLastError();
    LOG(ERROR) << "MoveFileTransactedW failed: "
               << move_file_transacted_error;
    return false;
  }

  if (!::SetFileAttributesTransactedW(
          to.c_str(), file_attribute_data.dwFileAttributes, handle.get())) {
    const DWORD set_file_attributes_error = ::GetLastError();
    LOG(ERROR) << "SetFileAttributesTransactedW failed: "
               << set_file_attributes_error;
    return false;
  }

  if (!::CommitTransaction(handle.get())) {
    const DWORD commit_transaction_error = ::GetLastError();
    LOG(ERROR) << "CommitTransaction failed: " << commit_transaction_error;
    return false;
  }

  return true;
}

}  // namespace

bool FileUtil::HideFile(const string &filename) {
  return HideFileWithExtraAttributes(filename, 0);
}

bool FileUtil::HideFileWithExtraAttributes(const string &filename,
                                           DWORD extra_attributes) {
  if (!FileUtil::FileExists(filename)) {
    LOG(WARNING) << "File not exists. " << filename;
    return false;
  }

  std::wstring wfilename;
  Util::UTF8ToWide(filename, &wfilename);

  const DWORD original_attributes = ::GetFileAttributesW(wfilename.c_str());
  const auto result = ::SetFileAttributesW(
      wfilename.c_str(),
      (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
       FILE_ATTRIBUTE_NOT_CONTENT_INDEXED | original_attributes |
       extra_attributes) & ~FILE_ATTRIBUTE_NORMAL);
  return result != 0;
}
#endif  // OS_WIN

bool FileUtil::CopyFile(const string &from, const string &to) {
  InputFileStream ifs(from.c_str(), std::ios::binary);
  if (!ifs) {
    LOG(ERROR) << "Can't open input file. " << from;
    return false;
  }

#ifdef OS_WIN
  std::wstring wto;
  Util::UTF8ToWide(to, &wto);
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
  Util::UTF8ToWide(from, &wfrom);
  ::SetFileAttributesW(wto.c_str(), ::GetFileAttributesW(wfrom.c_str()));
#endif  // OS_WIN

  return true;
}

bool FileUtil::IsEqualFile(const string &filename1,
                           const string &filename2) {
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

bool FileUtil::AtomicRename(const string &from, const string &to) {
#ifdef OS_WIN
  std::wstring fromw, tow;
  Util::UTF8ToWide(from, &fromw);
  Util::UTF8ToWide(to, &tow);

  if (TransactionalMoveFile(fromw, tow)) {
    return true;
  }

  const DWORD original_attributes = ::GetFileAttributesW(fromw.c_str());
  StripWritePreventingAttributesIfExists(to);
  if (!::MoveFileExW(fromw.c_str(), tow.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
    const DWORD move_file_ex_error = ::GetLastError();
    LOG(ERROR) << "MoveFileEx failed: " << move_file_ex_error;
    return false;
  }
  ::SetFileAttributesW(tow.c_str(), original_attributes);

  return true;
#elif defined(MOZC_USE_PEPPER_FILE_IO)
  // TODO(horo): PepperFileUtil::Rename() is not atomic operation.
  return PepperFileUtil::Rename(from, to);
#else  // !OS_WIN && !MOZC_USE_PEPPER_FILE_IO
  // Mac OSX: use rename(2), but rename(2) on Mac OSX
  // is not properly implemented, atomic rename is POSIX spec though.
  // http://www.weirdnet.nl/apple/rename.html
  return rename(from.c_str(), to.c_str()) == 0;
#endif  // OS_WIN
}

string FileUtil::JoinPath(const std::vector<StringPiece> &components) {
  string output;
  JoinPath(components, &output);
  return output;
}

void FileUtil::JoinPath(const std::vector<StringPiece> &components,
                        string *output) {
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
string FileUtil::Dirname(const string &filename) {
  const string::size_type p = filename.find_last_of(kFileDelimiter);
  if (p == string::npos) {
    return "";
  }
  return filename.substr(0, p);
}

string FileUtil::Basename(const string &filename) {
  const string::size_type p = filename.find_last_of(kFileDelimiter);
  if (p == string::npos) {
    return filename;
  }
  return filename.substr(p + 1, filename.size() - p);
}

string FileUtil::NormalizeDirectorySeparator(const string &path) {
#ifdef OS_WIN
  const char kFileDelimiterForUnix = '/';
  const char kFileDelimiterForWindows = '\\';
  string normalized;
  Util::StringReplace(path, string(1, kFileDelimiterForUnix),
                      string(1, kFileDelimiterForWindows), true, &normalized);
  return normalized;
#else
  return path;
#endif  // OS_WIN
}

bool FileUtil::GetModificationTime(const string &filename,
                                   FileTimeStamp *modified_at) {
#if defined (OS_WIN)
  std::wstring wide;
  if (!Util::UTF8ToWide(filename, &wide)) {
    return false;
  }
  WIN32_FILE_ATTRIBUTE_DATA info = {};
  if (!::GetFileAttributesEx(wide.c_str(), GetFileExInfoStandard, &info)) {
    const auto last_error = ::GetLastError();
    LOG(ERROR) << "GetFileAttributesEx(" << filename << ") failed. error="
               << last_error;
    return false;
  }
  *modified_at =
      (static_cast<uint64>(info.ftLastWriteTime.dwHighDateTime) << 32)
      + info.ftLastWriteTime.dwLowDateTime;
  return true;
#elif defined(OS_NACL)
  PP_FileInfo file_info;
  if (!PepperFileUtil::Query(filename, &file_info)) {
    return false;
  }
  *modified_at = file_info.last_modified_time;
  return true;
#else  // OS_WIN or OS_NACL
  struct stat stat_info;
  if (::stat(filename.c_str(), &stat_info)) {
    return false;
  }
  *modified_at = stat_info.st_mtime;
  return true;
#endif  // OS_WIN or OS_NACL
}

}  // namespace mozc
