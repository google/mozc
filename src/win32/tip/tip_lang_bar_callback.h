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

#ifndef THIRD_PARTY_MOZC_SRC_WIN32_TIP_TIP_LANG_BAR_CALLBACK_H_
#define THIRD_PARTY_MOZC_SRC_WIN32_TIP_TIP_LANG_BAR_CALLBACK_H_

#include <unknwn.h>

namespace mozc::win32::tsf {

class TipLangBarCallback : public IUnknown {
 public:
  enum ItemId {
    // Cancel something for general purpose
    kCancel = 1,

    // For input mode selection
    kDirect = 10,
    kHiragana = 11,
    kFullKatakana = 12,
    kHalfAlphanumeric = 13,
    kFullAlphanumeric = 14,
    kHalfKatakana = 15,

    // Tool menu
    kProperty = 20,
    kDictionary = 21,
    kWordRegister = 22,

    // Help Menu
    kHelp = 30,
    kAbout = 31,

    // Shortcut commands
    kReconversion = 41,
  };

  virtual ~TipLangBarCallback() = default;

  virtual STDMETHODIMP OnMenuSelect(ItemId menu_id) = 0;
  virtual STDMETHODIMP OnItemClick(const wchar_t *description) = 0;
};

}  // namespace mozc::win32::tsf

#endif  // THIRD_PARTY_MOZC_SRC_WIN32_TIP_TIP_LANG_BAR_CALLBACK_H_
