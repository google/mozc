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

#ifndef MOZC_BASE_CONST_H_
#define MOZC_BASE_CONST_H_

namespace mozc {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const char kProductNameInEnglish[] = "Google Japanese Input";
// "Google 日本語入力"
const char kProductNameLocalized[]
    = "Google \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE5\x85\xA5\xE5\x8A\x9B";
#define kProductPrefix "GoogleJapaneseInput"
#else
const char kProductNameInEnglish[] = "Mozc";
const char kProductNameLocalized[] = "Mozc";
#define kProductPrefix "Mozc"
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#if defined(OS_WINDOWS)
const char kCompanyNameInEnglish[] = "Google";
const char kCharacterPadMailslotPrefix[]
    = "\\\\.\\mailslot\\googlejapaneseinput.character_pad.";
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const char kEventPathPrefix[] = "Global\\GoogleJapaneseInput.event.";
const char kMutexPathPrefix[] = "Global\\GoogleJapaneseInput.mutex.";
#else
const char kEventPathPrefix[] = "Global\\Mozc.event.";
const char kMutexPathPrefix[] = "Global\\Mozc.mutex.";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
const char kMozcServerName[] = "GoogleIMEJaConverter.exe";
const char kIMEFile[] = "GoogleIMEJa.ime";
const char kMozcTIP32[] = "GoogleIMEJaTIP32.dll";
const char kMozcTIP64[] = "GoogleIMEJaTIP64.dll";
const char kMozcTool[] = "GoogleIMEJaTool.exe";
const char kMozcRenderer[] = "GoogleIMEJaRenderer.exe";
const char kCrashReportApp[] = "GoogleIMEJaCrashReporter.exe";
const wchar_t kMessageReceiverMessageName[]
    = L"googlejapaneseinput.renderer.message";
const wchar_t kMessageReceiverClassName[]
    = L"googlejapaneseinput.renderer.window";
const wchar_t kCandidateWindowClassName[]
    = L"GoogleJapaneseInputCandidateWindow";
const wchar_t kCompositionWindowClassName[]
    = L"GoogleJapaneseInputCompositionWindow";
// IMM32 caches UIWnd class name (probably per window station).
// Whenever you rename it, restart Windows before testing.
const wchar_t kDummyIMEUIWndClassName[] = L"GIMEJaUIWnd";
// |kIMEUIWndClassName| is chosen to be the same to |kDummyIMEUIWndClassName|
// because:
//  1. They are exclusive.  If an installer contains a dummy ime, it does not
//     contains a (real) ime.
//  2. IMM32 caches UIWnd class name.  To avoid a reboot during the transition
//     from a dummy ime to new (real) ime, their UIWnd class names should be
//     equal to each other.
const wchar_t kIMEUIWndClassName[] = L"GIMEJaUIWnd";
// UIWnd class name (including the null terminator) for the IMM32 can be up to
// 16 TCHARs.
const int kIMEUIwndClassNameLimitInTchars = 16;
const char kIPCPrefix[] = "\\\\.\\pipe\\googlejapaneseinput.";
const wchar_t kCandidateUIDescription[] = L"GoogleJapaneseInputCandidateUI";
const wchar_t kConfigurationDisplayname[]
    = L"GoogleJapaneseInput Configuration";
const wchar_t kMozcRegKey[] = L"Software\\Google\\Google Japanese Input";
const wchar_t kElevatedProcessDisabledKey[]
    = L"Software\\Policies\\Google\\Google Japanese Input\\Preferences";
const wchar_t kDefaultKeyboardLayout[] = L"00000411";
#elif defined(OS_MACOSX)
const char kMozcServerName[] = kProductPrefix "Converter.app";
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const char kEventPathPrefix[] = "GoogleJapaneseInput.event.";
#else
const char kEventPathPrefix[] = "Mozc.event.";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#else  // OS_LINUX
const char kMozcServerName[] = "mozc_server";
const char kEventPathPrefix[] = "mozc.event.";
const char kMozcTool[] = "mozc_tool";
#endif
}  // namespace mozc

#endif  // MOZC_BASE_CONST_H_
