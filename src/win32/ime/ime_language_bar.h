// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_WIN32_IME_IME_LANGUAGE_BAR_H_
#define MOZC_WIN32_IME_IME_LANGUAGE_BAR_H_

#include <windows.h>
#include <rpcsal.h>
#include <msctf.h>
#include <ctffunc.h>
// Workaround against KB813540
#include <atlbase_mozc.h>

#include "base/base.h"
#include "session/commands.pb.h"

class ImeSystemLangBarMenu;
class ImeCandidateList;
class ImeCharacterPad;

class LangBarCallback {
 public:
  enum MenuId {
    // Cancel something for general purpose
    kCancel            = 1,

    // For input mode selection
    kDirect            = 10,
    kHiragana          = 11,
    kFullKatakana      = 12,
    kHalfAlphanumeric  = 13,
    kFullAlphanumeric  = 14,
    kHalfKatakana      = 15,

    // Tool menu
    kProperty          = 20,
    kDictionary        = 21,
    kWordRegister      = 22,
    kHandWriting       = 23,
    kCharacterPalette  = 24,

    // Help Menu
    kHelp              = 30,
    kAbout             = 31,

    // Shortcut commands
    kReconversion      = 41,
  };

  virtual ULONG AddRef() = 0;
  virtual ULONG Release() = 0;
  virtual HRESULT OnMenuSelect(MenuId menu_id) = 0;
  virtual ~LangBarCallback() {}
};

class LanguageBar {
 public:
  LanguageBar();
  ~LanguageBar();

  // initialize and uninitialize ImeLangBarItemButton object.
  HRESULT InitLanguageBar(LangBarCallback *text_service);
  HRESULT UninitLanguageBar();

  // Updates the selected menu in the language bar.
  HRESULT UpdateLangbarMenu(mozc::commands::CompositionMode mode);

  // Sets the status of the language bar menu.
  HRESULT SetLangbarMenuEnabled(bool enable);

 private:
  // Represents the button menu in the language bar.
  // NOTE: ImeToggleButtonMenu inherits ITfLangBarItemButton and ITfSource,
  // which inherit IUnknown. Because of this, using CComPtr<ImeIconButtonMenu>
  // causes a compile error due to the ambiguous overload resolution.
  // To avoid the compile error we use CComPtr<ITfLangBarItemButton> instead.
  ATL::CComPtr<ITfLangBarItemButton> input_button_menu_;

  // The cookie issued for installing ITfLangBarItemSink of input_button_menu_.
  DWORD input_button_cookie_;

  // Represents the tool button menu in the language bar.
  ATL::CComPtr<ITfLangBarItemButton> tool_button_menu_;

  // Represents the help menu in the system language bar.
  ATL::CComPtr<ImeSystemLangBarMenu> help_menu_;

  // The cookie issued for installing ITfSystemLangBarItemSink of help_menu_.
  DWORD help_menu_cookie_;

  DISALLOW_COPY_AND_ASSIGN(LanguageBar);
};

#endif  // MOZC_WIN32_IME_IME_LANGUAGE_BAR_H_
