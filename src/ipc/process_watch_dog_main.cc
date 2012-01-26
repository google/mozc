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
#include <string>
#include "base/base.h"
#include "base/util.h"
#include "ipc/process_watch_dog.h"

DEFINE_int32(timeout, -1, "set timeout");

namespace mozc {
class TestProcessWatchDog : public ProcessWatchDog {
 public:
  void Signaled(ProcessWatchDog::SignalType type) {
    cout << "Signaled: " << static_cast<int>(type) << endl;
  }
};
}

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::TestProcessWatchDog dog;

  string line;
  vector<string> fields;

  while (getline(cin, line)) {
    fields.clear();
    mozc::Util::SplitStringUsing(line, "\t ", &fields);
    if (line == "exit") {
      break;
    }
    if (fields.size() < 2) {
      LOG(ERROR) << "format error: " << line;
      continue;
    }

    const int32 process_id = atoi32(fields[0].c_str());
    const int32 thread_id = atoi32(fields[1].c_str());

    if (!dog.SetID(static_cast<mozc::ProcessWatchDog::ProcessID>(process_id),
                   static_cast<mozc::ProcessWatchDog::ThreadID>(thread_id),
                   FLAGS_timeout)) {
      cout << "Error" << endl;
    } else {
      cout << "OK" << endl;
    }
  }

  LOG(INFO) << "Done";

  return 0;
}
