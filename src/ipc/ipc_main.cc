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

#include <cstring>
#include <iostream>  // NOLINT
#include <string>
#include <vector>

#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/thread.h"
#include "ipc/ipc.h"

DEFINE_string(server_address, "ipc_test", "");
DEFINE_bool(test, false, "automatic test mode");
DEFINE_bool(server, false, "invoke as server mode");
DEFINE_bool(client, false, "invoke as client mode");
DEFINE_string(server_path, "", "server path");
DEFINE_int32(num_threads, 10, "number of threads");
DEFINE_int32(num_requests, 100, "number of requests");

namespace mozc {

class MultiConnections : public Thread {
 public:
  void Run() {
    char buf[8192];
    for (int i = 0; i < mozc::GetFlag(FLAGS_num_requests); ++i) {
      mozc::IPCClient con(FLAGS_server_address,
                          mozc::GetFlag(FLAGS_server_path));
      CHECK(con.Connected());
      string input = "testtesttesttest";
      size_t length = sizeof(buf);
      ::memset(buf, 0, length);
      CHECK(con.Call(input.data(), input.size(), buf, &length, 1000));
      string output(buf, length);
      CHECK_EQ(input.size(), output.size());
      CHECK_EQ(input, output);
    }
  }
};

class EchoServer : public IPCServer {
 public:
  EchoServer(const string &path, int32 num_connections, int32 timeout)
      : IPCServer(path, num_connections, timeout) {}
  virtual bool Process(const char *input_buffer, size_t input_length,
                       char *output_buffer, size_t *output_length) {
    ::memcpy(output_buffer, input_buffer, input_length);
    *output_length = input_length;
    return ::memcmp("kill", input_buffer, 4) != 0;
  }
};

class EchoServerThread : public Thread {
 public:
  explicit EchoServerThread(EchoServer *con) : con_(con) {}
  virtual void Run() { con_->Loop(); }

 private:
  EchoServer *con_;
};

}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (mozc::GetFlag(FLAGS_test)) {
    mozc::EchoServer con(mozc::GetFlag(FLAGS_server_address), 10, 1000);
    mozc::EchoServerThread server_thread_main(&con);
    server_thread_main.SetJoinable(true);
    server_thread_main.Start("IpcMain");

    std::vector<mozc::MultiConnections> cons(mozc::GetFlag(FLAGS_num_threads));
    for (size_t i = 0; i < cons.size(); ++i) {
      cons[i].SetJoinable(true);
      cons[i].Start("MultiConnections");
    }
    for (size_t i = 0; i < cons.size(); ++i) {
      cons[i].Join();
    }

    mozc::IPCClient kill(FLAGS_server_address,
                         mozc::GetFlag(FLAGS_server_path));
    const char kill_cmd[32] = "kill";
    char output[32];
    size_t output_size = sizeof(output);
    kill.Call(kill_cmd, strlen(kill_cmd), output, &output_size, 1000);
    server_thread_main.Join();

    LOG(INFO) << "Done";

  } else if (mozc::GetFlag(FLAGS_server)) {
    mozc::EchoServer con(mozc::GetFlag(FLAGS_server_address), 10, -1);
    CHECK(con.Connected());
    LOG(INFO) << "Start Server at " << mozc::GetFlag(FLAGS_server_address);
    con.Loop();
  } else if (mozc::GetFlag(FLAGS_client)) {
    string line;
    char response[8192];
    while (getline(cin, line)) {
      mozc::IPCClient con(FLAGS_server_address,
                          mozc::GetFlag(FLAGS_server_path));
      CHECK(con.Connected());
      size_t response_size = sizeof(response);
      CHECK(con.Call(line.data(), line.size(), response, &response_size, 1000));
      std::cout << "Request: " << line << std::endl;
      std::cout << "Response: " << string(response, response_size) << std::endl;
    }
  } else {
    LOG(INFO) << "either --server or --client or --test must be set true";
  }

  return 0;
}
