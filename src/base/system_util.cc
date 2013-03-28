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

#include "base/system_util.h"

#ifdef OS_WIN
#include <Windows.h>
#include <LMCons.h>
#include <Sddl.h>
#include <ShlObj.h>
#else  // OS_WIN
#include <pwd.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef OS_MACOSX
#include <sys/stat.h>
#include <sys/sysctl.h>
#endif  // OS_MACOSX
#endif  // OS_WIN

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#ifdef OS_MACOSX
#include <cerrno>
#endif  // OS_MACOSX

#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/number_util.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/win_util.h"

#ifdef OS_ANDROID
// HACK to avoid a bug in sysconf in android.
#include "android/jni/sysconf.h"
#define sysconf mysysconf
#include "base/android_util.h"
#endif  // OS_ANDROID

namespace mozc {

namespace {
class UserProfileDirectoryImpl {
 public:
  UserProfileDirectoryImpl();
  // Copies dir_ string using c_str() here to prevent Copy-On-Write issues in
  // multi-thread environment.
  string get() { return dir_.c_str(); }
  void set(const string &dir) { dir_ = dir; }
 private:
  string dir_;
};

#ifdef OS_WIN
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class LocalAppDataDirectoryCache {
 public:
  LocalAppDataDirectoryCache() : result_(E_FAIL) {
    result_ = SafeTryGetLocalAppData(&path_);
  }
  HRESULT result() const {
    return result_;
  }
  const bool succeeded() const {
    return SUCCEEDED(result_);
  }
  const string &path() const {
    return path_;
  }

 private:
  // b/5707813 implies that TryGetLocalAppData causes an exception and makes
  // Singleton<LocalAppDataDirectoryCache> invalid state which results in an
  // infinite spin loop in CallOnce. To prevent this, the constructor of
  // LocalAppDataDirectoryCache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryGetLocalAppData.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryGetLocalAppData(string *dir) {
    __try {
      return TryGetLocalAppData(dir);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryGetLocalAppData(string *dir) {
    if (dir == NULL) {
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
    if (SystemUtil::IsVistaOrLater()) {
      return TryGetLocalAppDataLow(dir);
    }

    // Windows XP: use "%USERPROFILE%\Local Settings\Application Data"

    // Retrieve the directory "%USERPROFILE%\Local Settings\Application Data",
    // which is a user directory which serves a data repository for local
    // applications, to avoid user profiles from being roamed by indexers.
    wchar_t config[MAX_PATH] = {};
    const HRESULT result = ::SHGetFolderPathW(
        NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, &config[0]);
    if (FAILED(result)) {
      return result;
    }

    string buffer;
    if (Util::WideToUTF8(&config[0], &buffer) == 0) {
      return E_FAIL;
    }

    *dir = buffer;
    return S_OK;
  }

  static HRESULT TryGetLocalAppDataForAppContainer(string *dir) {
    // User profiles for processes running under AppContainer seem to be as
    // follows, while the scheme is not officially documented.
    //   "%LOCALAPPDATA%\Packages\<package sid>\..."
    // Note: You can also obtain this path by GetAppContainerFolderPath API.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/hh448543.aspx
    // Anyway, here we use heuristics to obtain "LocalLow" folder path.
    // TODO(yukawa): Establish more reliable way to obtain the path.
    wchar_t config[MAX_PATH] = {};
    const HRESULT result = ::SHGetFolderPathW(
        NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, &config[0]);
    if (FAILED(result)) {
      return result;
    }
    wstring path = config;
    const wstring::size_type local_pos = path.find(L"\\Packages\\");
    if (local_pos == wstring::npos) {
      return E_FAIL;
    }
    path.erase(local_pos);
    path += L"Low";
    if (Util::WideToUTF8(path, dir) == 0) {
      return E_FAIL;
    }
    return S_OK;
  }

  static HRESULT TryGetLocalAppDataLow(string *dir) {
    if (dir == NULL) {
      return E_FAIL;
    }
    dir->clear();

    if (!SystemUtil::IsVistaOrLater()) {
      return E_NOTIMPL;
    }

    // Windows Vista: use LocalLow
    // Call SHGetKnownFolderPath dynamically.
    // http://msdn.microsoft.com/en-us/library/bb762188(VS.85).aspx
    // http://msdn.microsoft.com/en-us/library/bb762584(VS.85).aspx
    // GUID: {A520A1A4-1780-4FF6-BD18-167343C5AF16}
    const HMODULE hLib = WinUtil::LoadSystemLibrary(L"shell32.dll");
    if (hLib == NULL) {
      return E_NOTIMPL;
    }

    typedef HRESULT (WINAPI *FPSHGetKnownFolderPath)(
        const GUID &, DWORD, HANDLE, PWSTR *);
    FPSHGetKnownFolderPath func = reinterpret_cast<FPSHGetKnownFolderPath>
        (::GetProcAddress(hLib, "SHGetKnownFolderPath"));
    if (func == NULL) {
      ::FreeLibrary(hLib);
      return E_NOTIMPL;
    }

    wchar_t *task_mem_buffer = NULL;
    const HRESULT result =
        (*func)(FOLDERID_LocalAppDataLow, 0, NULL, &task_mem_buffer);
    if (FAILED(result)) {
      if (task_mem_buffer != NULL) {
        ::CoTaskMemFree(task_mem_buffer);
      }
      ::FreeLibrary(hLib);
      return result;
    }

    if (task_mem_buffer == NULL) {
      ::FreeLibrary(hLib);
      return E_UNEXPECTED;
    }

    wstring wpath = task_mem_buffer;
    ::CoTaskMemFree(task_mem_buffer);

    string path;
    if (Util::WideToUTF8(wpath, &path) == 0) {
      ::FreeLibrary(hLib);
      return E_UNEXPECTED;
    }

    *dir = path;

    ::FreeLibrary(hLib);
    return S_OK;
  }

  HRESULT result_;
  string path_;
};
#endif  // OS_WIN

UserProfileDirectoryImpl::UserProfileDirectoryImpl() {
#ifdef MOZC_USE_PEPPER_FILE_IO
  // In NaCl, we can't call FileUtil::CreateDirectory() nor
  // FileUtil::DirectoryExists(). So we just set dir_ here.
  dir_ = "/";
  return;
#else  // MOZC_USE_PEPPER_FILE_IO
  string dir;

#ifdef OS_WIN
  DCHECK(SUCCEEDED(Singleton<LocalAppDataDirectoryCache>::get()->result()));
  dir = Singleton<LocalAppDataDirectoryCache>::get()->path();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, kCompanyNameInEnglish);
  FileUtil::CreateDirectory(dir);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, kProductNameInEnglish);

#elif defined(OS_MACOSX)
  dir = MacUtil::GetApplicationSupportDirectory();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, "Google");
  // The permission of ~/Library/Application Support/Google seems to be 0755.
  // TODO(komatsu): nice to make a wrapper function.
  ::mkdir(dir.c_str(), 0755);
  dir = FileUtil::JoinPath(dir, "JapaneseInput");
#else  //  GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, "Mozc");
#endif  //  GOOGLE_JAPANESE_INPUT_BUILD

#elif defined(OS_ANDROID)
  // For android, we just use pre-defined directory, which is under the package
  // directory, asssuming each device has single user.
  dir = FileUtil::JoinPath("/data/data", kMozcAndroidPackage);
  dir = FileUtil::JoinPath(dir, ".mozc");

#else  // !OS_WIN && !OS_MACOSX && !OS_ANDROID
  char buf[1024];
  struct passwd pw, *ppw;
  const uid_t uid = geteuid();
  CHECK_EQ(0, getpwuid_r(uid, &pw, buf, sizeof(buf), &ppw))
      << "Can't get passwd entry for uid " << uid << ".";
  CHECK_LT(0, strlen(pw.pw_dir))
      << "Home directory for uid " << uid << " is not set.";
  dir = FileUtil::JoinPath(pw.pw_dir, ".mozc");
#endif  // !OS_WIN && !OS_MACOSX && !OS_ANDROID

  FileUtil::CreateDirectory(dir);
  if (!FileUtil::DirectoryExists(dir)) {
    LOG(ERROR) << "Failed to create directory: " << dir;
  }

  // set User profile directory
  dir_ = dir;
#endif  // MOZC_USE_PEPPER_FILE_IO
}

}  // namespace

string SystemUtil::GetUserProfileDirectory() {
  return Singleton<UserProfileDirectoryImpl>::get()->get();
}

string SystemUtil::GetLoggingDirectory() {
#ifdef OS_MACOSX
  string dir = MacUtil::GetLoggingDirectory();
  FileUtil::CreateDirectory(dir);
  return dir;
#else  // OS_MACOSX
  return GetUserProfileDirectory();
#endif  // OS_MACOSX
}

void SystemUtil::SetUserProfileDirectory(const string &path) {
  Singleton<UserProfileDirectoryImpl>::get()->set(path);
}

#ifdef OS_WIN
namespace {
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class ProgramFilesX86Cache {
 public:
  ProgramFilesX86Cache() : result_(E_FAIL) {
    result_ = SafeTryProgramFilesPath(&path_);
  }
  const bool succeeded() const {
    return SUCCEEDED(result_);
  }
  const HRESULT result() const {
    return result_;
  }
  const string &path() const {
    return path_;
  }

 private:
  // b/5707813 implies that the Shell API causes an exception in some cases.
  // In order to avoid potential infinite loops in CallOnce. the constructor of
  // ProgramFilesX86Cache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryProgramFilesPath.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryProgramFilesPath(string *path) {
    __try {
      return TryProgramFilesPath(path);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryProgramFilesPath(string *path) {
    if (path == NULL) {
      return E_FAIL;
    }
    path->clear();

    wchar_t program_files_path_buffer[MAX_PATH] = {};
#if defined(_M_X64)
    // In 64-bit processes (such as Text Input Prosessor DLL for 64-bit apps),
    // CSIDL_PROGRAM_FILES points 64-bit Program Files directory. In this case,
    // we should use CSIDL_PROGRAM_FILESX86 to find server, renderer, and other
    // binaries' path.
    const HRESULT result = ::SHGetFolderPathW(
        NULL, CSIDL_PROGRAM_FILESX86, NULL,
        SHGFP_TYPE_CURRENT, program_files_path_buffer);
#elif defined(_M_IX86)
    // In 32-bit processes (such as server, renderer, and other binaries),
    // CSIDL_PROGRAM_FILES always points 32-bit Program Files directory
    // even if they are running in 64-bit Windows.
    const HRESULT result = ::SHGetFolderPathW(
        NULL, CSIDL_PROGRAM_FILES, NULL,
        SHGFP_TYPE_CURRENT, program_files_path_buffer);
#else  // !_M_X64 && !_M_IX86
#error "Unsupported CPU architecture"
#endif  // _M_X64, _M_IX86, and others
    if (FAILED(result)) {
      return result;
    }

    string program_files;
    if (Util::WideToUTF8(program_files_path_buffer, &program_files) == 0) {
      return E_FAIL;
    }
    *path = program_files;
    return S_OK;
  }
  HRESULT result_;
  string path_;
};
}  // namespace
#endif  // OS_WIN

string SystemUtil::GetServerDirectory() {
#ifdef OS_WIN
  DCHECK(SUCCEEDED(Singleton<ProgramFilesX86Cache>::get()->result()));
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  return FileUtil::JoinPath(
      FileUtil::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                         kCompanyNameInEnglish),
      kProductNameInEnglish);
#else  // GOOGLE_JAPANESE_INPUT_BUILD
  return FileUtil::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                            kProductNameInEnglish);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#elif defined(OS_MACOSX)
  return MacUtil::GetServerDirectory();

#elif defined(OS_LINUX)
  // TODO(mazda): Not to use hardcoded path.
  return kMozcServerDirectory;
#endif  // OS_WIN, OS_MACOSX, OS_LINUX
}

string SystemUtil::GetServerPath() {
  const string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcServerName);
}

string SystemUtil::GetRendererPath() {
  const string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcRenderer);
}

string SystemUtil::GetToolPath() {
  const string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcTool);
}

string SystemUtil::GetDocumentDirectory() {
#ifdef OS_MACOSX
  return GetServerDirectory();
#else  // OS_MACOSX
  return FileUtil::JoinPath(GetServerDirectory(), "documents");
#endif  // OS_MACOSX
}

string SystemUtil::GetUserNameAsString() {
#ifdef MOZC_USE_PEPPER_FILE_IO
  LOG(ERROR) << "SystemUtil::GetUserNameAsString() is not implemented in NaCl.";
  return "username";

#elif defined(OS_WIN)  // MOZC_USE_PEPPER_FILE_IO
  wchar_t wusername[UNLEN + 1];
  DWORD name_size = UNLEN + 1;
  // Call the same name Windows API.  (include Advapi32.lib).
  // TODO(komatsu, yukawa): Add error handling.
  // TODO(komatsu, yukawa): Consider the case where the current thread is
  //   or will be impersonated.
  const BOOL result = ::GetUserName(wusername, &name_size);
  DCHECK_NE(FALSE, result);
  string username;
  Util::WideToUTF8(&wusername[0], &username);
  return username;

#elif defined(OS_ANDROID)  // OS_WIN
  // Android doesn't seem to support getpwuid_r.
  struct passwd *ppw = getpwuid(geteuid());
  CHECK(ppw != NULL);
  return ppw->pw_name;

#else  // OS_ANDROID
  // OS_MACOSX or OS_LINUX
  struct passwd pw, *ppw;
  char buf[1024];
  CHECK_EQ(0, getpwuid_r(geteuid(), &pw, buf, sizeof(buf), &ppw));
  return pw.pw_name;
#endif  // !MOZC_USE_PEPPER_FILE_IO && !OS_WIN && !OS_ANDROID
}

#ifdef OS_WIN
namespace {

class UserSidImpl {
 public:
  UserSidImpl();
  const string &get() { return sid_; }
 private:
  string sid_;
};

UserSidImpl::UserSidImpl() {
  HANDLE htoken = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &htoken)) {
    sid_ = SystemUtil::GetUserNameAsString();
    LOG(ERROR) << "OpenProcessToken failed: " << ::GetLastError();
    return;
  }

  DWORD length = 0;
  ::GetTokenInformation(htoken, TokenUser, NULL, 0, &length);
  scoped_array<char> buf(new char[length]);
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

  Util::WideToUTF8(p_sid_user_name, &sid_);

  ::LocalFree(p_sid_user_name);
  ::CloseHandle(htoken);
}

}  // namespace
#endif  // OS_WIN

string SystemUtil::GetUserSidAsString() {
#ifdef OS_WIN
  return Singleton<UserSidImpl>::get()->get();
#else  // OS_WIN
  return GetUserNameAsString();
#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {

string GetObjectNameAsString(HANDLE handle) {
  if (handle == NULL) {
    LOG(ERROR) << "Unknown handle";
    return "";
  }

  DWORD size = 0;
  if (::GetUserObjectInformationA(handle, UOI_NAME,
                                  NULL, 0, &size) ||
      ERROR_INSUFFICIENT_BUFFER != ::GetLastError()) {
    LOG(ERROR) << "GetUserObjectInformationA() failed: "
               << ::GetLastError();
    return "";
  }

  if (size == 0) {
    LOG(ERROR) << "buffer size is 0";
    return "";
  }

  scoped_array<char> buf(new char[size]);
  DWORD return_size = 0;
  if (!::GetUserObjectInformationA(handle, UOI_NAME, buf.get(),
                                   size, &return_size)) {
    LOG(ERROR) << "::GetUserObjectInformationA() failed: "
               << ::GetLastError();
    return "";
  }

  if (return_size <= 1) {
    LOG(ERROR) << "result buffer size is too small";
    return "";
  }

  char *result = buf.get();
  result[return_size - 1] = '\0';  // just make sure NULL terminated

  return result;
}

bool GetCurrentSessionId(DWORD *session_id) {
  DCHECK(session_id);
  *session_id = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(),
                              session_id)) {
    LOG(ERROR) << "cannot get session id: " << ::GetLastError();
    return false;
  }
  return true;
}

}  // namespace
#endif  // OS_WIN

string SystemUtil::GetDesktopNameAsString() {
#ifdef OS_LINUX
  const char *display = getenv("DISPLAY");
  if (display == NULL) {
    return "";
  }
  return display;

#elif defined(OS_MACOSX)
  return "";

#elif defined(OS_WIN)
  DWORD session_id = 0;
  if (!GetCurrentSessionId(&session_id)) {
    return "";
  }

  char id[64];
  snprintf(id, sizeof(id), "%u", session_id);

  string result = id;
  result += ".";
  result += GetObjectNameAsString(::GetProcessWindowStation());
  result += ".";
  result += GetObjectNameAsString(::GetThreadDesktop(::GetCurrentThreadId()));

  return result;
#endif  // OS_LINUX, OS_MACOSX, OS_WIN
}

#ifdef OS_WIN
namespace {
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
template<DWORD MajorVersion, DWORD MinorVersion>
class IsWindowsVerXOrLaterCache {
 public:
  IsWindowsVerXOrLaterCache()
    : succeeded_(false),
      is_ver_x_or_later_(true) {
    // Examine if this system is greater than or equal to WinNT ver. X
    {
      OSVERSIONINFOEX osvi = {};
      osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
      osvi.dwMajorVersion = MajorVersion;
      osvi.dwMinorVersion = MinorVersion;
      DWORDLONG conditional = 0;
      VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
      VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
      const BOOL result =
        ::VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION,
                            conditional);
      if (result != FALSE) {
        succeeded_ = true;
        is_ver_x_or_later_ = true;
        return;
      }
    }

    // Examine if this system is less than WinNT ver. X
    {
      OSVERSIONINFOEX osvi = {};
      osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
      osvi.dwMajorVersion = MajorVersion;
      osvi.dwMinorVersion = MinorVersion;
      DWORDLONG conditional = 0;
      VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_LESS);
      VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_LESS);
      const BOOL result =
        ::VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION,
                            conditional);
      if (result != FALSE) {
        succeeded_ = true;
        is_ver_x_or_later_ = false;
        return;
      }
    }

    // Unexpected situation.
    succeeded_ = false;
    is_ver_x_or_later_ = false;
  }
  const bool succeeded() const {
    return succeeded_;
  }
  const bool is_ver_x_or_later() const {
    return is_ver_x_or_later_;
  }

 private:
  bool succeeded_;
  bool is_ver_x_or_later_;
};

typedef IsWindowsVerXOrLaterCache<6, 0> IsWindowsVistaOrLaterCache;
typedef IsWindowsVerXOrLaterCache<6, 1> IsWindows7OrLaterCache;
typedef IsWindowsVerXOrLaterCache<6, 2> IsWindows8OrLaterCache;

// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class SystemDirectoryCache {
 public:
  SystemDirectoryCache() : system_dir_(NULL) {
    const UINT copied_len_wo_null_if_success =
        ::GetSystemDirectory(path_buffer_, arraysize(path_buffer_));
    if (copied_len_wo_null_if_success >= arraysize(path_buffer_)) {
      // Function failed.
      return;
    }
    DCHECK_EQ(L'\0', path_buffer_[copied_len_wo_null_if_success]);
    system_dir_ = path_buffer_;
  }
  const bool succeeded() const {
    return system_dir_ != NULL;
  }
  const wchar_t *system_dir() const {
    return system_dir_;
  }
 private:
  wchar_t path_buffer_[MAX_PATH];
  wchar_t *system_dir_;
};
}  // namespace

// TODO(team): Support other platforms.
bool SystemUtil::EnsureVitalImmutableDataIsAvailable() {
  if (!Singleton<IsWindowsVistaOrLaterCache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<IsWindows7OrLaterCache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<IsWindows8OrLaterCache>::get()->succeeded()) {
    return false;
  }
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
#endif  // OS_WIN

void SystemUtil::CommandLineRotateArguments(int argc, char ***argv) {
  char *arg = **argv;
  memmove(*argv, *argv + 1, (argc - 1) * sizeof(**argv));
  (*argv)[argc - 1] = arg;
}

bool SystemUtil::CommandLineGetFlag(int argc,
                                    char **argv,
                                    string *key,
                                    string *value,
                                    int *used_args) {
  key->clear();
  value->clear();
  *used_args = 0;
  if (argc < 1) {
    return false;
  }

  *used_args = 1;
  const char *start = argv[0];
  if (start[0] != '-') {
    return false;
  }

  ++start;
  if (start[0] == '-') ++start;
  const string arg = start;
  const size_t n = arg.find("=");
  if (n != string::npos) {
    *key = arg.substr(0, n);
    *value = arg.substr(n + 1, arg.size() - n);
    return true;
  }

  key->assign(arg);
  value->clear();

  if (argc == 1) {
    return true;
  }
  start = argv[1];
  if (start[0] == '-') {
    return true;
  }

  *used_args = 2;
  value->assign(start);
  return true;
}

bool SystemUtil::IsPlatformSupported() {
#if defined(OS_MACOSX)
  // TODO(yukawa): support Mac.
  return true;
#elif defined(OS_LINUX)
  // TODO(yukawa): support Linux.
  return true;
#elif defined(OS_WIN)
  // Sometimes we suffer from version lie of GetVersion(Ex) such as
  // http://b/2430094
  // This is why we use VerifyVersionInfo here instead of GetVersion(Ex).

  // You can find a table of version number for each version of Windows in
  // the following page.
  // http://msdn.microsoft.com/en-us/library/ms724833.aspx
  {
    // Windows 7 <= |OSVERSION|: supported
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 1;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return true;  // supported
    }
  }
  {
    // Windows Vista SP1 <= |OSVERSION| < Windows 7: supported
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 0;
    osvi.wServicePackMajor = 1;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION |
                           VER_SERVICEPACKMAJOR;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return true;  // supported
    }
  }
  {
    // Windows Vista RTM <= |OSVERSION| < Windows Vista SP1: not supported
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    osvi.dwMinorVersion = 0;
    osvi.wServicePackMajor = 0;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION |
                           VER_SERVICEPACKMAJOR;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return false;  // not supported
    }
  }
  {
    // Windows XP x64/Server 2003 <= |OSVERSION| < Windows Vista RTM: supported
    // ---
    // Note: We do not oficially support these platforms but allows users to
    //   install Mozc into them.  See b/5182031 for the background information.
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 5;
    osvi.dwMinorVersion = 2;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return true;  // supported
    }
  }
  {
    // Windows XP SP2 <= |OSVERSION| < Windows XP x64/Server 2003: supported
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 5;
    osvi.dwMinorVersion = 1;
    osvi.wServicePackMajor = 2;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditional, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    const DWORD typemask = VER_MAJORVERSION | VER_MINORVERSION |
                           VER_SERVICEPACKMAJOR;
    if (::VerifyVersionInfo(&osvi, typemask, conditional) != 0) {
      return true;  // supported
    }
  }
  // |OSVERSION| < Windows XP SP2: not supported
  return false;  // not support
#else  // !OS_LINUX && !OS_MACOSX && !OS_WIN
  COMPILE_ASSERT(false, unsupported_platform);
#endif  // OS_LINUX, OS_MACOSX, OS_WIN
}

bool SystemUtil::IsVistaOrLater() {
#ifdef OS_WIN
  DCHECK(Singleton<IsWindowsVistaOrLaterCache>::get()->succeeded());
  return Singleton<IsWindowsVistaOrLaterCache>::get()->is_ver_x_or_later();
#else
  return false;
#endif  // OS_WIN
}

bool SystemUtil::IsWindows7OrLater() {
#ifdef OS_WIN
  DCHECK(Singleton<IsWindows7OrLaterCache>::get()->succeeded());
  return Singleton<IsWindows7OrLaterCache>::get()->is_ver_x_or_later();
#else
  return false;
#endif  // OS_WIN
}

bool SystemUtil::IsWindows8OrLater() {
#ifdef OS_WIN
  DCHECK(Singleton<IsWindows8OrLaterCache>::get()->succeeded());
  return Singleton<IsWindows8OrLaterCache>::get()->is_ver_x_or_later();
#else
  return false;
#endif  // OS_WIN
}

namespace {
volatile mozc::SystemUtil::IsWindowsX64Mode g_is_windows_x64_mode =
    mozc::SystemUtil::IS_WINDOWS_X64_DEFAULT_MODE;
}  // namespace

bool SystemUtil::IsWindowsX64() {
  switch (g_is_windows_x64_mode) {
    case IS_WINDOWS_X64_EMULATE_32BIT_MACHINE:
      return false;
    case IS_WINDOWS_X64_EMULATE_64BIT_MACHINE:
      return true;
    case IS_WINDOWS_X64_DEFAULT_MODE:
      // handled below.
      break;
    default:
      // Should never reach here.
      DLOG(FATAL) << "Unexpected mode specified.  mode = "
                  << g_is_windows_x64_mode;
      // handled below.
      break;
  }

#ifdef OS_WIN
  SYSTEM_INFO system_info = {};
  // This function never fails.
  ::GetNativeSystemInfo(&system_info);
  return (system_info.wProcessorArchitecture ==
          PROCESSOR_ARCHITECTURE_AMD64);
#else
  return false;
#endif  // OS_WIN
}

void SystemUtil::SetIsWindowsX64ModeForTest(IsWindowsX64Mode mode) {
  g_is_windows_x64_mode = mode;
  switch (g_is_windows_x64_mode) {
    case IS_WINDOWS_X64_EMULATE_32BIT_MACHINE:
    case IS_WINDOWS_X64_EMULATE_64BIT_MACHINE:
    case IS_WINDOWS_X64_DEFAULT_MODE:
      // Known mode. OK.
      break;
    default:
      DLOG(FATAL) << "Unexpected mode specified.  mode = "
                  << g_is_windows_x64_mode;
      break;
  }
}

#ifdef OS_WIN
const wchar_t *SystemUtil::GetSystemDir() {
  DCHECK(Singleton<SystemDirectoryCache>::get()->succeeded());
  return Singleton<SystemDirectoryCache>::get()->system_dir();
}

bool SystemUtil::GetFileVersion(const wstring &file_fullpath,
                          int *major, int *minor, int *build, int *revision) {
  DCHECK(major);
  DCHECK(minor);
  DCHECK(build);
  DCHECK(revision);
  string path;
  Util::WideToUTF8(file_fullpath.c_str(), &path);

  // Accoding to KB826496, we should check file existence.
  // http://support.microsoft.com/kb/826496
  if (!FileUtil::FileExists(path)) {
    LOG(ERROR) << "file not found";
    return false;
  }

  DWORD handle = 0;
  const DWORD version_size =
      ::GetFileVersionInfoSizeW(file_fullpath.c_str(), &handle);

  if (version_size == 0) {
    LOG(ERROR) << "GetFileVersionInfoSizeW failed."
               << " error = " << ::GetLastError();
    return false;
  }

  scoped_array<BYTE> version_buffer(new BYTE[version_size]);

  if (!::GetFileVersionInfoW(file_fullpath.c_str(), 0,
                             version_size, version_buffer.get())) {
    LOG(ERROR) << "GetFileVersionInfo failed."
               << " error = " << ::GetLastError();
    return false;
  }

  VS_FIXEDFILEINFO *fixed_fileinfo = NULL;
  UINT length = 0;
  if (!::VerQueryValueW(version_buffer.get(), L"\\",
                        reinterpret_cast<LPVOID *>(&fixed_fileinfo),
                        &length)) {
    LOG(ERROR) << "VerQueryValue failed."
               << " error = " << ::GetLastError();
    return false;
  }

  *major = HIWORD(fixed_fileinfo->dwFileVersionMS);
  *minor = LOWORD(fixed_fileinfo->dwFileVersionMS);
  *build = HIWORD(fixed_fileinfo->dwFileVersionLS);
  *revision = LOWORD(fixed_fileinfo->dwFileVersionLS);

  return true;
}

string SystemUtil::GetFileVersionString(const wstring &file_fullpath) {
  int major, minor, build, revision;
  if (!GetFileVersion(file_fullpath, &major, &minor, &build, &revision)) {
    return "";
  }

  stringstream buf;
  buf << major << "." << minor << "." << build << "." << revision;

  return buf.str();
}

string SystemUtil::GetMSCTFAsmCacheReadyEventName() {
  DWORD session_id = 0;
  if (!GetCurrentSessionId(&session_id)) {
    return "";
  }

  const string &desktop_name =
      GetObjectNameAsString(::GetThreadDesktop(::GetCurrentThreadId()));

  if (desktop_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve desktop name";
    return "";
  }

  // Compose "Local\MSCTF.AsmCacheReady.<desktop name><session #>".
  return ("Local\\MSCTF.AsmCacheReady." + desktop_name +
          Util::StringPrintf("%u", session_id));
}
#endif  // OS_WIN

// TODO(toshiyuki): move this to the initialization module and calculate
// version only when initializing.
string SystemUtil::GetOSVersionString() {
#ifdef OS_WIN
  string ret = "Windows";
  OSVERSIONINFOEX osvi = { 0 };
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&osvi))) {
    ret += "." + NumberUtil::SimpleItoa(osvi.dwMajorVersion);
    ret += "." + NumberUtil::SimpleItoa(osvi.dwMinorVersion);
    ret += "." + NumberUtil::SimpleItoa(osvi.wServicePackMajor);
    ret += "." + NumberUtil::SimpleItoa(osvi.wServicePackMinor);
  } else {
    LOG(WARNING) << "GetVersionEx failed";
  }
  return ret;
#elif defined(OS_MACOSX)
  const string ret = "MacOSX " + MacUtil::GetOSVersionString();
  // TODO(toshiyuki): get more specific info
  return ret;
#elif defined(OS_LINUX)
  const string ret = "Linux";
  return ret;
#else  // !OS_WIN && !OS_MACOSX && !OS_LINUX
  const string ret = "Unknown";
  return ret;
#endif  // OS_WIN, OS_MACOSX, OS_LINUX
}

bool SystemUtil::MacOSVersionIsGreaterOrEqual(int32 major,
                                              int32 minor,
                                              int32 fix) {
#ifdef OS_MACOSX
  return MacUtil::OSVersionIsGreaterOrEqual(major, minor, fix);
#else
  return false;
#endif  // OS_MACOSX
}

void SystemUtil::DisableIME() {
#ifdef OS_WIN
  // turn off IME:
  // AFAIK disabling TSF under Vista and later OS is almost impossible
  // so that all we have to do is to prevent from calling
  // ITfThreadMgr::Activate and ITfThreadMgrEx::ActivateEx in this thread.
  ::ImmDisableTextFrameService(-1);
  ::ImmDisableIME(-1);
#endif  // OS_WIN
}

uint64 SystemUtil::GetTotalPhysicalMemory() {
#if defined(OS_WIN)
  MEMORYSTATUSEX memory_status = { sizeof(MEMORYSTATUSEX) };
  if (!::GlobalMemoryStatusEx(&memory_status)) {
    return 0;
  }
  return memory_status.ullTotalPhys;
#elif defined(OS_MACOSX)
  int mib[] = { CTL_HW, HW_MEMSIZE };
  uint64 total_memory = 0;
  size_t size = sizeof(total_memory);
  const int error =
      sysctl(mib, arraysize(mib), &total_memory, &size, NULL, 0);
  if (error == -1) {
    const int error = errno;
    LOG(ERROR) << "sysctl with hw.memsize failed. "
               << "errno: " << error;
    return 0;
  }
  return total_memory;
#elif defined(OS_LINUX)
#if defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  const long page_size = sysconf(_SC_PAGESIZE);
  const long number_of_phyisical_pages = sysconf(_SC_PHYS_PAGES);
  if (number_of_phyisical_pages < 0) {
    // likely to be overflowed.
    LOG(FATAL) << number_of_phyisical_pages << ", " << page_size;
    return 0;
  }
  return static_cast<uint64>(number_of_phyisical_pages) * page_size;
#else  // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  return 0;
#endif  // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
#else  // !(defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX))
#error "unknown platform"
#endif  // OS_WIN, OS_MACOSX, OS_LINUX
}

bool SystemUtil::IsLittleEndian() {
#ifndef OS_WIN
  union {
    unsigned char c[4];
    unsigned int i;
  } u;
  COMPILE_ASSERT(sizeof(u.c) == sizeof(u.i), bad_sizeof_int);
  COMPILE_ASSERT(sizeof(u) == sizeof(u.i), misaligned_union);
  u.i = 0x12345678U;
  return u.c[0] == 0x78U;
#else  // OS_WIN
  return true;
#endif  // OS_WIN
}

int SystemUtil::MaybeMLock(const void *addr, size_t len) {
  // TODO(yukawa): Integrate mozc_cache service.
#if defined(OS_WIN) || defined(OS_ANDROID) || defined(__native_client__)
  return -1;
#else  // defined(OS_WIN) || defined(OS_ANDROID) ||
       // defined(__native_client__)
  return mlock(addr, len);
#endif  // defined(OS_WIN) || defined(OS_ANDROID) ||
        // defined(__native_client__)
}

int SystemUtil::MaybeMUnlock(const void *addr, size_t len) {
#if defined(OS_WIN) || defined(OS_ANDROID) || defined(__native_client__)
  return -1;
#else  // defined(OS_WIN) || defined(OS_ANDROID) ||
       // defined(__native_client__)
  return munlock(addr, len);
#endif  // defined(OS_WIN) || defined(OS_ANDROID) ||
        // defined(__native_client__)
}

}  // namespace mozc
