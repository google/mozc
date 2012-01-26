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

#include <iostream>
#include <vector>
#include <string>
#include "base/base.h"
#include "base/mutex.h"
#include "base/timer.h"
#include "base/util.h"

DEFINE_int32(sleep_time, 0, "sleep time in signaled");

namespace mozc {
namespace {
class TestTimer : public Timer {
 public:
  virtual void Signaled() {
    LOG(INFO) << "Start signaled";
    LOG(INFO) << "Sleeping " << FLAGS_sleep_time << " msec";
    Util::Sleep(FLAGS_sleep_time);
    LOG(INFO) << "Finish signaled";
  }
};
}
}  // mozc

// This is a simple command line tool
int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::TestTimer test_timer;
  string line;
  vector<string> tokens;
  while (getline(cin, line)) {
    if (line == "stop") {
      test_timer.Stop();
      continue;
    }

    if (line == "exit") {
      break;
    }

    tokens.clear();
    mozc::Util::SplitStringUsing(line, "\t ", &tokens);
    const uint32 due_time = atoi32(tokens[0].c_str());
    const uint32 interval = atoi32(tokens[1].c_str());
    FLAGS_sleep_time = atoi32(tokens[2].c_str());
    LOG(INFO) << "Start Timer";
    test_timer.Start(due_time, interval);
  }
}
