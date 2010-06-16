// Copyright 2010, Google Inc.
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

#define Uses_SCIM_FRONTEND_MODULE

#include <scim.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>

#include "base/base.h"  // for CHECK().
#include "base/logging.h"
#include "base/run_level.h"
#include "unix/scim/imengine_factory.h"
#include "unix/scim/mozc_connection.h"

namespace {

// The maximum number of IMEngines we can create.
const unsigned int kNumberOfIMEngines = 1;

// A pointer to the SCIM configuration.
const scim::ConfigPointer *scim_config = NULL;

void EnableDebug() {
  mozc::Logging::InitLogStream("scim_mozc");
  mozc::Logging::SetVerboseLevel(1);
}

void IgnoreSigChild() {
  // Don't wait() child process termination.
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  ::sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  ::sigaction(SIGCHLD, &sa, NULL);
  // TODO(yusukes): installing signal handler inside IMEngine might (badly)
  //                affect other SCIM modules' behavior. I'm not sure but it
  //                might be better to call ::posix_spawnp() in helper program?
}

bool IsRunLevelDeny() {
  return (mozc::RunLevel::GetRunLevel(mozc::RunLevel::CLIENT) ==
          mozc::RunLevel::DENY);
}

}  // namespace

extern "C" {
  // This function is called from SCIM framework.
  void mozc_LTX_scim_module_init() {
    if (IsRunLevelDeny()) {
      return;
    }

    // Logging is disabled by default.
    // EnableDebug();

    VLOG(1) << "mozc_LTX_scim_module_init. "
            << "my pid=" << ::getpid()
            << ", parent pid=" << ::getppid() << ".";
    IgnoreSigChild();
  }

  // This function is called from SCIM framework when users log out from their
  // workstation, or when users change SCIM configurations.
  void mozc_LTX_scim_module_exit() {
    if (IsRunLevelDeny()) {
      return;
    }
    VLOG(1) << "mozc_LTX_scim_module_exit";
    scim_config = NULL;
  }

  // This function is called from SCIM framework.
  // See /usr/include/scim-1.0/scim_imengine_module.h for details.
  unsigned int mozc_LTX_scim_imengine_module_init(
      const scim::ConfigPointer &config) {
    if (IsRunLevelDeny()) {
      return 0;  // remove mozc from the SCIM's IMEngine list.
    }
    VLOG(1) << "mozc_LTX_scim_imengine_module_init";
    scim_config = &config;
    return kNumberOfIMEngines;
  }

  // This function is called from SCIM framework.
  // See /usr/include/scim-1.0/scim_imengine_module.h for details.
  scim::IMEngineFactoryPointer mozc_LTX_scim_imengine_module_create_factory(
      unsigned int engine) {
    if (IsRunLevelDeny()) {
      return NULL;
    }
    VLOG(1) << "mozc_LTX_scim_imengine_module_create_factory";
    DCHECK_GT(kNumberOfIMEngines, engine) << "Invalid engine ID: " << engine;
    return new mozc_unix_scim::IMEngineFactory(scim_config);
  }
}  // extern "C"
