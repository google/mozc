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
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/singleton.h"
#include "base/strings/zstring_view.h"
#include "base/thread.h"
#include "ipc/ipc_path_manager.h"

#ifdef _WIN32
#include <wil/resource.h>
#include <windows.h>

#include "base/vlog.h"
#else  // _WIN32
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#endif  // _WIN32

namespace mozc {

void IPCServer::LoopAndReturn() {
  if (server_thread_ == nullptr) {
    server_thread_ = std::make_unique<Thread>([this] { this->Loop(); });
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

std::unique_ptr<IPCClientInterface> IPCClientFactory::NewClient(
    zstring_view name, zstring_view path_name) {
  return std::make_unique<IPCClient>(name, path_name);
}

std::unique_ptr<IPCClientInterface> IPCClientFactory::NewClient(
    zstring_view name) {
  return std::make_unique<IPCClient>(name);
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
bool IPCClient::TerminateServer(const absl::string_view name) {
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

#ifdef _WIN32
  wil::unique_process_handle handle(
      ::OpenProcess(PROCESS_TERMINATE, false, static_cast<DWORD>(pid)));
  if (!handle) {
    LOG(ERROR) << "OpenProcess failed: " << ::GetLastError();
    return false;
  }

  if (!::TerminateProcess(handle.get(), 0)) {
    LOG(ERROR) << "TerminateProcess failed: " << ::GetLastError();
    return false;
  }

  MOZC_VLOG(1) << "Success to terminate the server: " << name << " " << pid;

  return true;
#else   // _WIN32
  if (-1 == ::kill(static_cast<pid_t>(pid), 9)) {
    LOG(ERROR) << "kill failed: " << errno;
    return false;
  }

  return true;
#endif  // _WIN32
}
}  // namespace mozc
