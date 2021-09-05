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

#ifdef OS_WIN
#include <windows.h>
#if !defined(MOZC_NO_LOGGING)
#include <atlbase.h>
#include <atlstr.h>  // for CString
#endif               // !MOZC_NO_LOGGING
#include <psapi.h>

#include <algorithm>

#include "base/file_util.h"
#include "base/scoped_handle.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/winmain.h"  // use WinMain
#include "server/cache_service_manager.h"

namespace {
HANDLE g_stop_event = NULL;

#if defined(MOZC_NO_LOGGING)
#define LOG_WIN32_ERROR(message)
#else
template <size_t num_elements>
void LogMessageImpl(const wchar_t (&file)[num_elements], int line,
                    const wchar_t *message, int error_no) {
  CString buffer;
  buffer.Format(_T("%s (%d): %s (error: %d)\n"), file, line, message, error_no);
  ::OutputDebugString(buffer);
}
#define LOG_WIN32_ERROR(message) \
  LogMessageImpl(_T(__FILE__), __LINE__, message, ::GetLastError())
#endif

std::wstring GetMappedFileNameByAddress(LPVOID address) {
  wchar_t path[MAX_PATH];
  const int length = ::GetMappedFileName(::GetCurrentProcess(), address, path,
                                         arraysize(path));
  if (length == 0 || arraysize(path) <= length) {
    LOG_WIN32_ERROR(L"GetMappedFileName failed.");
    return L"";
  }
  return std::wstring(path, length);
}

// This function scans each section of a given mapped image section and
// changes memory protection attribute to read-only.
// Returns true if all sections are changed to one read-only region.
// |result_info| contains the memory block information of the combined
// region if succeeds.
bool MakeReadOnlyForMappedModule(LPVOID address,
                                 MEMORY_BASIC_INFORMATION *result_info) {
  // For an IMAGE section, the mapped size may be slightly different from the
  // file size since each PE section should be aligned to the page boundary.
  // To investigate the actual mapped size, we scan each block of memory pages.
  // Fortunately, we can get the source filename of a given virtual address.
  // We can also rely on the MEMORY_BASIC_INFORMATION::Type.  If a given
  // address is inside of a view of an IMAGE section,
  // MEMORY_BASIC_INFORMATION::Type should be MEM_IMAGE.

  // Store the source filename.
  const std::wstring &filename = ::GetMappedFileNameByAddress(address);
  if (filename.empty()) {
    return false;
  }

  void *start_address = NULL;
  DWORD total_region_size = 0;

  void *current = address;
  while (true) {
    MEMORY_BASIC_INFORMATION mem_info;
    if (::VirtualQuery(current, &mem_info, sizeof(mem_info)) == 0) {
      LOG_WIN32_ERROR(L"VirtualQuery failed.");
      return false;
    }
    if (mem_info.Type != MEM_IMAGE) {
      break;
    }
    if (GetMappedFileNameByAddress(current) != filename) {
      break;
    }
    // If this region is not PAGE_READONLY, change the protection to readonly.
    if (mem_info.Protect != PAGE_READONLY) {
      DWORD old_protect = 0;
      if (::VirtualProtect(mem_info.BaseAddress, mem_info.RegionSize,
                           PAGE_READONLY, &old_protect) == 0) {
        LOG_WIN32_ERROR(L"VirtualProtect failed.");
        return false;
      }
    }

    if (start_address == NULL) {
      start_address = mem_info.BaseAddress;
    }
    total_region_size += mem_info.RegionSize;

    current = static_cast<char *>(mem_info.BaseAddress) + mem_info.RegionSize;
  }

  if (result_info != NULL &&
      ::VirtualQuery(address, result_info, sizeof(MEMORY_BASIC_INFORMATION)) ==
          0) {
    LOG_WIN32_ERROR(L"VirtualQuery failed.");
    return false;
  }
  result_info->BaseAddress = start_address;
  result_info->RegionSize = total_region_size;
  return true;
}
}  // namespace

void WINAPI ServiceHandlerProc(DWORD control_code) {
  switch (control_code) {
    // ignores the following code
    case SERVICE_CONTROL_PAUSE:
    case SERVICE_CONTROL_CONTINUE:
    case SERVICE_CONTROL_INTERROGATE:
      break;
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
      if (NULL != g_stop_event) {
        if (!::SetEvent(g_stop_event)) {  // raise event
          // call ExitProcess() if SetEvent failed just in case.
          ::ExitProcess(0);
        }
      } else {
        ::ExitProcess(0);
      }
      break;
    default:
      break;
  }
}

#define STOP_SERVICE_AND_EXIT_FUNCTION()                        \
  do {                                                          \
    service_status.dwCurrentState = SERVICE_STOPPED;            \
    ::SetServiceStatus(service_status_handle, &service_status); \
    return;                                                     \
  } while (false)

#if defined(DEBUG)
// This function try to make a temporary file in the same directory of the
// service if the arguments contains "--verify_privilege_restriction".
// This attempt should fail by ERROR_ACCESS_DENIED since the cache service
// runs under write-restricted SID in Windows Vista or later by default.
// See http://b/2470180 for the whole story.
bool VerifyPrivilegeRestrictionIfNeeded(DWORD dwArgc, LPTSTR *lpszArgv) {
  bool verify_privilege = false;
  const std::wstring test_mode = L"--verify_privilege_restriction";
  for (size_t i = 0; i < dwArgc; ++i) {
    if (test_mode == lpszArgv[i]) {
      verify_privilege = true;
      break;
    }
  }
  if (!verify_privilege) {
    return true;
  }

  const std::string temp_path = mozc::FileUtil::JoinPath(
      mozc::SystemUtil::GetServerDirectory(), "delete_me.txt");
  std::wstring wtemp_path;
  mozc::Util::UTF8ToWide(temp_path, &wtemp_path);
  const HANDLE temp_file =
      ::CreateFileW(wtemp_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NOT_CONTENT_INDEXED |
                        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                    NULL);
  if (temp_file != INVALID_HANDLE_VALUE) {
    ::CloseHandle(temp_file);
    LOG_WIN32_ERROR(L"CreateEvent should have failed but succeeded.");
    return false;
  }
  if (ERROR_ACCESS_DENIED != ::GetLastError()) {
    LOG_WIN32_ERROR(L"Unexpected error code.");
    return false;
  }

  return true;
}
#endif  // DEBUG

VOID WINAPI ServiceMain(DWORD dwArgc, LPTSTR *lpszArgv) {
  SERVICE_STATUS_HANDLE service_status_handle = ::RegisterServiceCtrlHandler(
      mozc::CacheServiceManager::GetServiceName(), ServiceHandlerProc);

  if (NULL == service_status_handle) {
    return;
  }

  SERVICE_STATUS service_status = {0};

  service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status.dwWin32ExitCode = NO_ERROR;
  service_status.dwServiceSpecificExitCode = 0;
  service_status.dwCheckPoint = 0;
  service_status.dwWaitHint = 0;
  service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

  service_status.dwCurrentState = SERVICE_RUNNING;
  ::SetServiceStatus(service_status_handle, &service_status);

#if defined(_DEBUG)
  if (!VerifyPrivilegeRestrictionIfNeeded(dwArgc, lpszArgv)) {
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }
#endif

  if (!mozc::CacheServiceManager::HasEnoughMemory()) {
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  mozc::ScopedHandle stop_event(::CreateEvent(NULL, TRUE, FALSE, NULL));
  g_stop_event = stop_event.get();
  if (NULL == g_stop_event) {
    LOG_WIN32_ERROR(L"CreateEvent failed.");
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  // when low_memory_event is signaled, unkcok the memory image.
  mozc::ScopedHandle low_memory_event(
      ::CreateMemoryResourceNotification(LowMemoryResourceNotification));
  if (NULL == low_memory_event.get()) {
    LOG_WIN32_ERROR(L"CreateMemoryResourceNotification failed.");
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  // when high_memory_event is signaled, lcok the memory image.
  mozc::ScopedHandle high_memory_event(
      ::CreateMemoryResourceNotification(HighMemoryResourceNotification));
  if (NULL == high_memory_event.get()) {
    LOG_WIN32_ERROR(L"CreateMemoryResourceNotification failed.");
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  std::wstring server_path;
  mozc::Util::UTF8ToWide(mozc::SystemUtil::GetServerPath(), &server_path);

  mozc::ScopedHandle file_handle(::CreateFile(server_path.c_str(), GENERIC_READ,
                                              FILE_SHARE_READ, 0, OPEN_EXISTING,
                                              0, 0));
  if (NULL == file_handle.get()) {
    LOG_WIN32_ERROR(L"CreateFile failed.");
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  const size_t size = ::GetFileSize(file_handle.get(), 0);

  // Do not load image if the file size is too big
  constexpr DWORD kMaxImageSize = 100 * 1024 * 1024;  // 100MB
  if (size > kMaxImageSize) {
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  // In general, there are two types of section object for a given file:
  // an IMAGE section and a (normal) MAPPED-FILE section.  The former is used
  // to load an executable file as a module (HMODULE) as opposed to the later
  // which maps a file into the virtual address space as data file.
  // Apparently, multiple IMAGE sections for the same executable share most of
  // memory pages through their mapped view.  This should be also true for
  // multiple MAPPED-FILE sections.  However, can IMAGE section and MAPPED-FILE
  // section for the same file share their memory pages?
  // Anyway we can use SEC_IMAGE to make an IMAGE section for a given file
  // through which we can keep the content of the file on-memory.
  mozc::ScopedHandle mmap_handle(::CreateFileMapping(
      file_handle.get(), NULL, PAGE_READONLY | SEC_IMAGE, 0, 0, NULL));
  if (NULL == mmap_handle.get()) {
    LOG_WIN32_ERROR(L"CreateFileMapping failed.");
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  LPVOID image = reinterpret_cast<LPVOID>(
      ::MapViewOfFile(mmap_handle.get(), FILE_MAP_READ, 0, 0, 0));
  if (NULL == image) {
    LOG_WIN32_ERROR(L"MapViewOfFile failed.");
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  // A mapped view of an IMAGE section may have multiple sections whose
  // protection attributes correspond to the attributes of PE sections.
  // Here is a result of a version of mapped GoogleIMEJaConverter.exe.
  // Address    Type    Size    Protection             Details
  // 00ED0000   Image   48,488  Execute/Copy on Write  GoogleIMEJaConverter.exe
  //  00ED0000  Image   4       Read                   Header
  //  00ED1000  Image   888     Execute/Read           .text
  //  00FAF000  Image   47,316  Read                   .rdata
  //  03DE4000  Image   16      Copy on write          .data
  //  03DE8000  Image   8       Read/Write             .data
  //  03DEA000  Image   4       Read                   .rsrc
  //  03DEB000  Image   252     Read                   .reloc
  // Although an initial protection attribute is not always read-only, you can
  // change each protection to read-only by VirtualProtect API.
  MEMORY_BASIC_INFORMATION mem_info;
  if (!MakeReadOnlyForMappedModule(image, &mem_info)) {
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  constexpr DWORD kMinAdditionalSize = 512 * 1024;       // 512KB
  constexpr DWORD kMaxAdditionalSize = 2 * 1024 * 1024;  // 2MB

  if (!::SetProcessWorkingSetSize(::GetCurrentProcess(),
                                  mem_info.RegionSize + kMinAdditionalSize,
                                  mem_info.RegionSize + kMaxAdditionalSize)) {
    LOG_WIN32_ERROR(L"SetProcessWorkingSetSize failed.");
    STOP_SERVICE_AND_EXIT_FUNCTION();
  }

  DWORD lock_time = 0;
  DWORD unlock_time = 0;

WAIT_HIGH : {
  const HANDLE handles[] = {g_stop_event, high_memory_event.get()};
  const DWORD result =
      ::WaitForMultipleObjects(arraysize(handles), handles, FALSE, INFINITE);
  switch (result) {
    case WAIT_OBJECT_0 + 1:  // high is signaled
      goto TRY_LOCK;
      break;
    case WAIT_OBJECT_0:  // ctrl or error
    default:
      STOP_SERVICE_AND_EXIT_FUNCTION();
  }
}

TRY_LOCK : {
  constexpr int kMaxTimeout = 10 * 60 * 1000;  // 10 min.
  constexpr int kMinTimeout = 1 * 60 * 1000;   // 1min.

  // if the low event is signaled just after calling VirtualLock(),
  // the VirtualLock() itself may indirectly raise the low event.
  // This eventually causes an infinite loop. To prevent this, we check
  // the duration between the last VirtualUnlock() and VirtualLock() and
  // set longer timeout, when the duration is shorter.
  const int duration = static_cast<int>(unlock_time - lock_time);
  const int timeout =
      unlock_time == 0
          ? 0
          : std::max(kMinTimeout, kMaxTimeout - std::max(0, duration));

  const DWORD result = ::WaitForSingleObject(g_stop_event, timeout);
  switch (result) {
    case WAIT_TIMEOUT:
      switch (::WaitForSingleObject(high_memory_event.get(), 0)) {
        case WAIT_OBJECT_0:
          if (::VirtualLock(mem_info.BaseAddress, mem_info.RegionSize) == 0) {
            LOG_WIN32_ERROR(L"VirtualLock failed.");
            STOP_SERVICE_AND_EXIT_FUNCTION();
          }
          lock_time = ::GetTickCount();
          goto WAIT_LOW;
          break;
        case WAIT_TIMEOUT:
          goto TRY_LOCK;
          break;
        default:
          STOP_SERVICE_AND_EXIT_FUNCTION();
      }
      break;
    case WAIT_OBJECT_0:
    default:
      STOP_SERVICE_AND_EXIT_FUNCTION();
  }
}

WAIT_LOW : {
  const HANDLE handles[] = {g_stop_event, low_memory_event.get()};
  const DWORD result =
      ::WaitForMultipleObjects(arraysize(handles), handles, FALSE, INFINITE);
  switch (result) {
    case WAIT_OBJECT_0 + 1:  // low is signaled
      if (!::VirtualUnlock(mem_info.BaseAddress, mem_info.RegionSize)) {
        LOG_WIN32_ERROR(L"VirtualUnlock failed.");
        STOP_SERVICE_AND_EXIT_FUNCTION();
      }
      unlock_time = ::GetTickCount();
      goto WAIT_HIGH;
      break;
    case WAIT_OBJECT_0:  // ctrl or error
    default:
      STOP_SERVICE_AND_EXIT_FUNCTION();
  }
}

  STOP_SERVICE_AND_EXIT_FUNCTION();
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    SERVICE_TABLE_ENTRY kDispatchTable[] = {
        {const_cast<wchar_t *>(mozc::CacheServiceManager::GetServiceName()),
         ServiceMain},
        {NULL, NULL}};

    ::StartServiceCtrlDispatcher(kDispatchTable);
    return 0;
  }
  return 0;
}
#endif
