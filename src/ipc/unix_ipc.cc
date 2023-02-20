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

// __linux__ only. Note that __ANDROID__/__wasm__ don't reach here.
#if defined(__linux__)

#include <fcntl.h>
#include <stddef.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/thread.h"
#include "ipc/ipc.h"
#include "ipc/ipc_path_manager.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif  // UNIX_PATH_MAX

namespace mozc {
namespace {

constexpr int kInvalidSocket = -1;

absl::Status mkdir_p(const std::string &dirname) {
  const std::string parent_dir = FileUtil::Dirname(dirname);
  struct stat st;
  if (!parent_dir.empty() && ::stat(parent_dir.c_str(), &st) < 0) {
    if (absl::Status s = mkdir_p(parent_dir); !s.ok()) {
      return s;
    }
  }
  return FileUtil::CreateDirectory(dirname);
}

bool IsReadTimeout(int socket, int timeout) {
  if (timeout < 0) {
    return false;
  }
  fd_set fds;
  struct timeval tv;
  FD_ZERO(&fds);
  FD_SET(socket, &fds);
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = 1000 * (timeout % 1000);
  if (select(socket + 1, &fds, nullptr, nullptr, &tv) < 0) {
    // Mac OS X and glibc implementations of strerror() return a pointer to a
    // string literal whenever errno is in a valid range, and thus thread-safe.
    // Probably we don't have to use the cumbersome strerror_r() function.
    LOG(WARNING) << "select() failed: " << strerror(errno);
    return true;
  }
  if (FD_ISSET(socket, &fds)) {
    return false;
  }

  LOG(ERROR) << "FD_ISSET failed";
  return true;
}

bool IsWriteTimeout(int socket, int timeout) {
  if (timeout < 0) {
    return false;
  }
  fd_set fds;
  struct timeval tv;
  FD_ZERO(&fds);
  FD_SET(socket, &fds);
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = 1000 * (timeout % 1000);
  if (select(socket + 1, nullptr, &fds, nullptr, &tv) < 0) {
    LOG(WARNING) << "select() failed: " << strerror(errno);
    return true;
  }
  if (FD_ISSET(socket, &fds)) {
    return false;
  }

  LOG(ERROR) << "FD_ISSET failed";
  return true;
}

bool IsPeerValid(int socket, pid_t *pid) {
  *pid = 0;

  // On ARM Linux, we do nothing and just return true since the platform
  // sometimes doesn't support the getsockopt(sock, SOL_SOCKET, SO_PEERCRED)
  // system call.
  // TODO(yusukes): Add implementation for ARM Linux.
#ifndef __arm__
  struct ucred peer_cred;
  int peer_cred_len = sizeof(peer_cred);
  if (getsockopt(socket, SOL_SOCKET, SO_PEERCRED,
                 reinterpret_cast<void *>(&peer_cred),
                 reinterpret_cast<socklen_t *>(&peer_cred_len)) < 0) {
    LOG(ERROR) << "cannot get peer credential. Not a Unix socket?";
    return false;
  }

  if (peer_cred.uid != ::geteuid()) {
    LOG(WARNING) << "uid mismatch." << peer_cred.uid << "!=" << ::geteuid();
    return false;
  }

  *pid = peer_cred.pid;
#endif  // __arm__

  return true;
}

IPCErrorType SendMessage(int socket, const std::string &msg, int timeout) {
  int offset = 0;
  while (msg.size() != offset) {
    if (IsWriteTimeout(socket, timeout)) {
      LOG(WARNING) << "Write timeout " << timeout;
      return IPC_TIMEOUT_ERROR;
    }
    const ssize_t l =
        ::send(socket, msg.data() + offset, msg.size() - offset, MSG_NOSIGNAL);
    if (l < 0) {
      // An error occurs.
      LOG(ERROR) << "an error occurred during sending \"" << msg.substr(offset)
                 << "\": " << strerror(errno);
      return IPC_WRITE_ERROR;
    }
    offset += l;
  }
  VLOG(1) << offset << " bytes sent";
  return IPC_NO_ERROR;
}

IPCErrorType RecvMessage(int socket, std::string *msg, int timeout) {
  if (!msg) {
    LOG(WARNING) << "msg is nullptr";
    return IPC_UNKNOWN_ERROR;
  }
  msg->resize(IPC_RESPONSESIZE);
  ssize_t read_length = 0;
  int offset = 0;
  do {
    if (IsReadTimeout(socket, timeout)) {
      LOG(WARNING) << "Read timeout " << timeout;
      msg->clear();
      return IPC_TIMEOUT_ERROR;
    }
    read_length = ::recv(socket, msg->data() + offset, msg->size() - offset,
                         /* flags */ 0);
    if (read_length < 0) {
      LOG(ERROR) << "an error occurred during recv(): " << strerror(errno);
      msg->clear();
      return IPC_READ_ERROR;
    }
    offset += read_length;
    if (msg->size() == offset) {
      msg->resize(msg->size() * 2);
    }
  } while (read_length != 0);
  VLOG(1) << offset << " bytes received";
  msg->resize(offset);
  return IPC_NO_ERROR;
}

void SetCloseOnExecFlag(int fd) {
  int flags = ::fcntl(fd, F_GETFD, 0);
  if (flags < 0) {
    LOG(WARNING) << "fcntl(F_GETFD) for fd " << fd
                 << " failed: " << strerror(errno);
    return;
  }
  flags |= FD_CLOEXEC;
  if (::fcntl(fd, F_SETFD, flags) != 0) {
    LOG(WARNING) << "fcntl(F_SETFD) for fd " << fd
                 << " failed: " << strerror(errno);
  }
}

// Returns true if address is in abstract namespace. See unix(7) on Linux for
// details.
bool IsAbstractSocket(const std::string &address) {
  return (!address.empty()) && (address[0] == '\0');
}
}  // namespace

// Client
IPCClient::IPCClient(const absl::string_view name)
    : socket_(kInvalidSocket),
      connected_(false),
      ipc_path_manager_(nullptr),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, "");
}

IPCClient::IPCClient(const absl::string_view name,
                     const absl::string_view server_path)
    : socket_(kInvalidSocket),
      connected_(false),
      ipc_path_manager_(nullptr),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, server_path);
}

void IPCClient::Init(const absl::string_view name,
                     const absl::string_view server_path) {
  last_ipc_error_ = IPC_NO_CONNECTION;

  // Try twice, because key may be changed.
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager(name);
  if (manager == nullptr) {
    LOG(ERROR) << "IPCPathManager::GetIPCPathManager failed";
    return;
  }

  ipc_path_manager_ = manager;

  for (size_t trial = 0; trial < 2; ++trial) {
    std::string server_address;
    if (!manager->LoadPathName() || !manager->GetPathName(&server_address)) {
      continue;
    }
    sockaddr_un address;
    ::memset(&address, 0, sizeof(address));
    const size_t server_address_length =
        (server_address.size() >= UNIX_PATH_MAX) ? UNIX_PATH_MAX - 1
                                                 : server_address.size();
    if (server_address.size() >= UNIX_PATH_MAX) {
      LOG(WARNING) << "too long path: " << server_address;
    }
    socket_ = socket(PF_UNIX, SOCK_STREAM, 0);
    if (socket_ < 0) {
      LOG(WARNING) << "socket failed: " << strerror(errno);
      continue;
    }
    SetCloseOnExecFlag(socket_);
    address.sun_family = AF_UNIX;
    ::memcpy(address.sun_path, server_address.data(), server_address_length);
    address.sun_path[server_address_length] = '\0';
    const size_t sun_len = sizeof(address.sun_family) + server_address_length;
    pid_t pid = 0;
    if (::connect(socket_, reinterpret_cast<const sockaddr *>(&address),
                  sun_len) != 0 ||
        !IsPeerValid(socket_, &pid)) {
      if ((errno == ENOTSOCK || errno == ECONNREFUSED) &&
          !IsAbstractSocket(server_address)) {
        // If abstract namepace is not enabled, recreate server_addresss path.
        ::unlink(server_address.c_str());
      }
      LOG(WARNING) << "connect failed: " << strerror(errno);
      connected_ = false;
      manager->Clear();
      continue;
    } else {
      if (!manager->IsValidServer(static_cast<uint32_t>(pid), server_path)) {
        LOG(ERROR) << "Connecting to invalid server";
        last_ipc_error_ = IPC_INVALID_SERVER;
        break;
      }
      last_ipc_error_ = IPC_NO_ERROR;
      connected_ = true;
      break;
    }
  }
}

IPCClient::~IPCClient() {
  if (socket_ != kInvalidSocket) {
    if (::close(socket_) < 0) {
      LOG(WARNING) << "close failed: " << strerror(errno);
    }
    socket_ = kInvalidSocket;
  }
  connected_ = false;
  VLOG(1) << "connection closed (IPCClient destructed)";
}

// RPC call
bool IPCClient::Call(const std::string &request, std::string *response,
                     int32_t timeout) {
  last_ipc_error_ = SendMessage(socket_, request, timeout);
  if (last_ipc_error_ != IPC_NO_ERROR) {
    LOG(ERROR) << "SendMessage failed";
    return false;
  }

  // Half-close the socket so that mozc_server could know the length of the
  // request data. Without this, RecvMessage() in mozc_server would fail with
  // timeout.
  // TODO(yusukes): It would be also possible to modify Send and RecvMessage()
  // so that they send/recv the payload size explicitly together with the actual
  // data. Will revisit later.
  ::shutdown(socket_, SHUT_WR);

  last_ipc_error_ = RecvMessage(socket_, response, timeout);
  if (last_ipc_error_ != IPC_NO_ERROR) {
    LOG(ERROR) << "RecvMessage failed";
    return false;
  }
  VLOG(1) << "Call succeeded";
  return true;
}

bool IPCClient::Connected() const { return connected_; }

// Server
IPCServer::IPCServer(const std::string &name, int32_t num_connections,
                     int32_t timeout)
    : connected_(false), socket_(kInvalidSocket), timeout_(timeout) {
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager(name);
  if (!manager->CreateNewPathName() && !manager->LoadPathName()) {
    LOG(ERROR) << "Cannot prepare IPC path name";
    return;
  }

  if (!manager->GetPathName(&server_address_)) {
    LOG(ERROR) << "Cannot make IPC path name";
    return;
  }
  DCHECK(!server_address_.empty());
  if (server_address_.size() >= UNIX_PATH_MAX) {
    LOG(WARNING) << "server address is too long";
    return;
  }

  const bool is_file_socket = !IsAbstractSocket(server_address_);
  if (is_file_socket) {
    // Linux does not use files for IPC.
    const std::string dirname = FileUtil::Dirname(server_address_);
    if (absl::Status s = mkdir_p(dirname); !s.ok()) {
      LOG(ERROR) << s << ": Cannot create " << dirname;
    }
  }

  sockaddr_un addr;
  ::memset(&addr, 0, sizeof(addr));
  socket_ = ::socket(PF_UNIX, SOCK_STREAM, 0);
  if (socket_ < 0) {
    LOG(WARNING) << "socket failed: " << strerror(errno);
    return;
  }
  SetCloseOnExecFlag(socket_);

  addr.sun_family = AF_UNIX;
  server_address_.copy(addr.sun_path, server_address_.size());
  addr.sun_path[server_address_.size()] = '\0';

  int on = 1;
  ::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&on),
               sizeof(on));
  const size_t sun_len = sizeof(addr.sun_family) + server_address_.size();
  if (is_file_socket) {
    // Linux does not use files for IPC.
    ::chmod(server_address_.c_str(), 0600);
  }
  if (::bind(socket_, reinterpret_cast<sockaddr *>(&addr), sun_len) != 0) {
    // The UNIX domain socket file (server_address_) already exists?
    LOG(FATAL) << "bind() failed: " << strerror(errno);
    return;
  }

  if (::listen(socket_, num_connections) < 0) {
    LOG(FATAL) << "listen() failed: " << strerror(errno);
    return;
  }

  if (!manager->SavePathName()) {
    LOG(ERROR) << "Cannot save IPC path name";
    return;
  }

  connected_ = true;
  VLOG(1) << "IPCServer ready";
}

IPCServer::~IPCServer() {
  if (server_thread_ != nullptr) {
    server_thread_->Terminate();
  }
  ::shutdown(socket_, SHUT_RDWR);
  ::close(socket_);
  if (!IsAbstractSocket(server_address_)) {
    // When abstract namespace is used, unlink() is not necessary.
    ::unlink(server_address_.c_str());
  }
  connected_ = false;
  socket_ = kInvalidSocket;
  VLOG(1) << "IPCServer destructed";
}

bool IPCServer::Connected() const { return connected_; }

void IPCServer::Loop() {
  // The most portable and straightforward single-thread server
  bool error = false;
  pid_t pid = 0;
  std::string request;
  std::string response;
  while (!error) {
    const int new_sock = ::accept(socket_, nullptr, nullptr);
    if (new_sock < 0) {
      LOG(FATAL) << "accept() failed: " << strerror(errno);
      return;
    }
    if (!IsPeerValid(new_sock, &pid)) {
      continue;
    }

    if (RecvMessage(new_sock, &request, timeout_) != IPC_NO_ERROR) {
      LOG(WARNING) << "RecvMessage() failed";
      ::close(new_sock);
      continue;
    }

    if (!Process(request, &response)) {
      LOG(WARNING) << "Process() failed";
      ::close(new_sock);
      error = true;
      continue;
    }

    if (response.empty()) {
      LOG(WARNING) << "response is empty";
      ::close(new_sock);
      continue;
    }

    if (SendMessage(new_sock, response, timeout_) != IPC_NO_ERROR) {
      LOG(WARNING) << "SendMessage() failed";
    }
    ::close(new_sock);
  }

  ::shutdown(socket_, SHUT_RDWR);
  ::close(socket_);
  if (!IsAbstractSocket(server_address_)) {
    // When abstract namespace is used, unlink() is not necessary.
    ::unlink(server_address_.c_str());
  }
  connected_ = false;
  socket_ = kInvalidSocket;
}

void IPCServer::Terminate() { server_thread_->Terminate(); }

}  // namespace mozc

#endif  // __linux__
