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

#include "win32/ime/ime_language_bar_menu.h"

// clang-format off
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atlgdi.h>
#include <atluser.h>
// clang-format on

#include <ole2.h>
#include <olectl.h>
#include <uxtheme.h>

#include <limits>

#include "base/logging.h"
#include "base/win_util.h"
#include "win32/base/text_icon.h"
#include "win32/ime/ime_impl_imm.h"
#include "win32/ime/ime_language_bar.h"
#include "win32/ime/ime_resource.h"

namespace {

using ::WTL::CBitmap;
using ::WTL::CDC;
using ::WTL::CIcon;
using ::WTL::CSize;

using ::mozc::WinUtil;
using ::mozc::win32::TextIcon;

constexpr int kDefaultDPI = 96;

// Represents the cookie for the sink to an ImeLangBarItem object.
constexpr int kImeLangBarMenuCookie =
    (('M' << 24) | ('o' << 16) | ('z' << 8) | ('c' << 0));

constexpr char kTextIconFont[] = "ＭＳ ゴシック";

// TODO(yukawa): Refactor LangBar code so that we can configure following
// settings as a part of initialization.
std::string GetIconStringIfNecessary(UINT icon_id) {
  switch (icon_id) {
    case IDI_DIRECT_NT:
      return "A";
    case IDI_HIRAGANA_NT:
      return "あ";
    case IDI_FULL_KATAKANA_NT:
      return "ア";
    case IDI_HALF_ALPHANUMERIC_NT:
      return "_A";
    case IDI_FULL_ALPHANUMERIC_NT:
      return "Ａ";
    case IDI_HALF_KATAKANA_NT:
      return "_ｱ";
  }
  return "";
}

// Loads an icon which is appropriate for the current theme.
// An icon ID 0 represents "no icon".
HICON LoadIconFromResource(HINSTANCE instance, UINT icon_id_for_non_theme,
                           UINT icon_id_for_theme) {
  // We use a 32-bpp icon if we can observe the uxtheme is running.
  UINT id = icon_id_for_non_theme;
  if (icon_id_for_theme != 0) {
    const wchar_t kThemeDll[] = L"uxtheme.dll";
    typedef BOOL(WINAPI * FPIsThemeActive)();

    // mozc::Util::GetSystemModuleHandle is not safe when the specified DLL is
    // unloaded by other threads.
    // TODO(yukawa): Make a wrapper of GetModuleHandleEx to increment a
    // reference count of the theme DLL while we call IsThemeActive API.
    const HMODULE theme_dll =
        WinUtil::GetSystemModuleHandleAndIncrementRefCount(kThemeDll);
    if (theme_dll != nullptr) {
      FPIsThemeActive is_thread_active = reinterpret_cast<FPIsThemeActive>(
          ::GetProcAddress(theme_dll, "IsThemeActive"));
      if (is_thread_active != nullptr && is_thread_active()) {
        id = icon_id_for_theme;
      }
    }
    if (theme_dll != nullptr) {
      ::FreeLibrary(theme_dll);
    }
  }

  if (id == 0) {
    return nullptr;
  }

  const auto icon_size = ::GetSystemMetrics(SM_CYSMICON);

  // Replace some text icons with on-the-fly image drawn with MS-Gothic.
  const auto& icon_text = GetIconStringIfNecessary(id);
  if (!icon_text.empty()) {
    const COLORREF text_color = ::GetSysColor(COLOR_WINDOWTEXT);
    return TextIcon::CreateMonochromeIcon(icon_size, icon_size, icon_text,
                                          kTextIconFont, text_color);
  }

  return static_cast<HICON>(::LoadImage(instance, MAKEINTRESOURCE(id),
                                        IMAGE_ICON, icon_size, icon_size,
                                        LR_CREATEDIBSECTION));
}

// Retrieves the bitmap handle loaded by using an icon ID.
// Returns true if the specified icons is available as bitmaps.
// Caller can set nullptr for |color| and/or |mask| to represent not to receive
// the specified handle even if it exists.  Caller should releases any returned
// bitmap handle and this function releases any handle which is not received
// by the caller.
bool LoadIconAsBitmap(HINSTANCE instance, UINT icon_id_for_non_theme,
                      UINT icon_id_for_theme, HBITMAP* color, HBITMAP* mask) {
  if (color != nullptr) {
    *color = nullptr;
  }
  if (mask != nullptr) {
    *mask = nullptr;
  }

  CIcon icon(
      LoadIconFromResource(instance, icon_id_for_non_theme, icon_id_for_theme));

  if (icon.IsNull()) {
    return false;
  }

  ICONINFO icon_info = {0};
  if (!::GetIconInfo(icon, &icon_info)) {
    return false;
  }

  CBitmap color_bitmap(icon_info.hbmColor);
  CBitmap mask_bitmap(icon_info.hbmMask);

  if (color != nullptr) {
    *color = color_bitmap.Detach();
  }
  if (mask != nullptr) {
    *mask = mask_bitmap.Detach();
  }

  return true;
}

}  // namespace

HRESULT ImeLangBarMenuDataArray::Init(HINSTANCE instance,
                                      const ImeLangBarMenuItem* menu,
                                      int count) {
  HRESULT result = S_OK;

  // Attach menu texts and icons.
  for (int i = 0; i < count; ++i) {
    ImeLangBarMenuData data = {};
    int length = 0;
    HICON icon = nullptr;
    if ((menu[i].flags_ & TF_LBMENUF_SEPARATOR) == 0) {
      // Retrieve the menu text and button icon.
      length = ::LoadString(instance, menu[i].text_id_, &data.text_[0],
                            arraysize(data.text_));
    }
    data.flags_ = menu[i].flags_;
    data.menu_id_ = menu[i].menu_id_;
    data.text_id_ = menu[i].text_id_;
    data.length_ = length;
    data.icon_id_for_non_theme_ = menu[i].icon_id_for_non_theme_;
    data.icon_id_for_theme_ = menu[i].icon_id_for_theme_;
    data_.push_back(data);
  }

  return result;
}

size_t ImeLangBarMenuDataArray::size() const { return data_.size(); }

ImeLangBarMenuData* ImeLangBarMenuDataArray::data(size_t i) {
  if (i >= size()) {
    return nullptr;
  }
  return &data_[i];
}

// Implements the constructor of the ImeLangBarMenu class.
ImeLangBarMenu::ImeLangBarMenu(LangBarCallback* langbar_callback,
                               const GUID& guid, bool show_in_tray)
    : item_sink_(nullptr), langbar_callback_(nullptr), status_(0) {
  // Initialize its TF_LANGBARITEMINFO object, which contains the properties of
  // this item and is copied to the TSF manager in GetInfo() function.
  // We set CLSID_NULL because this item is not provided by a text service.
  item_info_.clsidService = CLSID_NULL;
  item_info_.guidItem = guid;
  // The visibility of a langbar-item in the minimized langbar is actually
  // controlled by TF_LBI_STYLE_SHOWNINTRAY flag despite the fact that the
  // document says "This flag is not currently supported".
  // http://msdn.microsoft.com/en-us/library/ms629078.aspx
  // http://b/2275633
  item_info_.dwStyle = TF_LBI_STYLE_BTN_MENU;
  if (show_in_tray) {
    item_info_.dwStyle |= TF_LBI_STYLE_SHOWNINTRAY;
  }
  item_info_.ulSort = 0;
  item_info_.szDescription[0] = L'\0';

  // Save the LangBarCallback object who owns this button, and increase its
  // reference count not to prevent the object from being deleted
  langbar_callback_ = langbar_callback;
  langbar_callback_->AddRef();
}

// Implements the destructor of the ImeLangBarMenu class.
ImeLangBarMenu::~ImeLangBarMenu() {
  // Release the owner LangBarCallback object.
  if (langbar_callback_ != nullptr) {
    langbar_callback_->Release();
  }
  langbar_callback_ = nullptr;
}

// Implements the ITfLangBarItem::GetInfo() function.
// This function is called by Windows to update this button menu.
STDAPI ImeLangBarMenu::GetInfo(TF_LANGBARITEMINFO* item_info) {
  // Just copies the cached TF_LANGBARITEMINFO object.
  *item_info = item_info_;
  return S_OK;
}

// Implements the ITfLangBarItem::GetStatus() function.
// This function is called by Windows to retrieve the current status of this
// button menu.
STDAPI ImeLangBarMenu::GetStatus(DWORD* status) {
  *status = status_;
  return S_OK;
}

// Implements the ITfLangBarItem::Show() function.
// This function is called by Windows to notify the display status of this
// button menu has been updated.
STDAPI ImeLangBarMenu::Show(BOOL show) {
  // Just return because this button is always shown, i.e. we do not have to
  // manage the display state of this button menu.
  return E_NOTIMPL;
}

// Implements the ITfLangBarItem::GetTooltipString() function.
// This function is called when Windows to retrieve the tool-tip string of
// this button menu.
STDAPI ImeLangBarMenu::GetTooltipString(BSTR* tooltip) {
  // Created a COM string from the description and copy it.
  *tooltip = ::SysAllocString(&item_info_.szDescription[0]);
  return (*tooltip ? S_OK : E_OUTOFMEMORY);
}

// Implements the ITfLangBarItemButton::OnClick() function.
// This function is not used for a button menu.
STDAPI ImeLangBarMenu::OnClick(TfLBIClick click, POINT point,
                               const RECT* rect) {
  // Just returns because Windows does not call this function when an
  // ITfLangBarItem object is a button menu.
  return S_OK;
}

// Implements the ITfLangBarItemButton::GetText() function.
// This function is called by Windows to retrieve the text label of this
// button menu.
STDAPI ImeLangBarMenu::GetText(BSTR* text) {
  *text = ::SysAllocString(&item_info_.szDescription[0]);
  return (*text ? S_OK : E_OUTOFMEMORY);
}

// Implements the ITfSource::AdviseSink() function.
STDAPI ImeLangBarMenu::AdviseSink(REFIID interface_id, IUnknown* unknown,
                                  DWORD* cookie) {
  // Return if the caller tries to start advising any events except the
  // ITfLangBarItemSink events.
  if (!IsEqualIID(IID_ITfLangBarItemSink, interface_id))
    return CONNECT_E_CANNOTCONNECT;

  // Exit if this object has a sink which advising ITfLangBarItemSink events.
  if (item_sink_ != nullptr) return CONNECT_E_ADVISELIMIT;

  // Retrieve the ITfLangBarItemSink interface from the given object and store
  // it into |item_sink_|.
  HRESULT result = unknown->QueryInterface(
      IID_ITfLangBarItemSink, reinterpret_cast<void**>(&item_sink_));
  if (result != S_OK) {
    item_sink_ = nullptr;
    return result;
  }

  // Return the cookie of this object.
  *cookie = kImeLangBarMenuCookie;
  return result;
}

STDAPI ImeLangBarMenu::UnadviseSink(DWORD cookie) {
  // Return if the given cookie
  if (cookie != kImeLangBarMenuCookie || item_sink_ == nullptr)
    return CONNECT_E_NOCONNECTION;

  // Release the copy of this event.
  item_sink_->Release();
  item_sink_ = nullptr;

  return S_OK;
}

// Initializes an ImeLangBarItem instance.
// This function is called by a text service to provide the information
// required for creating a menu button. A text service MUST call this function
// before calling the ITfLangBarItemMgr::AddItem() function and adding this
// button menu to a language bar.
HRESULT ImeLangBarMenu::Init(HINSTANCE instance, int string_id,
                             const ImeLangBarMenuItem* menu, int count) {
  HRESULT result = S_OK;
  // Retrieve the text label from the resource.
  // This string is also used as a tool-tip text.
  ::LoadString(instance, string_id, &item_info_.szDescription[0],
               arraysize(item_info_.szDescription));

  // Create a new ImeLangBarMenuItem object.
  menu_data_.Init(instance, menu, count);

  return S_OK;
}

HRESULT ImeLangBarMenu::OnUpdate(DWORD update_flag) {
  // For some reason, this method might be called when the event sink is not
  // available. See b/2977835 for details.
  if (item_sink_ == nullptr) {
    return E_FAIL;
  }
  return item_sink_->OnUpdate(update_flag);
}

HRESULT ImeLangBarMenu::SetEnabled(bool enabled) {
  HRESULT result = S_OK;

  if (enabled) {
    status_ &= ~TF_LBI_STATUS_DISABLED;
  } else {
    status_ |= TF_LBI_STATUS_DISABLED;
  }
  result = ImeLangBarMenu::OnUpdate(TF_LBI_STATUS);

  return result;
}

bool ImeLangBarMenu::CanContextMenuDisplay32bppIcon() {
  // We always use a non-theme icon for a context menu icon on the LangBar
  // unless the current display mode is 32-bpp.  We cannot assume we can
  // display a 32-bpp icon for a context menu icon on the LangBar unless the
  // current display mode is 32-bpp.  See http://b/2260057
  CDC display_dc(::GetDC(nullptr));
  return !display_dc.IsNull() && display_dc.GetDeviceCaps(PLANES) == 1 &&
         display_dc.GetDeviceCaps(BITSPIXEL) == 32;
}

ImeLangBarMenuData* ImeLangBarMenu::menu_data(size_t i) {
  return menu_data_.data(i);
}

size_t ImeLangBarMenu::menu_data_size() const { return menu_data_.size(); }

const TF_LANGBARITEMINFO* ImeLangBarMenu::item_info() const {
  return &item_info_;
}

// Basic implements the IUnknown::QueryInterface() function.
STDAPI ImeLangBarMenu::QueryInterfaceBase(REFIID interface_id, void** object) {
  if (!object) {
    return E_INVALIDARG;
  }

  // Find a matching interface from the ones implemented by this object
  // (i.e. IUnknown, ITfLangBarItem, ITfLangBarItemButton, ITfSource).
  if (::IsEqualIID(interface_id, __uuidof(IMozcLangBarMenu))) {
    *object = static_cast<IMozcLangBarMenu*>(this);
    AddRef();
    return S_OK;
  }

  if (::IsEqualIID(interface_id, IID_IUnknown)) {
    *object = static_cast<IUnknown*>(static_cast<ITfLangBarItemButton*>(this));
    AddRef();
    return S_OK;
  }

  if (::IsEqualIID(interface_id, IID_ITfLangBarItem)) {
    *object = static_cast<ITfLangBarItem*>(this);
    AddRef();
    return S_OK;
  }

  if (::IsEqualIID(interface_id, IID_ITfLangBarItemButton)) {
    *object = static_cast<ITfLangBarItemButton*>(this);
    AddRef();
    return S_OK;
  }

  if (::IsEqualIID(interface_id, IID_ITfSource)) {
    *object = static_cast<ITfSource*>(this);
    AddRef();
    return S_OK;
  }

  // This object does not implement the given interface.
  *object = nullptr;
  return E_NOINTERFACE;
}

ImeIconButtonMenu::ImeIconButtonMenu(LangBarCallback* langbar_callback,
                                     const GUID& guid, bool show_in_tray)
    : ImeLangBarMenu(langbar_callback, guid, show_in_tray),
      reference_count_(0),
      menu_icon_id_for_theme_(0),
      menu_icon_id_for_non_theme_(0) {}

// Implements the IUnknown::QueryInterface() function.
// This function is used by Windows to retrieve the interfaces implemented by
// this class.
STDAPI ImeIconButtonMenu::QueryInterface(REFIID interface_id, void** object) {
  return QueryInterfaceBase(interface_id, object);
}

// Implements the IUnknown::AddRef() function.
// This function is called by Windows and LangBarCallback instances to notify
// they need to have a copy of this object.
// This implementation just increases the reference count of this object
// to prevent this object from being deleted during it is in use.
STDAPI_(ULONG) ImeIconButtonMenu::AddRef() {
  const LONG count = ::InterlockedIncrement(&reference_count_);
  if (count < 0) {
    DLOG(INFO) << "Reference count is negative.";
    return 0;
  }
  return count;
}

// Implements the IUnknown::Release() function.
// This function is called by Windows and LangBarCallback instances to notify
// they do not need their local copies of this object any longer.
// This implementation just decreases the reference count of this object
// and delete it only if it is not referenced by any objects.
STDAPI_(ULONG) ImeIconButtonMenu::Release() {
  const LONG count = ::InterlockedDecrement(&reference_count_);
  if (count <= 0) {
    delete this;
    return 0;
  }
  return count;
}

STDAPI ImeIconButtonMenu::InitMenu(ITfMenu* menu) {
  HRESULT result = S_OK;

  // Add the menu items of this object to the given ITfMenu object.
  for (size_t i = 0; i < menu_data_size(); ++i) {
    const ImeLangBarMenuData* data = menu_data(i);

    const UINT icon_id_for_theme = CanContextMenuDisplay32bppIcon()
                                       ? data->icon_id_for_theme_
                                       : data->icon_id_for_non_theme_;
    CBitmap bitmap;
    CBitmap mask;
    // If LoadIconAsBitmap fails, |bitmap.m_hBitmap| and |mask.m_hBitmap|
    // remain nullptr bitmap handles.
    LoadIconAsBitmap(ImeGetResource(), data->icon_id_for_non_theme_,
                     icon_id_for_theme, &bitmap.m_hBitmap, &mask.m_hBitmap);
    result = menu->AddMenuItem(i, data->flags_, bitmap, mask, data->text_,
                               data->length_, nullptr);
    if (result != S_OK) {
      break;
    }
  }

  return result;
}

STDAPI ImeIconButtonMenu::OnMenuSelect(UINT menu_id) {
  // Call the LangBarCallback::OnMenuSelect() function to dispatch the
  // given event.
  ImeLangBarMenuData* data = menu_data(menu_id);
  if (data == nullptr) {
    return E_INVALIDARG;
  }
  if (data->menu_id_ == LangBarCallback::kCancel) {
    return S_OK;
  }
  const HRESULT result = langbar_callback_->OnMenuSelect(
      static_cast<LangBarCallback::MenuId>(data->menu_id_));
  return result;
}

// Implements the ITfLangBarItem::GetInfo() function.
// This function is called by Windows to update this button menu.
STDAPI ImeIconButtonMenu::GetInfo(TF_LANGBARITEMINFO* item_info) {
  HRESULT result = S_OK;

  if (item_info == nullptr) {
    return E_INVALIDARG;
  }

  // Just copies the cached TF_LANGBARITEMINFO object.
  *item_info = *this->item_info();
  return S_OK;
}

STDAPI ImeIconButtonMenu::GetIcon(HICON* icon) {
  if (icon == nullptr) {
    return E_INVALIDARG;
  }
  // Excerpt: http://msdn.microsoft.com/en-us/library/ms628718.aspx
  // The caller must free this icon when it is no longer required by
  // calling DestroyIcon.
  // Caveats: ITfLangBarMgr causes GDI handle leak when an icon which consists
  // only of mask bitmap (AND bitmap) is returned. |*icon| must have color
  // bitmap (XOR bitmap) as well as mask bitmap (XOR bitmap) to avoid GDI
  // handle leak.
  *icon = LoadIconFromResource(ImeGetResource(), menu_icon_id_for_non_theme_,
                               menu_icon_id_for_theme_);
  return (*icon ? S_OK : E_FAIL);
}

// Initializes an ImeButtonMenu instance.
// This function allocates resources for an ImeButtonMenu instance.
HRESULT ImeIconButtonMenu::Init(HINSTANCE instance, UINT string_id,
                                const ImeLangBarMenuItem* menu, int count,
                                UINT menu_icon_id_for_non_theme,
                                UINT menu_icon_id_for_theme) {
  menu_icon_id_for_theme_ = menu_icon_id_for_theme;
  menu_icon_id_for_non_theme_ = menu_icon_id_for_non_theme;
  return ImeLangBarMenu::Init(instance, string_id, menu, count);
}

ImeToggleButtonMenu::ImeToggleButtonMenu(LangBarCallback* langbar_callback,
                                         const GUID& guid, bool show_in_tray)
    : ImeLangBarMenu(langbar_callback, guid, show_in_tray),
      reference_count_(0),
      menu_selected_(0) {}

ImeToggleButtonMenu::~ImeToggleButtonMenu() {}

// Implements the IUnknown::QueryInterface() function.
// This function is used by Windows to retrieve the interfaces implemented by
// this class.
STDAPI ImeToggleButtonMenu::QueryInterface(REFIID interface_id, void** object) {
  if (!object) {
    return E_INVALIDARG;
  }

  if (::IsEqualIID(interface_id, __uuidof(IMozcToggleButtonMenu))) {
    *object = static_cast<IMozcToggleButtonMenu*>(this);
    AddRef();
    return S_OK;
  }

  return QueryInterfaceBase(interface_id, object);
}

// Implements the IUnknown::AddRef() function.
// This function is called by Windows and LangBarCallback instances to notify
// they need to have a copy of this object.
// This implementation just increases the reference count of this object
// to prevent this object from being deleted during it is in use.
STDAPI_(ULONG) ImeToggleButtonMenu::AddRef() {
  const LONG count = ::InterlockedIncrement(&reference_count_);
  if (count < 0) {
    DLOG(INFO) << "Reference count is negative.";
    return 0;
  }
  return count;
}

// Implements the IUnknown::Release() function.
// This function is called by Windows and LangBarCallback instances to notify
// they do not need their local copies of this object any longer.
// This implementation just decreases the reference count of this object
// and delete it only if it is not referenced by any objects.
STDAPI_(ULONG) ImeToggleButtonMenu::Release() {
  const LONG count = ::InterlockedDecrement(&reference_count_);
  if (count <= 0) {
    delete this;
    return 0;
  }
  return count;
}

// Implements the ITfLangBarItem::GetInfo() function.
// This function is called by Windows to update this button menu.
STDAPI ImeToggleButtonMenu::GetInfo(TF_LANGBARITEMINFO* item_info) {
  HRESULT result = S_OK;

  if (item_info == nullptr) {
    return E_INVALIDARG;
  }

  // Just copies the cached TF_LANGBARITEMINFO object.
  *item_info = *this->item_info();

  if (FAILED(result)) {
    return result;
  }
  return S_OK;
}

STDAPI ImeToggleButtonMenu::InitMenu(ITfMenu* menu) {
  HRESULT result = S_OK;

  // Add the menu items of this object to the given ITfMenu object.
  for (size_t i = 0; i < menu_data_size(); ++i) {
    const ImeLangBarMenuData* data = menu_data(i);
    result = menu->AddMenuItem(i, data->flags_, nullptr, nullptr, data->text_,
                               data->length_, nullptr);
    if (result != S_OK) {
      break;
    }
  }

  return result;
}

STDAPI ImeToggleButtonMenu::OnMenuSelect(UINT menu_id) {
  // Call the LangBarCallback::OnMenuSelect() function to dispatch the
  // given event.
  ImeLangBarMenuData* data = menu_data(menu_id);
  if (data == nullptr) {
    return E_INVALIDARG;
  }
  if (data->menu_id_ == LangBarCallback::kCancel) {
    return S_OK;
  }
  const HRESULT result = langbar_callback_->OnMenuSelect(
      static_cast<LangBarCallback::MenuId>(data->menu_id_));
  if (result != S_OK) {
    return result;
  }
  ImeLangBarMenuData* selected = menu_data(menu_selected_);
  selected->flags_ &= ~TF_LBMENUF_RADIOCHECKED;
  data->flags_ |= TF_LBMENUF_RADIOCHECKED;
  menu_selected_ = menu_id;
  return result;
}

STDAPI ImeToggleButtonMenu::GetIcon(HICON* icon) {
  if (icon == nullptr) {
    return E_INVALIDARG;
  }

  const ImeLangBarMenuData* selected = menu_data(menu_selected_);

  // Excerpt: http://msdn.microsoft.com/en-us/library/ms628718.aspx
  // The caller must free this icon when it is no longer required by
  // calling DestroyIcon.
  // Caveats: ITfLangBarMgr causes GDI handle leak when an icon which consists
  // only of mask bitmap (AND bitmap) is returned. |*icon| must have color
  // bitmap (XOR bitmap) as well as mask bitmap (XOR bitmap) to avoid GDI
  // handle leak.
  *icon =
      LoadIconFromResource(ImeGetResource(), selected->icon_id_for_non_theme_,
                           selected->icon_id_for_theme_);
  return (*icon ? S_OK : E_FAIL);
}

HRESULT ImeToggleButtonMenu::Init(HINSTANCE instance, int string_id,
                                  const ImeLangBarMenuItem* menu, int count) {
  return ImeLangBarMenu::Init(instance, string_id, menu, count);
}

// Implements the IMozcToggleButtonMenu::SelectMenuItem() function.
HRESULT ImeToggleButtonMenu::SelectMenuItem(UINT menu_id) {
  // Now SelectMenuItem may be called frequently to update LangbarItem for
  // every key input.  So we call ImeLangBarMenu::OnUpdate only if any item
  // state is updated.
  bool item_state_changed = false;
  for (size_t i = 0; i < menu_data_size(); ++i) {
    ImeLangBarMenuData* data = menu_data(i);
    if (data->menu_id_ == menu_id) {
      if ((data->flags_ & TF_LBMENUF_RADIOCHECKED) != 0 ||
          menu_selected_ != static_cast<UINT>(i)) {
        item_state_changed |= true;
      }
      data->flags_ |= TF_LBMENUF_RADIOCHECKED;
      menu_selected_ = static_cast<UINT>(i);
    } else {
      if (data->flags_ != 0) {
        item_state_changed |= true;
      }
      data->flags_ &= ~TF_LBMENUF_RADIOCHECKED;
    }
  }
  if (item_state_changed) {
    ImeLangBarMenu::OnUpdate(TF_LBI_ICON | TF_LBI_STATUS);
  }
  return S_OK;
}

// Implements the constructor of the ImeSystemLangBarMenu class.
ImeSystemLangBarMenu::ImeSystemLangBarMenu(LangBarCallback* langbar_callback,
                                           const GUID& guid)
    : reference_count_(0) {
  // Save the LangBarCallback object who owns this button, and increase its
  // reference count not to prevent the object from being deleted
  langbar_callback_ = langbar_callback;
  langbar_callback_->AddRef();
}

// Implements the destructor of the ImeSystemLangBarMenu class.
ImeSystemLangBarMenu::~ImeSystemLangBarMenu() {
  // Release the owner LangBarCallback object.
  if (langbar_callback_ != nullptr) {
    langbar_callback_->Release();
  }
  langbar_callback_ = nullptr;
}

// Implements the IUnknown::QueryInterface() function.
// This function is used by Windows to retrieve the interfaces implemented by
// this class.
STDAPI ImeSystemLangBarMenu::QueryInterface(REFIID interface_id,
                                            void** object) {
  if (!object) {
    return E_INVALIDARG;
  }

  // Find a matching interface from the ones implemented by this object
  // (i.e. IUnknown, ITfSystemLangBarItemSink, ITfSource).
  if (IsEqualIID(interface_id, IID_IUnknown) ||
      IsEqualIID(interface_id, IID_ITfSystemLangBarItemSink)) {
    *object = static_cast<ITfSystemLangBarItemSink*>(this);
    AddRef();
    return S_OK;
  } else {
    // This object does not implement the given interface.
    *object = nullptr;
    return E_NOINTERFACE;
  }
}

// Implements the IUnknown::AddRef() function.
// This function is called by Windows and LangBarCallback instances to notify
// they need to have a copy of this object.
// This implementation just increases the reference count of this object
// to prevent this object from being deleted during it is in use.
STDAPI_(ULONG) ImeSystemLangBarMenu::AddRef() {
  const LONG count = ::InterlockedIncrement(&reference_count_);
  if (count < 0) {
    DLOG(INFO) << "Reference count is negative.";
    return 0;
  }
  return count;
}

// Implements the IUnknown::Release() function.
// This function is called by Windows and LangBarCallback instances to notify
// they do not need their local copies of this object any longer.
// This implementation just decreases the reference count of this object
// and delete it only if it is not referenced by any objects.
STDAPI_(ULONG) ImeSystemLangBarMenu::Release() {
  const LONG count = ::InterlockedDecrement(&reference_count_);
  if (count <= 0) {
    delete this;
    return 0;
  }
  return count;
}

// Implements the ITfLangBarItemButton::InitMenu() function.
// This function is called by Windows to create the button menu.
STDAPI ImeSystemLangBarMenu::InitMenu(ITfMenu* menu) {
  HRESULT result = S_OK;

  // Add the menu items of this object to the given ITfMenu object.
  for (size_t i = 0; i < menu_data_.size(); ++i) {
    const ImeLangBarMenuData* data = menu_data_.data(i);
    const UINT icon_id_for_theme =
        ImeLangBarMenu::CanContextMenuDisplay32bppIcon()
            ? data->icon_id_for_theme_
            : data->icon_id_for_non_theme_;
    CBitmap bitmap;
    CBitmap mask;
    // If LoadIconAsBitmap fails, |bitmap.m_hBitmap| and |mask.m_hBitmap|
    // remain nullptr bitmap handles.
    LoadIconAsBitmap(ImeGetResource(), data->icon_id_for_non_theme_,
                     icon_id_for_theme, &bitmap.m_hBitmap, &mask.m_hBitmap);
    result = menu->AddMenuItem(i, data->flags_, bitmap, mask, data->text_,
                               data->length_, nullptr);
    if (result != S_OK) {
      break;
    }
  }

  return result;
}

// Implements the ITfLangBarItemButton::OnMenuSelect() function.
// This function is called by Windows to notify a user selects an item in
// this button menu.
STDAPI ImeSystemLangBarMenu::OnMenuSelect(UINT menu_id) {
  // Call the LangBarCallback::OnMenuSelect() function to dispatch the
  // given event.
  ImeLangBarMenuData* data = menu_data_.data(menu_id);
  if (data == nullptr) {
    return E_INVALIDARG;
  }
  if (data->menu_id_ == LangBarCallback::kCancel) {
    return S_OK;
  }
  const HRESULT result = langbar_callback_->OnMenuSelect(
      static_cast<LangBarCallback::MenuId>(data->menu_id_));
  return S_OK;
}

// Initializes an ImeLangBarItem instance.
// This function is called by a text service to provide the information
// required for creating a menu button. A text service MUST call this function
// before calling the ITfLangBarItemMgr::AddItem() function and adding this
// button menu to a language bar.
HRESULT ImeSystemLangBarMenu::Init(HINSTANCE instance,
                                   const ImeLangBarMenuItem* menu, int count) {
  return menu_data_.Init(instance, menu, count);
}
