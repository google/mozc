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

#include "ipc/ipc_path_manager.h"

#include <errno.h>
#include <stdlib.h>
#include <fstream>
#include <map>

#ifdef OS_WINDOWS
#include <windows.h>
#include <psapi.h>   // GetModuleFileNameExW
#else
// For stat system call
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif  // OS_WINDOWS

#ifdef OS_MACOSX
#include <sys/sysctl.h>
#include "base/mac_util.h"
#endif

#include "base/base.h"
#include "base/const.h"
#include "base/file_stream.h"
#include "base/mmap.h"
#include "base/mutex.h"
#include "base/process_mutex.h"
#include "base/scoped_handle.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/version.h"
#include "ipc/ipc.h"
#include "ipc/ipc.pb.h"

namespace mozc {
namespace {

// size of key
const size_t kKeySize = 32;

// Do not use ConfigFileStream, since client won't link
// to the embedded resource files
string GetIPCKeyFileName(const string &name) {
#ifdef OS_WINDOWS
  string basename;
#else
  string basename = ".";    // hidden file
#endif

  basename += name;
  basename += ".ipc";   // this is the extension part.

  return Util::JoinPath(Util::GetUserProfileDirectory(), basename);
}

bool IsValidKey(const string &name) {
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

void CreateIPCKey(char *value) {
  char buf[16];   // key is 128 bit
  bool error = false;

#ifdef OS_WINDOWS
  // LUID guaranties uniqueness
  LUID luid = { 0 };   // LUID is 64bit value

  DCHECK_EQ(sizeof(luid), sizeof(uint64));

  // first 64 bit is random sequence and last 64 bit is LUID
  if (::AllocateLocallyUniqueId(&luid) &&
      Util::GetSecureRandomSequence(buf, sizeof(buf) / 2)) {
    ::memcpy(buf + sizeof(buf) / 2, &luid, sizeof(buf) / 2);
  } else {
    LOG(ERROR) << "Cannot make random key: " << ::GetLastError();
    error = true;
  }
#else
  // get 128 bit key: Note that collision will happen.
  if (!Util::GetSecureRandomSequence(buf, sizeof(buf))) {
    LOG(ERROR) << "Cannot make random key";
    error = true;
  }
#endif

  // use rand() for failsafe
  if (error) {
    LOG(ERROR) << "make random key with rand()";
    for (size_t i = 0; i < sizeof(buf); ++i) {
      buf[i] = static_cast<char>(rand() % 256);
    }
  }

  // escape
  for (size_t i = 0; i < sizeof(buf); ++i) {
    const int hi = ((static_cast<int>(buf[i]) & 0xF0) >> 4);
    const int lo = (static_cast<int>(buf[i]) & 0x0F);
    value[2 * i]     = static_cast<char>(hi >= 10 ? hi - 10 + 'a' : hi + '0');
    value[2 * i + 1] = static_cast<char>(lo >= 10 ? lo - 10 + 'a' : lo + '0');
  }

  value[kKeySize] = '\0';
}

class IPCPathManagerMap {
 public:
  IPCPathManager *GetIPCPathManager(const string &name) {
    scoped_lock l(&mutex_);
    map<string, IPCPathManager *>::const_iterator it = manager_map_.find(name);
    if (it != manager_map_.end()) {
      return it->second;
    }
    IPCPathManager *manager = new IPCPathManager(name);
    manager_map_.insert(make_pair(name, manager));
    return manager;
  }

  IPCPathManagerMap() {}

  ~IPCPathManagerMap() {
    scoped_lock l(&mutex_);
    for (map<string, IPCPathManager *>::iterator it = manager_map_.begin();
         it != manager_map_.end(); ++it) {
      delete it->second;
    }
    manager_map_.clear();
  }

 private:
  map<string, IPCPathManager *> manager_map_;
  Mutex mutex_;
};
}  // namespace

IPCPathManager::IPCPathManager(const string &name)
    : mutex_(new Mutex),
      ipc_path_info_(new ipc::IPCPathInfo),
      name_(name),
      server_pid_(0),
      last_modified_(-1) {}

IPCPathManager::~IPCPathManager() {}

IPCPathManager *IPCPathManager::GetIPCPathManager(const string &name) {
  IPCPathManagerMap *manager_map = Singleton<IPCPathManagerMap>::get();
  DCHECK(manager_map != NULL);
  return manager_map->GetIPCPathManager(name);
}

bool IPCPathManager::CreateNewPathName() {
  scoped_lock l(mutex_.get());
  if (ipc_path_info_->key().empty()) {
    char ipc_key[kKeySize + 1];
    CreateIPCKey(ipc_key);
    ipc_path_info_->set_key(ipc_key);
  }
  return true;
}

bool IPCPathManager::SavePathName() {
  scoped_lock l(mutex_.get());
  if (path_mutex_.get() != NULL) {
    return true;
  }

  path_mutex_.reset(new ProcessMutex("ipc"));

  path_mutex_->set_lock_filename(GetIPCKeyFileName(name_));

  // a little tricky as CreateNewPathName() tries mutex lock
  CreateNewPathName();

  // set the server version
  ipc_path_info_->set_protocol_version(IPC_PROTOCOL_VERSION);
  ipc_path_info_->set_product_version(Version::GetMozcVersion());

#ifdef OS_WINDOWS
  ipc_path_info_->set_process_id(static_cast<uint32>(::GetCurrentProcessId()));
  ipc_path_info_->set_thread_id(static_cast<uint32>(::GetCurrentThreadId()));
#else
  ipc_path_info_->set_process_id(static_cast<uint32>(getpid()));
  ipc_path_info_->set_thread_id(0);
#endif

  string buf;
  if (!ipc_path_info_->SerializeToString(&buf)) {
    LOG(ERROR) << "SerializeToString failed";
    return false;
  }

  if (!path_mutex_->LockAndWrite(buf)) {
    LOG(ERROR) << "ipc key file is already locked";
    return false;
  }

  VLOG(1) << "ServerIPCKey: " << ipc_path_info_->key();

  last_modified_ = GetIPCFileTimeStamp();
  return true;
}

bool IPCPathManager::GetPathName(string *ipc_name) {
  if (ipc_name == NULL) {
    LOG(ERROR) << "ipc_name is NULL";
    return false;
  }

  if ((ShouldReload() || ipc_path_info_->key().empty()) && !LoadPathName()) {
    LOG(ERROR) << "GetPathName failed";
    return false;
  }

#ifdef OS_WINDOWS
  *ipc_name = mozc::kIPCPrefix;
#elif defined(OS_MACOSX)
  ipc_name->assign(MacUtil::GetLabelForSuffix(""));
#else  // not OS_WINDOWS nor OS_MACOSX
  // GetUserIPCName("<name>") => "/tmp/.mozc.<key>.<name>"
  const char kIPCPrefix[] = "/tmp/.mozc.";
  *ipc_name = kIPCPrefix;
#endif  // OS_WINDOWS

#ifdef OS_LINUX
  // On Linux, use abstract namespace which is independent of the file system.
  (*ipc_name)[0] = '\0';
#endif

  ipc_name->append(ipc_path_info_->key());
  ipc_name->append(".");
  ipc_name->append(name_);

  return true;
}

uint32 IPCPathManager::GetServerProtocolVersion() const {
  return ipc_path_info_->protocol_version();
}

const string &IPCPathManager::GetServerProductVersion() const {
  return ipc_path_info_->product_version();
}

uint32 IPCPathManager::GetServerProcessId() const {
  return ipc_path_info_->process_id();
}

void IPCPathManager::Clear() {
  scoped_lock l(mutex_.get());
  ipc_path_info_->Clear();
}

bool IPCPathManager::IsValidServer(uint32 pid,
                                   const string &server_path) {
  scoped_lock l(mutex_.get());
  // for backward compatibility
  if (pid == 0 || server_path.empty()) {
    return true;
  }

  if (pid == static_cast<uint32>(-1)) {
    VLOG(1) << "pid is -1. so assume that it is an invalid program";
    return false;
  }

  // compare path name
  if (pid == server_pid_) {
    return (server_path == server_path_);
  }

  server_pid_ = pid;
  server_path_.clear();

#ifdef OS_WINDOWS
  {
    ScopedHandle process_handle
        (::OpenProcess
         (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE,
          static_cast<DWORD>(server_pid_)));

    if (process_handle.get() == NULL) {
      LOG(ERROR) << "OpenProcess() failed: " << ::GetLastError();
      return false;
    }

    wchar_t filename[MAX_PATH];
    const DWORD result = ::GetModuleFileNameExW(process_handle.get(),
                                                NULL,
                                                filename,
                                                MAX_PATH);
    if (result == 0 || result >= MAX_PATH) {
      LOG(ERROR) << "GetModuleFileNameExW() failed: " << ::GetLastError();
      return false;
    }

    Util::WideToUTF8(filename, &server_path_);
  }
#endif

#ifdef OS_MACOSX
  int name[] = { CTL_KERN, KERN_PROCARGS, pid };
  size_t data_len = 0;
  if (sysctl(name, arraysize(name), NULL,
             &data_len, NULL, 0) < 0) {
    LOG(ERROR) << "sysctl KERN_PROCARGS failed";
    return false;
  }

  server_path_.resize(data_len);
  if (sysctl(name, arraysize(name), &server_path_[0],
             &data_len, NULL, 0) < 0) {
    LOG(ERROR) << "sysctl KERN_PROCARGS failed";
    return false;
  }
#endif

#ifdef OS_LINUX
  // load from /proc/<pid>/exe
  char proc[128];
  char filename[512];
  snprintf(proc, sizeof(proc) - 1, "/proc/%u/exe", server_pid_);
  const ssize_t size = readlink(proc, filename, sizeof(filename) - 1);
  if (size == -1) {
    LOG(ERROR) << "readlink failed: " << strerror(errno);
    return false;
  }
  filename[size] = '\0';

  server_path_ = filename;
#endif

  VLOG(1) << "server path: " << server_path << " " << server_path_;
  if (server_path == server_path_) {
    return true;
  }

#ifdef OS_LINUX
  if ((server_path + " (deleted)") == server_path_) {
    LOG(WARNING) << server_path << " on disk is modified";
    // If a user updates the server binary on disk during the server is running,
    // "readlink /proc/<pid>/exe" returns a path with the " (deleted)" suffix.
    // We allow the special case.
    server_path_ = server_path;
    return true;
  }
#endif

  return false;
}

bool IPCPathManager::ShouldReload() const {
#ifdef OS_WINDOWS
  // In windows, no reloading mechanism is necessary because IPC files
  // are automatically removed.
  return false;
#else
  scoped_lock l(mutex_.get());

  time_t last_modified = GetIPCFileTimeStamp();
  if (last_modified == last_modified_) {
    return false;
  }

  return true;
#endif  // OS_WINDOWS
}

time_t IPCPathManager::GetIPCFileTimeStamp() const {
#ifdef OS_WINDOWS
  // In windows, we don't need to get the exact file timestamp, so
  // just returns -1 at this time.
  return static_cast<time_t>(-1);
#else
  const string filename = GetIPCKeyFileName(name_);
  struct stat filestat;
  if (::stat(filename.c_str(), &filestat) == -1) {
    VLOG(2) << "stat(2) failed.  Skipping reload";
    return static_cast<time_t>(-1);
  }
  return filestat.st_mtime;
#endif  // OS_WINDOWS
}

bool IPCPathManager::LoadPathName() {
  scoped_lock l(mutex_.get());

  // try the new file name first.
  const string filename = GetIPCKeyFileName(name_);

  // Special code for Windows,
  // we want to pass FILE_SHRED_DELETE flag for CreateFile.
#ifdef OS_WINDOWS
  wstring wfilename;
  Util::UTF8ToWide(filename.c_str(), &wfilename);

  {
    ScopedHandle handle
        (::CreateFileW(wfilename.c_str(),
                       GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, 0, NULL));

    // ScopedHandle does not receive INVALID_HANDLE_VALUE and
    // NULL check is appropriate here.
    if (NULL == handle.get()) {
      LOG(ERROR) << "cannot open: " << filename << " " << ::GetLastError();
      return false;
    }

    const DWORD size = ::GetFileSize(handle.get(), NULL);
    if (-1 == static_cast<int>(size)) {
      LOG(ERROR) << "GetFileSize failed: " << ::GetLastError();
      return false;
    }

    const DWORD kMaxFileSize = 2096;
    if (size == 0 || size >= kMaxFileSize) {
      LOG(ERROR) << "Invalid file size: " << kMaxFileSize;
      return false;
    }

    scoped_array<char> buf(new char[size]);

    DWORD read_size = 0;
    if (!::ReadFile(handle.get(), buf.get(),
                    size, &read_size, NULL)) {
      LOG(ERROR) << "ReadFile failed: " << ::GetLastError();
      return false;
    }

    if (read_size != size) {
      LOG(ERROR) << "read_size != size";
      return false;
    }

    if (!ipc_path_info_->ParseFromArray(buf.get(), static_cast<int>(size))) {
      LOG(ERROR) << "ParseFromStream failed";
      return false;
    }
  }

#else

  InputFileStream is(filename.c_str(), ios::binary|ios::in);
  if (!is) {
    LOG(ERROR) << "cannot open: " << filename;
    return false;
  }

  if (!ipc_path_info_->ParseFromIstream(&is)) {
    LOG(ERROR) << "ParseFromStream failed";
    return false;
  }
#endif

  if (!IsValidKey(ipc_path_info_->key())) {
    LOG(ERROR) << "IPCServer::key is invalid";
    return false;
  }

  VLOG(1) << "ClientIPCKey: " << ipc_path_info_->key();
  VLOG(1) << "ProtocolVersion: " << ipc_path_info_->protocol_version();

  last_modified_ = GetIPCFileTimeStamp();
  return true;
}
}  // namespace mozc
