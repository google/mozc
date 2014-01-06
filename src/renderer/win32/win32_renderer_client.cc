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

#include "renderer/win32/win32_renderer_client.h"

#include "base/logging.h"
#include "base/mutex.h"
#include "base/scoped_handle.h"
#include "base/system_util.h"
#include "base/util.h"
#include "renderer/renderer_client.h"
#include "renderer/renderer_command.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {

namespace {
using ::mozc::commands::RendererCommand;

class SenderThread;
SenderThread *g_sender_thread = nullptr;

// Used to lock |g_sender_thread|.
Mutex *g_mutex = nullptr;

// Represents the module handle of this module.
volatile HMODULE g_module = nullptr;

// True if the the DLL received DLL_PROCESS_DETACH notification.
volatile bool g_module_unloaded = false;

// Represents the number of UI threads that are recognized by this module.
volatile LONG g_ui_thread_count = 0;

// Thread Local Storage (TLS) index to specify the current UI thread is
// initialized or not. if ::GetTlsValue(g_tls_index) returns non-zero
// value, the current thread is initialized.
volatile DWORD g_tls_index = TLS_OUT_OF_INDEXES;

class SenderThread {
 public:
  SenderThread(HANDLE command_event, HANDLE quit_event)
    : command_event_(command_event),
      quit_event_(quit_event) {
  }

  void RequestQuit() {
    ::SetEvent(quit_event_.get());
  }

  void UpdateCommand(const RendererCommand &new_command) {
    scoped_lock lock(&mutex_);
    renderer_command_.CopyFrom(new_command);
    ::SetEvent(command_event_.get());
  }

  void RenderLoop() {
    // Wait until desktop name is ready. b/10403163
    while (SystemUtil::GetDesktopNameAsString().empty()) {
      const DWORD wait_result = ::WaitForSingleObject(quit_event_.get(), 500);
      const DWORD wait_error = ::GetLastError();
      if (wait_result == WAIT_OBJECT_0) {
        return;
      }
      if (wait_result == WAIT_TIMEOUT) {
        continue;
      }
      LOG(ERROR) << "Unknown result: " << wait_result
                 << ", error: " << wait_error;
      return;
    }

    mozc::renderer::RendererClient renderer_client;
    while (true) {
      const HANDLE handles[] = {quit_event_.get(), command_event_.get()};
      const DWORD wait_result = ::WaitForMultipleObjects(
          arraysize(handles), handles, FALSE, INFINITE);
      const DWORD wait_error = ::GetLastError();
      if (g_module_unloaded) {
        break;
      }
      const DWORD kQuitEventSignaled = WAIT_OBJECT_0;
      const DWORD kRendererEventSignaled = WAIT_OBJECT_0 + 1;
      if (wait_result == kQuitEventSignaled) {
        // handles[0], that is, quit event is signaled.
        break;
      }
      if (wait_result != kRendererEventSignaled) {
        LOG(ERROR) << "WaitForMultipleObjects failed. error: " << wait_error;
        break;
      }
      // handles[1], that is, renderer event is signaled.
      RendererCommand command;
      {
        scoped_lock lock(&mutex_);
        command.Swap(&renderer_command_);
        ::ResetEvent(command_event_.get());
      }
      if (!renderer_client.ExecCommand(command)) {
        DLOG(ERROR) << "RendererClient::ExecCommand failed.";
      }
    }
  }

 private:
  ScopedHandle command_event_;
  ScopedHandle quit_event_;
  RendererCommand renderer_command_;
  Mutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(SenderThread);
};

static DWORD WINAPI ThreadProc(void * /*unused*/) {
  SenderThread *thread = nullptr;
  {
    scoped_lock lock(g_mutex);
    thread = g_sender_thread;
  }
  if (thread != nullptr) {
    thread->RenderLoop();
    delete thread;
  }
  ::FreeLibraryAndExitThread(g_module, 0);
}

SenderThread *CreateSenderThread() {
  // Here we directly use CreateThread API rather than _beginthreadex because
  // the command sender thread will be eventually terminated via
  // FreeLibraryAndExitThread API. Regarding CRT memory resources, this will
  // be OK because this code will be running as DLL and the CRT can manage
  // thread specific resources through attach/detach notification in DllMain.
  DWORD thread_id = 0;
  ScopedHandle thread_handle(::CreateThread(
      nullptr, 0, ThreadProc, nullptr, CREATE_SUSPENDED, &thread_id));
  if (thread_handle.get() == nullptr) {
    // Failed to create the thread. Restore the reference count of the DLL.
    return nullptr;
  }

  // Increment the reference count of the IME DLL so that the DLL will not
  // be unloaded while the sender thread is running. The reference count
  // will be decremented by FreeLibraryAndExitThread API in the thread routine.
  HMODULE loaded_module = nullptr;
  if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<const wchar_t *>(g_module),
                           &loaded_module) == FALSE) {
    ::TerminateThread(thread_handle.get(), 0);
    return nullptr;
  }
  if (loaded_module != g_module) {
    ::TerminateThread(thread_handle.get(), 0);
    ::FreeLibrary(loaded_module);
    return nullptr;
  }

  // Create shared objects. We use manual reset events for simplicity.
  ScopedHandle command_event(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
  ScopedHandle quit_event(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
  if ((command_event.get() == nullptr) || (quit_event.get() == nullptr)) {
    ::TerminateThread(thread_handle.get(), 0);
    return nullptr;
  }

  scoped_ptr<SenderThread> thread(new SenderThread(
      command_event.take(), quit_event.take()));

  // Resume the thread.
  if (::ResumeThread(thread_handle.get()) == -1) {
    ::TerminateThread(thread_handle.get(), 0);
    ::FreeLibrary(loaded_module);
    return nullptr;
  }

  return thread.release();
}

bool CanIgnoreRequest(const RendererCommand &command) {
  if (g_module_unloaded) {
    return true;
  }
  if (g_tls_index == TLS_OUT_OF_INDEXES) {
    return true;
  }
  if ((::TlsGetValue(g_tls_index) == nullptr) &&
      !command.visible()) {
    // The sender threaed is not initialized and |command| is to hide the
    // renderer. We are likely to be able to skip this request.
    return true;
  }
  return false;
}

// Returns true when the required initialization is finished successfully.
bool EnsureUIThreadInitialized() {
  if (g_module_unloaded) {
    return false;
  }
  if (g_tls_index == TLS_OUT_OF_INDEXES) {
    return false;
  }
  if (::TlsGetValue(g_tls_index) != nullptr) {
    // already initialized.
    return true;
  }
  {
    scoped_lock lock(g_mutex);
    ++g_ui_thread_count;
    if (g_ui_thread_count == 1) {
      g_sender_thread = CreateSenderThread();
    }
  }
  // Mark this thread is initialized.
  ::TlsSetValue(g_tls_index, reinterpret_cast<void *>(1));
  return true;
}

}  // namespace

void Win32RendererClient::OnModuleLoaded(HMODULE module_handle) {
  g_module = module_handle;
  g_mutex = new Mutex();
  g_tls_index = ::TlsAlloc();
}

void Win32RendererClient::OnModuleUnloaded() {
  if (g_tls_index != TLS_OUT_OF_INDEXES) {
    ::TlsFree(g_tls_index);
  }
  delete g_mutex;
  g_module_unloaded = true;
  g_module = nullptr;
}

void Win32RendererClient::OnUIThreadUninitialized() {
  if (g_module_unloaded) {
    return;
  }
  if (g_tls_index == TLS_OUT_OF_INDEXES) {
    return;
  }
  if (::TlsGetValue(g_tls_index) == nullptr) {
    // Do nothing because this thread did not increment |g_ui_thread_count|.
    return;
  }
  {
    scoped_lock lock(g_mutex);
    if (g_ui_thread_count > 0) {
      --g_ui_thread_count;
      if (g_ui_thread_count == 0 && g_sender_thread != nullptr) {
        g_sender_thread->RequestQuit();
        g_sender_thread = nullptr;
      }
    }
  }
  // Mark this thread is uninitialized.
  ::TlsSetValue(g_tls_index, nullptr);
}

void Win32RendererClient::OnUpdated(const RendererCommand &command) {
  if (CanIgnoreRequest(command)) {
    return;
  }
  if (!EnsureUIThreadInitialized()) {
    return;
  }
  SenderThread *thread = nullptr;
  {
    scoped_lock lock(g_mutex);
    thread = g_sender_thread;
  }
  if (thread != nullptr) {
    thread->UpdateCommand(command);
  }
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
