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

#include "base/base.h"
#include "base/file_stream.h"
#include "base/process_mutex.h"
#include "base/util.h"
#include "base/thread.h"
#include "ipc/ipc_path_manager.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {
const char kName[] = "process_mutex_test";

class CreateThread : public Thread {
 public:
  virtual void Run() {
    mozc::IPCPathManager *manager =
        mozc::IPCPathManager::GetIPCPathManager("test");
    EXPECT_TRUE(manager->CreateNewPathName());
    EXPECT_TRUE(manager->SavePathName());
    EXPECT_TRUE(manager->GetPathName(&path_));
    EXPECT_GT(manager->GetServerProtocolVersion(), 0);
    EXPECT_FALSE(manager->GetServerProductVersion().empty());
    EXPECT_GT(manager->GetServerProcessId(), 0);
  }

  const string &path() const {
    return path_;
  }

 private:
  string path_;
};

class BatchGetPathNameThread : public Thread {
 public:
  virtual void Run() {
    for (int i = 0; i < 100; ++i) {
      mozc::IPCPathManager *manager =
          mozc::IPCPathManager::GetIPCPathManager("test2");
      string path;
      EXPECT_TRUE(manager->CreateNewPathName());
      EXPECT_TRUE(manager->GetPathName(&path));
    }
  }
};
}  // anonymous namespace

TEST(IPCPathManagerTest, IPCPathManagerTest) {
  mozc::Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  CreateThread t;
  t.Start();
  Util::Sleep(1000);
  mozc::IPCPathManager *manager =
        mozc::IPCPathManager::GetIPCPathManager("test");
  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());
  string path;
  EXPECT_TRUE(manager->GetPathName(&path));
  EXPECT_GT(manager->GetServerProtocolVersion(), 0);
  EXPECT_FALSE(manager->GetServerProductVersion().empty());
  EXPECT_GT(manager->GetServerProcessId(), 0);
  EXPECT_EQ(t.path(), path);
}

// Test the thread-safeness of GetPathName() and
// GetIPCPathManager
TEST(IPCPathManagerTest, IPCPathManagerBatchTest) {
  vector<BatchGetPathNameThread> threads(8192);
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i].Start();
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i].Join();
  }
}

TEST(IPCPathManagerTest, ReloadTest) {
  // We have only mock implementations for Windows, so no test should be run.
#ifndef OS_WINDOWS
  mozc::IPCPathManager *manager =
      mozc::IPCPathManager::GetIPCPathManager("reload_test");

  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());

  // Just after the save, there are no need to reload.
  EXPECT_FALSE(manager->ShouldReload());

  // Modify the saved file explicitly.
  EXPECT_TRUE(manager->path_mutex_->UnLock());
  Util::Sleep(1000 /* msec */);
  string filename = Util::JoinPath(
      Util::GetUserProfileDirectory(), ".reload_test.ipc");
  OutputFileStream outf(filename.c_str());
  outf << "foobar";
  outf.close();

  EXPECT_TRUE(manager->ShouldReload());
#endif  // OS_WINDOWS
}
}  // mozc
