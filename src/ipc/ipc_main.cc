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

#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/thread.h"
#include "ipc/ipc.h"
#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

ABSL_FLAG(std::string, server_address, "ipc_test", "");
ABSL_FLAG(bool, test, false, "automatic test mode");
ABSL_FLAG(bool, server, false, "invoke as server mode");
ABSL_FLAG(bool, client, false, "invoke as client mode");
ABSL_FLAG(std::string, server_path, "", "server path");
ABSL_FLAG(int32_t, num_threads, 10, "number of threads");
ABSL_FLAG(int32_t, num_requests, 100, "number of requests");

namespace mozc {

class EchoServer : public IPCServer {
 public:
  EchoServer(const std::string &path, int32_t num_connections,
             absl::Duration timeout)
      : IPCServer(path, num_connections, timeout) {}
  bool Process(absl::string_view input, std::string *output) override {
    output->assign(input.data(), input.size());
    return (input != "kill");
  }
};

}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (absl::GetFlag(FLAGS_test)) {
    mozc::EchoServer con(absl::GetFlag(FLAGS_server_address), 10,
                         absl::Milliseconds(1000));
    mozc::Thread server_thread_main([&con] { con.Loop(); });

    std::vector<mozc::Thread> cons;
    cons.reserve(absl::GetFlag(FLAGS_num_threads));
    for (int i = 0; i < absl::GetFlag(FLAGS_num_threads); ++i) {
      cons.push_back(mozc::Thread([] {
        for (int i = 0; i < absl::GetFlag(FLAGS_num_requests); ++i) {
          mozc::IPCClient con(absl::GetFlag(FLAGS_server_address),
                              absl::GetFlag(FLAGS_server_path));
          CHECK(con.Connected());
          std::string input = "testtesttesttest";
          std::string output;
          CHECK(con.Call(input, &output, absl::Milliseconds(1000)));
          CHECK_EQ(input.size(), output.size());
          CHECK_EQ(input, output);
        }
      }));
    }
    for (auto &con : cons) {
      con.Join();
    }

    mozc::IPCClient kill(absl::GetFlag(FLAGS_server_address),
                         absl::GetFlag(FLAGS_server_path));
    const std::string kill_cmd = "kill";
    std::string output;
    kill.Call(kill_cmd, &output, absl::Milliseconds(1000));
    server_thread_main.Join();

    LOG(INFO) << "Done";

  } else if (absl::GetFlag(FLAGS_server)) {
    mozc::EchoServer con(absl::GetFlag(FLAGS_server_address), 10,
                         absl::Milliseconds(-1));
    CHECK(con.Connected());
    LOG(INFO) << "Start Server at " << absl::GetFlag(FLAGS_server_address);
    con.Loop();
  } else if (absl::GetFlag(FLAGS_client)) {
    std::string line;
    std::string response;
    while (std::getline(std::cin, line)) {
      mozc::IPCClient con(absl::GetFlag(FLAGS_server_address),
                          absl::GetFlag(FLAGS_server_path));
      CHECK(con.Connected());
      CHECK(con.Call(line, &response, absl::Milliseconds(1000)));
      std::cout << "Request: " << line << std::endl;
      std::cout << "Response: " << response << std::endl;
    }
  } else {
    LOG(INFO) << "either --server or --client or --test must be set true";
  }

  return 0;
}
