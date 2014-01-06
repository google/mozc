// Copyright 2010-2014, Google Inc.
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

#ifndef OS_WIN
#include <unistd.h>
#include <cstdlib>
#endif  // OS_WIN

#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_mutex.h"
#include "base/system_util.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {
static const char kName[] = "process_mutex_test";

class ProcessMutexTest : public testing::Test {
 protected:
  virtual void SetUp() {
    original_user_profile_dir_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }

  virtual void TearDown() {
    ProcessMutex mutex(kName);
    if (FileUtil::FileExists(mutex.lock_filename())) {
      LOG(FATAL) << "Lock file unexpectedly remains: " << mutex.lock_filename();
    }

    SystemUtil::SetUserProfileDirectory(original_user_profile_dir_);
  }

 private:
  string original_user_profile_dir_;
};

#ifndef OS_WIN
TEST_F(ProcessMutexTest, ForkProcessMutexTest) {
  const pid_t pid = ::fork();
  if (pid == 0) {  // child process
    ProcessMutex m(kName);
    EXPECT_TRUE(m.Lock());
    Util::Sleep(3000);
    EXPECT_TRUE(m.UnLock());
    ::exit(0);
  } else if (pid > 0) {  // parent process
    ProcessMutex m(kName);
    Util::Sleep(1000);

    // kName should be locked by child
    EXPECT_FALSE(m.Lock());

    Util::Sleep(5000);

    // child process already finished, so now we can lock
    EXPECT_TRUE(m.Lock());
    EXPECT_TRUE(m.UnLock());
  } else {
    LOG(FATAL) << "fork() failed";
  }
}
#endif

TEST_F(ProcessMutexTest, BasicTest) {
  ProcessMutex m1(kName);
  EXPECT_TRUE(m1.Lock());
  EXPECT_TRUE(m1.locked());

  ProcessMutex m2(kName);
  EXPECT_FALSE(m2.Lock());
  EXPECT_FALSE(m2.locked());

  ProcessMutex m3(kName);
  EXPECT_FALSE(m3.Lock());
  EXPECT_FALSE(m3.locked());

  EXPECT_TRUE(m1.UnLock());
  EXPECT_FALSE(m1.locked());

  EXPECT_TRUE(m2.Lock());
  EXPECT_TRUE(m2.locked());

  EXPECT_FALSE(m3.Lock());
  EXPECT_FALSE(m3.locked());
}

TEST_F(ProcessMutexTest, RecursiveLockTest) {
  ProcessMutex mutex(kName);
  EXPECT_TRUE(mutex.Lock());
  EXPECT_TRUE(mutex.locked());
  EXPECT_FALSE(mutex.Lock()) << "Recursive lock should fail.";
  EXPECT_TRUE(mutex.locked());
  EXPECT_TRUE(mutex.UnLock());
  EXPECT_FALSE(mutex.locked());
}

}  // namespace
}  // namespace mozc
