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

// skip all if Windows
#ifdef OS_LINUX

#include "ipc/ipc.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <libgen.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#ifdef OS_MACOSX
#include <sys/ucred.h>
#endif
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cstdlib>

#include "base/util.h"
#include "ipc/ipc_path_manager.h"

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

namespace mozc {

namespace {

const int kInvalidSocket = -1;

void mkdir_p(const string &dirname) {
  const string parent_dir = mozc::Util::Dirname(dirname);
  struct stat st;
  if (!parent_dir.empty() &&
      ::stat(parent_dir.c_str(), &st) < 0) {
    mkdir_p(parent_dir);
  }
  mozc::Util::CreateDirectory(dirname);
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
  if (select(socket + 1, &fds, NULL, NULL, &tv) < 0) {
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
  if (select(socket + 1, NULL, &fds, NULL, &tv) < 0) {
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

#ifdef OS_MACOSX
  // If the OS is MAC, we should validate the peer by using LOCAL_PEERCRED.
  struct xucred peer_cred;
  socklen_t peer_cred_len = sizeof(struct xucred);
  if (::getsockopt(socket, 0, LOCAL_PEERCRED,
                   &peer_cred, &peer_cred_len) < 0) {
    LOG(ERROR) << "cannot get peer credential.  NOT a Unix socket?";
    return false;
  }
  if (peer_cred.cr_version != XUCRED_VERSION) {
    LOG(WARNING) << "credential version mismatch.";
    return false;
  }
  if (peer_cred.cr_uid != ::geteuid()) {
    LOG(WARNING) << "uid mismatch." << peer_cred.cr_uid << "!=" << ::geteuid();
    return false;
  }

  // MacOS doesn't have cr_pid;
  *pid = 0;
#endif

#ifdef OS_LINUX
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
#endif

  return true;
}

bool SendMessage(int socket,
                 const char *buf,
                 size_t buf_length, int timeout,
                 IPCErrorType *last_ipc_error) {
  size_t buf_length_left = buf_length;
  while (buf_length_left > 0) {
    if (IsWriteTimeout(socket, timeout)) {
      LOG(WARNING) << "Write timeout " << timeout;
      *last_ipc_error = IPC_TIMEOUT_ERROR;
      return false;
    }
    const ssize_t l = ::send(socket, buf, buf_length_left,
#ifdef OS_MACOSX
                             SO_NOSIGPIPE
#else
                             MSG_NOSIGNAL
#endif
                             );
    if (l < 0) {
      // An error occurs.
      LOG(ERROR) << "an error occurred during sending \""
                 << string(buf, buf_length_left) << "\": " << strerror(errno);
      *last_ipc_error = IPC_WRITE_ERROR;
      return false;
    }
    buf += l;
    buf_length_left -= l;
  }
  VLOG(1) << buf_length << " bytes sent";
  return true;
}

bool RecvMessage(int socket,
                 char *buf,
                 size_t *buf_length,
                 int timeout,
                 IPCErrorType *last_ipc_error) {
  if (*buf_length == 0) {
    LOG(WARNING) << "buf_length is 0";
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }
  ssize_t buf_left = *buf_length;
  ssize_t read_length = 0;
  *buf_length = 0;
  do {
    if (IsReadTimeout(socket, timeout)) {
      LOG(WARNING) << "Read timeout " << timeout;
      *last_ipc_error = IPC_TIMEOUT_ERROR;
      return false;
    }
    read_length = ::recv(socket, buf, buf_left, 0);
    if (read_length < 0) {
      LOG(ERROR) << "an error occurred during recv(): " << strerror(errno);
      *buf_length = 0;
      *last_ipc_error = IPC_READ_ERROR;
      return false;
    }
    *buf_length += read_length;
    buf += read_length;
    buf_left -= read_length;
  } while (read_length != 0 && buf_left > 0);
  VLOG(1) << *buf_length << " bytes received";
  return true;
}

void SetCloseOnExecFlag(int fd) {
  int flags = ::fcntl(fd, F_GETFD, 0);
  if (flags < 0) {
    LOG(WARNING) << "fcntl(F_GETFD) for fd " << fd << " failed: "
                 << strerror(errno);
    return;
  }
  flags |= FD_CLOEXEC;
  if (::fcntl(fd, F_SETFD, flags) != 0) {
    LOG(WARNING) << "fcntl(F_SETFD) for fd " << fd << " failed: "
                 << strerror(errno);
  }
}

// Returns true if address is in abstract namespace. See unix(7) on Linux for
// details.
bool IsAbstractSocket(const string& address) {
  return (!address.empty()) && (address[0] == '\0');
}
}  // namespace

// Client
IPCClient::IPCClient(const string &name)
    : socket_(kInvalidSocket), connected_(false),
      ipc_path_manager_(NULL),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, "");
}

IPCClient::IPCClient(const string &name, const string &server_path)
    : socket_(kInvalidSocket), connected_(false),
      ipc_path_manager_(NULL),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, server_path);
}

void IPCClient::Init(const string &name, const string &server_path) {
  last_ipc_error_ = IPC_NO_CONNECTION;

  // Try twice, because key may be changed.
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager(name);
  if (manager == NULL) {
    LOG(ERROR) << "IPCPathManager::GetIPCPathManager failed";
    return;
  }

  ipc_path_manager_ = manager;

  for (size_t trial = 0; trial < 2; ++trial) {
    string server_address;
    if (!manager->GetPathName(&server_address)) {
      continue;
    }
    sockaddr_un address;
    ::memset(&address, 0, sizeof(address));
    const size_t server_address_length =
        (server_address.size() >= UNIX_PATH_MAX) ?
        UNIX_PATH_MAX - 1 : server_address.size();
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
#ifdef OS_MACOSX
    address.sun_len = SUN_LEN(&address);
    const size_t sun_len = sizeof(address);
#else
    const size_t sun_len = sizeof(address.sun_family) + server_address_length;
#endif
    pid_t pid = 0;
    if (::connect(socket_,
                  reinterpret_cast<const sockaddr*>(&address),
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
      if (!manager->IsValidServer(static_cast<uint32>(pid),
                                  server_path)) {
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
bool IPCClient::Call(const char *request_,
                     size_t input_length,
                     char *response_,
                     size_t *response_size,
                     int32 timeout) {
  last_ipc_error_ = IPC_NO_ERROR;
  if (!SendMessage(socket_, request_, input_length, timeout,
                   &last_ipc_error_)) {
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

  if (!RecvMessage(socket_, response_, response_size, timeout,
                   &last_ipc_error_)) {
    LOG(ERROR) << "RecvMessage failed";
    return false;
  }
  VLOG(1) << "Call succeeded";
  return true;
}

bool IPCClient::Connected() const {
  return connected_;
}

// Server
IPCServer::IPCServer(const string &name,
                     int32 num_connections,
                     int32 timeout)
    : connected_(false), socket_(kInvalidSocket), timeout_(timeout) {
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager(name);
  if (!manager->CreateNewPathName() ||
      !manager->GetPathName(&server_address_)) {
    LOG(ERROR) << "Cannot make IPC path name";
    return;
  }

  const string dirname = Util::Dirname(server_address_);
  mkdir_p(dirname);

  sockaddr_un addr;
  ::memset(&addr, 0, sizeof(addr));
  socket_ = ::socket(PF_UNIX, SOCK_STREAM, 0);
  if (socket_ < 0) {
    LOG(WARNING) << "socket failed: " << strerror(errno);
    return;
  }
  SetCloseOnExecFlag(socket_);

  if (server_address_.size() >= UNIX_PATH_MAX) {
    LOG(WARNING) << "server address is too long";
    return;
  }

  addr.sun_family = AF_UNIX;
  ::memcpy(addr.sun_path,
           server_address_.data(),
           server_address_.size());
  addr.sun_path[server_address_.size()] = '\0';

  int on = 1;
  ::setsockopt(socket_,
               SOL_SOCKET,
               SO_REUSEADDR,
               reinterpret_cast<char *>(&on),
               sizeof(on));
#ifdef OS_MACOSX
  addr.sun_len = SUN_LEN(&addr);
  const size_t sun_len = sizeof(addr);
#else
  const size_t sun_len = sizeof(addr.sun_family) + server_address_.size();
#endif
  if (!IsAbstractSocket(server_address_)) {
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
  if (server_thread_.get() != NULL) {
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

bool IPCServer::Connected() const {
  return connected_;
}

void IPCServer::Loop() {
  // The most portable and straightforward single-thread server
  bool error = false;
  IPCErrorType last_ipc_error = IPC_NO_ERROR;
  pid_t pid = 0;
  while (!error) {
    const int new_sock = ::accept(socket_, NULL, NULL);
    if (new_sock < 0) {
      LOG(FATAL) << "accept() failed: " << strerror(errno);
      return;
    }
    if (!IsPeerValid(new_sock, &pid)) {
      continue;
    }
    size_t request_size = sizeof(request_);
    size_t response_size = sizeof(response_);
    if (RecvMessage(new_sock,
                    &request_[0],
                    &request_size, timeout_, &last_ipc_error)) {
      if (!Process(&request_[0], request_size,
                   &response_[0], &response_size)) {
        LOG(WARNING) << "Process() failed";
        error = true;
      }
      if (response_size > 0) {
        SendMessage(new_sock,
                    &response_[0],
                    response_size, timeout_, &last_ipc_error);
      }
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

};  // namespace mozc

#endif  // OS_LINUX
