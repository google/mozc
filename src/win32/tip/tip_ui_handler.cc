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

#include "win32/tip/tip_ui_handler.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>
#include <msctf.h>
#include <wrl/client.h>

#include <cstdint>

#include "protocol/commands.pb.h"
#include "win32/tip/tip_input_mode_manager.h"
#include "win32/tip/tip_status.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_thread_context.h"
#include "win32/tip/tip_ui_handler_conventional.h"
#include "win32/tip/tip_ui_handler_immersive.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using Microsoft::WRL::ComPtr;
using mozc::commands::CompositionMode;

void UpdateLanguageBarOnFocusChange(TipTextService *text_service,
                                    ITfDocumentMgr *document_manager) {
  if (!text_service) {
    return;
  }

  if (!text_service->IsLangbarInitialized()) {
    // If language bar is not initialized, there is nothing to do here.
    return;
  }

  HRESULT result = S_OK;
  ITfThreadMgr *thread_manager = text_service->GetThreadManager();

  if (thread_manager == nullptr) {
    return;
  }

  bool disabled = false;
  {
    if (document_manager == nullptr) {
      // When |document_manager| is null, we should show "disabled" icon
      // as if |ImmAssociateContext(window_handle, nullptr)| was called.
      disabled = true;
    } else {
      ComPtr<ITfContext> context;
      result = document_manager->GetTop(&context);
      if (SUCCEEDED(result)) {
        disabled = TipStatus::IsDisabledContext(context);
      }
    }
  }

  const TipInputModeManager *input_mode_manager =
      text_service->GetThreadContext()->GetInputModeManager();
  const bool open = input_mode_manager->GetEffectiveOpenClose();
  const CompositionMode mozc_mode =
      open ? static_cast<CompositionMode>(
                 input_mode_manager->GetEffectiveConversionMode())
           : commands::DIRECT;
  text_service->UpdateLangbar(!disabled, static_cast<uint32_t>(mozc_mode));
}

bool UpdateInternal(TipTextService *text_service, ITfContext *context,
                    TfEditCookie read_cookie) {
  if (text_service->IsImmersiveUI()) {
    return TipUiHandlerImmersive::Update(text_service, context, read_cookie);
  } else {
    return TipUiHandlerConventional::Update(text_service, context, read_cookie);
  }
}

}  // namespace

ComPtr<ITfUIElement> TipUiHandler::CreateUI(
    UiType type, const ComPtr<TipTextService> &text_service,
    const ComPtr<ITfContext> &context) {
  if (text_service->IsImmersiveUI()) {
    return TipUiHandlerImmersive::CreateUI(type, text_service, context);
  } else {
    return TipUiHandlerConventional::CreateUI(type, text_service, context);
  }
}

void TipUiHandler::OnDestroyElement(const ComPtr<TipTextService> &text_service,
                                    const ComPtr<ITfUIElement> &element) {
  if (text_service->IsImmersiveUI()) {
    TipUiHandlerImmersive::OnDestroyElement(element);
  } else {
    TipUiHandlerConventional::OnDestroyElement(element);
  }
}

void TipUiHandler::OnActivate(TipTextService *text_service) {
  if (text_service->IsImmersiveUI()) {
    TipUiHandlerImmersive::OnActivate();
  } else {
    TipUiHandlerConventional::OnActivate(text_service);
  }
}

void TipUiHandler::OnDeactivate(TipTextService *text_service) {
  if (text_service->IsImmersiveUI()) {
    TipUiHandlerImmersive::OnDeactivate();
  } else {
    TipUiHandlerConventional::OnDeactivate();
  }
}

void TipUiHandler::OnDocumentMgrChanged(TipTextService *text_service,
                                        ITfDocumentMgr *document_manager) {
  UpdateLanguageBarOnFocusChange(text_service, document_manager);
}

void TipUiHandler::OnFocusChange(TipTextService *text_service,
                                 ITfDocumentMgr *focused_document_manager) {
  if (text_service->IsImmersiveUI()) {
    TipUiHandlerImmersive::OnFocusChange(text_service,
                                         focused_document_manager);
  } else {
    TipUiHandlerConventional::OnFocusChange(text_service,
                                            focused_document_manager);
  }
  UpdateLanguageBarOnFocusChange(text_service, focused_document_manager);
}

bool TipUiHandler::Update(TipTextService *text_service, ITfContext *context,
                          TfEditCookie read_cookie) {
  const TipInputModeManager *input_mode_manager =
      text_service->GetThreadContext()->GetInputModeManager();
  const bool open = input_mode_manager->GetEffectiveOpenClose();
  const CompositionMode mozc_mode = static_cast<CompositionMode>(
      input_mode_manager->GetEffectiveConversionMode());
  const bool result = UpdateInternal(text_service, context, read_cookie);
  if (open) {
    text_service->UpdateLangbar(true, mozc_mode);
  } else {
    text_service->UpdateLangbar(true, commands::DIRECT);
  }
  return result;
}

bool TipUiHandler::OnDllProcessAttach(HINSTANCE module_handle,
                                      bool static_loading) {
  // In DllMain, we must not call functions exported by user32.dll, which means
  // that we cannot determine if the current process is immersive mode or not.
  // So we call both initializer here.
  TipUiHandlerConventional::OnDllProcessAttach(module_handle, static_loading);
  TipUiHandlerImmersive::OnDllProcessAttach(module_handle, static_loading);
  return true;
}

void TipUiHandler::OnDllProcessDetach(HINSTANCE module_handle,
                                      bool process_shutdown) {
  // In DllMain, we must not call functions exported by user32.dll, which means
  // that we cannot determine if the current process is immersive mode or not.
  // So we call both uninitializers here.
  TipUiHandlerConventional::OnDllProcessDetach(module_handle, process_shutdown);
  TipUiHandlerImmersive::OnDllProcessDetach(module_handle, process_shutdown);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
