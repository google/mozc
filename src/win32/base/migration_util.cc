// Copyright 2010-2020, Google Inc.
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

#include <strsafe.h>

#include <memory>

#include "base/const.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/scoped_handle.h"
#include "base/system_util.h"
#include "win32/base/imm_registrar.h"
#include "win32/base/imm_util.h"
#include "win32/base/immdev.h"
#include "win32/base/input_dll.h"
#include "win32/base/keyboard_layout_id.h"
#include "win32/base/tsf_profile.h"
#include "win32/base/uninstall_helper.h"

namespace mozc {
namespace win32 {
namespace {

using ATL::CStringA;
using std::unique_ptr;

bool SpawnBroker(const string &arg) {
  // To workaround a bug around WoW version of LoadKeyboardLayout, 64-bit
  // version of mozc_broker should be launched on Windows x64.
  // See b/2958563 for details.
  const char *broker_name =
      (SystemUtil::IsWindowsX64() ? kMozcBroker64 : kMozcBroker32);

  return Process::SpawnMozcProcess(broker_name, arg);
}

}  // namespace

bool MigrationUtil::IsFullIMEAvailable() {
  return ImmRegistrar::GetKLIDForIME().has_id();
}

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

bool MigrationUtil::RestorePreload() {
  const KeyboardLayoutID &mozc_klid = ImmRegistrar::GetKLIDForIME();
  if (!mozc_klid.has_id()) {
    return false;
  }
  return SUCCEEDED(ImmRegistrar::RestorePreload(mozc_klid));
}

bool MigrationUtil::LaunchBrokerForSetDefault(bool do_not_ask_me_again) {
  if (SystemUtil::IsWindows8OrLater()) {
    if (!MigrationUtil::IsFullTIPAvailable()) {
      LOG(ERROR) << "Full TIP is not available";
      return false;
    }
  } else {
    if (!MigrationUtil::IsFullIMEAvailable()) {
      LOG(ERROR) << "Full IME is not available";
      return false;
    }
  }

  string arg = "--mode=set_default";
  if (do_not_ask_me_again) {
    arg += " --set_default_do_not_ask_again=true";
  }

  return SpawnBroker(arg);
}

bool MigrationUtil::DisableLegacyMozcForCurrentUserOnWin8() {
  if (!SystemUtil::IsWindows8OrLater()) {
    return false;
  }

  const KeyboardLayoutID imm32_mozc_klid(ImmRegistrar::GetKLIDForIME());
  if (!imm32_mozc_klid.has_id()) {
    // This means Mozc is not installed.
    return true;
  }

  const UINT num_element =
      ::EnumEnabledLayoutOrTip(nullptr, nullptr, nullptr, nullptr, 0);

  unique_ptr<LAYOUTORTIPPROFILE[]> buffer(new LAYOUTORTIPPROFILE[num_element]);

  const UINT num_copied = ::EnumEnabledLayoutOrTip(nullptr, nullptr, nullptr,
                                                   buffer.get(), num_element);

  // Look up IMM32 Mozc from |buffer|.
  for (size_t i = 0; i < num_copied; ++i) {
    const LAYOUTORTIPPROFILE &profile = buffer[i];
    if (profile.dwProfileType != LOTP_KEYBOARDLAYOUT) {
      // This profile is not a keyboard layout. Never be a IMM32 Mozc.
      continue;
    }
    if (profile.clsid != GUID_NULL) {
      // This profile is TIP. Never be a IMM32 Mozc.
      continue;
    }
    if (profile.guidProfile != GUID_NULL) {
      // This profile is TIP. Never be a IMM32 Mozc.
      continue;
    }
    if ((profile.dwFlags & LOT_DISABLED) == LOT_DISABLED) {
      // This profile is disabled.
      continue;
    }

    const std::wstring id(profile.szId);
    // A valid |profile.szId| should consists of language ID (LANGID) and
    // keyboard layout ID (KILD) as follows.
    //  <LangID 1>:<KLID 1>
    //       "0411:E0200411"
    // Check if |id.size()| is expected.
    if (id.size() != 13) {
      continue;
    }

    // Extract KLID.  It should be 8-letter hexadecimal code begins at 6th
    // character in the |id|, that is, |id.substr(5, 8)|.
    const KeyboardLayoutID klid(id.substr(5, 8));
    if (klid.id() != imm32_mozc_klid.id()) {
      continue;
    }

    // IMM32 Mozc is found.

    // If IMM32 Mozc is the default IME, we need to set TSF Mozc to be the
    // default IME before disabling the legacy IME.
    if ((profile.dwFlags & LOT_DEFAULT) == LOT_DEFAULT) {
      wchar_t clsid[64] = {};
      if (!::StringFromGUID2(TsfProfile::GetTextServiceGuid(), clsid,
                             arraysize(clsid))) {
        return false;
      }
      wchar_t profile_id[64] = {};
      if (!::StringFromGUID2(TsfProfile::GetProfileGuid(), profile_id,
                             arraysize(profile_id))) {
        return false;
      }

      const std::wstring &profile =
          std::wstring(L"0x0411:") + clsid + profile_id;
      if (!::SetDefaultLayoutOrTip(profile.c_str(), 0)) {
        DLOG(ERROR) << "SetDefaultLayoutOrTip failed";
        return false;
      }
    }

    // Disable IMM32 Mozc.
    {
      wchar_t profile_str[32] = {};
      if (FAILED(::StringCchPrintf(profile_str, arraysize(profile_str),
                                   L"%04x:%s", profile.langid,
                                   klid.ToString().c_str()))) {
        return false;
      }
      if (!::InstallLayoutOrTip(profile_str, ILOT_DISABLED)) {
        DLOG(ERROR) << "InstallLayoutOrTip failed";
        return false;
      }
    }
    return true;
  }

  return true;
}

}  // namespace win32
}  // namespace mozc
