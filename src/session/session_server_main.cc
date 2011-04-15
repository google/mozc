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

// Mocked Session Server runner used just for testing.
#include <stdio.h>

#include "session/session_server.h"
#include "session/commands.pb.h"

static const int kMaxBufSize = 1024;

namespace mozc {
void SendCommand(SessionServer *server,
                 const commands::Input &input,
                 commands::Output *output) {
  char buf[kMaxBufSize];
  size_t buf_len = kMaxBufSize;

  printf("input command:\n%s\n", input.Utf8DebugString().c_str());

  string input_str = input.SerializeAsString();
  server->Process(input_str.c_str(), input_str.size(),
                  buf, &buf_len);

  output->ParseFromArray(buf, buf_len);
  printf("output command:\n%s\n", output->Utf8DebugString().c_str());
}
}  // namespace mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::SessionServer server;
  mozc::commands::Input input;
  mozc::commands::Output output;

  // create session
  {
    input.set_type(mozc::commands::Input::CREATE_SESSION);
    mozc::SendCommand(&server, input, &output);
  }

  uint64 id = output.id();
  // send key
  {
    input.set_id(id);
    input.set_type(mozc::commands::Input::SEND_KEY);
    input.mutable_key()->set_special_key(
        mozc::commands::KeyEvent::SPACE);
    mozc::SendCommand(&server, input, &output);
  }

  return 0;
}
