// Copyright 2010-2020, Google Inc.
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

#include <string>

#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "ipc/ipc_path_manager.h"

DEFINE_bool(client, false, "client mode");
DEFINE_bool(server, false, "server mode");
DEFINE_string(name, "test", "ipc name");

// command line tool to check the behavior of IPCPathManager
int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  mozc::IPCPathManager *manager =
      mozc::IPCPathManager::GetIPCPathManager(FLAGS_name);
  CHECK(manager);

  string path;

  if (FLAGS_client) {
    CHECK(manager->GetPathName(&path));
    LOG(INFO) << "PathName: " << path;
    return 0;
  }

  if (FLAGS_server) {
    CHECK(manager->CreateNewPathName());
    CHECK(manager->SavePathName());
    CHECK(manager->GetPathName(&path));
    LOG(INFO) << "PathName: " << path;
    mozc::Util::Sleep(30000);  // sleep 30sec
    return 0;
  }

  LOG(INFO) << "use --client or --server";
  return 0;
}
