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

// Mozc renderer process for MacOS.

// TODO(b/110808149): Remove the ifdefs when we support cross-platform builds.
#ifdef __APPLE__
#include "renderer/init_mozc_renderer.h"
#include "renderer/mac/CandidateController.h"
#include "renderer/mac/mac_server.h"
#include "renderer/mac/mac_server_send_command.h"

int main(int argc, char* argv[]) {
  mozc::renderer::InitMozcRenderer(argv[0], &argc, &argv);

  mozc::renderer::mac::MacServer::Init();
  mozc::renderer::mac::MacServer server(argc, (const char**)argv);
  mozc::renderer::mac::CandidateController renderer;
  mozc::renderer::mac::MacServerSendCommand send_command;
  server.SetRendererInterface(&renderer);
  renderer.SetSendCommandInterface(&send_command);
  return server.StartServer();
}
#else  // __APPLE__

int main() { return 1; }

#endif  // __APPLE__
