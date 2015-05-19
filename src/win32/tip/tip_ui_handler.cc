// Copyright 2010-2013, Google Inc.
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

#include "win32/tip/tip_ui_handler.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_ui_handler_conventional.h"
#include "win32/tip/tip_ui_handler_immersive.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

// This flag is availabe in Windows SDK 8.0 and later.
#ifndef TF_TMF_IMMERSIVEMODE
#define TF_TMF_IMMERSIVEMODE  0x40000000
#endif  // !TF_TMF_IMMERSIVEMODE

bool IsImmersiveUI(TipTextService *text_service) {
  return (text_service->activate_flags() & TF_TMF_IMMERSIVEMODE)  ==
         TF_TMF_IMMERSIVEMODE;
}

}  // namespace

ITfUIElement *TipUiHandler::CreateUI(UiType type,
                                     TipTextService *text_service,
                                     ITfContext *context) {
  if (IsImmersiveUI(text_service)) {
    return TipUiHandlerImmersive::CreateUI(
        type, text_service, context);
  } else {
    return TipUiHandlerConventional::CreateUI(
        type, text_service, context);
  }
}

void TipUiHandler::OnDestroyElement(TipTextService *text_service,
                                    ITfUIElement *element) {
  if (IsImmersiveUI(text_service)) {
    TipUiHandlerImmersive::OnDestroyElement(element);
  } else {
    TipUiHandlerConventional::OnDestroyElement(element);
  }
}

void TipUiHandler::OnActivate(TipTextService *text_service) {
  if (IsImmersiveUI(text_service)) {
    TipUiHandlerImmersive::OnActivate();
  } else {
    TipUiHandlerConventional::OnActivate();
  }
}

void TipUiHandler::OnDeactivate(TipTextService *text_service) {
  if (IsImmersiveUI(text_service)) {
    TipUiHandlerImmersive::OnDeactivate();
  } else {
    TipUiHandlerConventional::OnDeactivate();
  }
}

void TipUiHandler::OnFocusChange(TipTextService *text_service,
                                 ITfDocumentMgr *focused_document_manager) {
  if (IsImmersiveUI(text_service)) {
    TipUiHandlerImmersive::OnFocusChange(text_service,
                                         focused_document_manager);
  } else {
    TipUiHandlerConventional::OnFocusChange(text_service,
                                            focused_document_manager);
  }
}

bool TipUiHandler::OnLayoutChange(TipTextService *text_service,
                                  ITfContext *context,
                                  TfLayoutCode layout_code,
                                  ITfContextView *context_view) {
  if (IsImmersiveUI(text_service)) {
    return TipUiHandlerImmersive::OnLayoutChange(text_service, context,
                                                 layout_code, context_view);
  } else {
    return TipUiHandlerConventional::OnLayoutChange(text_service, context,
                                                    layout_code, context_view);
  }
}

bool TipUiHandler::Update(TipTextService *text_service,
                          ITfContext *context,
                          TfEditCookie read_cookie) {
  if (IsImmersiveUI(text_service)) {
    return TipUiHandlerImmersive::Update(text_service, context, read_cookie);
  } else {
    return TipUiHandlerConventional::Update(text_service, context,
                                            read_cookie);
  }
}

bool TipUiHandler::OnDllProcessAttach(HINSTANCE module_handle,
                                      bool static_loading) {
  // In DllMain, we must not call functions exported by user32.dll, which means
  // that we cannot determine if the current process is immersive mode or not.
  // So we call both initializer here.
  TipUiHandlerConventional::OnDllProcessAttach(
      module_handle, static_loading);
  TipUiHandlerImmersive::OnDllProcessAttach(
      module_handle, static_loading);
   return true;
}

void TipUiHandler::OnDllProcessDetach(HINSTANCE module_handle,
                                      bool process_shutdown) {
  // In DllMain, we must not call functions exported by user32.dll, which means
  // that we cannot determine if the current process is immersive mode or not.
  // So we call both uninitializer here.
  TipUiHandlerConventional::OnDllProcessDetach(
      module_handle, process_shutdown);
  TipUiHandlerImmersive::OnDllProcessDetach(
      module_handle, process_shutdown);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
