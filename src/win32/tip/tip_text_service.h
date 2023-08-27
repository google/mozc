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

#ifndef MOZC_WIN32_TIP_TIP_TEXT_SERVICE_H_
#define MOZC_WIN32_TIP_TIP_TEXT_SERVICE_H_

#include <msctf.h>
#include <unknwn.h>
#include <wil/com.h>
#include <windows.h>

#include <cstdint>

namespace mozc {
namespace win32 {
namespace tsf {

class TipPrivateContext;
class TipThreadContext;

class TipTextService : public IUnknown {
 public:
  // Retrieves the ID of the client application.
  virtual TfClientId GetClientID() const = 0;

  // Retrieves the thread manager instance.
  virtual ITfThreadMgr *GetThreadManager() const = 0;

  // Returns the associated private context. Returns nullptr if not found.
  virtual TipPrivateContext *GetPrivateContext(ITfContext *context) = 0;

  // Returns the associated thread context.
  virtual TipThreadContext *GetThreadContext() = 0;

  // Sends UI update message to the renderer.
  virtual void PostUIUpdateMessage() = 0;

  // Returns the GUID atom for the display attributes.
  virtual TfGuidAtom input_attribute() const = 0;
  virtual TfGuidAtom converted_attribute() const = 0;

  // Returns the window handle of render callback window.
  // Returns nullptr if it is not available.
  virtual HWND renderer_callback_window_handle() const = 0;

  // Returns an instance of ITfCompositionSink object.
  virtual wil::com_ptr_nothrow<ITfCompositionSink> CreateCompositionSink(
      ITfContext *context) = 0;

  // Updates the language bar as needed.  Does nothing if the language bar is
  // not available.
  virtual void UpdateLangbar(bool enabled, uint32_t mozc_mode) = 0;

  // Returns true if the language bar is initialized.
  virtual bool IsLangbarInitialized() const = 0;
};

class TipTextServiceFactory {
 public:
  TipTextServiceFactory() = delete;
  TipTextServiceFactory(const TipTextServiceFactory &) = delete;
  TipTextServiceFactory &operator=(const TipTextServiceFactory &) = delete;

  static wil::com_ptr_nothrow<TipTextService> Create();

  static bool OnDllProcessAttach(HINSTANCE module_handle, bool static_loading);
  static void OnDllProcessDetach(HINSTANCE module_handle,
                                 bool process_shutdown);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_TEXT_SERVICE_H_
