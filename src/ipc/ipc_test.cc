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

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/util.h"
#include "ipc/ipc_test_util.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/flags/flag.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace {

// NOTE(komatsu): The name should not end with "_test", otherwise our
// testing tool rut.py misunderstood that the file named
// kServerAddress is a binary to be tested.
static constexpr char kServerAddress[] = "test_echo_server";
#ifdef OS_WIN
// On windows, multiple-connections failed.
static constexpr int kNumThreads = 1;
#else  // OS_WIN
static constexpr int kNumThreads = 5;
#endif  // OS_WIN
static constexpr int kNumRequests = 2000;

std::string GenRandomString(size_t size) {
  std::string result;
  while (result.length() < size) {
    result += static_cast<char>(mozc::Util::Random(256));
  }
  return result;
}

class MultiConnections : public mozc::Thread {
 public:
#ifdef __APPLE__
  MultiConnections() : mach_port_manager_(nullptr) {}

  void SetMachPortManager(mozc::MachPortManagerInterface *manager) {
    mach_port_manager_ = manager;
  }
#endif  // __APPLE__

  void Run() override {
    absl::SleepFor(absl::Seconds(2));
    for (int i = 0; i < kNumRequests; ++i) {
      mozc::IPCClient con(kServerAddress, "");
#ifdef __APPLE__
      con.SetMachPortManager(mach_port_manager_);
#endif  // __APPLE__
      ASSERT_TRUE(con.Connected());
      const int size = std::max(mozc::Util::Random(8000), 1);
      std::string input = "test";
      input += GenRandomString(size);
      std::string output;
      ASSERT_TRUE(con.Call(input, &output, 1000));
      EXPECT_EQ(output.size(), input.size());
      EXPECT_EQ(output, input);
    }
  }

 private:
#ifdef __APPLE__
  mozc::MachPortManagerInterface *mach_port_manager_;
#endif  // __APPLE__
};

class EchoServer : public mozc::IPCServer {
 public:
  EchoServer(const std::string &path, int32_t num_connections, int32_t timeout)
      : IPCServer(path, num_connections, timeout) {}
  bool Process(absl::string_view input, std::string *output) override {
    if (input == "kill") {
      output->clear();
      return false;
    }
    output->assign(input.data(), input.size());
    return true;
  }
};
}  // namespace

TEST(IPCTest, IPCTest) {
  mozc::SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
#ifdef __APPLE__
  mozc::TestMachPortManager manager;
#endif  // __APPLE__

  EchoServer con(kServerAddress, 10, 1000);
#ifdef __APPLE__
  con.SetMachPortManager(&manager);
#endif  // __APPLE__
  con.LoopAndReturn();

  // mozc::Thread is not designed as value-semantics.
  // So here we use pointers to maintain these instances.
  std::vector<MultiConnections *> cons(kNumThreads);
  for (size_t i = 0; i < cons.size(); ++i) {
    cons[i] = new MultiConnections;
#ifdef __APPLE__
    cons[i]->SetMachPortManager(&manager);
#endif  // __APPLE__
    cons[i]->SetJoinable(true);
    cons[i]->Start("IPCTest");
  }
  for (size_t i = 0; i < cons.size(); ++i) {
    cons[i]->Join();
    delete cons[i];
    cons[i] = nullptr;
  }

  mozc::IPCClient kill(kServerAddress, "");
  const std::string kill_cmd = "kill";
  std::string output;
#ifdef __APPLE__
  kill.SetMachPortManager(&manager);
#endif  // __APPLE__
  // We don't care the return value of this Call() because the return
  // value for server finish can change based on the platform
  // implementations.
  // TODO(mukai, team): determine the spec of return value for that
  // case and add EXPECT_(TRUE|FALSE) here.
  kill.Call(kill_cmd, &output, 1000);

  con.Wait();
}
