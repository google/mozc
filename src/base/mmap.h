// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_BASE_MMAP_H_
#define MOZC_BASE_MMAP_H_

extern "C" {
#ifdef OS_WINDOWS
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
}
#include <string>
#include "base/base.h"
#include "base/util.h"

// Change HAVE_MMAP 0 when target os has no mmap call
#define HAVE_MMAP 1

#ifndef O_BINARY
#define O_BINARY 0
#endif

namespace mozc {

template <class T> class Mmap {
 public:
  T&       operator[](size_t n)       { return *(text_ + n); }
  const T& operator[](size_t n) const { return *(text_ + n); }
  T*       begin()           { return text_; }
  const T* begin()    const  { return text_; }
  T*       end()             { return text_ + Size(); }
  const T* end()    const    { return text_ + Size(); }
  int      Size()            { return length_/sizeof(T); }
  int      GetFileSize()     { return length_; }
  bool     Empty()           { return (length_ == 0); }

#ifdef OS_WINDOWS
  bool Open(const char *filename, const char *mode = "r") {
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

    handle_ = ::CreateFileW(filename_wide.c_str(), mode1, mode4,
                            0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle_ == INVALID_HANDLE_VALUE) {
      LOG(ERROR) << "CreateFile() failed: " << filename;
      return false;
    }

    length_ = ::GetFileSize(handle_, 0);

    map_handle_ = ::CreateFileMapping(handle_, 0, mode2, 0, 0, 0);
    if (map_handle_ == NULL) {
      LOG(ERROR) << "CreateFileMapping() failed: " << filename;
      return false;
    }

    text_ = reinterpret_cast<T *>(::MapViewOfFile(map_handle_, mode3, 0, 0, 0));
    if (text_ == NULL) {
      LOG(WARNING) << "MapViewOfFile() failed: " << filename;
      return false;
    }

    return true;
  }

  void Close() {
    if (text_ != NULL) {
      ::UnmapViewOfFile(text_);
    }
    if (handle_ != INVALID_HANDLE_VALUE) {
      ::CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
    if (map_handle_ != NULL) {
      ::CloseHandle(map_handle_);
      map_handle_ = NULL;
    }
    text_ = NULL;
  }

  Mmap() : text_(NULL), length_(0),
           handle_(INVALID_HANDLE_VALUE), map_handle_(NULL) {}

#else  // OS_WINDOWS

  bool Open(const char *filename, const char *mode = "r") {
    Close();
    struct stat st;

    if (strcmp(mode, "r") == 0) {
      flag_ = O_RDONLY;
    } else if (strcmp(mode, "r+") == 0) {
      flag_ = O_RDWR;
    } else {
      LOG(WARNING) << "unknown open mode: " << filename;
      return false;
    }

    if ((fd_ = ::open(filename, flag_ | O_BINARY)) < 0) {
      LOG(WARNING) << "open failed: " << filename;
      return false;
    }

    if (fstat(fd_, &st) < 0) {
      LOG(WARNING) << "failed to get file size: " << filename;
      return false;
    }

    length_ = st.st_size;

#ifdef HAVE_MMAP
    int prot = PROT_READ;
    if (flag_ == O_RDWR) {
      prot |= PROT_WRITE;
    }

    char *p = NULL;
    if ((p = reinterpret_cast<char *>
         (mmap(0, length_, prot, MAP_SHARED, fd_, 0))) == MAP_FAILED) {
      LOG(WARNING) << "mmap() failed: " << filename;
      return false;
    }

    // We don't check the return value because it fails in some environments.
    // For linux, in the kernel version >= 2.6.9, user process can mlock.
    // In older kernel, it fails if the process is running in user priviledge.
#ifndef OS_WINDOWS
    mlock(p, length_);
#endif
    text_ = reinterpret_cast<T *>(p);
#else
    text_ = new T[length_];
    if (read(fd_, text_, length_) <= 0) {
      LOG(WARNING) << "read() failed: " << filename;
      return false;
    }
#endif
    ::close(fd_);
    fd_ = -1;

    return true;
  }

  void Close() {
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }

    if (text_ != NULL) {
#ifdef HAVE_MMAP
#ifndef OS_WINDOWS
      // Again, we don't check the return value here.
      munlock(text_, length_);
#endif  // OS_WINDOWS
      munmap(reinterpret_cast<char *>(text_), length_);
      text_ = NULL;
#else  // HAVE_MMAP
      if (flag_ == O_RDWR) {
        int fd2;
        if ((fd2 = ::open(fileName.c_str(), O_RDWR)) >= 0) {
          write(fd2, text_, length_);
          ::close(fd2);
        }
      }
      delete [] text_;
#endif  // HAVE_MMAP
    }

    text_ = NULL;
  }

  Mmap(): text_(NULL), length_(0), fd_(-1) {}
#endif   // OS_WINDOWS

  virtual ~Mmap() {
    Close();
  }

 private:
  T   *text_;
  int length_;

#ifdef OS_WINDOWS
  HANDLE handle_;
  HANDLE map_handle_;
#else  // OS_WINDOWS
  int fd_;
  int flag_;
#endif  // OS_WINDOWS
};
}  // namespace mozc

#endif  // MOZC_BASE_MMAP_H_
