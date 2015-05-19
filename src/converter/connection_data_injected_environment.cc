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

#include "converter/connection_data_injected_environment.h"

#include "base/mmap.h"
#include "base/port.h"
#include "base/singleton.h"
#include "converter/connector_interface.h"

DEFINE_string(mozc_connection_data_file, "",
              "The path to connection data file.");

namespace mozc {
namespace {
class ConnectionDataInjector {
 private:
  ConnectionDataInjector() {
    // TODO(hidehiko): We should get rid of SetConnectionData to inject
    //   mmapped files. We need redesign about the architecture of Connector
    //   (or other global like data structures), but to keep the current test
    //   pass.
    CHECK(mmapped_file_.Open(FLAGS_mozc_connection_data_file.c_str()));
    ConnectorFactory::SetConnectionData(
        mmapped_file_.begin(), mmapped_file_.GetFileSize());
  }

  friend class Singleton<ConnectionDataInjector>;

  Mmap<char> mmapped_file_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionDataInjector);
};
}  // namespace

void ConnectionDataInjectedEnvironment::SetUp() {
  // Initialize only once.
  Singleton<ConnectionDataInjector>::get();
}
}  // namespace mozc
