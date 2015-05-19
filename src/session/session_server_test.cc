// Copyright 2010-2012, Google Inc.
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

#include "session/session_server.h"

#include <string>
#include <vector>

#include "base/base.h"
#include "base/scheduler.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {
class JobRecorder : public Scheduler::SchedulerInterface {
 public:
  void RemoveAllJobs() {}
  bool RemoveJob(const string &name) { return true; }
  bool AddJob(const Scheduler::JobSetting &job_setting) {
    job_settings_.push_back(job_setting);
    return true;
  }
  const vector<Scheduler::JobSetting> &job_settings() const {
    return job_settings_;
  }

 private:
  vector<Scheduler::JobSetting> job_settings_;
};

bool FindJobByName(const vector<Scheduler::JobSetting> &job_settings,
                   const string &job_name) {
  for (size_t i = 0; i < job_settings.size(); ++i) {
    if (job_settings[i].name() == job_name) {
      return true;
    }
  }
  return false;
}
}  // namespace
class SessionServerTest : public testing::Test {
 protected:
  void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(SessionServerTest, SetSchedulerJobTest) {
  scoped_ptr<JobRecorder> job_recorder(new JobRecorder);
  Scheduler::SetSchedulerHandler(job_recorder.get());
  scoped_ptr<SessionServer> session_server(new SessionServer);
  const vector<Scheduler::JobSetting> &job_settings =
      job_recorder->job_settings();
  EXPECT_LE(2, job_settings.size());
  EXPECT_TRUE(FindJobByName(job_settings, "UsageStatsTimer"));
#ifdef ENABLE_CLOUD_SYNC
  EXPECT_LE(3, job_settings.size());
  EXPECT_TRUE(FindJobByName(job_settings, "CloudSync"));
#endif  // ENABLE_CLOUD_SYNC
  EXPECT_TRUE(FindJobByName(job_settings, "SaveCachedStats"));
  Scheduler::SetSchedulerHandler(NULL);
}
}  // namespace mozc
