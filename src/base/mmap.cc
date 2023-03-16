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

#include "base/mmap.h"

#include <cstdint>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "absl/cleanup/cleanup.h"

#ifdef __APPLE__
#include <TargetConditionals.h>  // for TARGET_OS_IPHONE
#endif                           // __APPLE__

#ifdef _WIN32
#include <windows.h>

#include "base/win32/scoped_handle.h"
#include "base/win32/wide_char.h"
#else  // _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _WIN32

namespace mozc {

#ifdef _WIN32

bool Mmap::Open(const absl::string_view filename,
                const absl::string_view mode) {
  Close();
  uint32_t mode1, mode2, mode3, mode4;
  if (mode == "r") {
    mode1 = GENERIC_READ;
    mode2 = PAGE_READONLY;
    mode3 = FILE_MAP_READ;
    mode4 = FILE_SHARE_READ;
  } else if (mode == "r+") {
    mode1 = GENERIC_READ | GENERIC_WRITE;
    mode2 = PAGE_READWRITE;
    mode3 = FILE_MAP_ALL_ACCESS;
    mode4 = FILE_SHARE_READ | FILE_SHARE_WRITE;
  } else {
    LOG(ERROR) << "unknown open mode:" << filename;
    return false;
  }

  const std::wstring filename_wide = win32::Utf8ToWide(filename);
  if (filename_wide.empty()) {
    return false;
  }

  ScopedHandle handle(::CreateFileW(filename_wide.c_str(), mode1, mode4, 0,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
  if (handle.get() == nullptr) {
    LOG(ERROR) << "CreateFile() failed: " << filename;
    return false;
  }

  ScopedHandle map_handle(::CreateFileMapping(handle.get(), 0, mode2, 0, 0, 0));
  if (map_handle.get() == nullptr) {
    LOG(ERROR) << "CreateFileMapping() failed: " << filename;
    return false;
  }

  void *ptr = ::MapViewOfFile(map_handle.get(), mode3, 0, 0, 0);
  if (ptr == nullptr) {
    LOG(ERROR) << "MapViewOfFile() failed: " << filename;
    return false;
  }

  data_ = absl::MakeSpan(reinterpret_cast<char *>(ptr),
                         ::GetFileSize(handle.get(), 0));

  return true;
}

void Mmap::Close() {
  if (data_.data() != nullptr) {
    ::UnmapViewOfFile(data_.data());
  }

  data_ = absl::Span<char>();
}

#else  // !_WIN32

#ifndef O_BINARY
#define O_BINARY 0
#endif  // O_BINARY

bool Mmap::Open(const absl::string_view filename,
                const absl::string_view mode) {
  Close();

  int flag;
  if (mode == "r") {
    flag = O_RDONLY;
  } else if (mode == "r+") {
    flag = O_RDWR;
  } else {
    LOG(WARNING) << "unknown open mode: " << filename;
    return false;
  }

  const std::string filename_s(filename);
  const int fd = ::open(filename_s.data(), flag | O_BINARY);
  if (fd < 0) {
    LOG(WARNING) << "open failed: " << filename;
    return false;
  }
  absl::Cleanup closer = [fd] { ::close(fd); };

  struct stat st;
  if (fstat(fd, &st) < 0) {
    LOG(WARNING) << "failed to get file size: " << filename;
    return false;
  }

  int prot = PROT_READ;
  if (flag == O_RDWR) {
    prot |= PROT_WRITE;
  }

  void *ptr = mmap(nullptr, st.st_size, prot, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    LOG(WARNING) << "mmap() failed: " << filename;
    return false;
  }

  MaybeMLock(ptr, st.st_size);
  data_ = absl::MakeSpan(reinterpret_cast<char *>(ptr), st.st_size);
  return true;
}

void Mmap::Close() {
  if (data_.data() != nullptr) {
    MaybeMUnlock(data_.data(), data_.size());
    munmap(data_.data(), data_.size());
  }

  data_ = absl::Span<char>();
}
#endif  // !_WIN32

// Define a macro (MOZC_HAVE_MLOCK) to indicate mlock support.

#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE) || \
    defined(_WIN32)
#define MOZC_HAVE_MLOCK 0
#else  // __ANDROID__ || TARGET_OS_IPHONE || _WIN32
#define MOZC_HAVE_MLOCK 1
#endif  // __ANDROID__ || TARGET_OS_IPHONE || _WIN32


#ifndef MOZC_HAVE_MLOCK
#error "MOZC_HAVE_MLOCK is not defined"
#endif  // MOZC_HAVE_MLOCK

#if MOZC_HAVE_MLOCK
bool Mmap::IsMLockSupported() { return true; }

int Mmap::MaybeMLock(const void *addr, size_t len) { return mlock(addr, len); }

int Mmap::MaybeMUnlock(const void *addr, size_t len) {
  return munlock(addr, len);
}

#else   // MOZC_HAVE_MLOCK

bool Mmap::IsMLockSupported() { return false; }

int Mmap::MaybeMLock(const void *addr, size_t len) { return -1; }

int Mmap::MaybeMUnlock(const void *addr, size_t len) { return -1; }
#endif  // MOZC_HAVE_MLOCK

#undef MOZC_HAVE_MLOCK

}  // namespace mozc
