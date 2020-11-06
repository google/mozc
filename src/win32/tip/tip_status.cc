// Copyright 2010-2020, Google Inc.
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

#include "win32/tip/tip_status.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>

#include <string>

#include "base/logging.h"
#include "protocol/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/tip/tip_compartment_util.h"

using ATL::CComPtr;
using ATL::CComQIPtr;
using ATL::CComVariant;

namespace mozc {
namespace win32 {
namespace tsf {

bool TipStatus::IsOpen(ITfThreadMgr *thread_mgr) {
  CComVariant var;

  // Retrieve the compartment manager from the thread manager, which contains
  // the configuration of the owner thread.
  if (!TipCompartmentUtil::Get(thread_mgr, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE,
                               &var)) {
    return false;
  }
  // Open/Close compartment should be Int32 (I4).
  return var.vt == VT_I4 && var.lVal != FALSE;
}

bool TipStatus::IsDisabledContext(ITfContext *context) {
  CComVariant var;

  // Retrieve the compartment manager from the |context|, which contains the
  // configuration of this context.
  if (!TipCompartmentUtil::Get(context, GUID_COMPARTMENT_KEYBOARD_DISABLED,
                               &var)) {
    return false;
  }
  // Disabled compartment should be Int32 (I4).
  return var.vt == VT_I4 && var.lVal != FALSE;
}

bool TipStatus::IsEmptyContext(ITfContext *context) {
  CComVariant var;

  // Retrieve the compartment manager from the |context|, which contains the
  // configuration of this context.
  if (!TipCompartmentUtil::Get(context, GUID_COMPARTMENT_EMPTYCONTEXT, &var)) {
    return false;
  }
  // Empty context compartment should be Int32 (I4).
  return var.vt == VT_I4 && var.lVal != FALSE;
}

bool TipStatus::GetInputModeConversion(ITfThreadMgr *thread_mgr,
                                       TfClientId client_id, DWORD *mode) {
  if (mode == nullptr) {
    return false;
  }

  const DWORD kDefaultMode =
      TF_CONVERSIONMODE_NATIVE | TF_CONVERSIONMODE_FULLSHAPE;  // Hiragana
  CComVariant default_var;
  default_var.vt = VT_I4;
  default_var.lVal = kDefaultMode;

  CComVariant var;
  if (!TipCompartmentUtil::GetAndEnsureDataExists(
          thread_mgr, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION, client_id,
          default_var, &var)) {
    return false;
  }

  // Conversion mode compartment should be Int32 (I4).
  if (var.vt != VT_I4) {
    return false;
  }

  *mode = var.lVal;
  return true;
}

bool TipStatus::SetIMEOpen(ITfThreadMgr *thread_mgr, TfClientId client_id,
                           bool open) {
  CComVariant var;
  var.vt = VT_I4;
  var.lVal = open ? TRUE : FALSE;
  return TipCompartmentUtil::Set(
      thread_mgr, GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, client_id, var);
}

bool TipStatus::SetInputModeConversion(ITfThreadMgr *thread_mgr,
                                       DWORD client_id, DWORD native_mode) {
  CComVariant var;
  var.vt = VT_I4;
  var.lVal = native_mode;
  return TipCompartmentUtil::Set(thread_mgr,
                                 GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION,
                                 client_id, var);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
