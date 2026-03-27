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
inline constexpr char kProductNameInEnglish[] = "Google Japanese Input";
#define kProductPrefix "GoogleJapaneseInput"
#else  // GOOGLE_JAPANESE_INPUT_BUILD
inline constexpr char kProductNameInEnglish[] = "Mozc";
#define kProductPrefix "Mozc"
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

inline constexpr char kVersionRewriterVersionPrefix[] = kProductPrefix "-";

#if defined(_WIN32)
// Safe length of IME name in terms of IME_ESC_IME_NAME request.
// See http://msdn.microsoft.com/en-us/library/dd318166.aspx for details.
inline constexpr int kSafeIMENameLengthForNTInTchars = 64;

// UIWnd class name (including the null terminator) for the IMM32 can be up to
// 16 TCHARs.
inline constexpr int kIMEUIwndClassNameLimitInTchars = 16;

inline constexpr wchar_t kDefaultKeyboardLayout[] = L"00000411";

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
inline constexpr char kCompanyNameInEnglish[] = "Google";
// Use Local prefix so that modules running under AppContainer can access.
inline constexpr char kEventPathPrefix[] = "Local\\GoogleJapaneseInput.event.";
inline constexpr char kMutexPathPrefix[] = "Local\\GoogleJapaneseInput.mutex.";
inline constexpr char kMozcServerName[] = "GoogleIMEJaConverter.exe";
inline constexpr char kIMEFile[] = "GIMEJa.ime";
inline constexpr char kMozcTIP32[] = "GoogleIMEJaTIP32.dll";
inline constexpr char kMozcTIP64[] = "GoogleIMEJaTIP64.dll";
inline constexpr char kMozcTIP64X[] = "GoogleIMEJaTIP64X.dll";
inline constexpr char kMozcBroker[] = "GoogleIMEJaBroker.exe";
inline constexpr char kMozcTool[] = "GoogleIMEJaTool.exe";
inline constexpr char kMozcRenderer[] = "GoogleIMEJaRenderer.exe";
inline constexpr char kMozcCacheServiceExeName[] =
    "GoogleIMEJaCacheService.exe";
inline constexpr wchar_t kMozcCacheServiceName[] = L"GoogleIMEJaCacheService";
inline constexpr wchar_t kMessageReceiverMessageName[] =
    L"googlejapaneseinput.renderer.message";
inline constexpr wchar_t kMessageReceiverClassName[] =
    L"googlejapaneseinput.renderer.window";
inline constexpr wchar_t kCandidateWindowClassName[] =
    L"GoogleJapaneseInputCandidateWindow";
inline constexpr wchar_t kCompositionWindowClassName[] =
    L"GoogleJapaneseInputCompositionWindow";
inline constexpr wchar_t kIndicatorWindowClassName[] =
    L"GoogleJapaneseInputIndicatorWindow";
inline constexpr wchar_t kInfolistWindowClassName[] =
    L"GoogleJapaneseInpuInfolistWindow";
// This UIWnd class name should be used by and only by the actual IMM32
// version.  Make sure that |kIMEUIWndClassName| is different from
// |kDummyIMEUIWndClassName| so that the dummy IME and the actual IME can
// coexist in the same process.  Such a situation is not pleasant but may occur
// in existing processes during silent / background upgrading.
// Please note IMM32 caches UIWnd class name (probably per window station).
// Whenever you rename it, restart Windows before testing.
inline constexpr wchar_t kIMEUIWndClassName[] = L"GIMEJaUIWindow";
inline constexpr char kIPCPrefix[] = "\\\\.\\pipe\\googlejapaneseinput.";
inline constexpr wchar_t kCandidateUIDescription[] =
    L"GoogleJapaneseInputCandidateUI";
inline constexpr wchar_t kConfigurationDisplayname[] =
    L"GoogleJapaneseInput Configuration";
inline constexpr wchar_t kMozcRegKey[] =
    L"Software\\Google\\Google Japanese Input";
inline constexpr wchar_t kElevatedProcessDisabledKey[] =
    L"Software\\Policies\\Google\\Google Japanese Input\\Preferences";
#else   // !GOOGLE_JAPANESE_INPUT_BUILD
inline constexpr char kCompanyNameInEnglish[] = "Mozc Project";
// Use Local prefix so that modules running under AppContainer can access.
inline constexpr char kEventPathPrefix[] = "Local\\Mozc.event.";
inline constexpr char kMutexPathPrefix[] = "Local\\Mozc.mutex.";
inline constexpr char kMozcServerName[] = "mozc_server.exe";
inline constexpr char kIMEFile[] = "mozc_ja.ime";
inline constexpr char kMozcTIP32[] = "mozc_tip32.dll";
inline constexpr char kMozcTIP64[] = "mozc_tip64.dll";
inline constexpr char kMozcTIP64X[] = "mozc_tip64x.dll";
inline constexpr char kMozcBroker[] = "mozc_broker.exe";
inline constexpr char kMozcTool[] = "mozc_tool.exe";
inline constexpr char kMozcRenderer[] = "mozc_renderer.exe";
inline constexpr char kMozcCacheServiceExeName[] = "mozc_cache_service.exe";
inline constexpr wchar_t kMozcCacheServiceName[] = L"MozcCacheService";
inline constexpr wchar_t kMessageReceiverMessageName[] =
    L"mozc.renderer.message";
inline constexpr wchar_t kMessageReceiverClassName[] = L"mozc.renderer.window";
inline constexpr wchar_t kCandidateWindowClassName[] = L"MozcCandidateWindow";
inline constexpr wchar_t kCompositionWindowClassName[] =
    L"MozcCompositionWindow";
inline constexpr wchar_t kIndicatorWindowClassName[] = L"MozcIndicatorWindow";
inline constexpr wchar_t kInfolistWindowClassName[] = L"MozcInfolistWindow";
inline constexpr wchar_t kIMEUIWndClassName[] = L"MozcUIWindow";
inline constexpr char kIPCPrefix[] = "\\\\.\\pipe\\mozc.";
inline constexpr wchar_t kCandidateUIDescription[] = L"MozcCandidateUI";
inline constexpr wchar_t kConfigurationDisplayname[] = L"Mozc Configuration";
inline constexpr wchar_t kMozcRegKey[] = L"Software\\Mozc Project\\Mozc";
inline constexpr wchar_t kElevatedProcessDisabledKey[] =
    L"Software\\Policies\\Mozc Project\\Mozc\\Preferences";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#elif defined(__APPLE__)
inline constexpr char kMozcServerName[] = kProductPrefix "Converter.app";
inline constexpr char kMozcRenderer[] = kProductPrefix "Renderer.app";
inline constexpr char kMozcTool[] = kProductPrefix "Tool.app";
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
inline constexpr char kEventPathPrefix[] = "GoogleJapaneseInput.event.";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
inline constexpr char kEventPathPrefix[] = "Mozc.event.";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#else   // __linux__ including __ANDROID__
inline constexpr char kMozcServerName[] = "mozc_server";
inline constexpr char kMozcRenderer[] = "mozc_renderer";
inline constexpr char kEventPathPrefix[] = "mozc.event.";
inline constexpr char kMozcTool[] = "mozc_tool";
#endif  // _WIN32, __APPLE__, or else

inline constexpr char kWordRegisterEnvironmentName[] =
    "default_entry_of_word_register";
inline constexpr char kWordRegisterEnvironmentReadingName[] =
    "default_reading_entry_of_word_register";
}  // namespace mozc

#endif  // MOZC_BASE_CONST_H_
