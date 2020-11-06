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

// This file will be used to create an import library.  Functions in this
// file must not be called directly.

#include <windows.h>

#include "base/logging.h"

struct LAYOUTORTIPPROFILE;
struct LAYOUTORTIP;

extern "C" UINT WINAPI EnumEnabledLayoutOrTip(
    __in_opt LPCWSTR pszUserReg, __in_opt LPCWSTR pszSystemReg,
    __in_opt LPCWSTR pszSoftwareReg,
    __out LAYOUTORTIPPROFILE *pLayoutOrTipProfile, __in UINT uBufLength) {
  CHECK(false) << "This is a stub function to create an import library. "
               << "Shouldn't be called from anywhere.";
  return 0;
}

extern "C" UINT WINAPI EnumLayoutOrTipForSetup(__in LANGID langid,
                                               __out_ecount(uBufLength)
                                                   LAYOUTORTIP *pLayoutOrTip,
                                               __in UINT uBufLength,
                                               __in DWORD dwFlags) {
  CHECK(false) << "This is a stub function to create an import library. "
               << "Shouldn't be called from anywhere.";
  return 0;
}

extern "C" BOOL WINAPI InstallLayoutOrTip(__in LPCWSTR psz,
                                          __in DWORD dwFlags) {
  CHECK(false) << "This is a stub function to create an import library. "
               << "Shouldn't be called from anywhere.";
  return FALSE;
}

extern "C" BOOL WINAPI InstallLayoutOrTipUserReg(
    __in_opt LPCWSTR pszUserReg, __in_opt LPCWSTR pszSystemReg,
    __in_opt LPCWSTR pszSoftwareReg, __in LPCWSTR psz, __in DWORD dwFlags) {
  CHECK(false) << "This is a stub function to create an import library. "
               << "Shouldn't be called from anywhere.";
  return FALSE;
}

extern "C" BOOL WINAPI SetDefaultLayoutOrTip(__in LPCWSTR psz, DWORD dwFlags) {
  CHECK(false) << "This is a stub function to create an import library. "
               << "Shouldn't be called from anywhere.";
  return FALSE;
}
