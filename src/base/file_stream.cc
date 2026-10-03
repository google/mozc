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

#include "base/file_stream.h"

#include <fstream>
#include <ios>

#include "absl/strings/string_view.h"
#include "base/strings/pfchar.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <wil/resource.h>
#include <windows.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <string>
#endif  // _WIN32

namespace mozc {

#ifdef _WIN32
namespace {

// Maps a Win32 error code to errno so that the callers of open() can rely on
// errno on failure as they can with std::ifstream.
int Win32ErrorToErrno(DWORD error) {
  switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
      return ENOENT;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
      return EACCES;
    case ERROR_TOO_MANY_OPEN_FILES:
      return EMFILE;
    default:
      return EINVAL;
  }
}

// Opens |filename| for reading with FILE_SHARE_DELETE so that the file can be
// deleted or replaced (see FileUtil::AtomicRename()) while it is being read, as
// on POSIX. This cannot be done through std::ifstream because the CRT never
// specifies FILE_SHARE_DELETE. Returns nullptr and sets errno on failure.
FILE* OpenForSharedReadForWin(const std::wstring& filename, bool binary) {
  wil::unique_hfile handle(
      ::CreateFileW(filename.c_str(), GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!handle) {
    errno = Win32ErrorToErrno(::GetLastError());
    return nullptr;
  }
  const int fd = ::_open_osfhandle(reinterpret_cast<intptr_t>(handle.get()),
                                   _O_RDONLY | (binary ? _O_BINARY : _O_TEXT));
  if (fd == -1) {
    return nullptr;  // errno is set by _open_osfhandle().
  }
  // The file descriptor now owns the handle.
  handle.release();
  FILE* file = ::_fdopen(fd, binary ? "rb" : "rt");
  if (file == nullptr) {
    const int err = errno;  // set by _fdopen().
    ::_close(fd);
    errno = err;
    return nullptr;
  }
  return file;
}

}  // namespace
#endif  // _WIN32

InputFileStream::InputFileStream(absl::string_view filename,
                                 std::ios_base::openmode mode) {
  open(filename, mode);
}

InputFileStream::~InputFileStream() {
#ifdef _WIN32
  // Explicitly closes the file because the destructor of std::filebuf does not
  // close a FILE* attached in open(). is_open() is false if the stream has
  // already been closed by close().
  if (is_open()) {
    close();
  }
#endif  // _WIN32
}

void InputFileStream::open(absl::string_view filename,
                           std::ios_base::openmode mode) {
#ifdef _WIN32
  constexpr std::ios_base::openmode kSupportedModes =
      std::ios_base::in | std::ios_base::binary | std::ios_base::ate;
  if ((mode & ~kSupportedModes) != 0) {
    // Falls back to std::ifstream for unusual modes.
    std::ifstream::open(to_pfstring(filename), mode);
    return;
  }
  if (is_open()) {
    setstate(std::ios_base::failbit);  // as std::ifstream::open() does.
    return;
  }
  FILE* file = OpenForSharedReadForWin(to_pfstring(filename),
                                       (mode & std::ios_base::binary) != 0);
  if (file == nullptr) {
    setstate(std::ios_base::failbit);
    return;
  }
  // Note that std::filebuf(FILE*) is a MSVC STL extension to attaches |file| to
  // this stream. Keep in mind that the destructor of std::filebuf does not
  // close a FILE* attached this way. See ~InputFileStream().
  std::filebuf buf(file);
  rdbuf()->swap(buf);
  clear();
  if ((mode & std::ios_base::ate) != 0) {
    seekg(0, std::ios_base::end);
  }
#else   // !_WIN32
  std::ifstream::open(to_pfstring(filename), mode);
#endif  // _WIN32
}

OutputFileStream::OutputFileStream(absl::string_view filename,
                                   std::ios_base::openmode mode) {
  open(filename, mode);
}

void OutputFileStream::open(absl::string_view filename,
                            std::ios_base::openmode mode) {
  // to_pfstring() changes encoding to utf-16 on Windows.
  std::ofstream::open(to_pfstring(filename), mode);
}

// Common implementations.

void InputFileStream::UnusedKeyMethod() {}   // go/definekeymethod
void OutputFileStream::UnusedKeyMethod() {}  // go/definekeymethod

}  // namespace mozc
