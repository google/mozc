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

#include "base/mmap.h"

#ifdef OS_WIN
#include <Windows.h>
#include <string>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // OS_WIN

#include <cstring>

#include "base/port.h"
#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"

#ifdef OS_WIN
#include "base/scoped_handle.h"
#endif  // OS_WIN

#ifdef MOZC_USE_PEPPER_FILE_IO
#include "base/pepper_file_util.h"
#endif  // MOZC_USE_PEPPER_FILE_IO

namespace mozc {

#ifndef MOZC_USE_PEPPER_FILE_IO

Mmap::Mmap() : text_(NULL), size_(0) {
}

#ifdef OS_WIN

bool Mmap::Open(const char *filename, const char *mode) {
  Close();
  uint32 mode1, mode2, mode3, mode4;
  if (strcmp(mode, "r") == 0) {
    mode1 = GENERIC_READ;
    mode2 = PAGE_READONLY;
    mode3 = FILE_MAP_READ;
    mode4 = FILE_SHARE_READ;
  } else if (strcmp(mode, "r+") == 0) {
    mode1 = GENERIC_READ | GENERIC_WRITE;
    mode2 = PAGE_READWRITE;
    mode3 = FILE_MAP_ALL_ACCESS;
    mode4 = FILE_SHARE_READ | FILE_SHARE_WRITE;
  } else {
    LOG(ERROR) << "unknown open mode:" << filename;
    return false;
  }

  wstring filename_wide;
  if (Util::UTF8ToWide(filename, &filename_wide) <= 0) {
    return false;
  }

  ScopedHandle handle(
      ::CreateFileW(filename_wide.c_str(), mode1, mode4,
                    0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0));
  if (handle.get() == NULL) {
    LOG(ERROR) << "CreateFile() failed: " << filename;
    return false;
  }

  ScopedHandle map_handle(
      ::CreateFileMapping(handle.get(), 0, mode2, 0, 0, 0));
  if (map_handle.get() == NULL) {
    LOG(ERROR) << "CreateFileMapping() failed: " << filename;
    return false;
  }

  void *ptr = ::MapViewOfFile(map_handle.get(), mode3, 0, 0, 0);
  if (ptr == NULL) {
    LOG(ERROR) << "MapViewOfFile() failed: " << filename;
    return false;
  }

  text_ = reinterpret_cast<char *>(ptr);
  size_ = ::GetFileSize(handle.get(), 0);

  return true;
}

void Mmap::Close() {
  if (text_ != NULL) {
    ::UnmapViewOfFile(text_);
  }

  text_ = NULL;
  size_ = 0;
}

#else  // OS_WIN

#ifndef O_BINARY
#define O_BINARY 0
#endif

namespace {
class ScopedCloser {
 public:
  explicit ScopedCloser(int fd) : fd_(fd) {
  }
  ~ScopedCloser() {
    ::close(fd_);
  }
 private:
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCloser);
};
}  // namespace

bool Mmap::Open(const char *filename, const char *mode) {
  Close();

  int flag;
  if (strcmp(mode, "r") == 0) {
    flag = O_RDONLY;
  } else if (strcmp(mode, "r+") == 0) {
    flag = O_RDWR;
  } else {
    LOG(WARNING) << "unknown open mode: " << filename;
    return false;
  }

  int fd = ::open(filename, flag | O_BINARY);
  if (fd < 0) {
    LOG(WARNING) << "open failed: " << filename;
    return false;
  }
  ScopedCloser closer(fd);

  struct stat st;
  if (fstat(fd, &st) < 0) {
    LOG(WARNING) << "failed to get file size: " << filename;
    return false;
  }

  int prot = PROT_READ;
  if (flag == O_RDWR) {
    prot |= PROT_WRITE;
  }

  void *ptr = mmap(0, st.st_size, prot, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED) {
    LOG(WARNING) << "mmap() failed: " << filename;
    return false;
  }

  SystemUtil::MaybeMLock(ptr, size_);
  text_ = reinterpret_cast<char *>(ptr);
  size_ = st.st_size;
  return true;
}

void Mmap::Close() {
  if (text_ != NULL) {
    SystemUtil::MaybeMUnlock(text_, size_);
    munmap(reinterpret_cast<char *>(text_), size_);
  }

  text_ = NULL;
  size_ = 0;
}
#endif  // OS_WIN

#else  // MOZC_USE_PEPPER_FILE_IO

Mmap::Mmap() : write_mode_(false), size_(0) {
}

bool Mmap::Open(const char *filename, const char *mode) {
  Close();

  if (strcmp(mode, "r") == 0) {
    write_mode_ = false;
  } else if (strcmp(mode, "r+") == 0) {
    write_mode_ = true;
  } else {
    LOG(WARNING) << "unknown open mode: " << filename;
    return false;
  }
  string buffer;
  if (!PepperFileUtil::ReadBinaryFile(filename, &buffer)) {
    LOG(WARNING) << "read failed: " << filename;
    return false;
  }
  if (buffer.empty()) {
    LOG(WARNING) << "file size = 0: " << filename;
    return false;
  }
  size_ = buffer.size();
  text_.reset(new char[size_]);
  buffer.copy(text_.get(), size_);
  filename_ = filename;
  if (write_mode_) {
    PepperFileUtil::RegisterMmap(this);
  }
  return true;
}

void Mmap::Close() {
  if (write_mode_) {
    PepperFileUtil::UnRegisterMmap(this);
    if (!PepperFileUtil::WriteBinaryFile(filename_,
                                         string(text_.get(), size_))) {
      LOG(WARNING) << "write failed: " << filename_;
    }
  }
  text_.reset();
  size_ = 0;
}

bool Mmap::SyncToFile() {
  if (!write_mode_) {
    LOG(ERROR) << "Mmap::SyncToFile error. This file is opened in read mode: "
               << filename_;
    return false;
  }
  return PepperFileUtil::WriteBinaryFile(filename_,
                                         string(text_.get(), size_));
}
#endif  // MOZC_USE_PEPPER_FILE_IO

}  // namespace mozc
