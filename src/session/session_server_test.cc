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
#include "base/scheduler.h"
#include "languages/global_language_spec.h"
#include "languages/japanese/lang_dep_spec.h"
#include "session/session_server.h"
#include "testing/base/public/gunit.h"

namespace {
class JobRecorder : public mozc::Scheduler::SchedulerInterface {
 public:
  void RemoveAllJobs() {}
  bool RemoveJob(const string &name) { return true; }
  bool AddJob(const mozc::Scheduler::JobSetting &job_setting) {
    job_settings_.push_back(job_setting);
    return true;
  }
  const vector<mozc::Scheduler::JobSetting> &job_settings() const {
    return job_settings_;
  }

 private:
  vector<mozc::Scheduler::JobSetting> job_settings_;
};
}  // namespace

class SessionServerTest : public testing::Test {
 protected:
  SessionServerTest() {}
  virtual ~SessionServerTest() {}

  virtual void SetUp() {
    mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(
        &language_dependency_spec_japanese_);
  }

  virtual void TearDown() {
    mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(NULL);
  }

 private:
  // We need to set a LangDepSpecJapanese to GlobalLanguageSpec on start up for
  // testing, and the actual instance does not have to be LangDepSpecJapanese.
  mozc::japanese::LangDepSpecJapanese language_dependency_spec_japanese_;
};

TEST_F(SessionServerTest, SetSchedulerJobTest) {
  scoped_ptr<JobRecorder> job_recorder(new JobRecorder);
  mozc::Scheduler::SetSchedulerHandler(job_recorder.get());
  scoped_ptr<mozc::SessionServer> session_server(new mozc::SessionServer);
  const vector<mozc::Scheduler::JobSetting> &job_settings =
      job_recorder->job_settings();
  EXPECT_GE(job_settings.size(), 1);
  EXPECT_EQ("UsageStatsTimer", job_settings[0].name());
  mozc::Scheduler::SetSchedulerHandler(NULL);
}
