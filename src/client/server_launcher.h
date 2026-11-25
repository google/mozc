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

#ifndef MOZC_CLIENT_SERVER_LAUNCHER_H_
#define MOZC_CLIENT_SERVER_LAUNCHER_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "client/client_interface.h"

namespace mozc {
namespace client {

class ServerLauncherInterface {
 public:
  enum ServerErrorType {
    SERVER_TIMEOUT,
    SERVER_BROKEN_MESSAGE,
    SERVER_VERSION_MISMATCH,
    SERVER_SHUTDOWN,
    SERVER_FATAL,
  };

  virtual ~ServerLauncherInterface() = default;

  virtual bool StartServer(ClientInterface* client) = 0;

  // Terminates the server.
  // You should not call this method unless a protocol version mismatch happens.
  virtual bool ForceTerminateServer(absl::string_view name) = 0;

  // Waits until the server terminates.
  virtual bool WaitServer(uint32_t pid) = 0;

  // Called when a fatal error occurs.
  virtual void OnFatal(ServerErrorType type) = 0;

  // Returns the full path of the server program.
  // This is used for making an IPC connection.
  virtual std::string server_program() const = 0;

  // Sets the flag of error dialog suppression.
  virtual void set_suppress_error_dialog(bool suppress) = 0;
};

// Default ServerLauncher implementation.
// This class uses fork & exec (Linux/Mac) and CreateProcess() (Windows) to
// launch the server process.
class ServerLauncher : public ServerLauncherInterface {
 public:
  ServerLauncher();
  ~ServerLauncher() override;

  bool StartServer(ClientInterface* client) override;

  bool ForceTerminateServer(absl::string_view name) override;

  bool WaitServer(uint32_t pid) override;

  void OnFatal(ServerLauncherInterface::ServerErrorType type) override;

  std::string server_program() const override;

  void set_suppress_error_dialog(bool suppress) override {
    suppress_error_dialog_ = suppress;
  }

 private:
  bool suppress_error_dialog_;
};

}  // namespace client
}  // namespace mozc

#endif  // MOZC_CLIENT_SERVER_LAUNCHER_H_
