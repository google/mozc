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

#include "win32/base/migration_util.h"

#include <windows.h>

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlstr.h>

#include "base/const.h"
#include "base/process.h"
#include "base/scoped_handle.h"
#include "base/util.h"
#include "win32/base/imm_util.h"
#include "win32/base/imm_registrar.h"
#include "win32/base/immdev.h"
#include "win32/base/keyboard_layout_id.h"

namespace mozc {
namespace win32 {
using ATL::CStringA;

namespace {
bool SpawnBroker(const string &arg) {
  // To workaround a bug around WoW version of LoadKeyboardLayout, 64-bit
  // version of mozc_broker should be launched on Windows x64.
  // See b/2958563 for details.
  const char *broker_name =
      (Util::IsWindowsX64() ? kMozcBroker64 : kMozcBroker32);

  return Process::SpawnMozcProcess(broker_name, arg);
}
}  // anonymous namespace

bool MigrationUtil::IsFullIMEAvailable() {
  return ImmRegistrar::GetKLIDForIME().has_id();
}

bool MigrationUtil::RestorePreload() {
  const KeyboardLayoutID &mozc_klid = ImmRegistrar::GetKLIDForIME();
  if (!mozc_klid.has_id()) {
    return false;
  }
  return SUCCEEDED(ImmRegistrar::RestorePreload(mozc_klid));
}

bool MigrationUtil::LaunchBrokerForSetDefault(bool do_not_ask_me_again) {
  if (!MigrationUtil::IsFullIMEAvailable()) {
    LOG(ERROR) << "Full IME is not available";
    return false;
  }

  string arg = "--mode=set_default";
  if (do_not_ask_me_again) {
    arg += " --set_default_do_not_ask_again=true";
  }

  return SpawnBroker(arg);
}
}  // namespace win32
}  // namespace mozc
