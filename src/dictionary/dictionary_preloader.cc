// Copyright 2010, Google Inc.
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

#include "dictionary/dictionary_preloader.h"

#if defined(OS_WINDOWS)
#include <process.h>
#include <windows.h>
#include <CommCtrl.h>  // for CCSIZEOF_STRUCT
#else
#include <pthread.h>   // for pthread_create
#endif

#ifdef OS_MACOSX
#include <mach/mach_host.h>
#endif

#include "base/util.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

DEFINE_int32(preload_memory_factor, 5,
             "A factor to be multiplied to the preload size "
             "and compared with available system memory."
             "Preload is enabled if available system memory is "
             "large enough");

namespace mozc {
namespace {
bool IsPreloadable(const char *image, size_t size) {
  if (!GET_CONFIG(use_dictionary_suggest)) {
    return false;
  }

  const int64 preload_size = size;

#ifdef OS_WINDOWS
  MEMORYSTATUSEX status;
  ::ZeroMemory(&status, sizeof(status));
  status.dwLength = CCSIZEOF_STRUCT(MEMORYSTATUSEX, ullAvailExtendedVirtual);
  if (!::GlobalMemoryStatusEx(&status)) {
    LOG(ERROR) <<
        "GlobalMemoryStatusEx failed. error = " << ::GetLastError();
    return false;
  }

  return status.ullAvailPhys > preload_size * FLAGS_preload_memory_factor;
#endif

#ifdef OS_MACOSX
  struct vm_statistics vm_info;
  mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
  if (KERN_SUCCESS !=
      host_statistics(mach_host_self(), HOST_VM_INFO,
                      reinterpret_cast<host_info_t>(&vm_info), &count)) {
    LOG(ERROR) << "host_statistics() failed: ";
    return false;
  }

  const uint64 available_memory = vm_info.free_count * vm_page_size;
  return available_memory > preload_size * FLAGS_preload_memory_factor;
#endif

#ifdef OS_LINUX
  // TOOD(taku): implement Linux version.
  // Since Linux is installed into heterogeneous environment,
  // we have to check the amount of available memory.
  LOG(WARNING) << "Dictionary preloading is not implemented: " << preload_size;
  return true;
  //  return false;
#endif
}

// Note: this thread proc may be terminated by the end of main thread.
class PreloaderThread : public Thread {
 public:
  PreloaderThread() : image_(NULL), size_(0) {}

  ~PreloaderThread() {
    Join();
  }

  void StartPreloader(const char *image, size_t size) {
    if (IsRunning()) {
      LOG(WARNING) << "Preloader is already running";
      return;
    }
    image_ = image;
    size_ = size;
    Thread::Start();
  }

  void Run() {
#ifdef OS_WINDOWS
    // GetCurrentThread returns pseudo handle, which you need not
    // to pass CloseHandle.
    const HANDLE thread_handle = ::GetCurrentThread();

    // Enter low priority mode.
    if (Util::IsVistaOrLater()) {
      // THREAD_MODE_BACKGROUND_BEGIN is beneficial for the preloader since
      // all I/Os occurred in the background-mode thread are marked as
      // "Low-Priority" so that the activity of the preloader is less likely
      // to interrupt normal I/O tasks.
      // Note that "all I/Os" includes implicit page-fault I/Os, which is
      // what the preloader aims to do.
      ::SetThreadPriority(thread_handle, THREAD_MODE_BACKGROUND_BEGIN);
    } else {
      ::SetThreadPriority(thread_handle, THREAD_PRIORITY_IDLE);
    }
#endif

    if (image_ == NULL || size_ == 0) {
      LOG(ERROR) << "image is NULL or size is 0";
      return;
    }

    // Preleoad dictionary region.
    // TODO(yukawa): determine the best region to load.
    Util::PreloadMappedRegion(image_, size_, NULL);

    VLOG(1) << "Preloader done!";
  }

 private:
  const char *image_;
  size_t size_;
};
}  // anonymous namespace

void DictionaryPreloader::PreloadIfApplicable(const char *image, size_t size) {
  // On Windows, dictionary preloader is no longer enabled because
  // GoogleIMEJaCacheService.exe is responsible for keeping the dictionary
  // on-page (or freeing the memory in low-memory condition).
  // See http://b/2354549 for details.
#if defined(OS_MACOSX) || defined(OS_LINUX)
  if (!IsPreloadable(image, size)) {
    return;
  }

  Singleton<PreloaderThread>::get()->StartPreloader(image, size);
#endif  // OS_MACOSX or OS_LINUX
}
}  // namespace mozc
