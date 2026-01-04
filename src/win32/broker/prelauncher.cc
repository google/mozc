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

#include "win32/broker/prelauncher.h"

#include <memory>

#include "base/win32/win_util.h"
#include "client/client.h"
#include "client/client_interface.h"
#include "renderer/renderer_client.h"

namespace mozc {
namespace win32 {
namespace {

constexpr int kErrorLevelSuccess = 0;
constexpr int kErrorLevelGeneralError = 1;

}  // namespace

int RunPrelaunchProcesses(int argc, char *argv[]) {
  bool is_service_process = false;
  if (!WinUtil::IsServiceProcess(&is_service_process)) {
    // Returns DENY conservatively.
    return kErrorLevelGeneralError;
  }
  if (is_service_process) {
    return kErrorLevelGeneralError;
  }

  {
    std::unique_ptr<client::ClientInterface> converter_client(
        client::ClientFactory::NewClient());
    converter_client->set_suppress_error_dialog(true);
    converter_client->EnsureConnection();
  }

  {
    std::unique_ptr<renderer::RendererClient> renderer_client =
        renderer::RendererClient::Create();
    renderer_client->set_suppress_error_dialog(true);
    renderer_client->Activate();
  }

  return kErrorLevelSuccess;
}

}  // namespace win32
}  // namespace mozc
