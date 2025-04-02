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

// Utility functions for testing with IPC.

#include "ipc/ipc_test_util.h"

#include <memory>

#include "base/strings/zstring_view.h"
#include "ipc/ipc.h"

#ifdef __APPLE__
#include "absl/log/check.h"
#endif  // __APPLE__

namespace mozc {
#ifdef __APPLE__
TestMachPortManager::TestMachPortManager() {
  ipc_space_t self_task = mach_task_self();
  // Create a new mach port with receive right.
  CHECK_EQ(KERN_SUCCESS,
           mach_port_allocate(self_task, MACH_PORT_RIGHT_RECEIVE, &port_));
  // Add send right to the new mach port.
  CHECK_EQ(KERN_SUCCESS, mach_port_insert_right(self_task, port_, port_,
                                                MACH_MSG_TYPE_MAKE_SEND));
}

TestMachPortManager::~TestMachPortManager() {
  mach_port_destroy(mach_task_self(), port_);
}

bool TestMachPortManager::GetMachPort(const std::string &name,
                                      mach_port_t *port) {
  *port = port_;
  return true;
}

// Server is always running for test because both client and server is
// running in a same process.
bool TestMachPortManager::IsServerRunning(const std::string &name) const {
  return true;
}
#endif  // __APPLE__

std::unique_ptr<IPCClientInterface> IPCClientFactoryOnMemory::NewClient(
    zstring_view name, zstring_view path_name) {
  auto new_client = std::make_unique<IPCClient>(name, path_name);
#ifdef __APPLE__
  new_client->SetMachPortManager(&mach_manager_);
#endif  // __APPLE__
  return new_client;
}

std::unique_ptr<IPCClientInterface> IPCClientFactoryOnMemory::NewClient(
    zstring_view name) {
  auto new_client = std::make_unique<IPCClient>(name);
#ifdef __APPLE__
  new_client->SetMachPortManager(&mach_manager_);
#endif  // __APPLE__
  return new_client;
}
}  // namespace mozc
