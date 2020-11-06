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

#ifndef MOZC_BASE_WIN_API_TEST_HELPER_H_
#define MOZC_BASE_WIN_API_TEST_HELPER_H_

#ifdef OS_WIN

#include <Windows.h>

#include <string>
#include <vector>

#include "base/port.h"

namespace mozc {

// A helper class to provide a way to hook Win32 API calls for unit testing.
// This class is designed to be used for testing purpose only. Do not use this
// class in production binaries.
//
// Note:
//   There are a lot of caveats in API-hook, For instance, data validation in
//   PE format, race condition while changing the memory protection, and error
//   recovery from such cases are not trivial.
//
// Example:
//   std::vector<WinAPITestHelper::HookRequest> requests;
//   requests.push_back(
//       DEFINE_HOOK("kernel32.dll", GetVersion, GetVersionHook));
//   requests.push_back(
//       DEFINE_HOOK("kernel32.dll", GetComputerName, GetComputerNameHook));
//   requests.push_back(
//       DEFINE_HOOK("kernel32.dll", GetProductInfo, GetProductInfoHook));
//   auto restore_info = WinAPITestHelper::DoHook(
//       ::GetModuleHandle(nullptr),  // hook API calls from our executable
//       requests);
//
//     (run tests...)
//
//   WinAPITestHelper::RestoreHook(restore_info);
class WinAPITestHelper {
 public:
  // An opaque data to restore API hook.
  class RestoreInfo;
  typedef RestoreInfo *RestoreInfoHandle;

  typedef void *FunctionPointer;
  struct HookRequest {
   public:
    HookRequest(const string &src_module, const string &src_proc_name,
                FunctionPointer new_proc_addr);
    const string module_name;
    const string proc_name;
    const FunctionPointer new_proc_address;
  };

  template <typename NewProcType>
  static HookRequest MakeHookRequest(const string &module,
                                     const string &proc_name,
                                     const NewProcType &new_proc_ref) {
    return HookRequest(module, proc_name, &new_proc_ref);
  }

  // Overwrites on-memory Import Address Table (IAT) of |target_module| with
  // given |requests| to hook export functions. API calls from other modules
  // will not be affected.
  // Returns a handle for clean-up task when succeeds.
  // Otherwise returns nullptr.
  // Note:
  //   This method is not thread-safe.
  // Caveats:
  //   Since this code is designed to be used in unit test, this method
  //   causes critical failure and stops execution when something fails.
  static RestoreInfoHandle DoHook(HMODULE target_module,
                                  const std::vector<HookRequest> &requests);

  // Restores the API hooks. |backup_info| cannot be used after this method
  // is called.
  // Note: This method is not thread-safe.
  static void RestoreHook(RestoreInfoHandle backup_info);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WinAPITestHelper);
};

#define DEFINE_HOOK(module_name, original_proc, new_proc)             \
  ::mozc::WinAPITestHelper::MakeHookRequest<decltype(original_proc)>( \
      module_name, AS_STRING(original_proc), new_proc)

}  // namespace mozc

#endif  // OS_WIN
#endif  // MOZC_BASE_WIN_API_TEST_HELPER_H_
