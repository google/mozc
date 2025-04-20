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

// skip all unless _WIN32
#ifdef _WIN32

// clang-format off
#include <windows.h>
#include <sddl.h>  // needs to be after windows.h
// clang-format on
#include <wil/resource.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/const.h"
#include "base/cpu_stats.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_sandbox.h"
#include "base/win32/win_util.h"
#include "ipc/ipc.h"
#include "ipc/ipc_path_manager.h"

namespace mozc {
namespace {

using win32::Utf8ToWide;

constexpr bool kReadTypeACK = true;
constexpr bool kReadTypeData = false;
constexpr bool kSendTypeData = false;
constexpr int kMaxSuccessiveConnectionFailureCount = 5;

size_t GetNumberOfProcessors() {
  // thread-safety is not required.
  static size_t num = CPUStats().GetNumberOfProcessors();
  return std::max<size_t>(num, 1);
}

// Least significant bit of OVERLAPPED::hEvent can be used for special
// purpose against GetQueuedCompletionStatus API.
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa364986.aspx
// This function provides a safe way to retrieve the actual event handle
// even in this situation.
HANDLE GetEventHandleFromOverlapped(const OVERLAPPED *overlapped) {
  return reinterpret_cast<HANDLE>(
      reinterpret_cast<DWORD_PTR>(overlapped->hEvent) & ~1);
}

// Returns true if the given |overlapped| is initialized in successful.
bool InitOverlapped(OVERLAPPED *overlapped, HANDLE wait_handle) {
  if (wait_handle == 0 || wait_handle == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "wait_handle is invalid.";
    return false;
  }
  ::ZeroMemory(overlapped, sizeof(OVERLAPPED));
  if (::ResetEvent(wait_handle) == FALSE) {
    const DWORD last_error = ::GetLastError();
    LOG(ERROR) << "::ResetEvent failed. error: " << last_error;
    return false;
  }
  overlapped->hEvent = wait_handle;
  return true;
}

class IPCClientMutexBase {
 public:
  explicit IPCClientMutexBase(const absl::string_view ipc_channel_name) {
    // Make a kernel mutex object so that multiple ipc connections are
    // serialized here. In Windows, there is no useful way to serialize
    // the multiple connections to the single-thread named pipe server.
    // WaitForNamedPipe doesn't work for this propose as it just lets
    // clients know that the connection becomes "available" right now.
    // It doesn't mean that connection is available for the current
    // thread. The "available" notification is sent to all waiting ipc
    // clients at the same time and only one client gets the connection.
    // This causes redundant and wasteful CreateFile calles.
    std::string mutex_name =
        absl::StrCat(kMutexPathPrefix, SystemUtil::GetUserSidAsString(), ".",
                     ipc_channel_name, ".ipc");
    std::wstring wmutex_name = Utf8ToWide(mutex_name);

    LPSECURITY_ATTRIBUTES security_attributes_ptr = nullptr;
    SECURITY_ATTRIBUTES security_attributes;
    if (!WinSandbox::MakeSecurityAttributes(WinSandbox::kSharableMutex,
                                            &security_attributes)) {
      LOG(ERROR) << "Cannot make SecurityAttributes";
    } else {
      security_attributes_ptr = &security_attributes;
    }

    // http://msdn.microsoft.com/en-us/library/ms682411(VS.85).aspx:
    // Two or more processes can call CreateMutex to create the same named
    // mutex. The first process actually creates the mutex, and subsequent
    // processes with sufficient access rights simply open a handle to
    // the existing mutex. This enables multiple processes to get handles
    // of the same mutex, while relieving the user of the responsibility
    // of ensuring that the creating process is started first.
    // When using this technique, you should set the
    // bInitialOwner flag to FALSE; otherwise, it can be difficult to be
    // certain which process has initial ownership.
    ipc_mutex_.reset(
        ::CreateMutex(security_attributes_ptr, FALSE, wmutex_name.c_str()));
    if (security_attributes_ptr != nullptr) {
      ::LocalFree(security_attributes_ptr->lpSecurityDescriptor);
    }

    const DWORD create_mutex_error = ::GetLastError();
    if (ipc_mutex_.get() == nullptr) {
      LOG(ERROR) << "CreateMutex failed: " << create_mutex_error;
      return;
    }
  }

  virtual ~IPCClientMutexBase() = default;

  const wil::unique_mutex_nothrow &mutex() const { return ipc_mutex_; }

 private:
  wil::unique_mutex_nothrow ipc_mutex_;
};

class ConverterClientMutex : public IPCClientMutexBase {
 public:
  ConverterClientMutex() : IPCClientMutexBase("converter") {}
  ConverterClientMutex(const ConverterClientMutex &) = delete;
  ConverterClientMutex &operator=(const ConverterClientMutex &) = delete;
};

class RendererClientMutex : public IPCClientMutexBase {
 public:
  RendererClientMutex() : IPCClientMutexBase("renderer") {}
  RendererClientMutex(const RendererClientMutex &) = delete;
  RendererClientMutex &operator=(const RendererClientMutex &) = delete;
};

class FallbackClientMutex : public IPCClientMutexBase {
 public:
  FallbackClientMutex() : IPCClientMutexBase("fallback") {}
  FallbackClientMutex(const FallbackClientMutex &) = delete;
  FallbackClientMutex &operator=(const FallbackClientMutex &) = delete;
};

// In Mozc client, we should support different IPC channels (client-converter
// and client-renderer) so we need to have different global mutexes to
// serialize each client. Currently |ipc_name| starts with "session" and
// "renderer" are expected.
const wil::unique_mutex_nothrow &GetClientMutex(
    const absl::string_view ipc_name) {
  if (ipc_name.starts_with("session")) {
    return Singleton<ConverterClientMutex>::get()->mutex();
  }
  if (ipc_name.starts_with("renderer")) {
    return Singleton<RendererClientMutex>::get()->mutex();
  }
  LOG(WARNING) << "unexpected IPC name: " << ipc_name;
  return Singleton<FallbackClientMutex>::get()->mutex();
}

uint32_t GetServerProcessIdImpl(HANDLE handle) {
  ULONG pid = 0;
  if (::GetNamedPipeServerProcessId(handle, &pid) == 0) {
    const DWORD get_named_pipe_server_process_id_error = ::GetLastError();
    LOG(ERROR) << "GetNamedPipeServerProcessId failed: "
               << get_named_pipe_server_process_id_error;
    return static_cast<uint32_t>(-1);  // always deny the connection
  }

  MOZC_VLOG(1) << "Got server ProcessID: " << pid;

  return static_cast<uint32_t>(pid);
}

void SafeCancelIO(HANDLE device_handle, OVERLAPPED *overlapped) {
  if (::CancelIo(device_handle) == FALSE) {
    const DWORD cancel_error = ::GetLastError();
    LOG(ERROR) << "Failed to CancelIo: " << cancel_error;
  }

  // Wait for the completion of the on-going request forever. This is not
  // _safe_ and should be fixed anyway.
  // TODO(yukawa): Avoid INFINITE if possible.
  ::WaitForSingleObject(GetEventHandleFromOverlapped(overlapped), INFINITE);
}

IPCErrorType WaitForQuitOrIOImpl(HANDLE device_handle, HANDLE quit_event,
                                 DWORD timeout, OVERLAPPED *overlapped) {
  const HANDLE events[] = {quit_event,
                           GetEventHandleFromOverlapped(overlapped)};
  const DWORD wait_result =
      ::WaitForMultipleObjects(std::size(events), events, FALSE, timeout);
  const DWORD wait_error = ::GetLastError();
  // Clear the I/O operation if still exists.
  if (!HasOverlappedIoCompleted(overlapped)) {
    // This is not safe because this operation may be blocked forever.
    // TODO(yukawa): Implement safer cancelation mechanism.
    SafeCancelIO(device_handle, overlapped);
  }
  if (wait_result == WAIT_TIMEOUT) {
    LOG(WARNING) << "Timeout: " << timeout;
    return IPC_TIMEOUT_ERROR;
  }
  if (wait_result == WAIT_OBJECT_0) {
    // Should be quit immediately
    return IPC_QUIT_EVENT_SIGNALED;
  }
  if (wait_result != (WAIT_OBJECT_0 + 1)) {
    LOG(WARNING) << "Unknown result: " << wait_result
                 << ", Error: " << wait_error;
    return IPC_UNKNOWN_ERROR;
  }
  return IPC_NO_ERROR;
}

IPCErrorType WaitForIOImpl(HANDLE device_handle, DWORD timeout,
                           OVERLAPPED *overlapped) {
  const DWORD wait_result =
      ::WaitForSingleObject(GetEventHandleFromOverlapped(overlapped), timeout);
  // Clear the I/O operation if still exists.
  if (!HasOverlappedIoCompleted(overlapped)) {
    // This is not safe because this operation may be blocked forever.
    // TODO(yukawa): Implement safer cancelation mechanism.
    SafeCancelIO(device_handle, overlapped);
  }
  if (wait_result == WAIT_TIMEOUT) {
    LOG(WARNING) << "Timeout: " << timeout;
    return IPC_TIMEOUT_ERROR;
  }
  if (wait_result != WAIT_OBJECT_0) {
    LOG(WARNING) << "Unknown result: " << wait_result;
    return IPC_UNKNOWN_ERROR;
  }
  return IPC_NO_ERROR;
}

IPCErrorType WaitForQuitOrIO(HANDLE device_handle, HANDLE quit_event,
                             DWORD timeout, OVERLAPPED *overlapped) {
  if (quit_event != nullptr) {
    return WaitForQuitOrIOImpl(device_handle, quit_event, timeout, overlapped);
  }
  return WaitForIOImpl(device_handle, timeout, overlapped);
}

// To work around a bug of GetOverlappedResult in Vista
// http://msdn.microsoft.com/en-us/library/dd371711.aspx
IPCErrorType SafeWaitOverlappedResult(HANDLE device_handle, HANDLE quit_event,
                                      DWORD timeout, OVERLAPPED *overlapped,
                                      DWORD *num_bytes_updated, bool wait_ack) {
  DCHECK(overlapped);
  DCHECK(num_bytes_updated);
  const IPCErrorType result =
      WaitForQuitOrIO(device_handle, quit_event, timeout, overlapped);
  if (result != IPC_NO_ERROR) {
    return result;
  }

  *num_bytes_updated = 0;
  const BOOL get_overlapped_result = ::GetOverlappedResult(
      device_handle, overlapped, num_bytes_updated, FALSE);
  if (get_overlapped_result == FALSE) {
    const DWORD get_overlapped_error = ::GetLastError();
    if (get_overlapped_error == ERROR_BROKEN_PIPE) {
      if (wait_ack) {
        // This is an expected behavior.
        return IPC_NO_ERROR;
      }
      LOG(ERROR) << "GetOverlappedResult() failed: ERROR_BROKEN_PIPE";
    } else if (get_overlapped_error == ERROR_MORE_DATA) {
      return IPC_MORE_DATA;
    } else {
      LOG(ERROR) << "GetOverlappedResult() failed: " << get_overlapped_error;
    }
    return IPC_UNKNOWN_ERROR;
  }
  return IPC_NO_ERROR;
}

IPCErrorType SendIpcMessage(HANDLE device_handle, HANDLE write_wait_handle,
                            const std::string &msg, absl::Duration timeout) {
  if (msg.empty()) {
    LOG(WARNING) << "msg is empty.";
    return IPC_UNKNOWN_ERROR;
  }

  DWORD num_bytes_written = 0;
  OVERLAPPED overlapped;
  if (!InitOverlapped(&overlapped, write_wait_handle)) {
    return IPC_WRITE_ERROR;
  }

  const bool write_file_result =
      (::WriteFile(device_handle, msg.data(), static_cast<DWORD>(msg.size()),
                   &num_bytes_written, &overlapped) != FALSE);
  const DWORD write_file_error = ::GetLastError();
  if (write_file_result) {
    // ::WriteFile is done as sync operation.
  } else {
    if (write_file_error != ERROR_IO_PENDING) {
      LOG(ERROR) << "WriteFile() failed: " << write_file_error;
      return IPC_WRITE_ERROR;
    }
    const IPCErrorType result = SafeWaitOverlappedResult(
        device_handle, nullptr, absl::ToInt64Milliseconds(timeout), &overlapped,
        &num_bytes_written, kSendTypeData);
    if (result != IPC_NO_ERROR) {
      return result;
    }
  }

  // As we use message-type namedpipe, all the data should be written in one
  // shot. Otherwise, single message will be split into multiple packets.
  if (num_bytes_written != msg.size()) {
    LOG(ERROR) << "Data truncated. msg.size(): " << msg.size()
               << ", num_bytes_written: " << num_bytes_written;
    return IPC_UNKNOWN_ERROR;
  }
  return IPC_NO_ERROR;
}

IPCErrorType RecvIpcMessage(HANDLE device_handle, HANDLE read_wait_handle,
                            std::string *msg, absl::Duration timeout,
                            bool read_type_ack) {
  if (!msg) {
    LOG(WARNING) << "msg is nullptr.";
    return IPC_UNKNOWN_ERROR;
  }

  msg->clear();
  DWORD num_bytes_read_total = 0;
  while (true) {
    OVERLAPPED overlapped;
    if (!InitOverlapped(&overlapped, read_wait_handle)) {
      msg->clear();
      return IPC_READ_ERROR;
    }
    if (num_bytes_read_total == 0) {
      msg->resize(IPC_INITIAL_READ_BUFFER_SIZE);
    } else {
      msg->resize(msg->size() * 2);
    }
    const DWORD num_bytes_writable = msg->size() - num_bytes_read_total;
    DWORD num_bytes_read = 0;
    const bool read_file_result =
        (::ReadFile(device_handle, msg->data() + num_bytes_read_total,
                    num_bytes_writable, &num_bytes_read, &overlapped) != FALSE);
    const DWORD read_file_error = ::GetLastError();
    if (read_file_result) {
      // ::ReadFile is done as sync operation.
      num_bytes_read_total += num_bytes_read;
      break;
    }
    if (read_file_error == ERROR_MORE_DATA) {
      // ::ReadFile is done as sync operation with pending data.
      num_bytes_read_total += num_bytes_writable;
      continue;
    }
    if (read_type_ack && (read_file_error == ERROR_BROKEN_PIPE)) {
      // The client has already disconnected this pipe. This is an expected
      // behavior and do not treat as an error.
      msg->clear();
      return IPC_NO_ERROR;
    }
    if (read_file_error != ERROR_IO_PENDING) {
      LOG(ERROR) << "ReadFile() failed: " << read_file_error;
      msg->clear();
      return IPC_READ_ERROR;
    }
    // Actually this is an async operation. Let's wait for its completion.
    const IPCErrorType result = SafeWaitOverlappedResult(
        device_handle, nullptr, absl::ToInt64Milliseconds(timeout), &overlapped,
        &num_bytes_read, read_type_ack);
    if (result == IPC_MORE_DATA) {
      num_bytes_read_total += num_bytes_read;
      continue;
    }
    if (result != IPC_NO_ERROR) {
      msg->clear();
      return result;
    }
    num_bytes_read_total += num_bytes_read;
    break;
  }

  if (!read_type_ack && (num_bytes_read_total == 0)) {
    LOG(WARNING) << "Received 0 result.";
  }
  msg->resize(num_bytes_read_total);
  return IPC_NO_ERROR;
}

wil::unique_event_nothrow CreateManualResetEvent() {
  wil::unique_event_nothrow event;
  event.create(wil::EventOptions::ManualReset, nullptr);
  return event;
}

// We do not care about the signaled state of the device handle itself.
// This slightly improves the performance.
// See http://msdn.microsoft.com/en-us/library/windows/desktop/aa365538.aspx
void MaybeDisableFileCompletionNotification(HANDLE device_handle) {
  // This is not a mandatory task. Just ignore the actual error (if any).
  ::SetFileCompletionNotificationModes(device_handle,
                                       FILE_SKIP_SET_EVENT_ON_HANDLE);
}

}  // namespace

IPCServer::IPCServer(const std::string &name, int32_t num_connections,
                     absl::Duration timeout)
    : connected_(false),
      quit_event_(CreateManualResetEvent()),
      pipe_event_(CreateManualResetEvent()),
      timeout_(timeout) {
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager(name);
  std::string server_address;

  if (!manager->CreateNewPathName() && !manager->LoadPathName()) {
    LOG(ERROR) << "Cannot prepare IPC path name";
    return;
  }

  if (!manager->GetPathName(&server_address)) {
    LOG(ERROR) << "Cannot make IPC path name";
    return;
  }
  DCHECK(!server_address.empty());

  SECURITY_ATTRIBUTES security_attributes;
  if (!WinSandbox::MakeSecurityAttributes(WinSandbox::kSharablePipe,
                                          &security_attributes)) {
    LOG(ERROR) << "Cannot make SecurityAttributes";
    return;
  }

  // Create a named pipe.
  std::wstring wserver_address = Utf8ToWide(server_address);
  HANDLE handle = ::CreateNamedPipe(
      wserver_address.c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
          PIPE_REJECT_REMOTE_CLIENTS,
      (num_connections <= 0 ? PIPE_UNLIMITED_INSTANCES : num_connections),
      IPC_INITIAL_READ_BUFFER_SIZE, IPC_INITIAL_READ_BUFFER_SIZE, 0,
      &security_attributes);
  const DWORD create_named_pipe_error = ::GetLastError();
  ::LocalFree(security_attributes.lpSecurityDescriptor);

  if (INVALID_HANDLE_VALUE == handle) {
    LOG(FATAL) << "CreateNamedPipe failed" << create_named_pipe_error;
    return;
  }

  pipe_handle_.reset(handle);

  MaybeDisableFileCompletionNotification(pipe_handle_.get());

  if (!manager->SavePathName()) {
    LOG(ERROR) << "Cannot save IPC path name";
    return;
  }

  connected_ = true;
}

IPCServer::~IPCServer() { Terminate(); }

bool IPCServer::Connected() const { return connected_; }

void IPCServer::Terminate() {
  if (server_thread_.get() == nullptr) {
    return;
  }

  if (!::SetEvent(quit_event_.get())) {
    LOG(ERROR) << "SetEvent failed";
  }

  // Close the named pipe.
  // This is a workaround for killing child thread
  server_thread_->Join();

  connected_ = false;
}

void IPCServer::Loop() {
  int successive_connection_failure_count = 0;
  std::string request;
  std::string response;
  while (connected_) {
    OVERLAPPED overlapped;
    if (!InitOverlapped(&overlapped, pipe_event_.get())) {
      connected_ = false;
      return;
    }

    const BOOL result = ::ConnectNamedPipe(pipe_handle_.get(), &overlapped);
    const DWORD connect_named_pipe_error = ::GetLastError();
    if (result == FALSE) {
      if (connect_named_pipe_error == ERROR_PIPE_CONNECTED) {
        // Already connected. Nothing to do.
      } else if (connect_named_pipe_error == ERROR_NO_DATA) {
        // client already closes the connection
        ::DisconnectNamedPipe(pipe_handle_.get());
        continue;
      } else if (connect_named_pipe_error == ERROR_IO_PENDING) {
        // Actually this is async operation.
        DWORD ignored = 0;
        const IPCErrorType ipc_error = SafeWaitOverlappedResult(
            pipe_handle_.get(), quit_event_.get(), INFINITE, &overlapped,
            &ignored, kReadTypeData);
        if (ipc_error == IPC_QUIT_EVENT_SIGNALED) {
          MOZC_VLOG(1) << "Received Control event from other thread";
          connected_ = false;
          return;
        }
        if (ipc_error != IPC_NO_ERROR) {
          ++successive_connection_failure_count;
          if (successive_connection_failure_count >=
              kMaxSuccessiveConnectionFailureCount) {
            LOG(ERROR) << "Give up to connect named pipe.";
            connected_ = false;
            return;
          }
          ::DisconnectNamedPipe(pipe_handle_.get());
          continue;
        }
      } else {
        LOG(FATAL) << "Unexpected error: " << connect_named_pipe_error;
      }
    }

    successive_connection_failure_count = 0;
    // Retrieve an incoming message.
    if (RecvIpcMessage(pipe_handle_.get(), pipe_event_.get(), &request,
                       timeout_, kReadTypeData) == IPC_NO_ERROR) {
      if (!Process(request, &response)) {
        connected_ = false;
      }

      // When Process() returns 0 result, force to call DisconnectNamedPipe()
      // instead of checking ACK message
      if (response.empty()) {
        LOG(WARNING) << "Process() return 0 result";
        ::DisconnectNamedPipe(pipe_handle_.get());
        continue;
      }

      // Send a response
      if (SendIpcMessage(pipe_handle_.get(), pipe_event_.get(), response,
                         timeout_) != IPC_NO_ERROR) {
        LOG(WARNING) << "SendIpcMessage failed.";
      }
    }

    // Special treatment for Windows per discussion with thatanaka:
    // It's hard to know that client has processed the server's response.
    // We will be able to call ::FlushFileHandles() here, but
    // FlushFileHandles() is blocked if client doesn't call ReadFile(). That
    // means that a malicious user can easily block the server not by calling
    // ReadFile. In order to know the transaction completes successfully,
    // client needs to send an ACK message to the server.

    // Wait ACK-like signal from client for 0.1 second. If we detect the pipe
    // disconnect event, so far so good. If we receive more data, we assume it
    // is an ACK signal (the IPC client of Mozc 1.5.x or earlier does this).
    std::string ack_request;
    static constexpr absl::Duration kAckTimeout = absl::Milliseconds(100);
    if (RecvIpcMessage(pipe_handle_.get(), pipe_event_.get(), &ack_request,
                       kAckTimeout, kReadTypeACK) != IPC_NO_ERROR) {
      // This case happens when the client did not receive the server's response
      // within timeout. Anyway we will close the connection so that the server
      // will not be blocked.
      LOG(WARNING) << "Client didn't respond within " << kAckTimeout
                   << " msec.";
    }
    ::DisconnectNamedPipe(pipe_handle_.get());
  }

  connected_ = false;
}

// old interface
IPCClient::IPCClient(const absl::string_view name)
    : pipe_event_(CreateManualResetEvent()),
      connected_(false),
      ipc_path_manager_(nullptr),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, "");
}

IPCClient::IPCClient(const absl::string_view name,
                     const absl::string_view server_path)
    : pipe_event_(CreateManualResetEvent()),
      connected_(false),
      ipc_path_manager_(nullptr),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, server_path);
}

void IPCClient::Init(const absl::string_view name,
                     const absl::string_view server_path) {
  last_ipc_error_ = IPC_NO_CONNECTION;

  // We should change the mutex based on which IPC server we will talk with.
  const wil::unique_mutex_nothrow &ipc_mutex = GetClientMutex(name);
  wil::mutex_release_scope_exit mutex_releaser;

  if (ipc_mutex.get() == nullptr) {
    LOG(ERROR) << "IPC mutex is not available";
  } else {
    constexpr int kMutexTimeout = 10 * 1000;  // wait at most 10sec.
    DWORD status;
    mutex_releaser = ipc_mutex.acquire(&status, kMutexTimeout);
    switch (status) {
      case WAIT_TIMEOUT:
        // TODO(taku): with suspend/resume, WaitForSingleObject may
        // return WAIT_TIMEOUT. We have to consider the case
        // in the future.
        LOG(ERROR) << "IPC client was not available even after "
                   << kMutexTimeout << " msec.";
        break;
      case WAIT_ABANDONED:
        DLOG(INFO) << "mutex object was removed";
        break;
      case WAIT_OBJECT_0:
        break;
      default:
        break;
    }
  }

  IPCPathManager *manager = IPCPathManager::GetIPCPathManager(name);
  if (manager == nullptr) {
    LOG(ERROR) << "IPCPathManager::GetIPCPathManager failed";
    return;
  }

  ipc_path_manager_ = manager;

  // TODO(taku): enable them on Mac/Linux
#ifdef DEBUG
  constexpr size_t kMaxTrial = 256;
#else   // DEBUG
  constexpr size_t kMaxTrial = 2;
#endif  // DEBUG

  for (size_t trial = 0; trial < kMaxTrial; ++trial) {
    std::string server_address;
    if (!manager->LoadPathName() || !manager->GetPathName(&server_address)) {
      continue;
    }
    std::wstring wserver_address = Utf8ToWide(server_address);

    if (GetNumberOfProcessors() == 1) {
      // When the code is running in single processor environment, sometimes
      // IPC server has not finished the clean-up tasks for the previous IPC
      // session here. So we intentionally call WaitNamedPipe API so that IPC
      // server has a chance to complete clean-up tasks if necessary.
      // NOTE: We cannot set 0 for the wait time because 0 has a special meaning
      // as |NMPWAIT_USE_DEFAULT_WAIT|.
      constexpr DWORD kMinWaitTimeForWaitNamedPipe = 1;
      ::WaitNamedPipe(wserver_address.c_str(), kMinWaitTimeForWaitNamedPipe);
    }

    wil::unique_hfile new_handle(
        ::CreateFile(wserver_address.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                     nullptr, OPEN_EXISTING,
                     FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT |
                         SECURITY_IDENTIFICATION | SECURITY_EFFECTIVE_ONLY,
                     nullptr));
    const DWORD create_file_error = ::GetLastError();
    if (new_handle) {
      pipe_handle_ = std::move(new_handle);
      MaybeDisableFileCompletionNotification(pipe_handle_.get());
      // Set PIPE_READMODE_MESSAGE so that we can rely on ERROR_MORE_DATA.
      // https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-client
      DWORD mode = PIPE_READMODE_MESSAGE;
      ::SetNamedPipeHandleState(pipe_handle_.get(), &mode, nullptr, nullptr);
      if (!manager->IsValidServer(GetServerProcessIdImpl(pipe_handle_.get()),
                                  server_path)) {
        LOG(ERROR) << "Connecting to invalid server";
        last_ipc_error_ = IPC_INVALID_SERVER;
        return;
      }

      last_ipc_error_ = IPC_NO_ERROR;
      connected_ = true;
      return;
    }

    if (ERROR_PIPE_BUSY != create_file_error) {
      LOG(ERROR) << "Server is not running: " << create_file_error;
      manager->Clear();
      continue;
    }

    // wait for 10 second until server is ready
    // TODO(taku): control the timeout via flag.
#ifdef DEBUG
    constexpr int kNamedPipeTimeout = 100000;  // 100 sec
#else                                          // DEBUG
    constexpr int kNamedPipeTimeout = 10000;  // 10 sec
#endif                                         // DEBUG
    DLOG(ERROR) << "Server is busy. waiting for " << kNamedPipeTimeout
                << " msec";
    if (!::WaitNamedPipe(wserver_address.c_str(), kNamedPipeTimeout)) {
      const DWORD wait_named_pipe_error = ::GetLastError();
      LOG(ERROR) << "WaitNamedPipe failed: " << wait_named_pipe_error;
      if ((trial + 1) == kMaxTrial) {
        last_ipc_error_ = IPC_TIMEOUT_ERROR;
        return;
      }
      continue;  // go 2nd trial
    }
  }
}

IPCClient::~IPCClient() {}

bool IPCClient::Connected() const { return connected_; }

bool IPCClient::Call(const std::string &request, std::string *response,
                     absl::Duration timeout) {
  last_ipc_error_ = IPC_NO_ERROR;
  if (!connected_) {
    LOG(ERROR) << "IPCClient is not connected";
    last_ipc_error_ = IPC_NO_CONNECTION;
    return false;
  }

  last_ipc_error_ =
      SendIpcMessage(pipe_handle_.get(), pipe_event_.get(), request, timeout);
  if (last_ipc_error_ != IPC_NO_ERROR) {
    LOG(ERROR) << "SendIpcMessage() failed";
    return false;
  }

  last_ipc_error_ = RecvIpcMessage(pipe_handle_.get(), pipe_event_.get(),
                                   response, timeout, kReadTypeData);
  if (last_ipc_error_ != IPC_NO_ERROR) {
    LOG(ERROR) << "RecvIpcMessage() failed";
    return false;
  }

  // Instead of sending ACK message to Server, we simply disconnect the named
  // pile to notify that client can read the message successfully.
  connected_ = false;
  pipe_handle_.reset(INVALID_HANDLE_VALUE);

  return true;
}

}  // namespace mozc

#endif  // _WIN32
