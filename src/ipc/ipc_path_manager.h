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

#ifndef MOZC_IPC_IPC_PATH_MANAGER_H_
#define MOZC_IPC_IPC_PATH_MANAGER_H_

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/process_mutex.h"
#include "ipc/ipc.pb.h"

#ifdef _WIN32
#include "absl/container/flat_hash_map.h"
#endif  // _WIN32

namespace mozc {

class IPCPathManager {
 public:
  // do not call the constructor directly.
  explicit IPCPathManager(std::string name)
      : ipc_path_info_(),
        name_(std::move(name)),
        server_pid_(0),
        last_modified_(-1) {}
  virtual ~IPCPathManager() = default;

  // return singleton instance corresponding to "name"
  static IPCPathManager *GetIPCPathManager(absl::string_view name);

  // Brief summary of CreateNew / Save / Load / Get PathName().
  // CreateNewPathName: Generates a pathname and save it into the heap.
  // SavePathName:      Saves a pathname into a file from the heap.
  // LoadPathName:      Loads a pathname from a file and save it into the heap.
  // GetPathName:       Sets a pathname on an argument using the heap.

  // Create new pathname and save it into the heap. If pathname is not empty,
  // do nothing.
  bool CreateNewPathName();

  // Save ipc path to ~/.mozc/.<name>.ipc
  // if server_ipc_key_ is empty, CreateNewPathName() is called automatically.
  // If the file is already locked, return false
  bool SavePathName();

  // Load a pathname from a disk and updates |ipc_path_info_| if pathname is
  // empty or ipc key file is updated. Returns false if it cannot load.
  bool LoadPathName();

  // Get a pathname from the heap. If pathname is empty, returns false.
  bool GetPathName(std::string *ipc_name) const;

  // return protocol version.
  // return 0 if protocol version is not defined.
  uint32_t GetServerProtocolVersion() const;

  // return product version.
  // return "0.0.0.0" if product version is not defined
  const std::string &GetServerProductVersion() const;

  // return process id of the server
  uint32_t GetServerProcessId() const;

  // Checks the server pid is the valid server specified with server_path.
  // server pid can be obtained by OS dependent method.
  // This API is only available on Windows Vista or Linux.
  // Due to the restriction of OpenProcess API, always returns true under
  // AppContainer sandbox on Windows 8 (No verification will be done).
  // Always returns true on other operating systems.
  // See also: GetNamedPipeServerProcessId
  // http://msdn.microsoft.com/en-us/library/aa365446.aspx
  // when pid of 0 is passed, IsValidServer() returns true.
  // when pid of static_cast<size_t>(-1) is passed, IsValidServer()
  // returns false.
  // To keep backward compatibility and other operating system
  // having no support of getting peer's pid, you can set 0 pid.
  bool IsValidServer(uint32_t pid, absl::string_view server_path);

  // clear ipc_key;
  void Clear();

 private:
  friend class IPCPathManagerTestPeer;

  bool LoadPathNameInternal();

  // Returns true if the ipc file is updated after it load.
  bool ShouldReload() const;

  bool CreateNewPathNameUnlocked();

  // Returns the last modified timestamp of the IPC file.
  time_t GetIPCFileTimeStamp() const;

  std::unique_ptr<ProcessMutex> path_mutex_;  // lock ipc path file
  mutable absl::Mutex mutex_;                 // mutex for methods
  ipc::IPCPathInfo ipc_path_info_;
  std::string name_;
  std::string server_path_;  // cache for server_path
  uint32_t server_pid_;      // cache for pid of server_path
  time_t last_modified_;
#ifdef _WIN32
  // std::less<> is a transparent comparator that's necessary to pass
  // absl::string_view to map::find(), etc.
  absl::flat_hash_map<std::string, std::wstring> expected_server_ntpath_cache_;
#endif  // _WIN32
};

}  // namespace mozc

#endif  // MOZC_IPC_IPC_PATH_MANAGER_H_
