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

#ifndef THIRD_PARTY_MOZC_SRC_WIN32_BASE_INPUT_DLL_H_
#define THIRD_PARTY_MOZC_SRC_WIN32_BASE_INPUT_DLL_H_

#include <windows.h>

// Structures and flags below have not been included header files in Windows
// SDK.  You can see the original source of this information at the following
// page.
// - http://msdn.microsoft.com/en-us/library/bb847907.aspx
// - http://msdn.microsoft.com/en-us/library/bb847908.aspx

// Flags used in LAYOUTORTIP::dwFlags.
// Also might be used in LAYOUTORTIP::dwFlags based on observations.
#define LOT_DEFAULT 0x0001
#define LOT_DISABLED 0x0002

// Flags used in LAYOUTORTIPPROFILE::dwProfileType
#define LOTP_INPUTPROCESSOR 1
#define LOTP_KEYBOARDLAYOUT 2

// Flags used with InstallLayoutOrTipUserReg.
#define ILOT_UNINSTALL 0x00000001
#define ILOT_DEFPROFILE 0x00000002
#define ILOT_NOAPPLYTOCURRENTSESSION 0x00000020
#define ILOT_CLEANINSTALL 0x00000040
#define ILOT_DISABLED 0x00000080

// Flags used with SetDefaultLayoutOrTip.
#define SDLOT_NOAPPLYTOCURRENTSESSION 0x00000001
#define SDLOT_APPLYTOCURRENTTHREAD 0x00000002

// Structure used with EnumLayoutOrTipForSetup API.
typedef struct tagLAYOUTORTIP {
  DWORD dwFlags;
  WCHAR szId[MAX_PATH];
  WCHAR szName[MAX_PATH];
} LAYOUTORTIP;

// Structure used with EnumEnabledLayoutOrTip API.
typedef struct tagLAYOUTORTIPPROFILE {
  DWORD dwProfileType;
  LANGID langid;
  CLSID clsid;
  GUID guidProfile;
  GUID catid;
  DWORD dwSubstituteLayout;
  DWORD dwFlags;
  WCHAR szId[MAX_PATH];
} LAYOUTORTIPPROFILE;

// Returns a function pointer to the EnumEnabledLayoutOrTip API, which
// is available on Vista or later via input.dll to enumerates all enabled
// keyboard layouts or text services of the specified user setting.
//
// EnumEnabledLayoutOrTip:
//   URL:
//     http://msdn.microsoft.com/en-us/library/bb847907.aspx
//   Return Value:
//     TRUE: The function was successful.
//     FALSE: An unspecified error occurred.
extern "C" __declspec(dllimport) UINT WINAPI
    EnumEnabledLayoutOrTip(__in_opt LPCWSTR pszUserReg,
                           __in_opt LPCWSTR pszSystemReg,
                           __in_opt LPCWSTR pszSoftwareReg,
                           __out LAYOUTORTIPPROFILE *pLayoutOrTipProfile,
                           __in UINT uBufLength);

// Returns a function pointer to the EnumEnabledLayoutOrTip API, which
// is available on Vista or later via input.dll to enumerates the installed
// keyboard layouts and text services.
//
// EnumLayoutOrTipForSetup:
//   URL:
//     http://msdn.microsoft.com/en-us/library/bb847908.aspx
//   Return Value:
//     |pLayoutOrTip == nullptr|
//        The number of elements to be returned.
//     |pLayoutOrTip != nullptr|
//        The number of elements actually copied to |pLayoutOrTip|.
extern "C" __declspec(dllimport) UINT WINAPI
    EnumLayoutOrTipForSetup(__in LANGID langid,
                            __out_ecount(uBufLength) LAYOUTORTIP *pLayoutOrTip,
                            __in UINT uBufLength, __in DWORD dwFlags);

// Returns a function pointer to the InstallLayoutOrTip API, which is
// available on Vista or later via input.dll to enable the specified
// keyboard layouts or text services for the current user.
//
// InstallLayoutOrTip:
//   URL:
//     http://msdn.microsoft.com/en-us/library/bb847909.aspx
//   Remarks:
//     The string format of the layout list is:
//       <LangID 1>:<KLID 1>;[...<LangID N>:<KLID N>
//     The string format of the text service profile list is:
//       <LangID 1>:{CLSID of TIP}{GUID of LanguageProfile};
//     where GUID should be like {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}.
//     This format seems to be corresponding to the registry key, e.g.
//       HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID of TIP}\LanguageProfile\
//       {Land ID}\{GUID of LanguageProfile}
//   Return Value:
//     TRUE: The function was successful.
//     FALSE: An unspecified error occurred.
extern "C" __declspec(dllimport) BOOL WINAPI
    InstallLayoutOrTip(__in LPCWSTR psz, __in DWORD dwFlags);

// Returns a function pointer to the InstallLayoutOrTipUserReg API, which
// is available on Vista or later via input.dll to enable the specified
// keyboard layouts or text services for the specified user.
//
// InstallLayoutOrTipUserReg:
//   URL:
//     http://msdn.microsoft.com/en-us/library/bb847910.aspx
//   Remarks:
//     The string format of the layout list is:
//       <LangID 1>:<KLID 1>;[...<LangID N>:<KLID N>
//     The string format of the text service profile list is:
//       <LangID 1>:{CLSID of TIP}{GUID of LanguageProfile};
//     where GUID should be like {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}.
//     This format seems to be corresponding to the registry key, e.g.
//       HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID of TIP}\LanguageProfile\
//       {Land ID}\{GUID of LanguageProfile}
//   Return Value:
//     TRUE: The function was successful.
//     FALSE: An unspecified error occurred.
//   Observational Facts:
//     Like ImmInstallIME API, calling InstallLayoutOrTipUserReg from 32-bit
//     process to install x64 binaries is not recommended.  Otherwise, we
//     will see some weird issues like b/2931871.
extern "C" __declspec(dllimport) BOOL WINAPI
    InstallLayoutOrTipUserReg(__in_opt LPCWSTR pszUserReg,
                              __in_opt LPCWSTR pszSystemReg,
                              __in_opt LPCWSTR pszSoftwareReg, __in LPCWSTR psz,
                              __in DWORD dwFlags);

// Returns a function pointer to the SetDefaultLayoutOrTip API, which sets
// the specified keyboard layout or a text service as the default input item
// of the current user.
//
// SetDefaultLayoutOrTip:
//   URL:
//     http://msdn.microsoft.com/en-us/library/bb847915.aspx
//   Remarks:
//     The string format of the layout list is:
//       <LangID 1>:<KLID 1>;[...<LangID N>:<KLID N>
//     The string format of the text service profile list is:
//       <LangID 1>:{CLSID of TIP}{GUID of LanguageProfile};
//     where GUID should be like {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}.
//     This format seems to be corresponding to the registry key, e.g.
//       HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID of TIP}\LanguageProfile\
//       {Land ID}\{GUID of LanguageProfile}
//   Return Value:
//     TRUE: The function was successful.
//     FALSE: An unspecified error occurred.
//   Observational Facts:
//     This API seems to be designed to modify per user settings, like HKCU,
//     so that the current user can modify it with their privileges.  In
//     oother words, no administrative privilege is required.
//     SetDefaultLayoutOrTipUserReg might be a phantom, which only exists in
//     MSDN Library.
//     This function returns fail if it is called to install an IME which is
//     not enabled (if we use undocumented terms to explain the condition,
//     "The IME is not listed in the Preload key").  It seems that the caller
//     is responsible to enable (e.g. calling InstallLayoutOrTipUserReg)
//     the target IME before call this function to set the IME default.
extern "C" __declspec(dllimport) BOOL WINAPI
    SetDefaultLayoutOrTip(__in LPCWSTR psz, DWORD dwFlags);

#endif  // THIRD_PARTY_MOZC_SRC_WIN32_BASE_INPUT_DLL_H_
