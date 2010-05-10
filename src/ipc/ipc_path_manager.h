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

#ifndef MOZC_IPC_IPC_PATH_MANAGER_H_
#define MOZC_IPC_IPC_PATH_MANAGER_H_

#include <string>
#include "base/base.h"
#include "base/mutex.h"

namespace mozc {

namespace ipc {
class IPCPathInfo;
}

class ProcessMutex;

class IPCPathManager {
 public:
  // Create new path name and save it inside internal memory.
  // if the path is already created, return true and do nothing.
  bool CreateNewPathName();

  // Save ipc path to ~/.mozc/.ipc.<name>
  // if server_ipc_key_ is empty, CreateNewPathName()
  // is called automatically
  // If the file is already locked, return false
  bool SavePathName();

  // Get the current path from memory.
  // return false if LoadPathName() failed.
  bool GetPathName(string *path_name);

  // Get the old path name from memory.
  // return false if LoadPathName() failed.
  bool GetOldPathName(string *path_name);

  // return protocol version.
  // return 0 if protocol version is not defined.
  uint32 GetServerProtocolVersion() const;

  // return product version.
  // return "0.0.0.0" if product version is not defined
  const string &GetServerProductVersion() const;

  // return process id of the server
  uint32 GetServerProcessId() const;

  // Check the server pid is the valid server specified with server_path.
  // server pid can be obtained by OS dependent method.
  // This API is only available on Windows Vista or Linux.
  // Always return true on other operating systems.
  // See also: GetNamedPipeServerProcessId
  // http://msdn.microsoft.com/en-us/library/aa365446(VS.85).aspx
  // when pid of 0 is passed, IsValidServer() returns true.
  // when pid of static_cast<size_t>(-1) is passed, IsValidServer()
  // returns false.
  // To keep backward compatibility and other operationg system
  // having no support of getting peer's pid, you can set 0 pid.
  bool IsValidServer(uint32 pid, const string &server_path);

  // clear ipc_key;
  void Clear();

  // return singleton instance corresponding to "name"
  static IPCPathManager *GetIPCPathManager(const string &name);

  // do not call constructor directly.
  explicit IPCPathManager(const string &name);
  virtual ~IPCPathManager();

 private:

  // Load ipc name from ~/.mozc/.ipc
  // Note that this method overwrites the ipc_key_
  bool LoadPathName();

  scoped_ptr<ProcessMutex> path_mutex_;   // lock ipc path file
  scoped_ptr<Mutex> mutex_;   // mutex for methods
  scoped_ptr<ipc::IPCPathInfo> ipc_path_info_;
  string name_;
  string server_path_;   // cache for server_path
  uint32 server_pid_;    // cache for pid of server_path
};
}  // mozc

#endif  // MOZC_IPC_IPC_PATH_MANAGER_H_
