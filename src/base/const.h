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

#ifndef MOZC_BASE_CONST_H_
#define MOZC_BASE_CONST_H_

namespace mozc {
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr char kProductNameInEnglish[] = "Google Japanese Input";
#define kProductPrefix "GoogleJapaneseInput"
#else  // GOOGLE_JAPANESE_INPUT_BUILD
constexpr char kProductNameInEnglish[] = "Mozc";
#define kProductPrefix "Mozc"
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

constexpr char kVersionRewriterVersionPrefix[] = kProductPrefix "-";

#if defined(OS_WIN)
// Safe length of IME name in terms of IME_ESC_IME_NAME request.
// See http://msdn.microsoft.com/en-us/library/dd318166.aspx for details.
const int kSafeIMENameLengthForNTInTchars = 64;

// UIWnd class name (including the null terminator) for the IMM32 can be up to
// 16 TCHARs.
const int kIMEUIwndClassNameLimitInTchars = 16;

constexpr wchar_t kDefaultKeyboardLayout[] = L"00000411";

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr char kCompanyNameInEnglish[] = "Google";
// Use Local prefix so that modules running under AppContainer can access.
constexpr char kEventPathPrefix[] = "Local\\GoogleJapaneseInput.event.";
constexpr char kMutexPathPrefix[] = "Local\\GoogleJapaneseInput.mutex.";
constexpr char kMozcServerName[] = "GoogleIMEJaConverter.exe";
constexpr char kIMEFile[] = "GIMEJa.ime";
constexpr char kMozcTIP32[] = "GoogleIMEJaTIP32.dll";
constexpr char kMozcTIP64[] = "GoogleIMEJaTIP64.dll";
constexpr char kMozcBroker32[] = "GoogleIMEJaBroker32.exe";
constexpr char kMozcBroker64[] = "GoogleIMEJaBroker64.exe";
constexpr char kMozcTool[] = "GoogleIMEJaTool.exe";
constexpr char kMozcRenderer[] = "GoogleIMEJaRenderer.exe";
constexpr char kMozcCacheServiceExeName[] = "GoogleIMEJaCacheService.exe";
constexpr wchar_t kMozcCacheServiceName[] = L"GoogleIMEJaCacheService";
constexpr wchar_t kMessageReceiverMessageName[] =
    L"googlejapaneseinput.renderer.message";
constexpr wchar_t kMessageReceiverClassName[] =
    L"googlejapaneseinput.renderer.window";
constexpr wchar_t kCandidateWindowClassName[] =
    L"GoogleJapaneseInputCandidateWindow";
constexpr wchar_t kCompositionWindowClassName[] =
    L"GoogleJapaneseInputCompositionWindow";
constexpr wchar_t kIndicatorWindowClassName[] =
    L"GoogleJapaneseInputIndicatorWindow";
constexpr wchar_t kInfolistWindowClassName[] =
    L"GoogleJapaneseInpuInfolistWindow";
// This UIWnd class name should be used by and only by the actual IMM32
// version.  Make sure that |kIMEUIWndClassName| is different from
// |kDummyIMEUIWndClassName| so that the dummy IME and the actual IME can
// coexist in the same process.  Such a situation is not pleasant but may occur
// in existing processes during silent / background upgrading.
// Please note IMM32 caches UIWnd class name (probably per window station).
// Whenever you rename it, restart Windows before testing.
constexpr wchar_t kIMEUIWndClassName[] = L"GIMEJaUIWindow";
constexpr char kIPCPrefix[] = "\\\\.\\pipe\\googlejapaneseinput.";
constexpr wchar_t kCandidateUIDescription[] = L"GoogleJapaneseInputCandidateUI";
constexpr wchar_t kConfigurationDisplayname[] =
    L"GoogleJapaneseInput Configuration";
constexpr wchar_t kMozcRegKey[] = L"Software\\Google\\Google Japanese Input";
constexpr wchar_t kElevatedProcessDisabledKey[] =
    L"Software\\Policies\\Google\\Google Japanese Input\\Preferences";
#else
constexpr char kCompanyNameInEnglish[] = "Mozc Project";
// Use Local prefix so that modules running under AppContainer can access.
constexpr char kEventPathPrefix[] = "Local\\Mozc.event.";
constexpr char kMutexPathPrefix[] = "Local\\Mozc.mutex.";
constexpr char kMozcServerName[] = "mozc_server.exe";
constexpr char kIMEFile[] = "mozc_ja.ime";
constexpr char kMozcTIP32[] = "mozc_tip32.dll";
constexpr char kMozcTIP64[] = "mozc_tip64.dll";
constexpr char kMozcBroker32[] = "mozc_broker32.exe";
constexpr char kMozcBroker64[] = "mozc_broker64.exe";
constexpr char kMozcTool[] = "mozc_tool.exe";
constexpr char kMozcRenderer[] = "mozc_renderer.exe";
constexpr char kMozcCacheServiceExeName[] = "mozc_cache_service.exe";
constexpr wchar_t kMozcCacheServiceName[] = L"MozcCacheService";
constexpr wchar_t kMessageReceiverMessageName[] = L"mozc.renderer.message";
constexpr wchar_t kMessageReceiverClassName[] = L"mozc.renderer.window";
constexpr wchar_t kCandidateWindowClassName[] = L"MozcCandidateWindow";
constexpr wchar_t kCompositionWindowClassName[] = L"MozcCompositionWindow";
constexpr wchar_t kIndicatorWindowClassName[] = L"MozcIndicatorWindow";
constexpr wchar_t kInfolistWindowClassName[] = L"MozcInfolistWindow";
constexpr wchar_t kIMEUIWndClassName[] = L"MozcUIWindow";
constexpr char kIPCPrefix[] = "\\\\.\\pipe\\mozc.";
constexpr wchar_t kCandidateUIDescription[] = L"MozcCandidateUI";
constexpr wchar_t kConfigurationDisplayname[] = L"Mozc Configuration";
constexpr wchar_t kMozcRegKey[] = L"Software\\Mozc Project\\Mozc";
constexpr wchar_t kElevatedProcessDisabledKey[] =
    L"Software\\Policies\\Mozc Project\\Mozc\\Preferences";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#elif defined(__APPLE__)
constexpr char kMozcServerName[] = kProductPrefix "Converter.app";
constexpr char kMozcRenderer[] = kProductPrefix "Renderer.app";
constexpr char kMozcTool[] = kProductPrefix "Tool.app";
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr char kEventPathPrefix[] = "GoogleJapaneseInput.event.";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
constexpr char kEventPathPrefix[] = "Mozc.event.";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#else   // OS_LINUX including OS_ANDROID
constexpr char kMozcServerName[] = "mozc_server";
constexpr char kMozcRenderer[] = "mozc_renderer";
constexpr char kEventPathPrefix[] = "mozc.event.";
constexpr char kMozcTool[] = "mozc_tool";
#endif

constexpr char kWordRegisterEnvironmentName[] =
    "default_entry_of_word_register";
constexpr char kWordRegisterEnvironmentReadingName[] =
    "default_reading_entry_of_word_register";
}  // namespace mozc

#endif  // MOZC_BASE_CONST_H_
