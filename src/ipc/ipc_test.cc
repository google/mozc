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
#include <string>
#include <vector>

#include "base/random.h"
#include "base/system_util.h"
#include "base/thread2.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/flags/flag.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#ifdef __APPLE__
#include "ipc/ipc_test_util.h"
#endif  // __APPLE__

namespace {

// NOTE(komatsu): The name should not end with "_test", otherwise our
// testing tool rut.py misunderstood that the file named
// kServerAddress is a binary to be tested.
static constexpr char kServerAddress[] = "test_echo_server";
#ifdef _WIN32
// On windows, multiple-connections failed.
static constexpr int kNumThreads = 1;
#else   // _WIN32
static constexpr int kNumThreads = 5;
#endif  // _WIN32
static constexpr int kNumRequests = 2000;

class EchoServer : public mozc::IPCServer {
 public:
  EchoServer(const std::string &path, int32_t num_connections,
             absl::Duration timeout)
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

  EchoServer con(kServerAddress, 10, absl::Milliseconds(1000));
#ifdef __APPLE__
  con.SetMachPortManager(&manager);
#endif  // __APPLE__
  con.LoopAndReturn();

  std::vector<mozc::Thread2> cons;
  for (int i = 0; i < kNumThreads; ++i) {
    cons.push_back(mozc::Thread2([
#ifdef __APPLE__
                                     &manager
#endif  // __APPLE__
    ] {
      absl::SleepFor(absl::Seconds(2));
      mozc::Random random;
      for (int i = 0; i < kNumRequests; ++i) {
        mozc::IPCClient con(kServerAddress, "");
#ifdef __APPLE__
        con.SetMachPortManager(&manager);
#endif  // __APPLE__
        ASSERT_TRUE(con.Connected());
        const int size = absl::Uniform(random, 1, 8000);
        const std::string input = absl::StrCat("test", random.ByteString(size));
        std::string output;
        ASSERT_TRUE(con.Call(input, &output, absl::Milliseconds(1000)));
        EXPECT_EQ(output.size(), input.size());
        EXPECT_EQ(output, input);
      }
    }));
  }

  for (mozc::Thread2 &con : cons) {
    con.Join();
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
  kill.Call(kill_cmd, &output, absl::Milliseconds(1000));

  con.Wait();
}
