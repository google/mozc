// Copyright 2010-2012, Google Inc.
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
// Safe length of IME name in terms of IME_ESC_IME_NAME request.
// See http://msdn.microsoft.com/en-us/library/dd318166.aspx for details.
const int kSafeIMENameLengthForNTInTchars = 64;

// UIWnd class name (including the null terminator) for the IMM32 can be up to
// 16 TCHARs.
const int kIMEUIwndClassNameLimitInTchars = 16;

const wchar_t kDefaultKeyboardLayout[] = L"00000411";

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const char kCompanyNameInEnglish[] = "Google";
const char kEventPathPrefix[] = "Global\\GoogleJapaneseInput.event.";
const char kMutexPathPrefix[] = "Global\\GoogleJapaneseInput.mutex.";
const char kIMEFile[] = "GIMEJa.ime";
const char kMozcBroker32[] = "GoogleIMEJaBroker32.exe";
const char kMozcBroker64[] = "GoogleIMEJaBroker64.exe";
const char kMozcTool[] = "GoogleIMEJaTool.exe";
const char kMozcRenderer[] = "GoogleIMEJaRenderer.exe";
const wchar_t kMessageReceiverMessageName[]
    = L"googlejapaneseinput.renderer.message";
const wchar_t kMessageReceiverClassName[]
    = L"googlejapaneseinput.renderer.window";
const wchar_t kCandidateWindowClassName[]
    = L"GoogleJapaneseInputCandidateWindow";
const wchar_t kCompositionWindowClassName[]
    = L"GoogleJapaneseInputCompositionWindow";
const wchar_t kInfolistWindowClassName[]
    = L"GoogleJapaneseInpuInfolistWindow";
// This UIWnd class name should be used by and only by the actual IMM32
// version.  Make sure that |kIMEUIWndClassName| is different from
// |kDummyIMEUIWndClassName| so that the dummy IME and the actual IME can
// coexist in the same process.  Such a situation is not pleasant but may occur
// in existing processes during silent / background upgrading.
// Please note IMM32 caches UIWnd class name (probably per window station).
// Whenever you rename it, restart Windows before testing.
const wchar_t kIMEUIWndClassName[] = L"GIMEJaUIWindow";
const char kIPCPrefix[] = "\\\\.\\pipe\\googlejapaneseinput.";
const wchar_t kCandidateUIDescription[] = L"GoogleJapaneseInputCandidateUI";
const wchar_t kConfigurationDisplayname[]
    = L"GoogleJapaneseInput Configuration";
const wchar_t kMozcRegKey[] = L"Software\\Google\\Google Japanese Input";
const wchar_t kElevatedProcessDisabledKey[]
    = L"Software\\Policies\\Google\\Google Japanese Input\\Preferences";
#else
const char kCompanyNameInEnglish[] = "Mozc Project";
const char kEventPathPrefix[] = "Global\\Mozc.event.";
const char kMutexPathPrefix[] = "Global\\Mozc.mutex.";
const char kIMEFile[] = "mozc_ja.ime";
const char kMozcBroker32[] = "mozc_broker32.exe";
const char kMozcBroker64[] = "mozc_broker64.exe";
const char kMozcTool[] = "mozc_tool.exe";
const char kMozcRenderer[] = "mozc_renderer.exe";
const wchar_t kMessageReceiverMessageName[]
    = L"mozc.renderer.message";
const wchar_t kMessageReceiverClassName[]
    = L"mozc.renderer.window";
const wchar_t kCandidateWindowClassName[]
    = L"MozcCandidateWindow";
const wchar_t kCompositionWindowClassName[]
    = L"MozcCompositionWindow";
const wchar_t kInfolistWindowClassName[]
    = L"MozcInfolistWindow";
const wchar_t kIMEUIWndClassName[] = L"MozcUIWindow";
const char kIPCPrefix[] = "\\\\.\\pipe\\mozc.";
const wchar_t kCandidateUIDescription[] = L"MozcCandidateUI";
const wchar_t kConfigurationDisplayname[]
    = L"Mozc Configuration";
const wchar_t kMozcRegKey[] = L"Software\\Mozc Project\\Mozc";
const wchar_t kElevatedProcessDisabledKey[]
    = L"Software\\Policies\\Mozc Project\\Mozc\\Preferences";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#elif defined(OS_MACOSX)
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const char kEventPathPrefix[] = "GoogleJapaneseInput.event.";
#else
const char kEventPathPrefix[] = "Mozc.event.";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#else  // OS_LINUX
const char kEventPathPrefix[] = "mozc.event.";
const char kMozcTool[] = "mozc_tool";
#endif

const char kWordRegisterEnvironmentName[] = "default_entry_of_word_register";
const char kWordRegisterEnvironmentReadingName[] =
    "default_reading_entry_of_word_register";
}  // namespace mozc

#endif  // MOZC_BASE_CONST_H_
