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

#include "ipc/ipc_path_manager.h"

#if defined(OS_ANDROID) || defined(OS_WASM)
#error "This platform is not supported."
#endif  // OS_ANDROID || OS_WASM

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/port.h"
#include "base/process_mutex.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/util.h"
#include "base/version.h"
#include "ipc/ipc.h"
#include "ipc/ipc.pb.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "absl/flags/flag.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace mozc {
namespace {

class CreateThread : public Thread {
 public:
  void Run() override {
    IPCPathManager *manager = IPCPathManager::GetIPCPathManager("test");
    EXPECT_TRUE(manager->CreateNewPathName());
    EXPECT_TRUE(manager->SavePathName());
    EXPECT_TRUE(manager->GetPathName(&path_));
    EXPECT_GT(manager->GetServerProtocolVersion(), 0);
    EXPECT_FALSE(manager->GetServerProductVersion().empty());
    EXPECT_GT(manager->GetServerProcessId(), 0);
  }

  const std::string &path() const { return path_; }

 private:
  std::string path_;
};

class BatchGetPathNameThread : public Thread {
 public:
  void Run() override {
    for (int i = 0; i < 100; ++i) {
      IPCPathManager *manager = IPCPathManager::GetIPCPathManager("test2");
      std::string path;
      EXPECT_TRUE(manager->CreateNewPathName());
      EXPECT_TRUE(manager->GetPathName(&path));
    }
  }
};
}  // namespace

class IPCPathManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_test_tmpdir));
  }
};

TEST_F(IPCPathManagerTest, IPCPathManagerTest) {
  CreateThread t;
  t.Start("IPCPathManagerTest");
  t.Join();
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager("test");
  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());
  std::string path;
  EXPECT_TRUE(manager->GetPathName(&path));
  EXPECT_GT(manager->GetServerProtocolVersion(), 0);
  EXPECT_FALSE(manager->GetServerProductVersion().empty());
  EXPECT_GT(manager->GetServerProcessId(), 0);
  EXPECT_EQ(t.path(), path);
#ifdef OS_LINUX
  // On Linux, |path| should be abstract (see man unix(7) for details.)
  ASSERT_FALSE(path.empty());
  EXPECT_EQ('\0', path[0]);
#endif  // OS_LINUX
}

// Test the thread-safeness of GetPathName() and
// GetIPCPathManager
TEST_F(IPCPathManagerTest, IPCPathManagerBatchTest) {
  // mozc::Thread is not designed as value-semantics.
  // So here we use pointers to maintain these instances.
  std::vector<BatchGetPathNameThread *> threads(64);
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i] = new BatchGetPathNameThread;
    threads[i]->Start("IPCPathManagerBatchTest");
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->Join();
    delete threads[i];
    threads[i] = nullptr;
  }
}

TEST_F(IPCPathManagerTest, ReloadTest) {
  // We have only mock implementations for Windows, so no test should be run.
#ifndef OS_WIN
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager("reload_test");

  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());

  // Just after the save, there are no need to reload.
  EXPECT_FALSE(manager->ShouldReload());

  // Modify the saved file explicitly.
  EXPECT_TRUE(manager->path_mutex_->UnLock());
  absl::SleepFor(absl::Seconds(1));
  std::string filename = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), ".reload_test.ipc");
  ASSERT_OK(FileUtil::SetContents(filename, "foobar"));
  EXPECT_TRUE(manager->ShouldReload());
#endif  // OS_WIN
}

TEST_F(IPCPathManagerTest, PathNameTest) {
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager("path_name_test");

  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());
  const ipc::IPCPathInfo original_path = *(manager->ipc_path_info_);
  EXPECT_EQ(IPC_PROTOCOL_VERSION, original_path.protocol_version());
  // Checks that versions are same.
  EXPECT_EQ(Version::GetMozcVersion(), original_path.product_version());
  EXPECT_TRUE(original_path.has_key());
  EXPECT_TRUE(original_path.has_process_id());
  EXPECT_TRUE(original_path.has_thread_id());

  manager->ipc_path_info_->Clear();
  EXPECT_TRUE(manager->LoadPathName());

  const ipc::IPCPathInfo loaded_path = *(manager->ipc_path_info_);
  EXPECT_EQ(original_path.protocol_version(), loaded_path.protocol_version());
  EXPECT_EQ(original_path.product_version(), loaded_path.product_version());
  EXPECT_EQ(original_path.key(), loaded_path.key());
  EXPECT_EQ(original_path.process_id(), loaded_path.process_id());
  EXPECT_EQ(original_path.thread_id(), loaded_path.thread_id());
}
}  // namespace mozc
