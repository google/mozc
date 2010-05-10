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
#include "base/process_mutex.h"
#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace {
static const char kName[] = "process_mutex_test";

#ifndef OS_WINDOWS
TEST(ProcessMutexTest, ForkProcessMutexTest) {
  mozc::Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  const pid_t pid = fork();
  if (pid == 0) {  // child process
    mozc::ProcessMutex m(kName);
    EXPECT_TRUE(m.Lock());
    mozc::Util::Sleep(3000);
    EXPECT_TRUE(m.UnLock());
    ::exit(0);
  } else if (pid > 0) {  // parent process
    mozc::ProcessMutex m(kName);
    mozc::Util::Sleep(1000);

    // kName should be locked by child
    EXPECT_FALSE(m.Lock());

    mozc::Util::Sleep(5000);

    // child process already finished, so now we can lock
    EXPECT_TRUE(m.Lock());
    EXPECT_TRUE(m.UnLock());
  } else {
    LOG(FATAL) << "fork() failed";
  }
}
#endif

TEST(ProcessMutexTest, ProcessMutexTest) {
  mozc::Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  mozc::ProcessMutex m1(kName);
  EXPECT_TRUE(m1.Lock());

  mozc::ProcessMutex m2(kName);
  EXPECT_FALSE(m2.Lock());

  mozc::ProcessMutex m3(kName);
  EXPECT_FALSE(m3.Lock());

  EXPECT_TRUE(m1.UnLock());

  EXPECT_TRUE(m2.Lock());
  EXPECT_FALSE(m3.Lock());
}
}  // namespace
