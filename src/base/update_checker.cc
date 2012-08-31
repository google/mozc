// Copyright 2010-2012, Google Inc.
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

#include "base/update_checker.h"

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <tchar.h>
#include <strsafe.h>
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>

#include <string>

#include "base/scoped_handle.h"
#include "base/win_util.h"
#endif  // OS_WINDOWS

using namespace std;

namespace mozc {
namespace {
#if defined(OS_WINDOWS) && defined(GOOGLE_JAPANESE_INPUT_BUILD)
// This GUID is specific to Google Japanese Input.
// Do not reuse this GUID for OSS Mozc.
const wchar_t kOmahaGUID[] = L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";

// Based on "google_update_idl.idl"
// http://code.google.com/p/omaha/source/browse/trunk/goopdate/google_update_idl.idl
enum CompletionCodes {
  COMPLETION_CODE_SUCCESS = 1,
  COMPLETION_CODE_SUCCESS_CLOSE_UI,
  COMPLETION_CODE_ERROR,
  COMPLETION_CODE_RESTART_ALL_BROWSERS,
  COMPLETION_CODE_REBOOT,
  COMPLETION_CODE_RESTART_BROWSER,
  COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY,
  COMPLETION_CODE_REBOOT_NOTICE_ONLY,
  COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY,
  COMPLETION_CODE_RUN_COMMAND,
};

// "GoogleUpdate UI-specific events Interface"
// ------
// Based on "google_update_idl.idl"
// http://code.google.com/p/omaha/source/browse/trunk/goopdate/google_update_idl.idl
MIDL_INTERFACE("1C642CED-CA3B-4013-A9DF-CA6CE5FF6503")
IProgressWndEvents : public IUnknown {
 public:
  // The UI is closing down. The user has clicked on either the "X" or the
  // other buttons of the UI to close the window.
  virtual HRESULT STDMETHODCALLTYPE DoClose() = 0;
  // Pause has been clicked on.
  virtual HRESULT STDMETHODCALLTYPE DoPause() = 0;
  // Resume has been clicked on.
  virtual HRESULT STDMETHODCALLTYPE DoResume() = 0;
  // RestartBrowsers button has been clicked on.
  virtual HRESULT STDMETHODCALLTYPE DoRestartBrowsers() = 0;
  // Reboot button has been clicked on.
  virtual HRESULT STDMETHODCALLTYPE DoReboot() = 0;
  // Launch Browser.
  virtual HRESULT STDMETHODCALLTYPE DoLaunchBrowser(
  /* [string][in] */ const WCHAR *url) = 0;
};

  // Based on "google_update_idl.idl"
  // http://code.google.com/p/omaha/source/browse/trunk/goopdate/google_update_idl.idl
  MIDL_INTERFACE("49D7563B-2DDB-4831-88C8-768A53833837")
IJobObserver : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE OnShow() = 0;
  virtual HRESULT STDMETHODCALLTYPE OnCheckingForUpdate() = 0;
  virtual HRESULT STDMETHODCALLTYPE OnUpdateAvailable(
    const WCHAR *version_string) = 0;  // [string][in]
  virtual HRESULT STDMETHODCALLTYPE OnWaitingToDownload() = 0;
  virtual HRESULT STDMETHODCALLTYPE OnDownloading(
    int time_remaining_ms,             // [in]
    int pos) = 0;                      // [in]
  virtual HRESULT STDMETHODCALLTYPE OnWaitingToInstall() = 0;
  virtual HRESULT STDMETHODCALLTYPE OnInstalling() = 0;
  virtual HRESULT STDMETHODCALLTYPE OnPause() = 0;
  virtual HRESULT STDMETHODCALLTYPE OnComplete(
    CompletionCodes code,              // [in]
    const WCHAR *reserved) = 0;        // [string][in]
  virtual HRESULT STDMETHODCALLTYPE SetEventSink(
    IProgressWndEvents *ui_sink) = 0;  // [in]
};

// Based on "google_update_idl.idl"
// http://code.google.com/p/omaha/source/browse/trunk/goopdate/google_update_idl.idl
MIDL_INTERFACE("31AC3F11-E5EA-4a85-8A3D-8E095A39C27B")
IGoogleUpdate : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE CheckForUpdate(
    const WCHAR *guid,                  // [string][in]
    IJobObserver *observer) = 0;        // [in]
  virtual HRESULT STDMETHODCALLTYPE Update(
    const WCHAR *guid,                  // [string][in]
    IJobObserver *observer) = 0;        // [in]
};

// OnDemand updates for per-user applications.
// ----
// Based on "google_update_idl.idl"
// http://code.google.com/p/omaha/source/browse/trunk/goopdate/google_update_idl.idl
// {6F8BD55B-E83D-4a47-85BE-81FFA8057A69};
const CLSID CLSID_OnDemandMachineAppsClass =
    {0x6F8BD55B, 0xE83D, 0x4a47,
        {0x85, 0xBE, 0x81, 0xFF, 0xA8, 0x05, 0x7A, 0x69}
    };

class UpdateCheckJob
  : public IJobObserver {
 public:
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(
      REFIID riid, void **ppvObject) {
    if (::IsEqualGUID(riid, _ATL_IIDOF(IJobObserver))) {
      *ppvObject = static_cast<IJobObserver *>(this);
      this->AddRef();
      return S_OK;
    }
    if (::IsEqualGUID(riid, IID_IUnknown)) {
      *ppvObject = static_cast<IUnknown *>(this);
      this->AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef() {
    return ::InterlockedIncrement(&reference_count_);
  }

  virtual ULONG STDMETHODCALLTYPE Release() {
    const ULONG count = ::InterlockedDecrement(&reference_count_);
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHOD(OnShow)() {
    return S_OK;
  }
  STDMETHOD(OnCheckingForUpdate)() {
    upgrade_check_started_ = true;
    return S_OK;
  }
  STDMETHOD(OnUpdateAvailable)(const TCHAR* version_string) {
    new_version_available_ = true;
    return S_OK;
  }
  STDMETHOD(OnWaitingToDownload)() {
    return S_OK;
  }
  STDMETHOD(OnDownloading)(int time_remaining_ms, int pos) {
    return S_OK;
  }
  STDMETHOD(OnWaitingToInstall)() {
    return S_OK;
  }
  STDMETHOD(OnInstalling)() {
    return S_OK;
  }
  STDMETHOD(OnPause)() {
    return S_OK;
  }
  STDMETHOD(OnComplete)(CompletionCodes code, const TCHAR* text) {
    progress_events_ = NULL;
    UpdateChecker::CallbackWParam wParam = UpdateChecker::UPGRADE_ERROR;
    switch (code) {
      case COMPLETION_CODE_SUCCESS:
      case COMPLETION_CODE_SUCCESS_CLOSE_UI:
      case COMPLETION_CODE_REBOOT:
      case COMPLETION_CODE_REBOOT_NOTICE_ONLY:
      case COMPLETION_CODE_RESTART_ALL_BROWSERS:
      case COMPLETION_CODE_RESTART_BROWSER:
      case COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
      case COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
      case COMPLETION_CODE_RUN_COMMAND:
        if (upgrade_check_started_) {
          if (new_version_available_) {
            wParam = UpdateChecker::UPGRADE_IS_AVAILABLE;
          } else {
            wParam = UpdateChecker::UPGRADE_ALREADY_UP_TO_DATE;
          }
        }
        break;
      case COMPLETION_CODE_ERROR:
      default:
        wParam = UpdateChecker::UPGRADE_ERROR;
        break;
    }
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, wParam, 0);

    // Stop the message-loop.
    ::PostQuitMessage(0);
    return S_OK;
  }

  STDMETHOD(SetEventSink)(IProgressWndEvents* event_sink) {
    progress_events_ = event_sink;
    return S_OK;
  }

  static UpdateCheckJob *CreateInstance(
      const UpdateChecker::CallbackInfo &info) {
    return new UpdateCheckJob(info);
  }

 private:
  explicit UpdateCheckJob(const UpdateChecker::CallbackInfo &info)
    : reference_count_(0),
      upgrade_check_started_(false),
      new_version_available_(false),
      callback_(info)
  {}

  volatile ULONG reference_count_;
  bool upgrade_check_started_;
  bool new_version_available_;

  const UpdateChecker::CallbackInfo callback_;

  CComPtr<IProgressWndEvents> progress_events_;
};

unsigned int __stdcall UpdateCheckWinThread(LPVOID lpThreadParameter) {
  UpdateChecker::CallbackInfo *copied_data =
      reinterpret_cast<UpdateChecker::CallbackInfo *>(lpThreadParameter);
  const UpdateChecker::CallbackInfo info = *copied_data;
  delete copied_data;
  copied_data = NULL;

  ScopedCOMInitializer com_init;

  CComPtr<IJobObserver> job_observer(UpdateCheckJob::CreateInstance(info));

  HRESULT hr = S_OK;

  CComPtr<IGoogleUpdate> google_update;
  hr = google_update.CoCreateInstance(CLSID_OnDemandMachineAppsClass);

  if (hr != S_OK) {
    return 0;
  }

  hr = google_update->CheckForUpdate(kOmahaGUID, job_observer);

  // Message loop is required to drive COM RPC.  This loop will be quitted
  // by PostQuitMessage API from UpdateCheckJob::OnComplete.
  MSG msg = {};
  while (::GetMessage(&msg, NULL, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  return 0;
}

bool BeginUpdateCheckWin(const UpdateChecker::CallbackInfo &info) {
  UpdateChecker::CallbackInfo *copied_info = new UpdateChecker::CallbackInfo();
  *copied_info = info;
  unsigned int thread_id = 0;
  ScopedHandle thread_handle(reinterpret_cast<HANDLE>(_beginthreadex(
      NULL, 0, UpdateCheckWinThread, copied_info, 0, &thread_id)));
  if (thread_handle.get() == NULL) {
    delete copied_info;
    copied_info = NULL;
    return false;
  }
  return true;
}

class UpdateInvokerJob
  : public IJobObserver {
 public:
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                                   void **ppvObject) {
    if (::IsEqualGUID(riid, _ATL_IIDOF(IJobObserver))) {
      *ppvObject = static_cast<IJobObserver *>(this);
      this->AddRef();
      return S_OK;
    }
    if (::IsEqualGUID(riid, IID_IUnknown)) {
      *ppvObject = static_cast<IUnknown *>(this);
      this->AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef() {
    return ::InterlockedIncrement(&reference_count_);
  }

  virtual ULONG STDMETHODCALLTYPE Release() {
    const ULONG count = ::InterlockedDecrement(&reference_count_);
    if (count == 0) {
      delete this;
    }
    return count;
  }

  STDMETHOD(OnShow)() {
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_SHOW, 0);
    return S_OK;
  }
  STDMETHOD(OnCheckingForUpdate)() {
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_CHECKING_FOR_UPDATE,
                  0);
    return S_OK;
  }
  STDMETHOD(OnUpdateAvailable)(const TCHAR* version_string) {
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_UPDATE_AVAILABLE, 0);
    return S_OK;
  }
  STDMETHOD(OnWaitingToDownload)() {
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_WAITING_TO_DOWNLOAD,
                  0);
    return S_OK;
  }
  STDMETHOD(OnDownloading)(int time_remaining_ms, int pos) {
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_DOWNLOADING, pos);
    return S_OK;
  }
  STDMETHOD(OnWaitingToInstall)() {
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_WAITING_TO_INSTALL, 0);
    return S_OK;
  }
  STDMETHOD(OnInstalling)() {
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_INSTALLING, 0);
    return S_OK;
  }
  STDMETHOD(OnPause)() {
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_PAUSE, 0);
    return S_OK;
  }
  STDMETHOD(OnComplete)(CompletionCodes code, const TCHAR* text) {
    progress_events_ = NULL;
    UpdateInvoker::CallbackOnCompleteLParam lParam = UpdateInvoker::JOB_FAILED;
    switch (code) {
      case COMPLETION_CODE_SUCCESS:
      case COMPLETION_CODE_SUCCESS_CLOSE_UI:
      case COMPLETION_CODE_REBOOT:
      case COMPLETION_CODE_REBOOT_NOTICE_ONLY:
      case COMPLETION_CODE_RESTART_ALL_BROWSERS:
      case COMPLETION_CODE_RESTART_BROWSER:
      case COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY:
      case COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY:
      case COMPLETION_CODE_RUN_COMMAND:
        lParam = UpdateInvoker::JOB_SUCCEEDED;
        break;
      case COMPLETION_CODE_ERROR:
      default:
        lParam = UpdateInvoker::JOB_FAILED;
        break;
    }
    ::PostMessage(callback_.mesage_receiver_window,
                  callback_.mesage_id, UpdateInvoker::ON_COMPLETE, lParam);

    // Stop the message-loop.
    ::PostQuitMessage(0);
    return S_OK;
  }

  STDMETHOD(SetEventSink)(IProgressWndEvents* event_sink) {
    progress_events_ = event_sink;
    return S_OK;
  }

  static UpdateInvokerJob *CreateInstance(
      const UpdateInvoker::CallbackInfo &info) {
    return new UpdateInvokerJob(info);
  }

 private:
  explicit UpdateInvokerJob(const UpdateInvoker::CallbackInfo &info)
    : reference_count_(0),
      callback_(info)
  {}

  volatile ULONG reference_count_;

  const UpdateInvoker::CallbackInfo callback_;

  CComPtr<IProgressWndEvents> progress_events_;
};

template <typename T>
HRESULT CoCreateInstanceAsAdmin(REFCLSID class_id, HWND window_handle,
                                T **interface_ptr) {
  if (interface_ptr == NULL) {
    return E_POINTER;
  }

  wchar_t class_id_as_string[MAX_PATH] = {};
  ::StringFromGUID2(class_id, class_id_as_string,
                    arraysize(class_id_as_string));

  wstring elevation_moniker_name = L"Elevation:Administrator!new:";
  elevation_moniker_name += class_id_as_string;

  BIND_OPTS3 bind_opts;
  memset(&bind_opts, 0, sizeof(bind_opts));
  bind_opts.cbStruct = sizeof(bind_opts);
  bind_opts.dwClassContext = CLSCTX_LOCAL_SERVER;
  bind_opts.hwnd = window_handle;

  return ::CoGetObject(elevation_moniker_name.c_str(), &bind_opts,
                       _ATL_IIDOF(T), reinterpret_cast<void**>(interface_ptr));
}

unsigned int __stdcall UpdateWinThread(LPVOID lpThreadParameter) {
  UpdateInvoker::CallbackInfo *copied_data =
      reinterpret_cast<UpdateInvoker::CallbackInfo *>(lpThreadParameter);
  const UpdateInvoker::CallbackInfo info = *copied_data;
  delete copied_data;
  copied_data = NULL;

  ScopedCOMInitializer com_init;

  CComPtr<IJobObserver> job_observer(UpdateInvokerJob::CreateInstance(info));

  HRESULT hr = S_OK;

  CComPtr<IGoogleUpdate> google_update;
  hr = CoCreateInstanceAsAdmin(CLSID_OnDemandMachineAppsClass,
                               info.mesage_receiver_window, &google_update);

  if (hr != S_OK) {
    return 0;
  }

  hr = google_update->Update(kOmahaGUID, job_observer);

  // Message loop is required to drive COM RPC.  This loop will be quitted
  // by PostQuitMessage API from UpdateInvokerJob::OnComplete.
  MSG msg = {};
  while (::GetMessage(&msg, NULL, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  return 0;
}

bool BeginUpdateWin(const UpdateInvoker::CallbackInfo &info) {
  UpdateInvoker::CallbackInfo *copied_info = new UpdateInvoker::CallbackInfo();
  *copied_info = info;
  unsigned int thread_id = 0;
  ScopedHandle thread_handle(reinterpret_cast<HANDLE>(_beginthreadex(
      NULL, 0, UpdateWinThread, copied_info, 0, &thread_id)));
  if (thread_handle.get() == NULL) {
    delete copied_info;
    copied_info = NULL;
    return false;
  }
  return true;
}
#endif  // OS_WINDOWS && GOOGLE_JAPANESE_INPUT_BUILD
}  // anonymouse namespace

bool UpdateChecker::BeginCheck(const CallbackInfo &info) {
#if !defined(OS_WINDOWS) || !defined(GOOGLE_JAPANESE_INPUT_BUILD)
  return false;
#else
  return BeginUpdateCheckWin(info);
#endif
}

bool UpdateInvoker::BeginUpdate(const CallbackInfo &info) {
#if !defined(OS_WINDOWS) || !defined(GOOGLE_JAPANESE_INPUT_BUILD)
  return false;
#else
  return BeginUpdateWin(info);
#endif
}
}  // namespace mozc
