// Copyright 2010-2011, Google Inc.
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

#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "ipc/ipc.h"
#include "ipc/ipc_test_util.h"
#include "languages/global_language_spec.h"
#include "languages/japanese/lang_dep_spec.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "base/thread.h"

DECLARE_string(test_tmpdir);

namespace {

// NOTE(komatsu): The name should not end with "_test", otherwise our
// testing tool rut.py misunderstood that the file named
// kServerAddress is a binary to be tested.
static const char kServerAddress[] = "test_echo_server";
#ifdef OS_WINDOWS
// On windows, multiple-connections failed.
static const int kNumThreads = 1;
#else
static const int kNumThreads = 5;
#endif
static const int kNumRequests = 2000;

int rand_range(int x) {
  return static_cast<int>(1.0 * rand() / RAND_MAX * x);
}

string GenRandomString(size_t size) {
  string result;
  while (result.length() < size) {
    result += static_cast<char>(rand_range(0xff));
  }
  return result;
}

class MultiConnections: public mozc::Thread {
 public:
#ifdef OS_MACOSX
  MultiConnections()
      : mach_port_manager_(NULL) { }

  void SetMachPortManager(mozc::MachPortManagerInterface *manager) {
    mach_port_manager_ = manager;
  }
#endif

  void Run() {
    char buf[8192];
    mozc::Util::Sleep(2000);
    for (int i = 0; i < kNumRequests; ++i) {
      mozc::IPCClient con(kServerAddress, "");
#ifdef OS_MACOSX
      con.SetMachPortManager(mach_port_manager_);
#endif
      ASSERT_TRUE(con.Connected());
      const int size = max(rand_range(8000), 1);
      string input = "test";
      input += GenRandomString(size);
      size_t length = sizeof(buf);
      ASSERT_TRUE(con.Call(input.data(), input.size(), buf, &length, 1000));
      string output(buf, length);
      EXPECT_EQ(input.size(), output.size());
      EXPECT_EQ(input, output);
    }
  }

#ifdef OS_MACOSX
 private:
  mozc::MachPortManagerInterface *mach_port_manager_;
#endif
};

class EchoServer: public mozc::IPCServer {
 public:
  EchoServer(const string &path,
             int32 num_connections,
             int32 timeout) :
      IPCServer(path, num_connections, timeout) {}
  virtual bool Process(const char *input_buffer,
                       size_t input_length,
                       char *output_buffer,
                       size_t *output_length) {
    if (::memcmp("kill", input_buffer, 4) == 0) {
      *output_length = 0;
      return false;
    }
    ::memcpy(output_buffer, input_buffer, input_length);
    *output_length = input_length;
    return true;
  }
};
}

TEST(IPCTest, IPCTest) {
  mozc::Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  mozc::japanese::LangDepSpecJapanese lang_spec;
  mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(&lang_spec);
#ifdef OS_MACOSX
  mozc::TestMachPortManager manager;
#endif

  EchoServer con(kServerAddress, 10, 1000);
#ifdef OS_MACOSX
  con.SetMachPortManager(&manager);
#endif
  con.LoopAndReturn();

  vector<MultiConnections> cons(kNumThreads);
  for (size_t i = 0; i < cons.size(); ++i) {
#ifdef OS_MACOSX
    cons[i].SetMachPortManager(&manager);
#endif
    cons[i].SetJoinable(true);
    cons[i].Start();
  }
  for (size_t i = 0; i < cons.size(); ++i) {
    cons[i].Join();
  }

  mozc::IPCClient kill(kServerAddress, "");
  const char kill_cmd[32] = "kill";
  char output[32];
  size_t output_size = sizeof(output);
#ifdef OS_MACOSX
  kill.SetMachPortManager(&manager);
#endif
  // We don't care the return value of this Call() because the return
  // value for server finish can change based on the platform
  // implementations.
  // TODO(mukai, team): determine the spec of return value for that
  // case and add EXPECT_(TRUE|FALSE) here.
  kill.Call(kill_cmd, strlen(kill_cmd), output, &output_size, 1000);

  con.Wait();
}
