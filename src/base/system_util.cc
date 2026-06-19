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

#include "base/system_util.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/const.h"
#include "base/environ.h"
#include "base/file_util.h"
#include "base/port.h"

#ifdef __ANDROID__
#include "base/android_util.h"
#endif  // __ANDROID__

#ifdef __APPLE__
#include <TargetConditionals.h>  // for TARGET_OS_*
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <cerrno>

#include "base/mac/mac_util.h"
#endif  // __APPLE__

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <lmcons.h>
#include <sddl.h>
#include <versionhelpers.h>
// clang-format on

#include <memory>  // for unique_ptr
#include <optional>

#include "base/win32/wide_char.h"
#include "base/win32/win_util.h"
#else  // _WIN32
#include <pwd.h>
#include <sys/mman.h>
#include <unistd.h>

#include "absl/container/fixed_array.h"
#endif  // !_WIN32

namespace mozc {
namespace {

#if defined(MOZC_SERVER_DIR)
constexpr absl::string_view kMozcServerDir = MOZC_SERVER_DIR;
#else  // MOZC_SERVER_DIR
constexpr absl::string_view kMozcServerDir = "/usr/lib/mozc";
#endif  // MOZC_SERVER_DIR

#if defined(MOZC_DOCUMENT_DIR)
constexpr absl::string_view kMozcDocumentDir = MOZC_DOCUMENT_DIR;
#else  // MOZC_DOCUMENT_DIR
constexpr absl::string_view kMozcDocumentDir = "/usr/lib/mozc/documents";
#endif  // MOZC_DOCUMENT_DIR

class ProgramInvocationNameHolder final {
 public:
  static ProgramInvocationNameHolder *GetInstance() {
    static absl::NoDestructor<ProgramInvocationNameHolder> impl;
    return impl.get();
  }

  void Set(absl::string_view name) {
    absl::MutexLock l(mutex_);
    name_ = std::string(name);
  }

  std::string Get() const {
    absl::MutexLock l(mutex_);
    return name_;
  }

 private:
  friend class absl::NoDestructor<ProgramInvocationNameHolder>;

  ProgramInvocationNameHolder() = default;
  ~ProgramInvocationNameHolder() = default;

  std::string name_;
  mutable absl::Mutex mutex_;
};

class UserProfileDirectoryImpl final {
 public:
  static UserProfileDirectoryImpl *GetInstance() {
    static absl::NoDestructor<UserProfileDirectoryImpl> impl;
    return impl.get();
  }

  std::string GetDir();
  void SetDir(const std::string& dir);

 private:
  friend class absl::NoDestructor<UserProfileDirectoryImpl>;

  UserProfileDirectoryImpl() = default;
  ~UserProfileDirectoryImpl() = default;

  std::string GetUserProfileDirectory() const;

  std::string dir_;
  absl::Mutex mutex_;
};

std::string UserProfileDirectoryImpl::GetDir() {
  absl::MutexLock l(mutex_);
  if (!dir_.empty()) {
    return dir_;
  }
  const std::string dir = GetUserProfileDirectory();
  if (absl::Status s = FileUtil::CreateDirectory(dir);
      !s.ok() && !absl::IsAlreadyExists(s)) {
    LOG(ERROR) << "Failed to create directory: " << dir << ": " << s;
  }
  if (absl::Status s = FileUtil::DirectoryExists(dir); !s.ok()) {
    LOG(ERROR) << "User profile directory doesn't exist: " << dir << ": " << s;
  }

  dir_ = dir;
  return dir_;
}

void UserProfileDirectoryImpl::SetDir(const std::string& dir) {
  absl::MutexLock l(mutex_);
  dir_ = dir;
}

std::string UserProfileDirectoryImpl::GetUserProfileDirectory() const {
  if constexpr (port::IsChromeos()) {
    // TODO(toka): Must use passed in user profile dir which passed in. If mojo
    // platform the user profile is determined on runtime.
    // It's hack, the user profile dir should be passed in. Although the value
    // in NaCL platform is correct.
    return "/mutable";
  } else if constexpr (port::IsWasm()) {
    // Use a temporary directory as the data is not persisted.
    return "/tmp";
  } else if constexpr (port::IsAndroid()) {
    // For android, we do nothing here because user profile directory,
    // of which the path depends on active user,
    // is injected from Java layer.
    return "";
  } else {
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    // On iOS, use Caches directory instead of Application Support directory
    // because the support directory doesn't exist by default.  Also, it is
    // backed up by iTunes and iCloud.
    return FileUtil::JoinPath({MacUtil::GetCachesDirectory(), kProductPrefix});

#elif defined(_WIN32)
    const std::optional<std::string>& local_app_data =
        WinUtil::GetLocalAppDataPath();
    DCHECK(local_app_data.has_value());
    std::string dir = local_app_data.value_or("");

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    dir = FileUtil::JoinPath(dir, kCompanyNameInEnglish);
    if (absl::Status s = FileUtil::CreateDirectory(dir); !s.ok()) {
      LOG(ERROR) << s;
    }
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
    return FileUtil::JoinPath(dir, kProductNameInEnglish);

#elif defined(TARGET_OS_OSX) && TARGET_OS_OSX
    std::string dir = MacUtil::GetApplicationSupportDirectory();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    dir = FileUtil::JoinPath(dir, "Google");
    // The permission of ~/Library/Application Support/Google seems to be 0755.
    // TODO(komatsu): nice to make a wrapper function.
    ::mkdir(dir.c_str(), 0755);
    return FileUtil::JoinPath(dir, "JapaneseInput");
#else   //  GOOGLE_JAPANESE_INPUT_BUILD
    return FileUtil::JoinPath(dir, "Mozc");
#endif  //  GOOGLE_JAPANESE_INPUT_BUILD

#elif defined(__linux__)
    // 1. If "$HOME/.mozc" already exists,
    //    use "$HOME/.mozc" for backward compatibility.
    // 2. If $XDG_CONFIG_HOME is defined
    //    use "$XDG_CONFIG_HOME/mozc".
    // 3. Otherwise
    //    use "$HOME/.config/mozc" as the default value of $XDG_CONFIG_HOME
    // https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
    const std::string home = Environ::GetEnv("HOME");
    if (home.empty()) {
      char buf[1024];
      struct passwd pw, *ppw;
      const uid_t uid = geteuid();
      CHECK_EQ(0, getpwuid_r(uid, &pw, buf, sizeof(buf), &ppw))
          << "Can't get passwd entry for uid " << uid << ".";
      CHECK_LT(0, strlen(pw.pw_dir))
          << "Home directory for uid " << uid << " is not set.";
      return FileUtil::JoinPath(pw.pw_dir, ".mozc");
    }

    std::string old_dir = FileUtil::JoinPath(home, ".mozc");
    if (FileUtil::DirectoryExists(old_dir).ok()) {
      return old_dir;
    }

    const std::string xdg_config_home = Environ::GetEnv("XDG_CONFIG_HOME");
    if (!xdg_config_home.empty()) {
      return FileUtil::JoinPath(xdg_config_home, "mozc");
    }
    return FileUtil::JoinPath(home, ".config/mozc");

#else  // Supported platforms
    LOG(ERROR) << "Undefined target platform.";
    return "";

#endif  // Platforms
  }
}

}  // namespace

std::string SystemUtil::GetUserProfileDirectory() {
  return UserProfileDirectoryImpl::GetInstance()->GetDir();
}

std::string SystemUtil::GetLoggingDirectory() {
#ifdef __APPLE__
  std::string dir = MacUtil::GetLoggingDirectory();
  if (absl::Status s = FileUtil::CreateDirectory(dir); !s.ok()) {
    LOG(ERROR) << s;
  }
  return dir;
#else   // __APPLE__
  return GetUserProfileDirectory();
#endif  // __APPLE__
}

void SystemUtil::SetUserProfileDirectory(const std::string& path) {
  UserProfileDirectoryImpl::GetInstance()->SetDir(path);
}

#ifdef _WIN32
namespace {
constexpr wchar_t kMozcTipClsid[] =
    L"SOFTWARE\\Classes\\CLSID\\"
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    L"{D5A86FD5-5308-47EA-AD16-9C4EB160EC3C}"
#else   // GOOGLE_JAPANESE_INPUT_BUILD
    L"{10A67BC8-22FA-4A59-90DC-2546652C56BF}"
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
    L"\\InprocServer32";

std::string GetMozcInstallDirFromRegistry() {
  // TSF requires the path of "mozc_tip64.dll" to be registered in the registry,
  // which tells us Mozc's installation directory.
  HKEY key = nullptr;
  LSTATUS result = ::RegOpenKeyExW(HKEY_LOCAL_MACHINE, kMozcTipClsid, 0,
                                   KEY_READ | KEY_WOW64_64KEY, &key);
  if (result != ERROR_SUCCESS) {
    return "";
  }

  DWORD type = 0;
  wchar_t buffer[MAX_PATH] = {};
  DWORD buffer_size = sizeof(buffer);
  result = ::RegQueryValueExW(key, nullptr, nullptr, &type,
                              reinterpret_cast<LPBYTE>(buffer), &buffer_size);
  ::RegCloseKey(key);
  if (result != ERROR_SUCCESS || type != REG_SZ) {
    return "";
  }
  return FileUtil::Dirname(win32::WideToUtf8(buffer));
}

}  // namespace
#endif  // _WIN32

std::string SystemUtil::GetServerDirectory() {
  if constexpr (port::IsLinuxBase() || port::IsWasm()) {
    return std::string(kMozcServerDir);
  }

#ifdef _WIN32
  const std::string install_dir_from_registry = GetMozcInstallDirFromRegistry();
  if (!install_dir_from_registry.empty()) {
    return install_dir_from_registry;
  }
  const std::optional<std::string>& program_files =
      WinUtil::GetProgramFilesX86Path();
  DCHECK(program_files.has_value());
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  return FileUtil::JoinPath(
      FileUtil::JoinPath(program_files.value_or(""), kCompanyNameInEnglish),
      kProductNameInEnglish);
#else   // GOOGLE_JAPANESE_INPUT_BUILD
  return FileUtil::JoinPath(program_files.value_or(""), kProductNameInEnglish);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#elif defined(__APPLE__)
  return MacUtil::GetServerDirectory();

#else  // !_WIN32 && !__APPLE__
  return "";
#endif  // !_WIN32 && !__APPLE__
}

std::string SystemUtil::GetServerPath() {
  const std::string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcServerName);
}

std::string SystemUtil::GetRendererPath() {
  const std::string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcRenderer);
}

std::string SystemUtil::GetToolPath() {
  const std::string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcTool);
}

std::string SystemUtil::GetDocumentDirectory() {
  if constexpr (port::IsLinuxBase()) {
    return std::string(kMozcDocumentDir);
  } else if constexpr (port::IsAppleBase()) {
    return GetServerDirectory();
  } else {
    return FileUtil::JoinPath(GetServerDirectory(), "documents");
  }
}

std::string SystemUtil::GetUserNameAsString() {
#if defined(_WIN32)
  wchar_t wusername[UNLEN + 1];
  DWORD name_size = UNLEN + 1;
  // Call the same name Windows API.  (include Advapi32.lib).
  // TODO(komatsu, yukawa): Add error handling.
  // TODO(komatsu, yukawa): Consider the case where the current thread is
  //   or will be impersonated.
  const BOOL result = ::GetUserName(wusername, &name_size);
  DCHECK_NE(FALSE, result);
  return win32::WideToUtf8(wusername);
#else   // _WIN32
  int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (bufsize == -1) {
    // Not all linux systems have _SC_GETPW_R_SIZE_MAX.
    // e.g. musl libc returns -1.
    bufsize = 1024;
  }
  absl::FixedArray<char> buf(bufsize);
  struct passwd pw, *ppw;
  CHECK_EQ(0, getpwuid_r(geteuid(), &pw, buf.data(), buf.size(), &ppw));
  return pw.pw_name;
#endif  // !_WIN32
}

#ifdef _WIN32
namespace {

class UserSidImpl {
 public:
  static UserSidImpl *GetInstance() {
    static absl::NoDestructor<UserSidImpl> impl;
    return impl.get();
  }
  const std::string& get() { return sid_; }

 private:
  friend class absl::NoDestructor<UserSidImpl>;

  UserSidImpl();

  std::string sid_;
};

UserSidImpl::UserSidImpl() {
  HANDLE htoken = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &htoken)) {
    sid_ = SystemUtil::GetUserNameAsString();
    LOG(ERROR) << "OpenProcessToken failed: " << ::GetLastError();
    return;
  }

  DWORD length = 0;
  ::GetTokenInformation(htoken, TokenUser, nullptr, 0, &length);
  auto buf = std::make_unique<char[]>(length);
  PTOKEN_USER p_user = reinterpret_cast<PTOKEN_USER>(buf.get());

  if (length == 0 ||
      !::GetTokenInformation(htoken, TokenUser, p_user, length, &length)) {
    ::CloseHandle(htoken);
    sid_ = SystemUtil::GetUserNameAsString();
    LOG(ERROR) << "OpenTokenInformation failed: " << ::GetLastError();
    return;
  }

  LPTSTR p_sid_user_name;
  if (!::ConvertSidToStringSidW(p_user->User.Sid, &p_sid_user_name)) {
    ::CloseHandle(htoken);
    sid_ = SystemUtil::GetUserNameAsString();
    LOG(ERROR) << "ConvertSidToStringSidW failed: " << ::GetLastError();
    return;
  }

  sid_ = win32::WideToUtf8(p_sid_user_name);

  ::LocalFree(p_sid_user_name);
  ::CloseHandle(htoken);
}

}  // namespace
#endif  // _WIN32

std::string SystemUtil::GetUserSidAsString() {
#ifdef _WIN32
  return UserSidImpl::GetInstance()->get();
#else   // _WIN32
  return GetUserNameAsString();
#endif  // _WIN32
}

#ifdef _WIN32
namespace {

std::string GetObjectNameAsString(HANDLE handle) {
  if (handle == nullptr) {
    LOG(ERROR) << "Unknown handle";
    return "";
  }

  DWORD size = 0;
  if (::GetUserObjectInformationA(handle, UOI_NAME, nullptr, 0, &size) ||
      ERROR_INSUFFICIENT_BUFFER != ::GetLastError()) {
    LOG(ERROR) << "GetUserObjectInformationA() failed: " << ::GetLastError();
    return "";
  }

  if (size == 0) {
    LOG(ERROR) << "buffer size is 0";
    return "";
  }

  auto buf = std::make_unique<char[]>(size);
  DWORD return_size = 0;
  if (!::GetUserObjectInformationA(handle, UOI_NAME, buf.get(), size,
                                   &return_size)) {
    LOG(ERROR) << "::GetUserObjectInformationA() failed: " << ::GetLastError();
    return "";
  }

  if (return_size <= 1) {
    LOG(ERROR) << "result buffer size is too small";
    return "";
  }

  char* result = buf.get();
  result[return_size - 1] = '\0';  // just make sure nullptr terminated

  return result;
}

bool GetCurrentSessionId(uint32_t* session_id) {
  DCHECK(session_id);
  *session_id = 0;
  DWORD id = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &id)) {
    LOG(ERROR) << "cannot get session id: " << ::GetLastError();
    return false;
  }
  static_assert(sizeof(DWORD) == sizeof(uint32_t),
                "DWORD and uint32_t must be equivalent");
  *session_id = static_cast<uint32_t>(id);
  return true;
}

// Here we use the input desktop instead of the desktop associated with the
// current thread. One reason behind this is that some applications such as
// Adobe Reader XI use multiple desktops in a process. Basically the input
// desktop is most appropriate and important desktop for our use case.
// See
// http://blogs.adobe.com/asset/2012/10/new-security-capabilities-in-adobe-reader-and-acrobat-xi-now-available.html
std::string GetInputDesktopName() {
  const HDESK desktop_handle =
      ::OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
  if (desktop_handle == nullptr) {
    return "";
  }
  const std::string desktop_name = GetObjectNameAsString(desktop_handle);
  ::CloseDesktop(desktop_handle);
  return desktop_name;
}

std::string GetProcessWindowStationName() {
  // We must not close the returned value of GetProcessWindowStation().
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683225.aspx
  const HWINSTA window_station = ::GetProcessWindowStation();
  if (window_station == nullptr) {
    return "";
  }

  return GetObjectNameAsString(window_station);
}

std::string GetSessionIdString() {
  uint32_t session_id = 0;
  if (!GetCurrentSessionId(&session_id)) {
    return "";
  }
  return std::to_string(session_id);
}

}  // namespace
#endif  // _WIN32

std::string SystemUtil::GetDesktopNameAsString() {
#if defined(__linux__) || defined(__wasm__)
  return Environ::GetEnv("DISPLAY");
#endif  // __linux__ || __wasm__

#if defined(__APPLE__)
  return "";
#endif  // __APPLE__

#if defined(_WIN32)
  const std::string& session_id = GetSessionIdString();
  if (session_id.empty()) {
    DLOG(ERROR) << "Failed to retrieve session id";
    return "";
  }

  const std::string& window_station_name = GetProcessWindowStationName();
  if (window_station_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve window station name";
    return "";
  }

  const std::string& desktop_name = GetInputDesktopName();
  if (desktop_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve desktop name";
    return "";
  }

  return absl::StrCat(session_id, ".", window_station_name, ".", desktop_name);
#endif  // _WIN32
}

#ifdef _WIN32
const wchar_t* SystemUtil::GetSystemDir() {
  const std::optional<std::wstring>& dir = WinUtil::GetSystem32Path();
  DCHECK(dir.has_value());
  return dir.has_value() ? dir->c_str() : nullptr;
}
#endif  // _WIN32

// TODO(toshiyuki): move this to the initialization module and calculate
// version only when initializing.
std::string SystemUtil::GetOSVersionString() {
#ifdef _WIN32
  OSVERSIONINFOEX osvi = {0};
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi))) {
    return absl::StrCat("Windows.", osvi.dwMajorVersion, ".",
                        osvi.dwMinorVersion, ".", osvi.wServicePackMajor, ".",
                        osvi.wServicePackMinor);
  }
  LOG(WARNING) << "GetVersionEx failed";
  return "Windows";
#elif defined(__APPLE__)
  // TODO(toshiyuki): get more specific info
  return absl::StrCat("MacOSX ", MacUtil::GetOSVersionString());
#elif defined(__ANDROID__)
  return absl::StrCat("Android ",
                      AndroidUtil::GetSystemProperty(
                          AndroidUtil::kSystemPropertyOsVersion, "Unknown"));
#elif defined(__linux__)
  return "Linux";
#else   // !_WIN32 && !__APPLE__ && !__linux__
  return "Unknown";
#endif  // _WIN32, __APPLE__, __linux__
}

void SystemUtil::DisableIME() {
#ifdef _WIN32
  // Note that ImmDisableTextFrameService API is no longer supported on
  // Windows Vista and later.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd318537.aspx
  ::ImmDisableIME(-1);
#endif  // _WIN32
}

uint64_t SystemUtil::GetTotalPhysicalMemory() {
#if defined(_WIN32)
  MEMORYSTATUSEX memory_status = {sizeof(MEMORYSTATUSEX)};
  if (!::GlobalMemoryStatusEx(&memory_status)) {
    return 0;
  }
  return memory_status.ullTotalPhys;
#endif  // _WIN32

#if defined(__APPLE__)
  int mib[] = {CTL_HW, HW_MEMSIZE};
  uint64_t total_memory = 0;
  size_t size = sizeof(total_memory);
  const int error =
      sysctl(mib, std::size(mib), &total_memory, &size, nullptr, 0);
  if (error == -1) {
    const int error = errno;
    LOG(ERROR) << "sysctl with hw.memsize failed. " << "errno: " << error;
    return 0;
  }
  return total_memory;
#endif  // __APPLE__

#if defined(__linux__) || defined(__wasm__)
#if defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  const int32_t page_size = sysconf(_SC_PAGESIZE);
  const int32_t number_of_phyisical_pages = sysconf(_SC_PHYS_PAGES);
  if (number_of_phyisical_pages < 0) {
    // likely to be overflowed.
    LOG(FATAL) << number_of_phyisical_pages << ", " << page_size;
    return 0;
  }
  return static_cast<uint64_t>(number_of_phyisical_pages) * page_size;
#else   // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  return 0;
#endif  // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
#endif  // __linux__ || __wasm__

  // If none of the above platforms is specified, the compiler raises an error
  // because of no return value.
}

void SystemUtil::SetProgramInvocationName(absl::string_view name) {
  ProgramInvocationNameHolder::GetInstance()->Set(name);
}

std::string SystemUtil::GetProgramRunfilesDirectory() {
  if constexpr (port::IsWasm()) {
    return "/";
  }
  const std::string name = ProgramInvocationNameHolder::GetInstance()->Get();
  if (name.empty()) {
    return "";
  }
  return absl::StrCat(name, ".runfiles");
}

}  // namespace mozc
