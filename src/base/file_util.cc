// Copyright 2010-2013, Google Inc.
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
#include "base/system_util.h"
#include "base/util.h"
#include "base/win_util.h"

namespace {

const char kFileDelimiterForUnix = '/';
const char kFileDelimiterForWindows = '\\';
#ifdef OS_WIN
const char kFileDelimiter = kFileDelimiterForWindows;
#else
const char kFileDelimiter = kFileDelimiterForUnix;
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

#ifndef MOZC_USE_PEPPER_FILE_IO
bool FileUtil::CreateDirectory(const string &path) {
#ifdef OS_WIN
  wstring wide;
  return (Util::UTF8ToWide(path.c_str(), &wide) > 0 &&
          ::CreateDirectoryW(wide.c_str(), NULL) != 0);
#else  // OS_WIN
  return ::mkdir(path.c_str(), 0700) == 0;
#endif  // OS_WIN
}

bool FileUtil::RemoveDirectory(const string &dirname) {
#ifdef OS_WIN
  wstring wide;
  return (Util::UTF8ToWide(dirname.c_str(), &wide) > 0 &&
          ::RemoveDirectoryW(wide.c_str()) != 0);
#else  // OS_WIN
  return ::rmdir(dirname.c_str()) == 0;
#endif  // OS_WIN
}
#endif  // MOZC_USE_PEPPER_FILE_IO

bool FileUtil::Unlink(const string &filename) {
#ifdef OS_WIN
  wstring wide;
  return (Util::UTF8ToWide(filename.c_str(), &wide) > 0 &&
          ::DeleteFileW(wide.c_str()) != 0);
#elif defined(MOZC_USE_PEPPER_FILE_IO)
  return PepperFileUtil::DeleteFile(filename);;
#else  // !OS_WIN && !MOZC_USE_PEPPER_FILE_IO
  return ::unlink(filename.c_str()) == 0;
#endif  // OS_WIN
}

bool FileUtil::FileExists(const string &filename) {
#ifdef OS_WIN
  wstring wide;
  return (Util::UTF8ToWide(filename.c_str(), &wide) > 0 &&
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
  wstring wide;
  if (Util::UTF8ToWide(dirname.c_str(), &wide) <= 0) {
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
typedef HANDLE (WINAPI *FPCreateTransaction)(LPSECURITY_ATTRIBUTES,
                                             LPGUID,
                                             DWORD,
                                             DWORD,
                                             DWORD,
                                             DWORD,
                                             LPWSTR);
typedef BOOL (WINAPI *FPMoveFileTransactedW)(LPCTSTR,
                                             LPCTSTR,
                                             LPPROGRESS_ROUTINE,
                                             LPVOID,
                                             DWORD,
                                             HANDLE);
typedef BOOL (WINAPI *FPCommitTransaction)(HANDLE);

FPCreateTransaction   g_create_transaction    = NULL;
FPMoveFileTransactedW g_move_file_transactedw = NULL;
FPCommitTransaction   g_commit_transaction    = NULL;

static once_t g_init_tx_move_file_once = MOZC_ONCE_INIT;

void InitTxMoveFile() {
  if (!SystemUtil::IsVistaOrLater()) {
    return;
  }

  const HMODULE lib_ktmw = WinUtil::LoadSystemLibrary(L"ktmw32.dll");
  if (lib_ktmw == NULL) {
    LOG(ERROR) << "LoadSystemLibrary for ktmw32.dll failed.";
    return;
  }

  const HMODULE lib_kernel = WinUtil::GetSystemModuleHandle(L"kernel32.dll");
  if (lib_kernel == NULL) {
    LOG(ERROR) << "LoadSystemLibrary for kernel32.dll failed.";
    return;
  }

  g_create_transaction =
      reinterpret_cast<FPCreateTransaction>
      (::GetProcAddress(lib_ktmw, "CreateTransaction"));

  g_move_file_transactedw =
      reinterpret_cast<FPMoveFileTransactedW>
      (::GetProcAddress(lib_kernel, "MoveFileTransactedW"));

  g_commit_transaction =
      reinterpret_cast<FPCommitTransaction>
      (::GetProcAddress(lib_ktmw, "CommitTransaction"));

  LOG_IF(ERROR, g_create_transaction == NULL)
      << "CreateTransaction init failed";
  LOG_IF(ERROR, g_move_file_transactedw == NULL)
      << "MoveFileTransactedW init failed";
  LOG_IF(ERROR, g_commit_transaction == NULL)
      << "CommitTransaction init failed";
}

bool TransactionalMoveFile(const wstring &from, const wstring &to) {
  CallOnce(&g_init_tx_move_file_once, &InitTxMoveFile);

  if (g_commit_transaction == NULL || g_move_file_transactedw == NULL ||
      g_create_transaction == NULL) {
    // Transactional NTFS is not available.
    return false;
  }

  const DWORD kTimeout = 5000;  // 5 sec.
  ScopedHandle handle((*g_create_transaction)(
      NULL, 0, 0, 0, 0, kTimeout, NULL));
  const DWORD create_transaction_error = ::GetLastError();
  if (handle.get() == 0) {
    LOG(ERROR) << "CreateTransaction failed: " << create_transaction_error;
    return false;
  }

  if (!(*g_move_file_transactedw)(from.c_str(), to.c_str(),
                                  NULL, NULL,
                                  MOVEFILE_COPY_ALLOWED |
                                  MOVEFILE_REPLACE_EXISTING,
                                  handle.get())) {
    const DWORD move_file_transacted_error = ::GetLastError();
    LOG(ERROR) << "MoveFileTransactedW failed: "
               << move_file_transacted_error;
    return false;
  }

  if (!(*g_commit_transaction)(handle.get())) {
    const DWORD commit_transaction_error = ::GetLastError();
    LOG(ERROR) << "CommitTransaction failed: " << commit_transaction_error;
    return false;
  }

  return true;
}

}  // namespace
#endif  // OS_WIN

bool FileUtil::CopyFile(const string &from, const string &to) {
  Mmap input;
  if (!input.Open(from.c_str(), "r")) {
    LOG(ERROR) << "Can't open input file. " << from;
    return false;
  }

  OutputFileStream ofs(to.c_str(), ios::binary);
  if (!ofs) {
    LOG(ERROR) << "Can't open output file. " << to;
    return false;
  }

  // TOOD(taku): opening file with mmap could not be
  // a best solution. Also, we have to check disk quota
  // in advance.
  ofs.write(input.begin(), input.size());

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
  wstring fromw, tow;
  Util::UTF8ToWide(from.c_str(), &fromw);
  Util::UTF8ToWide(to.c_str(), &tow);

  if (TransactionalMoveFile(fromw, tow)) {
    return true;
  }

  if (!::MoveFileExW(fromw.c_str(), tow.c_str(),
                     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
    const DWORD move_file_ex_error = ::GetLastError();
    LOG(ERROR) << "MoveFileEx failed: " << move_file_ex_error;
    return false;
  }

  return true;
#elif defined(MOZC_USE_PEPPER_FILE_IO)
  // TODO(horo): PepperFileUtil::RenameFile() is not atomic operation.
  return PepperFileUtil::RenameFile(from, to);
#else  // !OS_WIN && !MOZC_USE_PEPPER_FILE_IO
  // Mac OSX: use rename(2), but rename(2) on Mac OSX
  // is not properly implemented, atomic rename is POSIX spec though.
  // http://www.weirdnet.nl/apple/rename.html
  return rename(from.c_str(), to.c_str()) == 0;
#endif  // OS_WIN
}

string FileUtil::JoinPath(const string &path1, const string &path2) {
  string output;
  JoinPath(path1, path2, &output);
  return output;
}

void FileUtil::JoinPath(const string &path1, const string &path2,
                        string *output) {
  *output = path1;
  if (path1.size() > 0 && path1[path1.size() - 1] != kFileDelimiter) {
    *output += kFileDelimiter;
  }
  *output += path2;
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
  string normalized;
  Util::StringReplace(path, string(1, kFileDelimiterForUnix),
                      string(1, kFileDelimiterForWindows), true, &normalized);
  return normalized;
#else
  return path;
#endif  // OS_WIN
}

}  // namespace mozc
