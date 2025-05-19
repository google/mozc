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

#include <memory>
#include <string>
#include <vector>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "base/port.h"
#include "base/process_mutex.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/version.h"
#include "ipc/ipc.h"
#include "ipc/ipc.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "testing/test_peer.h"

#if defined(__ANDROID__) || defined(__wasm__)
#error "This platform is not supported."
#endif  // __ANDROID__ || __wasm__

namespace mozc {

class IPCPathManagerTestPeer : public testing::TestPeer<IPCPathManager> {
 public:
  explicit IPCPathManagerTestPeer(IPCPathManager &manager)
      : testing::TestPeer<IPCPathManager>(manager) {}

  PEER_METHOD(ShouldReload);
  PEER_VARIABLE(path_mutex_);
  PEER_VARIABLE(ipc_path_info_);
};

class IPCPathManagerTest : public testing::TestWithTempUserProfile {};

TEST_F(IPCPathManagerTest, IPCPathManagerTest) {
  std::string path_created;
  Thread([&path_created] {
    IPCPathManager *manager = IPCPathManager::GetIPCPathManager("test");
    EXPECT_TRUE(manager->CreateNewPathName());
    EXPECT_TRUE(manager->SavePathName());
    EXPECT_TRUE(manager->GetPathName(&path_created));
    EXPECT_GT(manager->GetServerProtocolVersion(), 0);
    EXPECT_FALSE(manager->GetServerProductVersion().empty());
    EXPECT_GT(manager->GetServerProcessId(), 0);
  }).Join();

  IPCPathManager *manager = IPCPathManager::GetIPCPathManager("test");
  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());
  std::string path;
  EXPECT_TRUE(manager->GetPathName(&path));
  EXPECT_GT(manager->GetServerProtocolVersion(), 0);
  EXPECT_FALSE(manager->GetServerProductVersion().empty());
  EXPECT_GT(manager->GetServerProcessId(), 0);
  EXPECT_EQ(path, path_created);
  if constexpr (TargetIsLinux()) {
    // On Linux, |path| should be abstract (see man unix(7) for details.)
    ASSERT_FALSE(path.empty());
    EXPECT_EQ(path[0], '\0');
  }
}

// Test the thread-safeness of GetPathName() and
// GetIPCPathManager
TEST_F(IPCPathManagerTest, IPCPathManagerBatchTest) {
  std::vector<Thread> threads;
  for (int i = 0; i < 64; ++i) {
    threads.push_back(Thread([] {
      for (int i = 0; i < 100; ++i) {
        IPCPathManager *manager = IPCPathManager::GetIPCPathManager("test2");
        std::string path;
        EXPECT_TRUE(manager->CreateNewPathName());
        EXPECT_TRUE(manager->GetPathName(&path));
      }
    }));
  }
  for (auto &thread : threads) {
    thread.Join();
  }
}

TEST_F(IPCPathManagerTest, ReloadTest) {
  // We have only mock implementations for Windows, so no test should be run.
#ifndef _WIN32
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager("reload_test");
  IPCPathManagerTestPeer manager_peer(*manager);

  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());

  // Just after the save, there are no need to reload.
  EXPECT_FALSE(manager_peer.ShouldReload());

  // Modify the saved file explicitly.
  EXPECT_TRUE(manager_peer.path_mutex_()->UnLock());
  absl::SleepFor(absl::Seconds(1));
  std::string filename = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), ".reload_test.ipc");
  ASSERT_OK(FileUtil::SetContents(filename, "foobar"));
  EXPECT_TRUE(manager_peer.ShouldReload());
#endif  // _WIN32
}

TEST_F(IPCPathManagerTest, PathNameTest) {
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager("path_name_test");
  IPCPathManagerTestPeer manager_peer(*manager);

  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());
  const ipc::IPCPathInfo original_path = manager_peer.ipc_path_info_();
  EXPECT_EQ(original_path.protocol_version(), IPC_PROTOCOL_VERSION);
  // Checks that versions are same.
  EXPECT_EQ(original_path.product_version(), Version::GetMozcVersion());
  EXPECT_TRUE(original_path.has_key());
  EXPECT_TRUE(original_path.has_process_id());
  EXPECT_TRUE(original_path.has_thread_id());

  manager_peer.ipc_path_info_().Clear();
  EXPECT_TRUE(manager->LoadPathName());

  const ipc::IPCPathInfo loaded_path = manager_peer.ipc_path_info_();
  EXPECT_EQ(loaded_path.protocol_version(), original_path.protocol_version());
  EXPECT_EQ(loaded_path.product_version(), original_path.product_version());
  EXPECT_EQ(loaded_path.key(), original_path.key());
  EXPECT_EQ(loaded_path.process_id(), original_path.process_id());
  EXPECT_EQ(loaded_path.thread_id(), original_path.thread_id());
}
}  // namespace mozc
