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

#include "ipc/ipc_path_manager.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <ios>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/port.h"
#include "base/process_mutex.h"
#include "base/random.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/version.h"
#include "base/vlog.h"
#include "ipc/ipc.h"
#include "ipc/ipc.pb.h"

#if defined(__ANDROID__) || defined(__wasm__)
#error "This platform is not supported."
#endif  // __ANDROID__ || __wasm__

#ifdef __APPLE__
#include <sys/sysctl.h>

#include "base/mac/mac_util.h"
#endif  // __APPLE__

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <psapi.h>  // GetModuleFileNameExW
// clang-format on
#include <wil/resource.h>

#include "base/const.h"
#include "base/unverified_sha1.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_util.h"
#else  // _WIN32
// For stat system call
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // _WIN32

namespace mozc {
namespace {

// size of key
constexpr size_t kKeySize = 32;

// Do not use ConfigFileStream, since client won't link
// to the embedded resource files
std::string GetIPCKeyFileName(const std::string &name) {
  std::string basename =
      absl::StrCat(TargetIsWindows() ? "" : ".",  // hidden file on POSIX
                   name, ".ipc");
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), basename);
}

bool IsValidKey(const std::string &name) {
  if (kKeySize != name.size()) {
    LOG(ERROR) << "IPCKey is invalid length";
    return false;
  }

  for (size_t i = 0; i < name.size(); ++i) {
    if ((name[i] >= '0' && name[i] <= '9') ||
        (name[i] >= 'a' && name[i] <= 'f')) {
      // do nothing
    } else {
      LOG(ERROR) << "key name is invalid: " << name[i];
      return false;
    }
  }

  return true;
}

std::string CreateIPCKey() {
  // key is 128 bit
#ifdef _WIN32
  const std::string sid = SystemUtil::GetUserSidAsString();
  const std::string buf = internal::UnverifiedSHA1::MakeDigest(sid);
#else   // _WIN32
  // get 128 bit key: Note that collision will happen.
  Random random;
  const std::string buf = random.ByteString(16);
#endif  // _WIN32

  // escape
  std::string value;
  value.reserve(kKeySize);
  for (char c : buf) {
    const int hi = (static_cast<int>(c) & 0xF0) >> 4;
    const int lo = (static_cast<int>(c) & 0x0F);
    value.push_back(static_cast<char>(hi >= 10 ? hi - 10 + 'a' : hi + '0'));
    value.push_back(static_cast<char>(lo >= 10 ? lo - 10 + 'a' : lo + '0'));
  }

  return value;
}

class IPCPathManagerMap {
 public:
  IPCPathManagerMap() = default;

  ~IPCPathManagerMap() {
    absl::MutexLock l(mutex_);
    manager_map_.clear();
  }

  IPCPathManager *GetIPCPathManager(const absl::string_view name) {
    absl::MutexLock l(mutex_);
    const auto it = manager_map_.find(name);
    if (it != manager_map_.end()) {
      return it->second.get();
    }
    auto manager = std::make_unique<IPCPathManager>(std::string(name));
    IPCPathManager *ptr = manager.get();
    manager_map_.emplace(name, std::move(manager));
    return ptr;
  }

 private:
  absl::flat_hash_map<std::string, std::unique_ptr<IPCPathManager>>
      manager_map_;
  absl::Mutex mutex_;
};

}  // namespace

IPCPathManager *IPCPathManager::GetIPCPathManager(
    const absl::string_view name) {
  IPCPathManagerMap *manager_map = Singleton<IPCPathManagerMap>::get();
  DCHECK(manager_map != nullptr);
  return manager_map->GetIPCPathManager(name);
}

bool IPCPathManager::CreateNewPathName() {
  absl::MutexLock l(mutex_);
  return CreateNewPathNameUnlocked();
}

bool IPCPathManager::CreateNewPathNameUnlocked() {
  if (ipc_path_info_.key().empty()) {
    ipc_path_info_.set_key(CreateIPCKey());
  }
  return true;
}

bool IPCPathManager::SavePathName() {
  absl::MutexLock l(mutex_);
  if (path_mutex_) {
    return true;
  }

  path_mutex_ = std::make_unique<ProcessMutex>("ipc");
  path_mutex_->set_lock_filename(GetIPCKeyFileName(name_));

  CreateNewPathNameUnlocked();

  // set the server version
  ipc_path_info_.set_protocol_version(IPC_PROTOCOL_VERSION);
  ipc_path_info_.set_product_version(Version::GetMozcVersion());

#ifdef _WIN32
  ipc_path_info_.set_process_id(static_cast<uint32_t>(::GetCurrentProcessId()));
  ipc_path_info_.set_thread_id(static_cast<uint32_t>(::GetCurrentThreadId()));
#else   // _WIN32
  ipc_path_info_.set_process_id(static_cast<uint32_t>(getpid()));
  ipc_path_info_.set_thread_id(0);
#endif  // _WIN32

  std::string buf;
  if (!ipc_path_info_.SerializeToString(&buf)) {
    LOG(ERROR) << "SerializeToString failed";
    return false;
  }

  if (!path_mutex_->LockAndWrite(buf)) {
    LOG(ERROR) << "ipc key file is already locked";
    return false;
  }

  MOZC_VLOG(1) << "ServerIPCKey: " << ipc_path_info_.key();

  last_modified_ = GetIPCFileTimeStamp();
  return true;
}

bool IPCPathManager::LoadPathName() {
  // On Windows, ShouldReload() always returns false.
  // On other platform, it returns true when timestamp of the file is different
  // from that of previous one.
  const bool should_load = (ShouldReload() || ipc_path_info_.key().empty());
  if (!should_load) {
    return true;
  }

  if (LoadPathNameInternal()) {
    return true;
  }

  if constexpr (TargetIsWindows()) {
    // Fill the default values as fallback.
    // Applications conerted by Desktop App Converter (DAC) does not read
    // a file of ipc session name in the LocalLow directory.
    // For a workaround, let applications to connect the named pipe directly.
    // See: b/71338191.
    CreateNewPathName();
    DCHECK(!ipc_path_info_.key().empty());
    ipc_path_info_.set_protocol_version(IPC_PROTOCOL_VERSION);
    ipc_path_info_.set_product_version(Version::GetMozcVersion());
    return true;
  } else {
    LOG(ERROR) << "LoadPathName failed";
    return false;
  }
}

bool IPCPathManager::GetPathName(std::string *ipc_name) const {
  if (ipc_name == nullptr) {
    LOG(ERROR) << "ipc_name is nullptr";
    return false;
  }

  if (ipc_path_info_.key().empty()) {
    LOG(ERROR) << "ipc_path_info_ is empty";
    return false;
  }

#ifdef _WIN32
  *ipc_name = mozc::kIPCPrefix;
#elif defined(__APPLE__)
  ipc_name->assign(MacUtil::GetLabelForSuffix(""));
#else   // not _WIN32 nor __APPLE__
  // GetUserIPCName("<name>") => "/tmp/.mozc.<key>.<name>"
  constexpr char kIPCPrefix[] = "/tmp/.mozc.";
  *ipc_name = kIPCPrefix;
#endif  // _WIN32

#ifdef __linux__
  // On Linux, use abstract namespace which is independent of the file system.
  (*ipc_name)[0] = '\0';
#endif  // __linux__

  ipc_name->append(ipc_path_info_.key());
  ipc_name->append(".");
  ipc_name->append(name_);

  return true;
}

uint32_t IPCPathManager::GetServerProtocolVersion() const {
  return ipc_path_info_.protocol_version();
}

const std::string &IPCPathManager::GetServerProductVersion() const {
  return ipc_path_info_.product_version();
}

uint32_t IPCPathManager::GetServerProcessId() const {
  return ipc_path_info_.process_id();
}

void IPCPathManager::Clear() {
  absl::MutexLock l(mutex_);
  ipc_path_info_.Clear();
}

bool IPCPathManager::IsValidServer(uint32_t pid,
                                   const absl::string_view server_path) {
  absl::MutexLock l(mutex_);
  if (pid == 0) {
    // For backward compatibility.
    return true;
  }
  if (server_path.empty()) {
    // This means that we do not check the server path.
    return true;
  }

  if (pid == static_cast<uint32_t>(-1)) {
    MOZC_VLOG(1) << "pid is -1. so assume that it is an invalid program";
    return false;
  }

  // compare path name
  if (pid == server_pid_) {
    return (server_path == server_path_);
  }

  server_pid_ = 0;
  server_path_.clear();

#ifdef _WIN32
  {
    std::wstring expected_server_ntpath;
    const auto it = expected_server_ntpath_cache_.find(server_path);
    if (it != expected_server_ntpath_cache_.end()) {
      expected_server_ntpath = it->second;
    } else {
      std::wstring wide_server_path = win32::Utf8ToWide(server_path);
      if (WinUtil::GetNtPath(wide_server_path, &expected_server_ntpath)) {
        // Caches the relationship from |server_path| to
        // |expected_server_ntpath| in case |server_path| is renamed later.
        // (This can happen during the updating).
        expected_server_ntpath_cache_.emplace(server_path,
                                              expected_server_ntpath);
      }
    }

    if (expected_server_ntpath.empty()) {
      return false;
    }

    std::wstring actual_server_ntpath;
    if (!WinUtil::GetProcessInitialNtPath(pid, &actual_server_ntpath)) {
      return false;
    }

    if (expected_server_ntpath != actual_server_ntpath) {
      return false;
    }

    // Here we can safely assume that |server_path| (expected one) should be
    // the same to |server_path_| (actual one).
    server_path_ = std::string(server_path);
    server_pid_ = pid;
  }
#endif  // _WIN32

#ifdef __APPLE__
  int name[] = {CTL_KERN, KERN_PROCARGS, static_cast<int>(pid)};
  size_t data_len = 0;
  if (sysctl(name, std::size(name), nullptr, &data_len, nullptr, 0) < 0) {
    LOG(ERROR) << "sysctl KERN_PROCARGS failed";
    return false;
  }

  server_path_.resize(data_len);
  if (sysctl(name, std::size(name), &server_path_[0], &data_len, nullptr, 0) <
      0) {
    LOG(ERROR) << "sysctl KERN_PROCARGS failed";
    return false;
  }
  server_pid_ = pid;
#endif  // __APPLE__

#ifdef __linux__
  // load from /proc/<pid>/exe
  std::string proc = absl::StrFormat("/proc/%u/exe", pid);
  char filename[512];
  const ssize_t size = readlink(proc.c_str(), filename, sizeof(filename) - 1);
  if (size == -1) {
    LOG(ERROR) << "readlink failed: " << strerror(errno);
    return false;
  }
  filename[size] = '\0';

  server_path_ = filename;
  server_pid_ = pid;
#endif  // __linux__

  MOZC_VLOG(1) << "server path: " << server_path << " " << server_path_;
  if (server_path == server_path_) {
    return true;
  }

#ifdef __linux__
  if (absl::StrCat(server_path, " (deleted)") == server_path_) {
    LOG(WARNING) << server_path << " on disk is modified";
    // If a user updates the server binary on disk during the server is running,
    // "readlink /proc/<pid>/exe" returns a path with the " (deleted)" suffix.
    // We allow the special case.
    server_path_ = std::string(server_path);
    return true;
  }
#endif  // __linux__

  return false;
}

bool IPCPathManager::ShouldReload() const {
  if constexpr (TargetIsWindows()) {
    // In windows, no reloading mechanism is necessary because IPC files
    // are automatically removed.
    return false;
  } else {
    absl::MutexLock l(mutex_);
    time_t last_modified = GetIPCFileTimeStamp();
    if (last_modified == last_modified_) {
      return false;
    }
    return true;
  }
}

time_t IPCPathManager::GetIPCFileTimeStamp() const {
#ifdef _WIN32
  // In windows, we don't need to get the exact file timestamp, so
  // just returns -1 at this time.
  return static_cast<time_t>(-1);
#else   // _WIN32
  const std::string filename = GetIPCKeyFileName(name_);
  struct stat filestat;
  if (::stat(filename.c_str(), &filestat) == -1) {
    MOZC_VLOG(2) << "stat(2) failed.  Skipping reload";
    return static_cast<time_t>(-1);
  }
  return filestat.st_mtime;
#endif  // _WIN32
}

bool IPCPathManager::LoadPathNameInternal() {
  absl::MutexLock l(mutex_);

  // try the new file name first.
  const std::string filename = GetIPCKeyFileName(name_);

  // Special code for Windows,
  // we want to pass FILE_SHRED_DELETE flag for CreateFile.
#ifdef _WIN32
  std::wstring wfilename = win32::Utf8ToWide(filename);

  {
    wil::unique_hfile handle(
        ::CreateFileW(wfilename.c_str(), GENERIC_READ,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr, OPEN_EXISTING, 0, nullptr));

    if (!handle) {
      LOG(ERROR) << "cannot open: " << filename << " " << ::GetLastError();
      return false;
    }

    const DWORD size = ::GetFileSize(handle.get(), nullptr);
    if (static_cast<int>(size) == -1) {
      LOG(ERROR) << "GetFileSize failed: " << ::GetLastError();
      return false;
    }

    constexpr DWORD kMaxFileSize = 2096;
    if (size == 0 || size >= kMaxFileSize) {
      LOG(ERROR) << "Invalid file size: " << kMaxFileSize;
      return false;
    }

    std::string buf;
    buf.resize(size);
    DWORD read_size = 0;
    if (!::ReadFile(handle.get(), buf.data(), size, &read_size, nullptr)) {
      LOG(ERROR) << "ReadFile failed: " << ::GetLastError();
      return false;
    }

    if (read_size != size) {
      LOG(ERROR) << "read_size != size";
      return false;
    }

    if (!ipc_path_info_.ParseFromArray(buf.data(), static_cast<int>(size))) {
      LOG(ERROR) << "ParseFromArray failed";
      return false;
    }
  }

#else   // _WIN32

  InputFileStream is(filename, std::ios::binary | std::ios::in);
  if (!is) {
    LOG(ERROR) << "cannot open: " << filename;
    return false;
  }

  if (!ipc_path_info_.ParseFromIstream(&is)) {
    LOG(ERROR) << "ParseFromStream failed";
    return false;
  }
#endif  // _WIN32

  if (!IsValidKey(ipc_path_info_.key())) {
    LOG(ERROR) << "IPCServer::key is invalid";
    return false;
  }

  MOZC_VLOG(1) << "ClientIPCKey: " << ipc_path_info_.key();
  MOZC_VLOG(1) << "ProtocolVersion: " << ipc_path_info_.protocol_version();

  last_modified_ = GetIPCFileTimeStamp();
  return true;
}
}  // namespace mozc
