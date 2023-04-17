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

#include "win32/base/migration_util.h"

#include <windows.h>

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlstr.h>

#include <string>
#include <vector>

#include "base/const.h"
#include "base/logging.h"
#include "base/process.h"
#include "win32/base/tsf_profile.h"
#include "win32/base/uninstall_helper.h"

namespace mozc {
namespace win32 {
namespace {

using ATL::CStringA;

bool SpawnBroker(const std::string &arg) {
  return Process::SpawnMozcProcess(kMozcBroker, arg);
}

}  // namespace

bool MigrationUtil::IsFullTIPAvailable() {
  const LANGID kLANGJaJP = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
  std::vector<LayoutProfileInfo> profile_list;
  if (!UninstallHelper::GetInstalledProfilesByLanguage(kLANGJaJP,
                                                       &profile_list)) {
    return false;
  }

  for (size_t i = 0; i < profile_list.size(); ++i) {
    const LayoutProfileInfo &profile = profile_list[i];
    if (::IsEqualCLSID(TsfProfile::GetTextServiceGuid(), profile.clsid) &&
        ::IsEqualGUID(TsfProfile::GetProfileGuid(), profile.profile_guid)) {
      return true;
    }
  }
  return false;
}

bool MigrationUtil::LaunchBrokerForSetDefault(bool do_not_ask_me_again) {
  if (!MigrationUtil::IsFullTIPAvailable()) {
    LOG(ERROR) << "Full TIP is not available";
    return false;
  }

  std::string arg = "--mode=set_default";
  if (do_not_ask_me_again) {
    arg += " --set_default_do_not_ask_again=true";
  }

  return SpawnBroker(arg);
}
}  // namespace win32
}  // namespace mozc
