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

#include <sstream>
#include "base/logging.h"

#include "base/base.h"
#include "testing/base/public/gunit.h"

namespace mozc {
TEST(LoggingTest, CompileTest) {
  if (false) {
    LOG(INFO) << "";
    LOG(WARNING) << "";
    LOG(ERROR) << "";
    LOG(FATAL) << "";
    VLOG(0) << "";
    VLOG(1) << "";
    VLOG(2) << "";
    VLOG(3) << "";
  }
  if (false) {
    DLOG(INFO) << "";
    DLOG(WARNING) << "";
    DLOG(ERROR) << "";
    DLOG(FATAL) << "";
    DVLOG(0) << "";
    DVLOG(1) << "";
    DVLOG(2) << "";
    DVLOG(3) << "";
  }

  LOG_IF(INFO, false) << "";
  LOG_IF(WARNING, false) << "";
  LOG_IF(ERROR, false) << "";
  LOG_IF(FATAL, false) << "";

  DLOG_IF(INFO, false) << "";
  DLOG_IF(WARNING, false) << "";
  DLOG_IF(ERROR, false) << "";
  DLOG_IF(FATAL, false) << "";

  VLOG_IF(0, false) << "";
  VLOG_IF(1, false) << "";
  VLOG_IF(2, false) << "";
  VLOG_IF(3, false) << "";

  DVLOG_IF(0, false) << "";
  DVLOG_IF(1, false) << "";
  DVLOG_IF(2, false) << "";
  DVLOG_IF(3, false) << "";

  CHECK(true) << "";
  CHECK_EQ(true, true) << "";
  CHECK_NE(true, false) << "";
  CHECK_GE(2, 1) << "";
  CHECK_GE(1, 1) << "";
  CHECK_LE(1, 2) << "";
  CHECK_LE(1, 1) << "";
  CHECK_GT(2, 1) << "";
  CHECK_LT(1, 2) << "";

  DCHECK(true) << "";
  DCHECK_EQ(true, true) << "";
  DCHECK_NE(true, false) << "";
  DCHECK_GE(2, 1) << "";
  DCHECK_GE(1, 1) << "";
  DCHECK_LE(1, 2) << "";
  DCHECK_LE(1, 1) << "";
  DCHECK_GT(2, 1) << "";
  DCHECK_LT(1, 2) << "";
}

TEST(LoggingTest, SideEffectTest) {
  bool flag = false;

#ifdef NO_LOGGING
  // LOG_(INFO|WARNING|ERROR) are not executed on release mode
  flag = true;
  LOG_IF(INFO, flag = false) << "";
  EXPECT_TRUE(flag);

  flag = true;
  LOG_IF(WARNING, flag = false) << "";
  EXPECT_TRUE(flag);

  flag = true;
  LOG_IF(ERROR, flag = false) << "";
  EXPECT_TRUE(flag);
#else
  flag = true;
  LOG_IF(INFO, flag = false) << "";
  EXPECT_FALSE(flag);

  flag = true;
  LOG_IF(WARNING, flag = false) << "";
  EXPECT_FALSE(flag);

  flag = true;
  LOG_IF(ERROR, flag = false) << "";
  EXPECT_FALSE(flag);
#endif

  flag = true;
  LOG_IF(FATAL, flag = false) << "";
  EXPECT_FALSE(flag);

  flag = false;
  CHECK(flag = true) << "";
  EXPECT_TRUE(flag);

  flag = false;
  CHECK_EQ(true, flag = true) << "";
  EXPECT_TRUE(flag);

  flag = false;
  CHECK_NE(false, flag = true) << "";
  EXPECT_TRUE(flag);

  int i = 0;

  i = 10;
  CHECK_GE(20, i = 11) << "";
  EXPECT_EQ(i, 11);

  i = 10;
  CHECK_GT(20, i = 11) << "";
  EXPECT_EQ(i, 11);

  i = 10;
  CHECK_LE(1, i = 11) << "";
  EXPECT_EQ(i, 11);

  i = 10;
  CHECK_LT(1, i = 11) << "";
  EXPECT_EQ(i, 11);
}

namespace {
int g_counter = 0;
string DebugString() {
  ++g_counter;
  ostringstream os;
  os << g_counter << " test!";
  return os.str();
}
}  // namespace

TEST(LoggingTest, RightHandSideEvaluation) {
  g_counter = 0;
  LOG(INFO) << "test: " << DebugString();
  LOG(ERROR) << "test: " << DebugString();
  LOG(WARNING) << "test: " << DebugString();

#ifdef NO_LOGGING
  EXPECT_EQ(0, g_counter);
#else
  EXPECT_EQ(3, g_counter);
#endif

  g_counter = 0;
  LOG_IF(INFO, true) << "test: " << DebugString();
  LOG_IF(ERROR, true) << "test: " << DebugString();
  LOG_IF(WARNING, true) << "test: " << DebugString();

#ifdef NO_LOGGING
  EXPECT_EQ(0, g_counter);
#else
  EXPECT_EQ(3, g_counter);
#endif

  g_counter = 0;
  LOG_IF(INFO, false) << "test: " << DebugString();
  LOG_IF(ERROR, false) << "test: " << DebugString();
  LOG_IF(WARNING, false) << "test: " << DebugString();

  EXPECT_EQ(0, g_counter);
}
}  // namespace mozc
