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

#include <cstddef>
#include <optional>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "base/strings/zstring_view.h"

#ifdef __APPLE__
#include <TargetConditionals.h>  // for TARGET_OS_IPHONE
#endif                           // __APPLE__

#ifdef _WIN32
#include <wil/resource.h>
#include <windows.h>

#include <limits>

#include "base/win32/wide_char.h"
#else  // _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#endif  // _WIN32

namespace mozc {
namespace {

// In what follows, the following helper functions are defined that wrap
// platform dependent system calls:
//
//      SyscallParams: Parameters for system calls.
//     FileDescriptor: A native file handle.
//           FdCloser: RAII for FileDescriptor.
// GetSyscallParams(): Converts Mmap::Mode value to parameters of file APIs.
//         OpenFile(): Opens a file and returns FileDescriptor.
//      GetFileSize(): Gets the file size.
//      GetPageSize(): Gets the number satisfying mmap alignment.
//          MapFile(): Performs mmap.
//            Unmap(): Releases a mmap.
#ifdef _WIN32

struct SyscallParams {
  DWORD desired_access;      // for CreateFileW()
  DWORD share_mode;          // for CreateFileW()
  DWORD protect;             // for CreateFileMapping()
  DWORD map_desired_access;  // for MapViewOfFile()
};

using FileDescriptor = HANDLE;
using FdCloser = wil::unique_hfile;

absl::StatusOr<SyscallParams> GetSyscallParams(Mmap::Mode mode) {
  SyscallParams params;
  switch (mode) {
    case Mmap::READ_ONLY:
      params.desired_access = GENERIC_READ;
      params.share_mode = FILE_SHARE_READ;
      params.protect = PAGE_READONLY;
      params.map_desired_access = FILE_MAP_READ;
      break;
    case Mmap::READ_WRITE:
      params.desired_access = GENERIC_READ | GENERIC_WRITE;
      params.share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
      params.protect = PAGE_READWRITE;
      params.map_desired_access = FILE_MAP_ALL_ACCESS;
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid mode %d", mode));
  }
  return params;
}

absl::StatusOr<FileDescriptor> OpenFile(zstring_view filename,
                                        const SyscallParams& params) {
  const std::wstring filename_w = win32::Utf8ToWide(filename);
  if (filename_w.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid file name: %v", filename));
  }
  FileDescriptor fd = ::CreateFileW(filename_w.c_str(), params.desired_access,
                                    params.share_mode, 0, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
  if (fd == nullptr) {
    return absl::UnknownError(absl::StrFormat(
        "Error %d: CreateFileW failed for %v", GetLastError(), filename));
  }
  return fd;
}

absl::StatusOr<size_t> GetFileSize(FileDescriptor fd) {
  const DWORD size = ::GetFileSize(fd, 0);
  if (size == INVALID_FILE_SIZE) {
    return absl::UnknownError(
        absl::StrFormat("Error %d: GetFileSize failed", GetLastError()));
  }
  return size;
}

absl::StatusOr<size_t> GetPageSize() {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  // We should use `dwAllocationGranularity` instead of `dwPageSize` because
  // `MapViewOfFile()` requires the offset to be a multiple of the allocation
  // granularity.
  return info.dwAllocationGranularity;
}

constexpr std::pair<DWORD, DWORD> GetHiAndLo(size_t value) {
  if constexpr (sizeof(size_t) <= sizeof(DWORD)) {
    return {0, value};
  } else {
    static_assert(sizeof(size_t) <= 2 * sizeof(DWORD));
    const DWORD hi = value >> (CHAR_BIT * sizeof(DWORD));
    const DWORD lo = value & std::numeric_limits<DWORD>::max();
    return {hi, lo};
  }
}

absl::StatusOr<void*> MapFile(FileDescriptor fd, size_t offset, size_t size,
                              const SyscallParams& params) {
  const auto [max_size_hi, max_size_lo] = GetHiAndLo(size);
  wil::unique_handle handle(::CreateFileMapping(
      fd, 0, params.protect, max_size_hi, max_size_lo, nullptr));
  if (!handle) {
    return absl::UnknownError(
        absl::StrFormat("Error %d: CreateFileMapping failed", GetLastError()));
  }

  const auto [offset_hi, offset_lo] = GetHiAndLo(offset);
  const LPVOID ptr = ::MapViewOfFile(handle.get(), params.map_desired_access,
                                     offset_hi, offset_lo, size);
  if (ptr == nullptr) {
    return absl::UnknownError(
        absl::StrFormat("Error %d: MapViewOfFile failed", GetLastError()));
  }
  return ptr;
}

void Unmap(void* ptr, size_t /*unused_size*/) {
  if (::UnmapViewOfFile(ptr) == 0) {
    LOG(ERROR) << "Failed to unmap a view of file";
  }
}

#else  // _WIN32

struct SyscallParams {
  int flags;  // for open()
  int prot;   // for mmap()
};

using FileDescriptor = int;

class FdCloser final {
 public:
  explicit FdCloser(FileDescriptor fd) : fd_{fd} {}

  FdCloser(const FdCloser&) = delete;
  FdCloser& operator=(const FdCloser&) = delete;

  FdCloser(FdCloser&&) = delete;
  FdCloser& operator=(FdCloser&&) = delete;

  ~FdCloser() { ::close(fd_); }

 private:
  FileDescriptor fd_;
};

#ifndef O_BINARY
#define O_BINARY 0
#endif  // O_BINARY

absl::StatusOr<SyscallParams> GetSyscallParams(Mmap::Mode mode) {
  SyscallParams params;
  switch (mode) {
    case Mmap::READ_ONLY:
      params.flags = O_RDONLY | O_BINARY;
      params.prot = PROT_READ;
      break;
    case Mmap::READ_WRITE:
      params.flags = O_RDWR | O_BINARY;
      params.prot = PROT_READ | PROT_WRITE;
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown mode: %d", mode));
  }
  return params;
}

absl::StatusOr<FileDescriptor> OpenFile(zstring_view filename,
                                        const SyscallParams& params) {
  const FileDescriptor fd = ::open(filename.c_str(), params.flags);
  if (fd == -1) {
    const int err = errno;
    return absl::ErrnoToStatus(
        err, absl::StrFormat("Failed to open %v with flags %d", filename,
                             params.flags));
  }
  return fd;
}

absl::StatusOr<size_t> GetFileSize(FileDescriptor fd) {
  struct stat st;
  if (fstat(fd, &st) == -1) {
    return absl::ErrnoToStatus(errno, "fstat failed");
  }
  return st.st_size;
}

absl::StatusOr<size_t> GetPageSize() {
  const long size = sysconf(_SC_PAGESIZE);  // NOLINT
  if (size == -1) {
    return absl::ErrnoToStatus(errno, "sysconf(_SC_PAGESIZE) failed");
  }
  return size;
}

absl::StatusOr<void*> MapFile(FileDescriptor fd, size_t offset, size_t size,
                              const SyscallParams& params) {
  void* const ptr = mmap(nullptr, size, params.prot, MAP_SHARED, fd, offset);
  if (ptr == MAP_FAILED) {
    return absl::ErrnoToStatus(errno, "mmap() failed");
  }
  return ptr;
}

void Unmap(void* ptr, size_t size) {
  if (munmap(ptr, size) == -1) {
    LOG(ERROR) << absl::ErrnoToStatus(errno, "munmap() failed");
  }
}

#endif  // _WIN32

}  // namespace

absl::StatusOr<Mmap> Mmap::Map(zstring_view filename, size_t offset,
                               std::optional<size_t> size, Mode mode) {
  absl::StatusOr<SyscallParams> params = GetSyscallParams(mode);
  if (!params.ok()) {
    return std::move(params).status();
  }

  absl::StatusOr<FileDescriptor> fd = OpenFile(filename, *params);
  if (!fd.ok()) {
    return std::move(fd).status();
  }
  const FdCloser closer(*fd);

  // If the size is not given, calculate the mapping size from file size and
  // offset.
  if (!size.has_value()) {
    absl::StatusOr<size_t> file_size = GetFileSize(*fd);
    if (!file_size.ok()) {
      return std::move(file_size).status();
    }
    if (offset > *file_size) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "offset %d exceeds the file size %d", offset, *file_size));
    }
    size = *file_size - offset;
  }
  if (*size == 0) {
    return absl::InvalidArgumentError("Mapping of zero byte is invalid");
  }

  absl::StatusOr<size_t> page_size = GetPageSize();
  if (!page_size.ok()) {
    return std::move(page_size).status();
  }

  // Mmap offset must be a multiple of page size. Therefore, we adjust the mmap
  // start offset to the nearest page boundary before `offset`.
  const size_t adjust = offset % *page_size;
  const size_t map_offset = offset - adjust;
  const size_t map_size = *size + adjust;

  absl::StatusOr<void*> ptr = MapFile(*fd, map_offset, map_size, *params);
  if (!ptr.ok()) {
    return std::move(ptr).status();
  }

  MaybeMLock(*ptr, map_size);

  Mmap mmap;
  mmap.data_ = absl::MakeSpan(static_cast<char*>(*ptr) + adjust, *size);
  mmap.adjust_ = adjust;
  return mmap;
}

Mmap::Mmap(Mmap&& x) : data_{x.data_}, adjust_{x.adjust_} {
  x.data_ = absl::Span<char>();
  x.adjust_ = 0;
}

Mmap& Mmap::operator=(Mmap&& x) {
  Close();
  data_ = x.data_;
  adjust_ = x.adjust_;
  x.data_ = absl::Span<char>();
  x.adjust_ = 0;
  return *this;
}

void Mmap::Close() {
  if (data_.data() != nullptr) {
    void* const ptr = data_.data() - adjust_;
    const size_t map_size = data_.size() + adjust_;
    MaybeMUnlock(ptr, map_size);
    Unmap(ptr, map_size);
  }
  data_ = absl::Span<char>();
  adjust_ = 0;
}

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

int Mmap::MaybeMLock(const void* addr, size_t len) { return mlock(addr, len); }

int Mmap::MaybeMUnlock(const void* addr, size_t len) {
  return munlock(addr, len);
}

#else   // MOZC_HAVE_MLOCK

bool Mmap::IsMLockSupported() { return false; }

int Mmap::MaybeMLock(const void* addr, size_t len) { return -1; }

int Mmap::MaybeMUnlock(const void* addr, size_t len) { return -1; }
#endif  // MOZC_HAVE_MLOCK

#undef MOZC_HAVE_MLOCK

}  // namespace mozc
