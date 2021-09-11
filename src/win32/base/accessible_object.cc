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

#include "win32/base/accessible_object.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>

#include <string>

#include "base/util.h"

namespace mozc {
namespace win32 {
namespace {

using ::ATL::CComBSTR;
using ::ATL::CComPtr;
using ::ATL::CComQIPtr;
using ::ATL::CComVariant;

std::string UTF16ToUTF8(const std::wstring &str) {
  std::string utf8;
  Util::WideToUtf8(str, &utf8);
  return utf8;
}

std::string BSTRToUTF8(const BSTR &bstr) {
  if (bstr == nullptr) {
    return "";
  }
  return UTF16ToUTF8(std::wstring(bstr, ::SysStringLen(bstr)));
}

std::string RoleToString(const CComVariant &role) {
  if (role.vt == VT_I4) {
    switch (role.lVal) {
      case ROLE_SYSTEM_TITLEBAR:
        return "ROLE_SYSTEM_TITLEBAR";
      case ROLE_SYSTEM_MENUBAR:
        return "ROLE_SYSTEM_MENUBAR";
      case ROLE_SYSTEM_SCROLLBAR:
        return "ROLE_SYSTEM_SCROLLBAR";
      case ROLE_SYSTEM_GRIP:
        return "ROLE_SYSTEM_GRIP";
      case ROLE_SYSTEM_SOUND:
        return "ROLE_SYSTEM_SOUND";
      case ROLE_SYSTEM_CURSOR:
        return "ROLE_SYSTEM_CURSOR";
      case ROLE_SYSTEM_CARET:
        return "ROLE_SYSTEM_CARET";
      case ROLE_SYSTEM_ALERT:
        return "ROLE_SYSTEM_ALERT";
      case ROLE_SYSTEM_WINDOW:
        return "ROLE_SYSTEM_WINDOW";
      case ROLE_SYSTEM_CLIENT:
        return "ROLE_SYSTEM_CLIENT";
      case ROLE_SYSTEM_MENUPOPUP:
        return "ROLE_SYSTEM_MENUPOPUP";
      case ROLE_SYSTEM_MENUITEM:
        return "ROLE_SYSTEM_MENUITEM";
      case ROLE_SYSTEM_TOOLTIP:
        return "ROLE_SYSTEM_TOOLTIP";
      case ROLE_SYSTEM_APPLICATION:
        return "ROLE_SYSTEM_APPLICATION";
      case ROLE_SYSTEM_DOCUMENT:
        return "ROLE_SYSTEM_DOCUMENT";
      case ROLE_SYSTEM_PANE:
        return "ROLE_SYSTEM_PANE";
      case ROLE_SYSTEM_CHART:
        return "ROLE_SYSTEM_CHART";
      case ROLE_SYSTEM_DIALOG:
        return "ROLE_SYSTEM_DIALOG";
      case ROLE_SYSTEM_BORDER:
        return "ROLE_SYSTEM_BORDER";
      case ROLE_SYSTEM_GROUPING:
        return "ROLE_SYSTEM_GROUPING";
      case ROLE_SYSTEM_SEPARATOR:
        return "ROLE_SYSTEM_SEPARATOR";
      case ROLE_SYSTEM_TOOLBAR:
        return "ROLE_SYSTEM_TOOLBAR";
      case ROLE_SYSTEM_STATUSBAR:
        return "ROLE_SYSTEM_STATUSBAR";
      case ROLE_SYSTEM_TABLE:
        return "ROLE_SYSTEM_TABLE";
      case ROLE_SYSTEM_COLUMNHEADER:
        return "ROLE_SYSTEM_COLUMNHEADER";
      case ROLE_SYSTEM_ROWHEADER:
        return "ROLE_SYSTEM_ROWHEADER";
      case ROLE_SYSTEM_COLUMN:
        return "ROLE_SYSTEM_COLUMN";
      case ROLE_SYSTEM_ROW:
        return "ROLE_SYSTEM_ROW";
      case ROLE_SYSTEM_CELL:
        return "ROLE_SYSTEM_CEL";
      case ROLE_SYSTEM_LINK:
        return "ROLE_SYSTEM_LINK";
      case ROLE_SYSTEM_HELPBALLOON:
        return "ROLE_SYSTEM_HELPBALLOON";
      case ROLE_SYSTEM_CHARACTER:
        return "ROLE_SYSTEM_CHARACTER";
      case ROLE_SYSTEM_LIST:
        return "ROLE_SYSTEM_LIST";
      case ROLE_SYSTEM_LISTITEM:
        return "ROLE_SYSTEM_LISTITEM";
      case ROLE_SYSTEM_OUTLINE:
        return "ROLE_SYSTEM_OUTLINE";
      case ROLE_SYSTEM_OUTLINEITEM:
        return "ROLE_SYSTEM_OUTLINEITEM";
      case ROLE_SYSTEM_PAGETAB:
        return "ROLE_SYSTEM_PAGETAB";
      case ROLE_SYSTEM_PROPERTYPAGE:
        return "ROLE_SYSTEM_PROPERTYPAGE";
      case ROLE_SYSTEM_INDICATOR:
        return "ROLE_SYSTEM_INDICATOR";
      case ROLE_SYSTEM_GRAPHIC:
        return "ROLE_SYSTEM_GRAPHIC";
      case ROLE_SYSTEM_STATICTEXT:
        return "ROLE_SYSTEM_STATICTEXT";
      case ROLE_SYSTEM_TEXT:
        return "ROLE_SYSTEM_TEXT";
      case ROLE_SYSTEM_PUSHBUTTON:
        return "ROLE_SYSTEM_PUSHBUTTON";
      case ROLE_SYSTEM_CHECKBUTTON:
        return "ROLE_SYSTEM_CHECKBUTTON";
      case ROLE_SYSTEM_RADIOBUTTON:
        return "ROLE_SYSTEM_RADIOBUTTON";
      case ROLE_SYSTEM_COMBOBOX:
        return "ROLE_SYSTEM_COMBOBOX";
      case ROLE_SYSTEM_DROPLIST:
        return "ROLE_SYSTEM_DROPLIST";
      case ROLE_SYSTEM_PROGRESSBAR:
        return "ROLE_SYSTEM_PROGRESSBAR";
      case ROLE_SYSTEM_DIAL:
        return "ROLE_SYSTEM_DIAL";
      case ROLE_SYSTEM_HOTKEYFIELD:
        return "ROLE_SYSTEM_HOTKEYFIELD";
      case ROLE_SYSTEM_SLIDER:
        return "ROLE_SYSTEM_SLIDER";
      case ROLE_SYSTEM_SPINBUTTON:
        return "ROLE_SYSTEM_SPINBUTTON";
      case ROLE_SYSTEM_DIAGRAM:
        return "ROLE_SYSTEM_DIAGRAM";
      case ROLE_SYSTEM_ANIMATION:
        return "ROLE_SYSTEM_ANIMATION";
      case ROLE_SYSTEM_EQUATION:
        return "ROLE_SYSTEM_EQUATION";
      case ROLE_SYSTEM_BUTTONDROPDOWN:
        return "ROLE_SYSTEM_BUTTONDROPDOWN";
      case ROLE_SYSTEM_BUTTONMENU:
        return "ROLE_SYSTEM_BUTTONMENU";
      case ROLE_SYSTEM_BUTTONDROPDOWNGRID:
        return "ROLE_SYSTEM_BUTTONDROPDOWNGRID";
      case ROLE_SYSTEM_WHITESPACE:
        return "ROLE_SYSTEM_WHITESPACE";
      case ROLE_SYSTEM_PAGETABLIST:
        return "ROLE_SYSTEM_PAGETABLIST";
      case ROLE_SYSTEM_CLOCK:
        return "ROLE_SYSTEM_CLOCK";
      case ROLE_SYSTEM_SPLITBUTTON:
        return "ROLE_SYSTEM_SPLITBUTTON";
      case ROLE_SYSTEM_IPADDRESS:
        return "ROLE_SYSTEM_IPADDRESS";
      case ROLE_SYSTEM_OUTLINEBUTTON:
        return "ROLE_SYSTEM_OUTLINEBUTTON";
      default:
        return "";
    }
  } else if (role.vt == VT_BSTR) {
    return BSTRToUTF8(role.bstrVal);
  } else {
    return "";
  }
}

AccessibleObjectInfo GetInfo(const ATL::CComVariant &role,
                             const std::string &name,
                             const std::string &value) {
  AccessibleObjectInfo info;
  info.role = RoleToString(role);
  info.is_builtin_role = (role.vt == VT_I4);
  info.name = name;
  info.value = value;
  return info;
}

CComVariant GetChildId(int32 child_id) {
  CComVariant variant;
  variant.vt = VT_I4;
  variant.lVal = child_id;
  return variant;
}

}  // namespace

AccessibleObject::AccessibleObject() : child_id_(CHILDID_SELF), valid_(false) {}

AccessibleObject::AccessibleObject(CComPtr<IAccessible> container)
    : container_(container),
      child_id_(CHILDID_SELF),
      valid_(container != nullptr) {}

AccessibleObject::AccessibleObject(CComPtr<IAccessible> container,
                                   int32 child_id)
    : container_(container),
      child_id_(child_id),
      valid_(container != nullptr) {}

AccessibleObjectInfo AccessibleObject::GetInfo() const {
  AccessibleObjectInfo info;
  const auto child = GetChildId(child_id_);
  {
    CComVariant role;
    if (SUCCEEDED(container_->get_accRole(child, &role))) {
      info.is_builtin_role = (role.vt == VT_I4);
      info.role = RoleToString(role);
    }
  }
  {
    CComBSTR bstr;
    if (SUCCEEDED(container_->get_accName(child, &bstr))) {
      info.name = BSTRToUTF8(bstr);
    }
  }
  {
    CComBSTR bstr;
    if (SUCCEEDED(container_->get_accValue(child, &bstr))) {
      info.value = BSTRToUTF8(bstr);
    }
  }
  return info;
}

std::vector<AccessibleObject> AccessibleObject::GetChildren() const {
  std::vector<AccessibleObject> result;
  if (!container_) {
    result;
  }
  LONG num_children = 0;
  if (FAILED(container_->get_accChildCount(&num_children))) {
    return result;
  }
  if (num_children == 0) {
    return result;
  }

  std::vector<CComVariant> buffer;
  buffer.resize(num_children);

  LONG num_fetched = 0;
  if (FAILED(::AccessibleChildren(container_, 0, num_children, &buffer[0],
                                  &num_fetched))) {
    return result;
  }
  buffer.resize(num_fetched);

  for (size_t i = 0; i < num_fetched; ++i) {
    const auto &element = buffer[i];
    if (element.vt == VT_DISPATCH) {
      CComQIPtr<IAccessible> accesible(element.pdispVal);
      if (accesible != nullptr) {
        result.push_back(AccessibleObject(accesible));
      }
    } else if (element.vt == VT_I4) {
      result.push_back(AccessibleObject(container_, element.lVal));
    } else {
      // not supported.
    }
  }
  return result;
}

AccessibleObject AccessibleObject::GetParent() const {
  if (child_id_ != CHILDID_SELF) {
    // not supported.
    return AccessibleObject();
  }

  CComPtr<IDispatch> dispatch;
  if (FAILED(container_->get_accParent(&dispatch))) {
    return AccessibleObject();
  }

  return AccessibleObject(CComQIPtr<IAccessible>(dispatch));
}

AccessibleObject AccessibleObject::GetFocus() const {
  CComVariant variant;
  if (FAILED(container_->get_accFocus(&variant))) {
    return AccessibleObject();
  }
  if (variant.vt == VT_I4) {
    return AccessibleObject(container_, variant.lVal);
  } else if (variant.vt == VT_DISPATCH) {
    return AccessibleObject(CComQIPtr<IAccessible>(variant.pdispVal));
  }
  return AccessibleObject();
}

bool AccessibleObject::GetWindowHandle(HWND *window_handle) const {
  if (window_handle == nullptr) {
    return false;
  }
  *window_handle = nullptr;
  if (child_id_ != CHILDID_SELF) {
    // not supported.
    return false;
  }

  return SUCCEEDED(::WindowFromAccessibleObject(container_, window_handle));
}

bool AccessibleObject::GetProcessId(DWORD *process_id) const {
  if (process_id == nullptr) {
    return false;
  }
  *process_id = 0;

  HWND window_handle = nullptr;
  if (!GetWindowHandle(&window_handle)) {
    return false;
  }
  if (::GetWindowThreadProcessId(window_handle, process_id) == 0) {
    return false;
  }
  return true;
}

bool AccessibleObject::IsValid() const { return valid_; }

// static
AccessibleObject AccessibleObject::FromWindow(HWND window_handle) {
  if (::IsWindow(window_handle) == FALSE) {
    return AccessibleObject();
  }
  CComPtr<IAccessible> accesible;
  if (FAILED(::AccessibleObjectFromWindow(
          window_handle, OBJID_WINDOW, __uuidof(IAccessible),
          reinterpret_cast<void **>(&accesible)))) {
    return AccessibleObject();
  }

  return AccessibleObject(accesible);
}

}  // namespace win32
}  // namespace mozc
