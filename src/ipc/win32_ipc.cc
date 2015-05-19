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

// skip all unless OS_WINDOWS
#ifdef OS_WINDOWS

#include "ipc/ipc.h"

#include <Windows.h>
#include <Sddl.h>

#include <string>

#include "base/base.h"
#include "base/const.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "base/util.h"
#include "base/win_sandbox.h"
#include "base/win_util.h"
#include "ipc/ipc_path_manager.h"

namespace mozc {
namespace {

const bool kReadTypeACK = true;
const bool kReadTypeData = false;
const bool kSendTypeData = false;
const int kMaxSuccessiveConnectionFailureCount = 5;

typedef BOOL (WINAPI *FPGetNamedPipeServerProcessId)(HANDLE, PULONG);
FPGetNamedPipeServerProcessId g_get_named_pipe_server_process_id = NULL;

typedef BOOL (WINAPI *FPSetFileCompletionNotificationModes)(HANDLE, UCHAR);
FPSetFileCompletionNotificationModes
    g_set_file_completion_notification_modes = NULL;

// Defined when _WIN32_WINNT >= 0x600
#ifndef FILE_SKIP_SET_EVENT_ON_HANDLE
#define FILE_SKIP_SET_EVENT_ON_HANDLE 0x2
#endif  // FILE_SKIP_SET_EVENT_ON_HANDLE

static once_t g_once = MOZC_ONCE_INIT;

void InitAPIsForVistaAndLater() {
  // We have to load the function pointer dynamically
  // as GetNamedPipeServerProcessId() is only available on Windows Vista.
  if (!Util::IsVistaOrLater()) {
    return;
  }

  VLOG(1) << "Initializing GetNamedPipeServerProcessId";
  // kernel32.dll must be loaded in client.
  const HMODULE lib = WinUtil::GetSystemModuleHandle(L"kernel32.dll");
  if (lib == NULL) {
    LOG(ERROR) << "GetSystemModuleHandle for kernel32.dll failed.";
    return;
  }

  g_get_named_pipe_server_process_id =
      reinterpret_cast<FPGetNamedPipeServerProcessId>
      (::GetProcAddress(lib, "GetNamedPipeServerProcessId"));

  g_set_file_completion_notification_modes =
      reinterpret_cast<FPSetFileCompletionNotificationModes>
      (::GetProcAddress(lib, "SetFileCompletionNotificationModes"));
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
  explicit IPCClientMutexBase(const string &ipc_channel_name) {
    // Make a kernel mutex object so that multiple ipc connections are
    // serialized here. In Windows, there is no useful way to serialize
    // the multiple connections to the single-thread named pipe server.
    // WaitForNamedPipe doesn't work for this propose as it just lets
    // clients know that the connection becomes "available" right now.
    // It doesn't mean that connection is available for the current
    // thread. The "available" notification is sent to all waiting ipc
    // clients at the same time and only one client gets the connection.
    // This causes redundant and wasteful CreateFile calles.
    string mutex_name = kMutexPathPrefix;
    mutex_name += Util::GetUserSidAsString();
    mutex_name += ".";
    mutex_name += ipc_channel_name;
    mutex_name += ".ipc";
    wstring wmutex_name;
    Util::UTF8ToWide(mutex_name.c_str(), &wmutex_name);

    LPSECURITY_ATTRIBUTES security_attributes_ptr = NULL;
    SECURITY_ATTRIBUTES security_attributes;
    if (!WinSandbox::MakeSecurityAttributes(&security_attributes)) {
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
    ipc_mutex_.reset(::CreateMutex(security_attributes_ptr,
                                   FALSE, wmutex_name.c_str()));
    const DWORD create_mutex_error = ::GetLastError();
    if (ipc_mutex_.get() == NULL) {
      LOG(ERROR) << "CreateMutex failed: " << create_mutex_error;
      return;
    }

    // permit the access from a process runinning with low integrity level
    if (Util::IsVistaOrLater()) {
      WinSandbox::SetMandatoryLabelW(ipc_mutex_.get(), SE_KERNEL_OBJECT,
                                     SDDL_NO_EXECUTE_UP, SDDL_ML_LOW);
    }
  }

  virtual ~IPCClientMutexBase() {}

  HANDLE get() const {
    return ipc_mutex_.get();
  }

 private:
  ScopedHandle ipc_mutex_;
};

class ConverterClientMutex : public IPCClientMutexBase {
 public:
  ConverterClientMutex()
      : IPCClientMutexBase("converter") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConverterClientMutex);
};

class RendererClientMutex : public IPCClientMutexBase {
 public:
  RendererClientMutex()
      : IPCClientMutexBase("renderer") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererClientMutex);
};

class FallbackClientMutex : public IPCClientMutexBase {
 public:
  FallbackClientMutex()
      : IPCClientMutexBase("fallback") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FallbackClientMutex);
};

// In Mozc client, we should support different IPC channels (client-converter
// and client-renderer) so we need to have different global mutexes to
// serialize each client. Currently |ipc_name| starts with "session" and
// "renderer" are expected.
HANDLE GetClientMutex(const string &ipc_name) {
  if (Util::StartsWith(ipc_name, "session")) {
    return Singleton<ConverterClientMutex>::get()->get();
  }
  if (Util::StartsWith(ipc_name, "renderer")) {
    return Singleton<RendererClientMutex>::get()->get();
  }
  LOG(WARNING) << "unexpected IPC name: " << ipc_name;
  return Singleton<FallbackClientMutex>::get()->get();
}

// RAII class for calling ReleaseMutex in destructor.
class ScopedReleaseMutex {
 public:
  explicit ScopedReleaseMutex(HANDLE handle)
      : pipe_handle_(handle) {}

  virtual ~ScopedReleaseMutex() {
    if (NULL != pipe_handle_) {
      ::ReleaseMutex(pipe_handle_);
    }
    pipe_handle_ = NULL;
  }

  HANDLE get() const {
    return pipe_handle_;
  }

 private:
  HANDLE pipe_handle_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedReleaseMutex);
};

uint32 GetServerProcessIdImpl(HANDLE handle) {
  CallOnce(&g_once, &InitAPIsForVistaAndLater);

  if (g_get_named_pipe_server_process_id == NULL) {
    return static_cast<uint32>(0);   // must be Windows XP
  }

  ULONG pid = 0;
  if ((*g_get_named_pipe_server_process_id)(handle, &pid) == 0) {
    const DWORD get_named_pipe_server_process_id_error = ::GetLastError();
    LOG(ERROR) << "GetNamedPipeServerProcessId failed: "
               << get_named_pipe_server_process_id_error;
    return static_cast<uint32>(-1);   // always deny the connection
  }

  VLOG(1) << "Got server ProcessID: " << pid;

  return static_cast<uint32>(pid);
}

void SafeCancelIO(HANDLE device_handle, OVERLAPPED *overlapped) {
  if (::CancelIo(device_handle) == FALSE) {
    const DWORD cancel_error = ::GetLastError();
    // In this case, the issued I/O is still on-going. We must keep
    // |overlapped| and the memory block used for this I/O accessible from the
    // I/O module. Otherwise, unexpected memory corruption may happen.
    LOG(ERROR) << "Failed to CancelIo: " << cancel_error;
    return;
  }

  // Wait for the completion of the cancel request forever. This is not _safe_
  // and should be fixed anyway.
  // TODO(yukawa): Do not use INFINITE here.
  ::WaitForSingleObject(GetEventHandleFromOverlapped(overlapped), INFINITE);
}

bool WaitForQuitOrIOImpl(
    HANDLE device_handle, HANDLE quit_event, DWORD timeout,
    OVERLAPPED *overlapped, IPCErrorType *last_ipc_error) {
  const HANDLE events[] = {
    quit_event, GetEventHandleFromOverlapped(overlapped)
  };
  const DWORD wait_result = ::WaitForMultipleObjects(
      ARRAYSIZE(events), events, FALSE, timeout);
  const DWORD wait_error = ::GetLastError();
  // Clear the I/O operation if still exists.
  if (!HasOverlappedIoCompleted(overlapped)) {
    // This is not safe because this operation may be blocked forever.
    // TODO(yukawa): Implement safer cancelation mechanism.
    SafeCancelIO(device_handle, overlapped);
  }
  if (wait_result == WAIT_TIMEOUT) {
    LOG(WARNING) << "Timeout: " << timeout;
    *last_ipc_error = IPC_TIMEOUT_ERROR;
    return false;
  }
  if (wait_result == WAIT_OBJECT_0) {
    // Should be quit immediately
    *last_ipc_error = IPC_QUIT_EVENT_SIGNALED;
    return false;
  }
  if (wait_result != (WAIT_OBJECT_0 + 1)) {
    LOG(WARNING) << "Unknown result: " << wait_result
                 << ", Error: " << wait_error;
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }
  return true;
}

bool WaitForIOImpl(HANDLE device_handle, DWORD timeout,
                   OVERLAPPED *overlapped, IPCErrorType *last_ipc_error) {
  const DWORD wait_result = ::WaitForSingleObject(
      GetEventHandleFromOverlapped(overlapped), timeout);
  // Clear the I/O operation if still exists.
  if (!HasOverlappedIoCompleted(overlapped)) {
    // This is not safe because this operation may be blocked forever.
    // TODO(yukawa): Implement safer cancelation mechanism.
    SafeCancelIO(device_handle, overlapped);
  }
  if (wait_result == WAIT_TIMEOUT) {
    LOG(WARNING) << "Timeout: " << timeout;
    *last_ipc_error = IPC_TIMEOUT_ERROR;
    return false;
  }
  if (wait_result != WAIT_OBJECT_0) {
    LOG(WARNING) << "Unknown result: " << wait_result;
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }
  return true;
}

bool WaitForQuitOrIO(
    HANDLE device_handle, HANDLE quit_event, DWORD timeout,
    OVERLAPPED *overlapped, IPCErrorType *last_ipc_error) {
  if (quit_event != NULL) {
    return WaitForQuitOrIOImpl(device_handle, quit_event, timeout,
                               overlapped, last_ipc_error);
  }
  return WaitForIOImpl(device_handle, timeout, overlapped, last_ipc_error);
}

// To work around a bug of GetOverlappedResult in Vista
// http://msdn.microsoft.com/en-us/library/dd371711.aspx
bool SafeWaitOverlappedResult(
    HANDLE device_handle, HANDLE quit_event, DWORD timeout,
    OVERLAPPED *overlapped, DWORD *num_bytes_updated,
    IPCErrorType *last_ipc_error, bool wait_ack) {
  DCHECK(overlapped);
  DCHECK(num_bytes_updated);
  DCHECK(last_ipc_error);
  if (!WaitForQuitOrIO(device_handle, quit_event, timeout,
                       overlapped, last_ipc_error)) {
    return false;
  }

  *num_bytes_updated = 0;
  const BOOL get_overlapped_result = ::GetOverlappedResult(
      device_handle, overlapped, num_bytes_updated, FALSE);
  if (get_overlapped_result == FALSE) {
    const DWORD get_overlapped_error = ::GetLastError();
    if (get_overlapped_error == ERROR_BROKEN_PIPE) {
      if (wait_ack) {
        // This is an expected behavior.
        return true;
      }
      LOG(ERROR) << "GetOverlappedResult() failed: ERROR_BROKEN_PIPE";
    } else {
      LOG(ERROR) << "GetOverlappedResult() failed: " << get_overlapped_error;
    }
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }
  return true;
}

bool SendIPCMessage(HANDLE device_handle, HANDLE write_wait_handle,
                    const char *buf, size_t buf_length, int timeout,
                    IPCErrorType *last_ipc_error) {
  if (buf_length == 0) {
    LOG(WARNING) << "buf length is 0";
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }

  DWORD num_bytes_written = 0;
  OVERLAPPED overlapped;
  if (!InitOverlapped(&overlapped, write_wait_handle)) {
    *last_ipc_error = IPC_WRITE_ERROR;
    return false;
  }

  const bool write_file_result = (::WriteFile(
      device_handle, buf, static_cast<DWORD>(buf_length),
      &num_bytes_written, &overlapped) != FALSE);
  const DWORD write_file_error = ::GetLastError();
  if (write_file_result) {
    // ::WriteFile is done as sync operation.
  } else {
    if (write_file_error != ERROR_IO_PENDING) {
      LOG(ERROR) << "WriteFile() failed: " << write_file_error;
      *last_ipc_error = IPC_WRITE_ERROR;
      return false;
    }
    if (!SafeWaitOverlappedResult(
            write_wait_handle, NULL, timeout, &overlapped,
            &num_bytes_written, last_ipc_error, kSendTypeData)) {
      return false;
    }
  }

  // As we use message-type namedpipe, all the data should be written in one
  // shot. Otherwise, single message will be split into multiple packets.
  if (num_bytes_written != buf_length) {
    LOG(ERROR) << "Data truncated. buf_length: " << buf_length
               << ", num_bytes_written: " << num_bytes_written;
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }
  return true;
}

bool RecvIPCMessage(HANDLE device_handle, HANDLE read_wait_handle, char *buf,
                    size_t *buf_length, int timeout, bool read_type_ack,
                    IPCErrorType *last_ipc_error) {
  if (*buf_length == 0) {
    LOG(WARNING) << "buf length is 0";
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }

  OVERLAPPED overlapped;
  if (!InitOverlapped(&overlapped, read_wait_handle)) {
    *last_ipc_error = IPC_READ_ERROR;
    return false;
  }

  DWORD num_bytes_read = 0;
  const bool read_file_result = (::ReadFile(
      device_handle, buf, static_cast<DWORD>(*buf_length), &num_bytes_read,
      &overlapped) != FALSE);
  const DWORD read_file_error = ::GetLastError();
  if (read_file_result) {
    // ::ReadFile is done as sync operation.
  } else {
    if (read_type_ack && (read_file_error == ERROR_BROKEN_PIPE)) {
      // The client has already disconnected this pipe. This is an expected
      // behavior and do not treat as an error.
      return true;
    }
    if (read_file_error != ERROR_IO_PENDING) {
      LOG(ERROR) << "ReadFile() failed: " << read_file_error;
      *last_ipc_error = IPC_READ_ERROR;
      return false;
    }
    // Actually this is an async operation. Let's wait for its completion.
    if (!SafeWaitOverlappedResult(
            read_wait_handle, NULL, timeout, &overlapped,
            &num_bytes_read, last_ipc_error, read_type_ack)) {
      return false;
    }
  }

  if (!read_type_ack && (num_bytes_read == 0)) {
    LOG(WARNING) << "Received 0 result.";
  }

  *buf_length = num_bytes_read;

  return true;
}

HANDLE CreateManualResetEvent() {
  return ::CreateEvent(NULL, TRUE, FALSE, NULL);
}

// We do not care about the signaled state of the device handle itself.
// This slightly improves the performance.
// See http://msdn.microsoft.com/en-us/library/windows/desktop/aa365538.aspx
void MaybeDisableFileCompletionNotification(HANDLE device_handle) {
  CallOnce(&g_once, &InitAPIsForVistaAndLater);
  if (g_set_file_completion_notification_modes != NULL) {
    // This is not a mandatory task. Just ignore the actual error (if any).
    g_set_file_completion_notification_modes(device_handle,
                                             FILE_SKIP_SET_EVENT_ON_HANDLE);
  }
}

}  // namespace

IPCServer::IPCServer(const string &name,
                     int32 num_connections,
                     int32 timeout)
    : connected_(false),
      pipe_event_(CreateManualResetEvent()),
      quit_event_(CreateManualResetEvent()),
      timeout_(timeout) {
  IPCPathManager *manager = IPCPathManager::GetIPCPathManager(name);
  string server_address;

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
  if (!WinSandbox::MakeSecurityAttributes(&security_attributes)) {
    LOG(ERROR) << "Cannot make SecurityAttributes";
    return;
  }

  // Create a named pipe.
  wstring wserver_address;
  Util::UTF8ToWide(server_address.c_str(), &wserver_address);
  HANDLE handle = ::CreateNamedPipe(wserver_address.c_str(),
                                    PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                    FILE_FLAG_FIRST_PIPE_INSTANCE,
                                    PIPE_TYPE_MESSAGE |
                                    PIPE_READMODE_MESSAGE |
                                    PIPE_WAIT,
                                    (num_connections <= 0
                                     ? PIPE_UNLIMITED_INSTANCES
                                     : num_connections),
                                    sizeof(request_),
                                    sizeof(response_),
                                    0,
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

IPCServer::~IPCServer() {
  Terminate();
}

bool IPCServer::Connected() const {
  return connected_;
}

void IPCServer::Terminate() {
  if (server_thread_.get() == NULL) {
    return;
  }

  if (!server_thread_->IsRunning()) {
    return;
  }

  if (!::SetEvent(quit_event_.get())) {
    LOG(ERROR) << "SetEvent failed";
  }

  // Close the named pipe.
  // This is a workaround for killing child thread
  if (server_thread_.get() != NULL) {
    server_thread_->Join();
    server_thread_->Terminate();
  }

  connected_ = false;
}

void IPCServer::Loop() {
  IPCErrorType last_ipc_error = IPC_NO_ERROR;

  int successive_connection_failure_count = 0;
  while (connected_) {
    OVERLAPPED overlapped;
    if (!InitOverlapped(&overlapped, pipe_event_.get())) {
      connected_ = false;
      return;
    }

    const HRESULT result = ::ConnectNamedPipe(pipe_handle_.get(), &overlapped);
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
        IPCErrorType ipc_error = IPC_NO_ERROR;
        if (!SafeWaitOverlappedResult(pipe_handle_.get(), quit_event_.get(),
                                      INFINITE, &overlapped, &ignored,
                                      &ipc_error, kReadTypeData)) {
          if (ipc_error == IPC_QUIT_EVENT_SIGNALED) {
            VLOG(1) << "Recived Conrol event from other thread";
            connected_ = false;
            return;
          }
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
    size_t request_size = sizeof(request_);
    if (RecvIPCMessage(pipe_handle_.get(), pipe_event_.get(),
                       &request_[0], &request_size, timeout_,
                       kReadTypeData, &last_ipc_error)) {
      size_t response_size = sizeof(response_);
      if (!Process(&request_[0], request_size,
                   &response_[0], &response_size)) {
        connected_ = false;
      }

      // When Process() returns 0 result, force to call DisconnectNamedPipe()
      // instead of checking ACK message
      if (response_size == 0) {
        LOG(WARNING) << "Process() return 0 result";
        ::DisconnectNamedPipe(pipe_handle_.get());
        continue;
      }

      // Send a response
      SendIPCMessage(pipe_handle_.get(), pipe_event_.get(),
                     &response_[0], response_size, timeout_, &last_ipc_error);
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
    char ack_request[1] = {0};
    size_t ack_request_size = 1;
    static const int kAckTimeout = 100;
    if (!RecvIPCMessage(pipe_handle_.get(), pipe_event_.get(),
                        ack_request, &ack_request_size, kAckTimeout,
                        kReadTypeACK, &last_ipc_error)) {
      // This case happens when the client did not recive the server's response
      // within timeout. Anyway we will close the connection so that the server
      // will not be blocked.
      LOG(WARNING) << "Client didn't respond within "
                   << kAckTimeout << " msec.";
    }
    ::DisconnectNamedPipe(pipe_handle_.get());
  }

  connected_ = false;
}

// old interface
IPCClient::IPCClient(const string &name)
    : pipe_event_(CreateManualResetEvent()),
      connected_(false),
      ipc_path_manager_(NULL),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, "");
}

IPCClient::IPCClient(const string &name, const string &server_path)
    : pipe_event_(CreateManualResetEvent()),
      connected_(false),
      ipc_path_manager_(NULL),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, server_path);
}

void IPCClient::Init(const string &name, const string &server_path) {
  last_ipc_error_ = IPC_NO_CONNECTION;

  // We should change the mutex based on which IPC server we will talk with.
  ScopedReleaseMutex ipc_mutex(GetClientMutex(name));

  if (ipc_mutex.get() == NULL) {
    LOG(ERROR) << "IPC mutex is not available";
  } else {
    const int kMutexTimeout = 10 * 1000;  // wait at most 10sec.
    switch (::WaitForSingleObject(ipc_mutex.get(), kMutexTimeout)) {
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
  if (manager == NULL) {
    LOG(ERROR) << "IPCPathManager::GetIPCPathManager failed";
    return;
  }

  ipc_path_manager_ = manager;

  // TODO(taku): enable them on Mac/Linux
#ifdef DEBUG
  const size_t kMaxTrial = 256;
#else
  const size_t kMaxTrial = 2;
#endif

  for (size_t trial = 0; trial < kMaxTrial; ++trial) {
    string server_address;
    if (!manager->LoadPathName() || !manager->GetPathName(&server_address)) {
      continue;
    }
    wstring wserver_address;
    Util::UTF8ToWide(server_address.c_str(), &wserver_address);

    const HANDLE handle = ::CreateFile(wserver_address.c_str(),
                                       GENERIC_READ | GENERIC_WRITE,
                                       0, NULL, OPEN_EXISTING,
                                       FILE_FLAG_OVERLAPPED |
                                       SECURITY_SQOS_PRESENT |
                                       SECURITY_IDENTIFICATION |
                                       SECURITY_EFFECTIVE_ONLY,
                                       NULL);
    const DWORD create_file_error = ::GetLastError();
    if (INVALID_HANDLE_VALUE != handle) {
      DWORD mode = PIPE_READMODE_MESSAGE;
      if (::SetNamedPipeHandleState(handle, &mode, NULL, NULL) == FALSE) {
        const DWORD set_namedpipe_handle_state_error = ::GetLastError();
        LOG(ERROR) << "SetNamedPipeHandleState failed. error: "
                   << set_namedpipe_handle_state_error;
        last_ipc_error_ = IPC_UNKNOWN_ERROR;
        return;
      }
      pipe_handle_.reset(handle);
      MaybeDisableFileCompletionNotification(pipe_handle_.get());
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
    const int kNamedPipeTimeout = 100000;   // 100 sec
#else
    const int kNamedPipeTimeout = 10000;    // 10 sec
#endif
    DLOG(ERROR) << "Server is busy. waiting for "
                << kNamedPipeTimeout << " msec";
    if (!::WaitNamedPipe(wserver_address.c_str(),
                         kNamedPipeTimeout)) {
      const DWORD wait_named_pipe_error = ::GetLastError();
      LOG(ERROR) << "WaitNamedPipe failed: " << wait_named_pipe_error;
      if ((trial + 1) == kMaxTrial) {
        last_ipc_error_ = IPC_TIMEOUT_ERROR;
        return;
      }
      continue;   // go 2nd trial
    }
  }
}

IPCClient::~IPCClient() {}

bool IPCClient::Connected() const {
  return connected_;
}

bool IPCClient::Call(const char *request, size_t request_size,
                     char *response, size_t *response_size,
                     int32 timeout) {
  last_ipc_error_ = IPC_NO_ERROR;
  if (!connected_) {
    LOG(ERROR) << "IPCClient is not connected";
    last_ipc_error_ = IPC_NO_CONNECTION;
    return false;
  }

  if (!SendIPCMessage(pipe_handle_.get(), pipe_event_.get(), request,
                      request_size, timeout, &last_ipc_error_)) {
    LOG(ERROR) << "SendIPCMessage() failed";
    return false;
  }

  if (!RecvIPCMessage(pipe_handle_.get(), pipe_event_.get(), response,
                      response_size, timeout, kReadTypeData,
                      &last_ipc_error_)) {
    LOG(ERROR) << "RecvIPCMessage() failed";
    return false;
  }

  // Instead of sending ACK message to Server, we simply disconnect the named
  // pile to notify that client can read the message successfully.
  connected_ = false;
  pipe_handle_.reset(INVALID_HANDLE_VALUE);

  return true;
}

}  // namespace mozc

#endif  // OS_WINDOWS
