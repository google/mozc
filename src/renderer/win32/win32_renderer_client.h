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

#ifndef MOZC_RENDERER_WIN32_WIN32_RENDERER_CLIENT_H_
#define MOZC_RENDERER_WIN32_WIN32_RENDERER_CLIENT_H_

#include <windows.h>

#include "protocol/renderer_command.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {

// A (virtually) non-blocking library that transfers renderer commands to the
// renderer process by using a background thread. These functions are designed
// to be used in a dynamic link libraries (DLLs).
class Win32RendererClient {
 public:
  Win32RendererClient() = delete;
  Win32RendererClient(const Win32RendererClient&) = delete;
  Win32RendererClient& operator=(const Win32RendererClient&) = delete;
  // Must be called when the DLL is loaded.
  static void OnModuleLoaded(HMODULE module_handle);
  // Must be called when the DLL is unloaded.
  static void OnModuleUnloaded();
  // Must be called when a UI thread is about to be shut down.
  static void OnUIThreadUninitialized();
  // Passes the |command| to the renderer. This function returns
  // asynchronously and only the last call will be used.
  static void OnUpdated(const commands::RendererCommand& command);
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_WIN32_RENDERER_CLIENT_H_
