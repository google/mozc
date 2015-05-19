// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_WIN32_IME_IME_LANGUAGE_BAR_MENU_H_
#define MOZC_WIN32_IME_IME_LANGUAGE_BAR_MENU_H_

#include <windows.h>
#include <rpcsal.h>
#include <msctf.h>
#include <vector>

// MIDL_INTERFACE expects a string literal rather than a constant array of
// characters.
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
#define IIDSTR_IMozcLangBarMenu "85B8A2CD-88A0-469f-BC39-8333620AE1F5"
#define IIDSTR_IMozcToggleButtonMenu "E625A19B-C56D-4511-8B48-5A7C2AA10DF5"
#else
#define IIDSTR_IMozcLangBarMenu "6419BEBA-28B7-458D-B7C7-46657FE468D9"
#define IIDSTR_IMozcToggleButtonMenu "14CAC0FE-90C3-4DFC-97FA-B7113F93BD74"
#endif

class LangBarCallback;

enum ImeLangBarItemFlags {
  kImeLangBarItemTypeDefault = 0,
  kImeLangBarItemTypeChecked = TF_LBMENUF_CHECKED,            // 0x1
  // ImeLangBarItemTypeSubMenu = TF_LBMENUF_SUBMENU  // 0x2 (not supported)
  kImeLangBarItemTypeSeparator = TF_LBMENUF_SEPARATOR,        // 0x4
  kImeLangBarItemTypeRadioChecked = TF_LBMENUF_RADIOCHECKED,  // 0x8
  kImeLangBarItemTypeGrayed = TF_LBMENUF_GRAYED,              // 0x10
};

// Represents a tuple to specify the content of a language bar menu item.
struct ImeLangBarMenuItem {
  ImeLangBarItemFlags flags_;
  UINT menu_id_;
  UINT text_id_;
  UINT icon_id_for_non_theme_;
  UINT icon_id_for_theme_;
};

// Reperesents the data possessed internally by a language bar menu item.
struct ImeLangBarMenuData {
  int flags_;
  UINT menu_id_;
  UINT text_id_;
  UINT icon_id_for_non_theme_;
  UINT icon_id_for_theme_;
  int length_;
  wchar_t text_[TF_LBI_DESC_MAXLEN];
};

// Reperesents the data possessed by a language bar menu.
class ImeLangBarMenuDataArray {
 public:
  HRESULT Init(HINSTANCE instance,
               const ImeLangBarMenuItem* menu,
               int count);

  size_t size() const;
  ImeLangBarMenuData* data(size_t i);
 private:
  vector<ImeLangBarMenuData> data_;
};

MIDL_INTERFACE(IIDSTR_IMozcLangBarMenu)
IMozcLangBarMenu : public IUnknown {
  // Sets the status of this language bar menu.
  virtual STDMETHODIMP SetEnabled(bool enabled) = 0;
};


MIDL_INTERFACE(IIDSTR_IMozcToggleButtonMenu)
IMozcToggleButtonMenu : public IUnknown {
  // Selects a menu item which has the given |menu_id|.
  virtual STDMETHODIMP SelectMenuItem(UINT menu_id) = 0;
};

// Represents the common operations for a button-menu item in the language bar.
class ImeLangBarMenu
    : public ITfLangBarItemButton,
      public ITfSource,
      public IMozcLangBarMenu {
 public:
  explicit ImeLangBarMenu(LangBarCallback* langbar_callback,
                          const GUID& guid,
                          bool show_in_tray);
  // It is OK to declare the destructor as non-virtual because all the
  // insbtances will be deleted from and only from the Release() method by
  // |delete this| where |this| has the true (derived) type of the instance.
  ~ImeLangBarMenu();

  // The IUnknown interface methods
  virtual STDMETHODIMP QueryInterface(REFIID guid, void** object) = 0;
  virtual STDMETHODIMP_(ULONG) AddRef() = 0;
  virtual STDMETHODIMP_(ULONG) Release() = 0;

  // The ITfLangBarItem interface methods
  virtual STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* item_info) = 0;
  virtual STDMETHODIMP GetStatus(DWORD* status);
  virtual STDMETHODIMP Show(BOOL show);
  virtual STDMETHODIMP GetTooltipString(BSTR* tooltip);

  // The ITfLangBarItemButton interface methods
  virtual STDMETHODIMP OnClick(TfLBIClick clink, POINT point,
                               const RECT* rect);
  virtual STDMETHODIMP InitMenu(ITfMenu* menu) = 0;
  virtual STDMETHODIMP OnMenuSelect(UINT menu_id) = 0;
  virtual STDMETHODIMP GetIcon(HICON* icon) = 0;
  virtual STDMETHODIMP GetText(BSTR* text);

  // The ITfSource interface methods
  virtual STDMETHODIMP AdviseSink(REFIID interface_id,
                                  IUnknown* unknown,
                                  DWORD* cookie);
  virtual STDMETHODIMP UnadviseSink(DWORD cookie);

  // The IMozcLangBarMenu interface method
  virtual STDMETHODIMP SetEnabled(bool enabled);

  // Initializes an ImeButtonMenu instance.
  // This function allocates resources for an ImeButtonMenu instance.
  HRESULT Init(HINSTANCE instance,
               int string_id,
               const ImeLangBarMenuItem* menu,
               int count);

  // Notifies the language bar of a change in a language bar item.
  STDMETHODIMP OnUpdate(DWORD update_flag);

  // Returns true if a 32-bpp icon can be displayed as a context menu item on
  // the LangBar.  See http://b/2260057 and http://b/2265755
  // for details.
  static bool CanContextMenuDisplay32bppIcon();

 protected:
  STDMETHODIMP QueryInterfaceBase(REFIID guid, void** object);

  // Returns the i-th data in the language bar menu.
  // Returns NULL if i is out of bounds.
  ImeLangBarMenuData* menu_data(size_t i);

  // Returns the size of the language bar menu.
  size_t menu_data_size() const;

  // Returns the item information for langbar menu.
  const TF_LANGBARITEMINFO* item_info() const;

  ITfLangBarItemSink* item_sink_;
  LangBarCallback* langbar_callback_;

 private:
  // Represents the information of an instance copied to the TSF manager.
  // The TSF manager uses this information to identifies an instance as
  // a menu button.
  TF_LANGBARITEMINFO item_info_;

  // Represents the data possessed by the language bar menu.
  ImeLangBarMenuDataArray menu_data_;

  // Records TF_LBI_STATUS_* bits and represents the status of the this langbar
  // menu.
  DWORD status_;
};

// Represents the common operations for a button-menu item with an icon in the
// language bar.
class ImeIconButtonMenu : public ImeLangBarMenu {
 public:
  explicit ImeIconButtonMenu(LangBarCallback* langbar_callback,
                             const GUID& guid,
                             bool show_in_tray);

  // The IUnknown interface methods
  virtual STDMETHODIMP QueryInterface(REFIID guid, void** object);
  virtual STDMETHODIMP_(ULONG) AddRef();
  virtual STDMETHODIMP_(ULONG) Release();

  virtual STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* item_info);

  // A part of the ITfLangBarItemButton interface methods
  virtual STDMETHODIMP InitMenu(ITfMenu* menu);
  virtual STDMETHODIMP OnMenuSelect(UINT menu_id);
  virtual STDMETHODIMP GetIcon(HICON* icon);

  // Initializes an ImeButtonMenu instance.
  // This function allocates resources for an ImeButtonMenu instance.
  HRESULT Init(HINSTANCE instance,
               UINT string_id,
               const ImeLangBarMenuItem* menu,
               int count,
               UINT menu_icon_id_for_non_theme,
               UINT menu_icon_id_for_theme);

 private:
  // Represents the reference count to an instance.
  // volatile modifier is added to conform with InterlockedIncrement API.
  volatile LONG reference_count_;

  // Represents the icon of the language bar menu.
  UINT menu_icon_id_for_theme_;
  UINT menu_icon_id_for_non_theme_;
};

// Represents the common operations for a toggle button-menu item in the
// language bar.
class ImeToggleButtonMenu
    : public ImeLangBarMenu,
      public IMozcToggleButtonMenu {
 public:
  explicit ImeToggleButtonMenu(LangBarCallback* langbar_callback,
                               const GUID& guid,
                               bool show_in_tray);
  ~ImeToggleButtonMenu();

  // The IUnknown interface methods
  virtual STDMETHODIMP QueryInterface(REFIID guid, void** object);
  virtual STDMETHODIMP_(ULONG) AddRef();
  virtual STDMETHODIMP_(ULONG) Release();

  // The IToggleButtonMenu interface methods
  virtual STDMETHODIMP SelectMenuItem(UINT menu_id);

  virtual STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* item_info);

  STDMETHODIMP InitMenu(ITfMenu* menu);
  STDMETHODIMP OnMenuSelect(UINT menu_id);
  STDMETHODIMP GetIcon(HICON* icon);

  // Initializes an ImeButtonMenu instance.
  // This function allocates resources for an ImeButtonMenu instance.
  HRESULT Init(HINSTANCE instance,
               int string_id,
               const ImeLangBarMenuItem* menu,
               int count);

 private:
  // Represents the reference count to an instance.
  // volatile modifier is added to conform with InterlockedIncrement API.
  volatile LONG reference_count_;

  // Represents the index of the selected menu item.
  UINT menu_selected_;
};

// Represents the common operations for a button-menu item in the system
// language bar.
class ImeSystemLangBarMenu
    : public ITfSystemLangBarItemSink {
 public:
  explicit ImeSystemLangBarMenu(LangBarCallback* langbar_callback,
                                const GUID& guid);
  ~ImeSystemLangBarMenu();

  // The IUnknown interface methods
  STDMETHODIMP QueryInterface(REFIID guid, void** object);
  STDMETHODIMP_(ULONG) AddRef(void);
  STDMETHODIMP_(ULONG) Release(void);

  // The ITfLangBarSystemItem interface methods
  STDMETHODIMP InitMenu(ITfMenu* menu);
  STDMETHODIMP OnMenuSelect(UINT menu_id);

  // Initializes an ImeButtonMenu instance.
  // This function allocates resources for an ImeButtonMenu instance.
  HRESULT Init(HINSTANCE instance,
               const ImeLangBarMenuItem* menu,
               int count);

 private:
  // Represents the reference count to an instance.
  // volatile modifier is added to conform with InterlockedIncrement API.
  volatile LONG reference_count_;

  LangBarCallback* langbar_callback_;

  // Represents the data possessed by the language bar menu.
  ImeLangBarMenuDataArray menu_data_;
};

#endif  // MOZC_WIN32_IME_IME_LANGUAGE_BAR_MENU_H_
