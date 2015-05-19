// Copyright 2010-2013, Google Inc.
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

#include <windows.h>

#include "win32/base/imm_registrar.h"
#include "win32/base/uninstall_helper.h"

namespace mozc {
namespace win32 {
namespace {

const int kErrorLevelSuccess = 0;
const int kErrorLevelGeneralError = 1;

}  // namespace

int RunRegisterIME(int argc, char *argv[]) {
  const wstring ime_path = ImmRegistrar::GetFullPathForIME();
  if (ime_path.empty()) {
    return kErrorLevelGeneralError;
  }
  const wstring ime_filename = ImmRegistrar::GetFileNameForIME();

  const wstring layout_name = ImmRegistrar::GetLayoutName();
  if (layout_name.empty()) {
    return kErrorLevelGeneralError;
  }

  // Install IME and obtain the corresponding HKL value.
  HKL hkl = nullptr;
  HRESULT result = mozc::win32::ImmRegistrar::Register(
      ime_filename, layout_name, ime_path,
      ImmRegistrar::GetLayoutDisplayNameResourceId(), &hkl);
  if (result != S_OK) {
    return kErrorLevelGeneralError;
  }

  return kErrorLevelSuccess;
}

int RunUnregisterIME(int argc, char *argv[]) {
  UninstallHelper::RestoreUserIMEEnvironmentMain();

  const wstring ime_filename = ImmRegistrar::GetFileNameForIME();
  if (ime_filename.empty()) {
    return kErrorLevelGeneralError;
  }
  ImmRegistrar::Unregister(ime_filename);

  return kErrorLevelSuccess;
}
}  // namespace win32
}  // namespace mozc
