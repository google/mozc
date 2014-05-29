// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_IPC_IPC_TEST_UTIL_H_
#define MOZC_IPC_IPC_TEST_UTIL_H_

#include "ipc/ipc.h"

namespace mozc {
#ifdef OS_MACOSX
// Mach port manager for testing: it allocates a mach port locally and
// shares it among client-server.
class TestMachPortManager : public mozc::MachPortManagerInterface {
 public:
  TestMachPortManager();
  ~TestMachPortManager();

  virtual bool GetMachPort(const string &name, mach_port_t *port);
  virtual bool IsServerRunning(const string &name) const;

 private:
  mach_port_t port_;
};
#endif


// An IPCClientFactory which holds an onmemory port instead of actual
// connections.  It is only available for Mac.  Otherwise it is same
// as a normal IPCClientFactory.
class IPCClientFactoryOnMemory : public IPCClientFactoryInterface {
 public:
  IPCClientFactoryOnMemory() { }

  virtual IPCClientInterface *NewClient(
      const string &name, const string &port_name);

  virtual IPCClientInterface *NewClient(const string &name);

#ifdef OS_MACOSX
  // Returns MachPortManager to share the mach port between client and server.
  MachPortManagerInterface *OnMemoryPortManager() {
    return &mach_manager_;
  }
#endif
 private:
#ifdef OS_MACOSX
  TestMachPortManager mach_manager_;
#endif
  DISALLOW_COPY_AND_ASSIGN(IPCClientFactoryOnMemory);
};
}  // namespace mozc
#endif  // MOZC_IPC_IPC_TEST_UTIL_H_
