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

#include "ipc/ipc.h"

#include <cstdint>

#ifdef OS_WIN
#include <windows.h>
#else  // OS_WIN
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#endif  // OS_WIN

#include <cstdlib>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "ipc/ipc_path_manager.h"

namespace mozc {

namespace {

class IPCServerThread : public Thread {
 public:
  explicit IPCServerThread(IPCServer *server) : server_(server) {}
  ~IPCServerThread() override {}
  void Run() override {
    if (server_ != nullptr) {
      server_->Loop();
    }
  }

 private:
  IPCServer *server_;

  DISALLOW_COPY_AND_ASSIGN(IPCServerThread);
};

}  // namespace

void IPCServer::LoopAndReturn() {
  if (server_thread_ == nullptr) {
    server_thread_ = std::make_unique<IPCServerThread>(this);
    server_thread_->SetJoinable(true);
    server_thread_->Start("IPCServer");
  } else {
    LOG(WARNING) << "Another thead is already running";
  }
}

void IPCServer::Wait() {
  if (server_thread_ != nullptr) {
    server_thread_->Join();
    server_thread_.reset();
  }
}

IPCClientInterface::~IPCClientInterface() {}

IPCClientFactoryInterface::~IPCClientFactoryInterface() {}

IPCClientFactory::~IPCClientFactory() {}

IPCClientInterface *IPCClientFactory::NewClient(const std::string &name,
                                                const std::string &path_name) {
  return new IPCClient(name, path_name);
}

IPCClientInterface *IPCClientFactory::NewClient(const std::string &name) {
  return new IPCClient(name);
}

// static
IPCClientFactory *IPCClientFactory::GetIPCClientFactory() {
  return Singleton<IPCClientFactory>::get();
}

uint32_t IPCClient::GetServerProtocolVersion() const {
  DCHECK(ipc_path_manager_);
  return ipc_path_manager_->GetServerProtocolVersion();
}

const std::string &IPCClient::GetServerProductVersion() const {
  DCHECK(ipc_path_manager_);
  return ipc_path_manager_->GetServerProductVersion();
}

uint32_t IPCClient::GetServerProcessId() const {
  DCHECK(ipc_path_manager_);
  return ipc_path_manager_->GetServerProcessId();
}

// static
bool IPCClient::TerminateServer(const std::string &name) {
  IPCClient client(name);

  if (!client.Connected()) {
    LOG(ERROR) << "Server " << name << " is not running";
    return true;
  }

  const uint32_t pid = client.GetServerProcessId();
  if (pid == 0) {
    LOG(ERROR) << "pid is not a valid value: " << pid;
    return false;
  }

#ifdef OS_WIN
  HANDLE handle =
      ::OpenProcess(PROCESS_TERMINATE, false, static_cast<DWORD>(pid));
  if (nullptr == handle) {
    LOG(ERROR) << "OpenProcess failed: " << ::GetLastError();
    return false;
  }

  if (!::TerminateProcess(handle, 0)) {
    LOG(ERROR) << "TerminateProcess failed: " << ::GetLastError();
    ::CloseHandle(handle);
    return false;
  }

  VLOG(1) << "Success to terminate the server: " << name << " " << pid;

  ::CloseHandle(handle);

  return true;
#else   // OS_WIN
  if (-1 == ::kill(static_cast<pid_t>(pid), 9)) {
    LOG(ERROR) << "kill failed: " << errno;
    return false;
  }

  return true;
#endif  // OS_WIN
}
}  // namespace mozc
