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

#include "base/stats_config_util.h"

#ifdef OS_WINDOWS
#include <windows.h>
#include <Lmcons.h>
#include <shlobj.h>
#include <time.h>
#include <sddl.h>
#include <atlbase.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef OS_MACOSX
#include <fstream>
#include <string>

#include "base/mac_util.h"
#include "base/mutex.h"
#endif

#include "base/singleton.h"
#include "base/util.h"

namespace mozc {

namespace {
#ifdef OS_WINDOWS
const wchar_t kOmahaGUID[] = L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kOmahaUsageKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kOmahaUsageKeyForEveryone[] =
    L"Software\\Google\\Update\\ClientStateMedium\\"
    L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";

const wchar_t kSendStatsName[] = L"usagestats";

// Deletes "HKCU/../ClientState/../usagestats" if exists.
// See http://b/2451942 and http://b/2452672 for details.
void DeleteWrongOmahaUsagestatsValueUnderHKCU() {
  CRegKey key;
  // KEY_SET_VALUE is required to delete a value.
  // Note that KEY_WOW64_32KEY is not required because the following key is
  // shared by both 32-bit processes and 64-bit processes.
  const REGSAM sam_desired = KEY_SET_VALUE;
  const LONG result = key.Open(HKEY_CURRENT_USER, kOmahaUsageKey, sam_desired);
  if (ERROR_SUCCESS != result || !key) {
    // Couldn't open the key with KEY_SET_VALUE permission.  Nothing to do.
    return;
  }
  key.DeleteValue(kSendStatsName);
}

// Fix up a registry key under HLCU, which was used by the previous version of
// WinStatsConfigUtilImpl used it by mistake.  This function migrates only
// opt-out against usagestarts.
// Although this function is not thread-safe, we expect no harmful side-effect.
// See http://b/2451942 and http://b/2452672 for details.
// |*disabled_by_hkcu| will be set to |true| if the user disabled the usagestats
// by the registry key under HKUC.  The caller should behave as if the
// usagestats is disabled regardless of the settings under HKLM.
void FixupUsagestatsSettingsUnderHKCU(bool *disabled_by_hkcu) {
  bool dummy = false;
  bool &usagestats_disabled_by_hkcu =
      (disabled_by_hkcu != NULL ? *disabled_by_hkcu : dummy);
  usagestats_disabled_by_hkcu = false;

  {
    CRegKey hkcu_key;
    // KEY_WOW64_32KEY is not required because the following key is shared by
    // both 32-bit processes and 64-bit processes.
    const REGSAM sam_desired_for_hkcu = KEY_QUERY_VALUE;
    LONG result = hkcu_key.Open(HKEY_CURRENT_USER, kOmahaUsageKey,
                                sam_desired_for_hkcu);
    if (ERROR_SUCCESS != result || !hkcu_key) {
      // Couldn't open the key with.  Nothing to do.
      return;
    }

    DWORD value = 0;
    result = hkcu_key.QueryDWORDValue(kSendStatsName, value);
    switch (result) {
      case ERROR_SUCCESS:
        usagestats_disabled_by_hkcu = (value == 0);
        break;
      case ERROR_FILE_NOT_FOUND:
        // The value does not exist.
        usagestats_disabled_by_hkcu = false;
        break;
      default:
        // Otherwise, treat it as disabled conservatively.
        usagestats_disabled_by_hkcu = true;
        break;
    }
  }  // Close the key not to prevent the removal of the key.

  // Unless the user disabled the usagestats, we will ignore the settings
  // because other user in the same system may not want to participate the
  // usagestats.
  if (!usagestats_disabled_by_hkcu) {
    DeleteWrongOmahaUsagestatsValueUnderHKCU();
    return;
  }

  // The usagestas should be disabled as the current user expected.
  const DWORD usage_stats = 0;

  {
    CRegKey hklm_key;
    // Request some write access rights to update (or create) a value under
    // ClientStateMedium key, which can be updated not only by an administrator
    // but also by a normal user.
    // Note that KEY_WOW64_32KEY may be required unlike the counterpart of this
    // key under HKCL.
    const REGSAM sam_desired_for_hklm = KEY_SET_VALUE |
        (mozc::Util::IsWindowsX64() ? KEY_WOW64_32KEY : 0);
    LONG result = hklm_key.Open(HKEY_LOCAL_MACHINE,
                                kOmahaUsageKeyForEveryone,
                                sam_desired_for_hklm);
    if (ERROR_SUCCESS != result || !hklm_key) {
      // Failed to open the key with delete permission.
      // There are some possible cases, for example, the current process is
      // running under low-integrity.
      // Keep the |hkcu_key| as it was for next chance.
      return;
    }

    result = hklm_key.SetDWORDValue(kSendStatsName, usage_stats);
    if (ERROR_SUCCESS != result) {
      // Failed to update the key.
      // Keep the |hkcu_key| as it was for next chance.
      return;
    }
  }  // Close |hklm_key|

  // Everything is OK so we can delete the wrong key.
  DeleteWrongOmahaUsagestatsValueUnderHKCU();
}

class WinStatsConfigUtilImpl : public StatsConfigUtilInterface {
 public:
  WinStatsConfigUtilImpl();
  virtual ~WinStatsConfigUtilImpl();
  bool IsEnabled();
  bool SetEnabled(bool val);
};

WinStatsConfigUtilImpl::WinStatsConfigUtilImpl() {}

WinStatsConfigUtilImpl::~WinStatsConfigUtilImpl() {}

bool WinStatsConfigUtilImpl::IsEnabled() {
  // Try to clean the registry if wrongly set.
  bool disabled_by_hkcu = false;
  FixupUsagestatsSettingsUnderHKCU(&disabled_by_hkcu);
#ifdef CHANNEL_DEV
  return true;
#else
  // In beta and stable channels, disable usage stats even if it
  // is disabled by the wrong key under HKCU.
  if (disabled_by_hkcu) {
    return false;
  }
  const REGSAM sam_desired = KEY_QUERY_VALUE |
      (mozc::Util::IsWindowsX64() ? KEY_WOW64_32KEY : 0);
  // Like the crash handler, check the "ClientStateMedium" key first.
  // Then we check "ClientState" key.
  {
    CRegKey key_medium;
    LONG result = key_medium.Open(HKEY_LOCAL_MACHINE, kOmahaUsageKeyForEveryone,
                                  sam_desired);
    if (result == ERROR_SUCCESS && key_medium) {
      DWORD value = 0;
      result = key_medium.QueryDWORDValue(kSendStatsName, value);
      if (result == ERROR_SUCCESS) {
        return (value != 0);
      }
    }
  }  // Close |key_medium|

  CRegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, kOmahaUsageKey, sam_desired);
  if (result != ERROR_SUCCESS || !key) {
    return false;
  }
  DWORD value = 0;
  result = key.QueryDWORDValue(kSendStatsName, value);
  if (result != ERROR_SUCCESS) {
    return false;
  }
  return (value != 0);
#endif  // CHANNEL_DEV
}

bool WinStatsConfigUtilImpl::SetEnabled(bool val) {
#ifdef CHANNEL_DEV
  // On Dev channel, usage stats and crash report should be always sent.
  val = true;
  // We always returns true in DevChannel.
  const bool kReturnCodeInError = true;
#else
  const bool kReturnCodeInError = false;
#endif  // CHANNEL_DEV

  CRegKey key;
  const REGSAM sam_desired = KEY_WRITE |
      (mozc::Util::IsWindowsX64() ? KEY_WOW64_32KEY : 0);
  LONG result = key.Create(HKEY_LOCAL_MACHINE, kOmahaUsageKey,
                           REG_NONE, REG_OPTION_NON_VOLATILE,
                           sam_desired);
  if (ERROR_SUCCESS != result) {
    return kReturnCodeInError;
  }

  result = key.SetDWORDValue(kSendStatsName, val ? 1 : 0);
  if (ERROR_SUCCESS != result) {
    return kReturnCodeInError;
  }

  // We've successfully set 1 to the proper entry.
  // Remove the corresponding entry under "ClientStateMedium" just in case.
  CRegKey key_medium;
  result = key_medium.Open(HKEY_LOCAL_MACHINE,
                           kOmahaUsageKeyForEveryone,
                           sam_desired);
  if (result == ERROR_SUCCESS && key_medium) {
    // We do not check the result here.
    key_medium.DeleteValue(kSendStatsName);
  }
  return true;
}

#endif  // OS_WINDOWS

#ifdef OS_MACOSX
class MacStatsConfigUtilImpl : public StatsConfigUtilInterface {
 public:
  MacStatsConfigUtilImpl();
  virtual ~MacStatsConfigUtilImpl();
  bool IsEnabled();
  bool SetEnabled(bool val);

 private:
  string config_file_;
  Mutex mutex_;
};

MacStatsConfigUtilImpl::MacStatsConfigUtilImpl() {
  config_file_ =
      Util::GetUserProfileDirectory() + "/.usagestats.db";  // hidden file
}

MacStatsConfigUtilImpl::~MacStatsConfigUtilImpl() {}

bool MacStatsConfigUtilImpl::IsEnabled() {
#ifdef CHANNEL_DEV
  return true;
#else
  scoped_lock l(&mutex_);
  const bool kDefaultValue = false;

  ifstream ifs(config_file_.c_str(), ios::binary | ios::in);

  if (!ifs.is_open()) {
    return kDefaultValue;
  }

  uint32 value = kDefaultValue;
  if (!ifs.read(reinterpret_cast<char *>(&value), sizeof(value))) {
    return kDefaultValue;
  }

  // The value of usage stats is a 32-bit int and 1 (true) means
  // "sending the usage stats to Google".  When the meaning of the
  // value changed, we should fix mac/ActivatePane.m too.
  // TODO(mukai): export MacStatsConfigUtilImpl and share the code
  // among them.
  return static_cast<bool>(value);
#endif  // CHANNEL_DEV
}

bool MacStatsConfigUtilImpl::SetEnabled(bool val) {
#ifdef CHANNEL_DEV
  return true;
#else
  scoped_lock l(&mutex_);
  const uint32 value = static_cast<uint32>(val);

  if (Util::FileExists(config_file_)) {
    ::chmod(config_file_.c_str(), S_IRUSR | S_IWUSR);  // read/write
  }
  ofstream ofs(config_file_.c_str(), ios::binary | ios::out | ios::trunc);
  if (!ofs) {
    return false;
  }
  ofs.write(reinterpret_cast<const char *>(&value), sizeof(value));
  if (!ofs.good()) {
    return false;
  }
  return 0 == ::chmod(config_file_.c_str(), S_IRUSR);  // read only
#endif  // CHANNEL_DEV
}
#endif  // MACOSX

#ifdef OS_LINUX
class LinuxStatsConfigUtilImpl : public StatsConfigUtilInterface {
  // TODO(toshiyuki): implement this
 public:
  LinuxStatsConfigUtilImpl();
  virtual ~LinuxStatsConfigUtilImpl();
  bool IsEnabled();
  bool SetEnabled(bool val);
};

LinuxStatsConfigUtilImpl::LinuxStatsConfigUtilImpl() {}

LinuxStatsConfigUtilImpl::~LinuxStatsConfigUtilImpl() {}

bool LinuxStatsConfigUtilImpl::IsEnabled() {
  return false;
}

bool LinuxStatsConfigUtilImpl::SetEnabled(bool val) {
  return true;
}
#endif  // OS_LINUX

StatsConfigUtilInterface *g_stats_config_util_handler = NULL;

// GetStatsConfigUtil and SetHandler are not thread safe.

#ifdef OS_WINDOWS
typedef WinStatsConfigUtilImpl DefaultConfigUtilImpl;
#elif defined(OS_MACOSX)
typedef MacStatsConfigUtilImpl DefaultConfigUtilImpl;
#else
typedef LinuxStatsConfigUtilImpl DefaultConfigUtilImpl;
#endif

StatsConfigUtilInterface &GetStatsConfigUtil() {
  if (g_stats_config_util_handler == NULL) {
    return *(Singleton<DefaultConfigUtilImpl>::get());
  } else {
    return *g_stats_config_util_handler;
  }
}
}  // namespace

void StatsConfigUtil::SetHandler(StatsConfigUtilInterface *handler) {
  g_stats_config_util_handler = handler;
}

bool StatsConfigUtil::IsEnabled() {
  return GetStatsConfigUtil().IsEnabled();
}

bool StatsConfigUtil::SetEnabled(bool val) {
  return GetStatsConfigUtil().SetEnabled(val);
}
}  // namespace mozc
