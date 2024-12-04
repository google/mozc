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

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "base/const.h"
#include "base/environ.h"
#include "base/file_util.h"
#include "base/singleton.h"

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
#include <shlobj.h>
#include <versionhelpers.h>
// clang-format on

#include <memory>  // for unique_ptr

#include "absl/strings/str_cat.h"
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

class UserProfileDirectoryImpl final {
 public:
  UserProfileDirectoryImpl() = default;
  ~UserProfileDirectoryImpl() = default;

  std::string GetDir();
  void SetDir(const std::string &dir);

 private:
  std::string GetUserProfileDirectory() const;

  std::string dir_;
  absl::Mutex mutex_;
};

std::string UserProfileDirectoryImpl::GetDir() {
  absl::MutexLock l(&mutex_);
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

void UserProfileDirectoryImpl::SetDir(const std::string &dir) {
  absl::MutexLock l(&mutex_);
  dir_ = dir;
}

#ifdef _WIN32
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class LocalAppDataDirectoryCache {
 public:
  LocalAppDataDirectoryCache() : result_(E_FAIL) {
    result_ = SafeTryGetLocalAppData(&path_);
  }
  HRESULT result() const { return result_; }
  const bool succeeded() const { return SUCCEEDED(result_); }
  const std::string &path() const { return path_; }

 private:
  // b/5707813 implies that TryGetLocalAppData causes an exception and makes
  // Singleton<LocalAppDataDirectoryCache> invalid state which results in an
  // infinite spin loop in call_once. To prevent this, the constructor of
  // LocalAppDataDirectoryCache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryGetLocalAppData.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryGetLocalAppData(std::string *dir) {
    __try {
      return TryGetLocalAppData(dir);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryGetLocalAppData(std::string *dir) {
    if (dir == nullptr) {
      return E_FAIL;
    }
    dir->clear();

    bool in_app_container = false;
    if (!WinUtil::IsProcessInAppContainer(::GetCurrentProcess(),
                                          &in_app_container)) {
      return E_FAIL;
    }
    if (in_app_container) {
      return TryGetLocalAppDataForAppContainer(dir);
    }
    return TryGetLocalAppDataLow(dir);
  }

  static HRESULT TryGetLocalAppDataForAppContainer(std::string *dir) {
    // User profiles for processes running under AppContainer seem to be as
    // follows, while the scheme is not officially documented.
    //   "%LOCALAPPDATA%\Packages\<package sid>\..."
    // Note: You can also obtain this path by GetAppContainerFolderPath API.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/hh448543.aspx
    // Anyway, here we use heuristics to obtain "LocalLow" folder path.
    // TODO(yukawa): Establish more reliable way to obtain the path.
    wchar_t config[MAX_PATH] = {};
    const HRESULT result = ::SHGetFolderPathW(
        nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, &config[0]);
    if (FAILED(result)) {
      return result;
    }
    std::wstring path = config;
    const std::wstring::size_type local_pos = path.find(L"\\Packages\\");
    if (local_pos == std::wstring::npos) {
      return E_FAIL;
    }
    path.erase(local_pos);
    path += L"Low";
    *dir = win32::WideToUtf8(path);
    if (dir->empty()) {
      return E_FAIL;
    }
    return S_OK;
  }

  static HRESULT TryGetLocalAppDataLow(std::string *dir) {
    if (dir == nullptr) {
      return E_FAIL;
    }
    dir->clear();

    wchar_t *task_mem_buffer = nullptr;
    const HRESULT result = ::SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0,
                                                  nullptr, &task_mem_buffer);
    if (FAILED(result)) {
      if (task_mem_buffer != nullptr) {
        ::CoTaskMemFree(task_mem_buffer);
      }
      return result;
    }

    if (task_mem_buffer == nullptr) {
      return E_UNEXPECTED;
    }

    std::wstring wpath = task_mem_buffer;
    ::CoTaskMemFree(task_mem_buffer);

    const std::string path = win32::WideToUtf8(wpath);
    if (path.empty()) {
      return E_UNEXPECTED;
    }

    *dir = path;
    return S_OK;
  }

  HRESULT result_;
  std::string path_;
};
#endif  // _WIN32

std::string UserProfileDirectoryImpl::GetUserProfileDirectory() const {
#if defined(OS_CHROMEOS)
  // TODO(toka): Must use passed in user profile dir which passed in. If mojo
  // platform the user profile is determined on runtime.
  // It's hack, the user profile dir should be passed in. Although the value in
  // NaCL platform is correct.
  return "/mutable";

#elif defined(__wasm__)
  // Do nothing for WebAssembly.
  return "";

#elif defined(__ANDROID__)
  // For android, we do nothing here because user profile directory,
  // of which the path depends on active user,
  // is injected from Java layer.
  return "";

#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
  // On iOS, use Caches directory instead of Application Spport directory
  // because the support directory doesn't exist by default.  Also, it is backed
  // up by iTunes and iCloud.
  return FileUtil::JoinPath({MacUtil::GetCachesDirectory(), kProductPrefix});

#elif defined(_WIN32)
  DCHECK(SUCCEEDED(Singleton<LocalAppDataDirectoryCache>::get()->result()));
  std::string dir = Singleton<LocalAppDataDirectoryCache>::get()->path();

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
#error Undefined target platform.

#endif  // Platforms
}

}  // namespace

std::string SystemUtil::GetUserProfileDirectory() {
  return Singleton<UserProfileDirectoryImpl>::get()->GetDir();
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

void SystemUtil::SetUserProfileDirectory(const std::string &path) {
  Singleton<UserProfileDirectoryImpl>::get()->SetDir(path);
}

#ifdef _WIN32
namespace {
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class ProgramFilesX86Cache {
 public:
  ProgramFilesX86Cache() : result_(E_FAIL) {
    result_ = SafeTryProgramFilesPath(&path_);
  }
  const bool succeeded() const { return SUCCEEDED(result_); }
  const HRESULT result() const { return result_; }
  const std::string &path() const { return path_; }

 private:
  // b/5707813 implies that the Shell API causes an exception in some cases.
  // In order to avoid potential infinite loops in call_once. the constructor
  // of ProgramFilesX86Cache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryProgramFilesPath.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryProgramFilesPath(
      std::string *path) {
    __try {
      return TryProgramFilesPath(path);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryProgramFilesPath(std::string *path) {
    if (path == nullptr) {
      return E_FAIL;
    }
    path->clear();

    wchar_t program_files_path_buffer[MAX_PATH] = {};
    // For historical reasons Mozc executables have been installed under
    // %ProgramFiles(x86)%.
    // TODO(https://github.com/google/mozc/issues/1086): Stop using "(x86)".
    const HRESULT result =
        ::SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr,
                           SHGFP_TYPE_CURRENT, program_files_path_buffer);
    if (FAILED(result)) {
      return result;
    }

    const std::string program_files =
        win32::WideToUtf8(program_files_path_buffer);
    if (program_files.empty()) {
      return E_FAIL;
    }
    *path = program_files;
    return S_OK;
  }
  HRESULT result_;
  std::string path_;
};

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
  LSTATUS result =::RegOpenKeyExW(
      HKEY_LOCAL_MACHINE, kMozcTipClsid, 0, KEY_READ | KEY_WOW64_64KEY, &key);
  if (result != ERROR_SUCCESS) {
    return "";
  }

  DWORD type = 0;
  wchar_t buffer[MAX_PATH] = {};
  DWORD buffer_size = sizeof(buffer);
  result = ::RegQueryValueExW(
      key, nullptr, nullptr, &type, reinterpret_cast<LPBYTE>(buffer),
      &buffer_size);
  ::RegCloseKey(key);
  if (result != ERROR_SUCCESS || type != REG_SZ) {
    return "";
  }
  return FileUtil::Dirname(win32::WideToUtf8(buffer));
}

}  // namespace
#endif  // _WIN32

std::string SystemUtil::GetServerDirectory() {
#ifdef _WIN32
  const std::string install_dir_from_registry = GetMozcInstallDirFromRegistry();
  if (!install_dir_from_registry.empty()) {
    return install_dir_from_registry;
  }
  DCHECK(SUCCEEDED(Singleton<ProgramFilesX86Cache>::get()->result()));
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  return FileUtil::JoinPath(
      FileUtil::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                         kCompanyNameInEnglish),
      kProductNameInEnglish);
#else   // GOOGLE_JAPANESE_INPUT_BUILD
  return FileUtil::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                            kProductNameInEnglish);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#endif  // _WIN32

#if defined(__APPLE__)
  return MacUtil::GetServerDirectory();
#endif  // __APPLE__

#if defined(__linux__) || defined(__wasm__)
#ifndef MOZC_SERVER_DIR
#define MOZC_SERVER_DIR "/usr/lib/mozc"
#endif  // MOZC_SERVER_DIR
  return MOZC_SERVER_DIR;
#endif  // __linux__ || __wasm__

  // If none of the above platforms is specified, the compiler raises an error
  // because of no return value.
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
#if defined(__linux__)

#ifndef MOZC_DOCUMENT_DIR
#define MOZC_DOCUMENT_DIR "/usr/lib/mozc/documents"
#endif  // MOZC_DOCUMENT_DIR
  return MOZC_DOCUMENT_DIR;

#elif defined(__APPLE__)
  return GetServerDirectory();
#else   // __linux__, __APPLE__
  return FileUtil::JoinPath(GetServerDirectory(), "documents");
#endif  // __linux__, __APPLE__
}

std::string SystemUtil::GetCrashReportDirectory() {
  constexpr char kCrashReportDirectory[] = "CrashReports";
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kCrashReportDirectory);
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
  const int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  CHECK_NE(bufsize, -1);
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
  UserSidImpl();
  const std::string &get() { return sid_; }

 private:
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
  std::unique_ptr<char[]> buf(new char[length]);
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
  return Singleton<UserSidImpl>::get()->get();
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

  std::unique_ptr<char[]> buf(new char[size]);
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

  char *result = buf.get();
  result[return_size - 1] = '\0';  // just make sure nullptr terminated

  return result;
}

bool GetCurrentSessionId(uint32_t *session_id) {
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
  const std::string &session_id = GetSessionIdString();
  if (session_id.empty()) {
    DLOG(ERROR) << "Failed to retrieve session id";
    return "";
  }

  const std::string &window_station_name = GetProcessWindowStationName();
  if (window_station_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve window station name";
    return "";
  }

  const std::string &desktop_name = GetInputDesktopName();
  if (desktop_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve desktop name";
    return "";
  }

  return absl::StrCat(session_id, ".", window_station_name, ".", desktop_name);
#endif  // _WIN32
}

#ifdef _WIN32
namespace {

// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class SystemDirectoryCache {
 public:
  SystemDirectoryCache() : system_dir_(nullptr) {
    const UINT copied_len_wo_null_if_success =
        ::GetSystemDirectory(path_buffer_, std::size(path_buffer_));
    if (copied_len_wo_null_if_success >= std::size(path_buffer_)) {
      // Function failed.
      return;
    }
    DCHECK_EQ(L'\0', path_buffer_[copied_len_wo_null_if_success]);
    system_dir_ = path_buffer_;
  }
  const bool succeeded() const { return system_dir_ != nullptr; }
  const wchar_t *system_dir() const { return system_dir_; }

 private:
  wchar_t path_buffer_[MAX_PATH];
  wchar_t *system_dir_;
};

}  // namespace

// TODO(team): Support other platforms.
bool SystemUtil::EnsureVitalImmutableDataIsAvailable() {
  if (!Singleton<SystemDirectoryCache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<ProgramFilesX86Cache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<LocalAppDataDirectoryCache>::get()->succeeded()) {
    return false;
  }
  return true;
}

const wchar_t *SystemUtil::GetSystemDir() {
  DCHECK(Singleton<SystemDirectoryCache>::get()->succeeded());
  return Singleton<SystemDirectoryCache>::get()->system_dir();
}
#endif  // _WIN32

// TODO(toshiyuki): move this to the initialization module and calculate
// version only when initializing.
std::string SystemUtil::GetOSVersionString() {
#ifdef _WIN32
  std::string ret = "Windows";
  OSVERSIONINFOEX osvi = {0};
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&osvi))) {
    ret += ".";
    ret += std::to_string(static_cast<uint32_t>(osvi.dwMajorVersion));
    ret += ".";
    ret += std::to_string(static_cast<uint32_t>(osvi.dwMinorVersion));
    ret += "." + std::to_string(osvi.wServicePackMajor);
    ret += "." + std::to_string(osvi.wServicePackMinor);
  } else {
    LOG(WARNING) << "GetVersionEx failed";
  }
  return ret;
#elif defined(__APPLE__)
  const std::string ret = "MacOSX " + MacUtil::GetOSVersionString();
  // TODO(toshiyuki): get more specific info
  return ret;
#elif defined(__linux__)
  const std::string ret = "Linux";
  return ret;
#else   // !_WIN32 && !__APPLE__ && !__linux__
  const std::string ret = "Unknown";
  return ret;
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

}  // namespace mozc
