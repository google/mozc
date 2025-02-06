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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "base/init_mozc.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "engine/engine_factory.h"
#include "protocol/commands.pb.h"
#include "session/random_keyevents_generator.h"
#include "session/session_handler.h"

#ifdef _WIN32
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using ssize_t = SSIZE_T;
#else  // _WIN32
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif  // _WIN32

ABSL_FLAG(std::string, host, "localhost", "server host name");
ABSL_FLAG(bool, server, true, "server mode");
ABSL_FLAG(bool, client, false, "client mode");
ABSL_FLAG(int32_t, client_test_size, 100, "client test size");
ABSL_FLAG(int32_t, port, 8000, "port of RPC server");
ABSL_FLAG(int32_t, rpc_timeout, 60000, "timeout");
ABSL_FLAG(std::string, user_profile_directory, "", "user profile directory");

namespace mozc {

namespace {

constexpr size_t kMaxRequestSize = 32 * 32 * 8192;
constexpr size_t kMaxOutputSize = 32 * 32 * 8192;
constexpr int kInvalidSocket = -1;

// TODO(taku): timeout should be handled.
bool Recv(int socket, char *buf, size_t buf_size, int timeout) {
  ssize_t buf_left = buf_size;
  while (buf_left > 0) {
    const ssize_t read_size = ::recv(socket, buf, buf_left, 0);
    if (read_size < 0) {
      LOG(ERROR) << "an error occurred during recv()";
      return false;
    }
    buf += read_size;
    buf_left -= read_size;
  }
  return buf_left == 0;
}

// TODO(taku): timeout should be handled.
bool Send(int socket, const char *buf, size_t buf_size, int timeout) {
  ssize_t buf_left = buf_size;
  while (buf_left > 0) {
#if defined(_WIN32)
    constexpr int kFlag = 0;
#elif defined(__APPLE__)  // defined(_WIN32)
    constexpr int kFlag = SO_NOSIGPIPE;
#else                     // defined(__APPLE__)
    constexpr int kFlag = MSG_NOSIGNAL;
#endif                    // defined(__APPLE__)
    const ssize_t read_size = ::send(socket, buf, buf_left, kFlag);
    if (read_size < 0) {
      LOG(ERROR) << "an error occurred during sending";
      return false;
    }
    buf += read_size;
    buf_left -= read_size;
  }
  return buf_left == 0;
}

void CloseSocket(int client_socket) {
#ifdef _WIN32
  ::closesocket(client_socket);
  ::shutdown(client_socket, SD_BOTH);
#else   // _WIN32
  ::close(client_socket);
  ::shutdown(client_socket, SHUT_RDWR);
#endif  // _WIN32
}

// Standalone RPCServer.
// TODO(taku): Make a RPC class inherited from IPCInterface.
// This allows us to reuse client::Session library and SessionServer.
class RPCServer {
 public:
  RPCServer()
      : server_socket_(kInvalidSocket),
        handler_(new SessionHandler(EngineFactory::Create().value())) {
    server_socket_ = ::socket(AF_INET, SOCK_STREAM, 0);

    CHECK_NE(server_socket_, kInvalidSocket) << "socket failed";

#ifndef _WIN32
    int flags = ::fcntl(server_socket_, F_GETFD, 0);
    CHECK_GE(flags, 0) << "fcntl(F_GETFD) failed";
    flags |= FD_CLOEXEC;
    CHECK_EQ(::fcntl(server_socket_, F_SETFD, flags), 0)
        << "fctl(F_SETFD) failed";
#endif  // !_WIN32

    struct sockaddr_in sin = {};
    sin.sin_port = htons(absl::GetFlag(FLAGS_port));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    int on = 1;
    ::setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<char *>(&on), sizeof(on));

    CHECK_GE(::bind(server_socket_, reinterpret_cast<struct sockaddr *>(&sin),
                    sizeof(sin)),
             0)
        << "bind failed";

    CHECK_GE(::listen(server_socket_, SOMAXCONN), 0) << "listen failed";
    CHECK_NE(server_socket_, 0);
  }

  ~RPCServer() {
    CloseSocket(server_socket_);
    server_socket_ = kInvalidSocket;
  }

  void Loop() {
    LOG(INFO) << "Start Mozc RPCServer";

    while (true) {
      const int client_socket = ::accept(server_socket_, nullptr, nullptr);

      if (client_socket == kInvalidSocket) {
        LOG(ERROR) << "accept failed";
        continue;
      }

      uint32_t request_size = 0;
      // Receive the size of data.
      if (!Recv(client_socket, reinterpret_cast<char *>(&request_size),
                sizeof(request_size), absl::GetFlag(FLAGS_rpc_timeout))) {
        LOG(ERROR) << "Cannot receive request_size header.";
        CloseSocket(client_socket);
        continue;
      }
      request_size = ntohl(request_size);
      CHECK_GT(request_size, 0);
      CHECK_LT(request_size, kMaxRequestSize);

      // Receive the body of serialized protobuf.
      std::unique_ptr<char[]> request_str(new char[request_size]);
      if (!Recv(client_socket, request_str.get(), request_size,
                absl::GetFlag(FLAGS_rpc_timeout))) {
        LOG(ERROR) << "cannot receive body of request.";
        CloseSocket(client_socket);
        continue;
      }

      commands::Command command;
      if (!command.mutable_input()->ParseFromArray(request_str.get(),
                                                   request_size)) {
        LOG(ERROR) << "ParseFromArray failed";
        CloseSocket(client_socket);
        continue;
      }

      CHECK(handler_->EvalCommand(&command));

      std::string output_str;
      // Return the result.
      CHECK(command.output().SerializeToString(&output_str));

      uint32_t output_size = output_str.size();
      CHECK_GT(output_size, 0);
      CHECK_LT(output_size, kMaxOutputSize);
      output_size = htonl(output_size);

      if (!Send(client_socket, reinterpret_cast<char *>(&output_size),
                sizeof(output_size), absl::GetFlag(FLAGS_rpc_timeout)) ||
          !Send(client_socket, output_str.data(), output_str.size(),
                absl::GetFlag(FLAGS_rpc_timeout))) {
        LOG(ERROR) << "Cannot send reply.";
      }

      CloseSocket(client_socket);
    }
  }

 private:
  int server_socket_;
  std::unique_ptr<SessionHandler> handler_;
};

// Standalone RPCClient.
// TODO(taku): Make a RPC class inherited from IPCInterface.
// This allows us to reuse client::Session library and SessionServer.
class RPCClient {
 public:
  RPCClient() : id_(0) {}

  bool CreateSession() {
    id_ = 0;
    commands::Input input;
    commands::Output output;
    input.set_type(commands::Input::CREATE_SESSION);
    if (!Call(input, &output) ||
        output.error_code() != commands::Output::SESSION_SUCCESS) {
      return false;
    }
    id_ = output.id();
    return true;
  }

  bool DeleteSession() {
    commands::Input input;
    commands::Output output;
    id_ = 0;
    input.set_type(commands::Input::DELETE_SESSION);
    return (Call(input, &output) &&
            output.error_code() == commands::Output::SESSION_SUCCESS);
  }

  bool SendKey(const mozc::commands::KeyEvent &key,
               mozc::commands::Output *output) const {
    if (id_ == 0) {
      return false;
    }
    commands::Input input;
    input.set_type(commands::Input::SEND_KEY);
    input.set_id(id_);
    *input.mutable_key() = key;
    return (Call(input, output) &&
            output->error_code() == commands::Output::SESSION_SUCCESS);
  }

 private:
  bool Call(const commands::Input &input, commands::Output *output) const {
    struct addrinfo hints = {}, *res;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    const std::string port_str = std::to_string(absl::GetFlag(FLAGS_port));
    CHECK_EQ(::getaddrinfo(absl::GetFlag(FLAGS_host).c_str(), port_str.c_str(),
                           &hints, &res),
             0)
        << "getaddrinfo failed";

    const int client_socket =
        ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    CHECK_NE(client_socket, kInvalidSocket) << "socket failed";
    CHECK_GE(::connect(client_socket, res->ai_addr, res->ai_addrlen), 0)
        << "connect failed";

    std::string request_str;
    CHECK(input.SerializeToString(&request_str));
    uint32_t request_size = request_str.size();
    CHECK_GT(request_size, 0);
    CHECK_LT(request_size, kMaxRequestSize);
    request_size = htonl(request_size);

    CHECK(Send(client_socket, reinterpret_cast<char *>(&request_size),
               sizeof(request_size), absl::GetFlag(FLAGS_rpc_timeout)));
    CHECK(Send(client_socket, request_str.data(), request_str.size(),
               absl::GetFlag(FLAGS_rpc_timeout)));

    uint32_t output_size = 0;
    CHECK(Recv(client_socket, reinterpret_cast<char *>(&output_size),
               sizeof(output_size), absl::GetFlag(FLAGS_rpc_timeout)));
    output_size = ntohl(output_size);
    CHECK_GT(output_size, 0);
    CHECK_LT(output_size, kMaxOutputSize);

    std::unique_ptr<char[]> output_str(new char[output_size]);
    CHECK(Recv(client_socket, output_str.get(), output_size,
               absl::GetFlag(FLAGS_rpc_timeout)));

    CHECK(output->ParseFromArray(output_str.get(), output_size));

    ::freeaddrinfo(res);

    CloseSocket(client_socket);

    return true;
  }

  uint64_t id_;
};

// Wrapper class for WSAStartup on Windows.
class ScopedWSAData {
 public:
  ScopedWSAData() {
#ifdef _WIN32
    WSADATA wsaData;
    CHECK_EQ(::WSAStartup(MAKEWORD(2, 1), &wsaData), 0) << "WSAStartup failed";
#endif  // _WIN32
  }
  ~ScopedWSAData() {
#ifdef _WIN32
    ::WSACleanup();
#endif  // _WIN32
  }
};
}  // namespace

}  // namespace mozc

int main(int argc, char *argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);

  mozc::ScopedWSAData wsadata;

  if (!absl::GetFlag(FLAGS_user_profile_directory).empty()) {
    LOG(INFO) << "Setting user profile directory to "
              << absl::GetFlag(FLAGS_user_profile_directory);
    mozc::SystemUtil::SetUserProfileDirectory(
        absl::GetFlag(FLAGS_user_profile_directory));
  }

  if (absl::GetFlag(FLAGS_client)) {
    mozc::RPCClient client;
    CHECK(client.CreateSession());
    mozc::session::RandomKeyEventsGenerator key_events_generator;
    for (int n = 0; n < absl::GetFlag(FLAGS_client_test_size); ++n) {
      std::vector<mozc::commands::KeyEvent> keys;
      key_events_generator.GenerateSequence(&keys);
      for (size_t i = 0; i < keys.size(); ++i) {
        LOG(INFO) << "Sending to Server: " << keys[i];
        mozc::commands::Output output;
        CHECK(client.SendKey(keys[i], &output));
        LOG(INFO) << "Output of SendKey: " << output;
      }
    }
    CHECK(client.DeleteSession());
    return 0;
  } else if (absl::GetFlag(FLAGS_server)) {
    mozc::RPCServer server;
    server.Loop();
  } else {
    LOG(ERROR) << "use --server or --client option";
    return -1;
  }

  return 0;
}
