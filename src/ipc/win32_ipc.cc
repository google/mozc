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
#include "base/util.h"
#include "base/win_sandbox.h"
#include "ipc/ipc_path_manager.h"

namespace mozc {
namespace {

typedef BOOL (WINAPI *FPGetNamedPipeServerProcessId)(HANDLE, PULONG);
FPGetNamedPipeServerProcessId g_get_named_pipe_server_process_id = NULL;

static once_t g_once = MOZC_ONCE_INIT;

void InitFPGetNamedPipeServerProcessId() {
  // We have to load the function pointer dynamically
  // as GetNamedPipeServerProcessId() is only available on Windows Vista.
  if (!Util::IsVistaOrLater()) {
    return;
  }

  VLOG(1) << "Initializing GetNamedPipeServerProcessId";
  // kernel32.dll must be loaded in client.
  const HMODULE lib = Util::GetSystemModuleHandle(L"kernel32.dll");
  if (lib == NULL) {
    LOG(ERROR) << "GetSystemModuleHandle for kernel32.dll failed.";
    return;
  }

  g_get_named_pipe_server_process_id =
      reinterpret_cast<FPGetNamedPipeServerProcessId>
      (::GetProcAddress(lib, "GetNamedPipeServerProcessId"));
}

class IPCClientMutex {
 public:
  IPCClientMutex() {
    // Make a kernel mutex object so that multiple ipc connections are
    // serialized here. In Windows, there is no useful way to serialize
    // the multiple connections to the single-thread named pipe server.
    // WaitForNamedPipe doesn't work for this propose as it just lets
    // clients know that the connection becomes "available" right now.
    // It doesn't mean that connection is available for the current
    /// thread. The "available" notification is sent to all waiting ipc
    // clients at the same time and only one client gets the connection.
    // This causes redundant and wasteful CreateFile calles.
    string mutex_name = kMutexPathPrefix;
    mutex_name += Util::GetUserSidAsString();
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

    if (ipc_mutex_.get() == NULL) {
      LOG(ERROR) << "CreateMutex failed: " << ::GetLastError();
      return;
    }

    // permit the access from a process runinning with low integrity level
    if (Util::IsVistaOrLater()) {
      WinSandbox::SetMandatoryLabelW(ipc_mutex_.get(), SE_KERNEL_OBJECT,
                                     SDDL_NO_EXECUTE_UP, SDDL_ML_LOW);
    }
  }

  virtual ~IPCClientMutex() {}

  HANDLE get() const {
    return ipc_mutex_.get();
  }

 private:
  ScopedHandle ipc_mutex_;
};

// RAII class for calling ReleaseMutex in destructor.
class ScopedReleaseMutex {
 public:
  explicit ScopedReleaseMutex(HANDLE handle)
      : handle_(handle) {}

  virtual ~ScopedReleaseMutex() {
    if (NULL != handle_) {
      ::ReleaseMutex(handle_);
    }
  }

  HANDLE get() const { return handle_; }
 private:
  HANDLE handle_;
};

uint32 GetServerProcessId(HANDLE handle) {
  CallOnce(&g_once, &InitFPGetNamedPipeServerProcessId);

  if (g_get_named_pipe_server_process_id == NULL) {
    return static_cast<uint32>(0);   // must be Windows XP
  }

  ULONG pid = 0;
  if ((*g_get_named_pipe_server_process_id)(handle, &pid) == 0) {
    LOG(ERROR) << "GetNamedPipeServerProcessId failed: " << ::GetLastError();
    return static_cast<uint32>(-1);   // always deny the connection
  }

  VLOG(1) << "Got server ProcessID: " << pid;

  return static_cast<uint32>(pid);
}

void SafeCancelIO(HANDLE handle, OVERLAPPED *overlapped) {
  if (::CancelIo(handle)) {
    // wait for the cancel to complete
    // ignore the result, as we're exiting anyway
    DWORD size = 0;
    ::GetOverlappedResult(handle, overlapped, &size, TRUE);
  } else {
    LOG(WARNING) << "Failed to CancelIo: " << ::GetLastError();
  }
}

bool SendIPCMessage(HANDLE handle,
                    const char *buf, size_t buf_length, int timeout,
                    IPCErrorType *last_ipc_error) {
  if (buf_length == 0) {
    LOG(WARNING) << "buf length is 0";
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }

  OVERLAPPED Overlapped;

  bool error = false;
  while (buf_length > 0) {
    ::ZeroMemory(&Overlapped, sizeof(Overlapped));
    if (!::WriteFile(handle, buf,
                     static_cast<DWORD>(buf_length), NULL, &Overlapped) &&
        ERROR_IO_PENDING != ::GetLastError()) {
      LOG(ERROR) << "WriteFile failed: " << ::GetLastError();
      *last_ipc_error = IPC_WRITE_ERROR;
      error = true;
      break;
    }

    if (WAIT_OBJECT_0 != ::WaitForSingleObject(handle, timeout)) {
      SafeCancelIO(handle, &Overlapped);
      LOG(WARNING) << "Write timeout: " << timeout;
      *last_ipc_error = IPC_TIMEOUT_ERROR;
      error = true;
      break;
    }

    DWORD size = 0;
    if (!::GetOverlappedResult(handle, &Overlapped, &size, FALSE)) {
      LOG(ERROR) << "GetOverlappedResult() failed: " << ::GetLastError();
      *last_ipc_error = IPC_UNKNOWN_ERROR;
      error = true;
      break;
    }
    buf_length -= size;
    buf += size;
  }

  return !error;
}

bool RecvIPCMessage(HANDLE handle, char *buf, size_t *buf_length, int timeout,
                    IPCErrorType *last_ipc_error) {
  if (*buf_length == 0) {
    LOG(WARNING) << "buf length is 0";
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }

  OVERLAPPED Overlapped;
  ::ZeroMemory(&Overlapped, sizeof(Overlapped));

  if (!::ReadFile(handle, buf,
                  static_cast<DWORD>(*buf_length), NULL, &Overlapped) &&
      ERROR_IO_PENDING != ::GetLastError()) {
    LOG(ERROR) << "ReadFile() failed: " << ::GetLastError();
    *last_ipc_error = IPC_READ_ERROR;
    return false;
  }

  if (WAIT_OBJECT_0 != ::WaitForSingleObject(handle, timeout)) {
    SafeCancelIO(handle, &Overlapped);
    LOG(WARNING) << "Read timeout: " << timeout;
    *last_ipc_error = IPC_TIMEOUT_ERROR;
    return false;
  }

  DWORD size = 0;
  if (!::GetOverlappedResult(handle, &Overlapped, &size, FALSE)) {
    LOG(ERROR) << "GetOverlappedResult() failed: " << ::GetLastError();
    *last_ipc_error = IPC_UNKNOWN_ERROR;
    return false;
  }

  if (size <= 0) {
    LOG(WARNING) << "Received 0 result. ignored";
  }

  *buf_length = size;

  return true;
}
}  // namespace

IPCServer::IPCServer(const string &name,
                     int32 num_connections,
                     int32 timeout)
    : connected_(false),
      event_(::CreateEvent(NULL, TRUE, FALSE, NULL)),
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

  if (INVALID_HANDLE_VALUE == handle) {
    LOG(FATAL) << "CreateNamedPipe failed" << ::GetLastError();
    return;
  }

  handle_.reset(handle);

  ::LocalFree(security_attributes.lpSecurityDescriptor);

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

  if (!::SetEvent(event_.get())) {
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

  while (connected_) {
    OVERLAPPED Overlapped;
    ::ZeroMemory(&Overlapped, sizeof(Overlapped));
    const HRESULT result = ::ConnectNamedPipe(handle_.get(), &Overlapped);
    // ConnectNamedPipe always return 0 with Overlapped IO
    CHECK_EQ(0, result);

    DWORD size = 0;
    const DWORD last_error = ::GetLastError();
    // WaitForMultipleObjects returns the smallest index value of all the
    // signaled objects if bWaitAll parameter is FALSE.  Thus we should sort
    // the handle array by their priorities.
    HANDLE handles[2] = { event_.get(), handle_.get() };

    switch (last_error) {
      case ERROR_IO_PENDING:  // wait for incoming message
        switch (::WaitForMultipleObjects(2, handles, FALSE, INFINITE)) {
          case WAIT_OBJECT_0:
            LOG(WARNING) << "Recived Conrol event from other thread";
            SafeCancelIO(handle_.get(), &Overlapped);
            connected_ = false;
            return;
          case WAIT_OBJECT_0 + 1:
            break;
          default:
            LOG(FATAL) << "::WaitForMultipleObjects() failed: "
                       << GetLastError();
            SafeCancelIO(handle_.get(), &Overlapped);
            return;
        }
        if (!::GetOverlappedResult(handle_.get(), &Overlapped, &size, FALSE)) {
          LOG(FATAL) << "::GetOverlappedResult() failed: " << GetLastError();
          return;
        }
        break;
      case ERROR_NO_DATA:  // client already closes the connection
        ::DisconnectNamedPipe(handle_.get());
        continue;
        break;
      case ERROR_PIPE_CONNECTED:  // client is sending requests
        break;
      default:
        LOG(FATAL) << "::ConnectNamedPipe() failed: " << GetLastError();
        break;
    }

    // Retrieve an incoming message.
    size_t request_size = sizeof(request_);
    if (RecvIPCMessage(handle_.get(), &request_[0], &request_size, timeout_,
                       &last_ipc_error)) {
      size_t response_size = sizeof(response_);
      if (!Process(&request_[0], request_size,
                   &response_[0], &response_size)) {
        connected_ = false;
      }

      // When Process() returns 0 result, force to call DisconnectNamedPipe()
      // instead of checking ACK message
      if (response_size == 0) {
        LOG(WARNING) << "Process() return 0 result";
        ::DisconnectNamedPipe(handle_.get());
        continue;
      }

      // Send a response
      SendIPCMessage(handle_.get(), &response_[0], response_size,
                     timeout_, &last_ipc_error);
    }

    // Special treatment for Windows per discussion with thatanaka:
    // It's hard to know that client has processed the server's response.
    // We will be able to call ::FlushFileHandles() here, but FlushFileHandles()
    // is blocked if client doesn't call ReadFile(). That means that
    // a malicious user can easily block the server not by calling ReadFile.
    // In order to know the transaction completes successfully, client needs to
    // send an ACK message to the server.

    // Wait ACK message from client for 0.1 second
    char ack_request[1] = {0};
    size_t ack_request_size = 1;
    static const int kAckTimeout = 100;
    if (!RecvIPCMessage(handle_.get(), ack_request,
                        &ack_request_size, kAckTimeout, &last_ipc_error)) {
      // This case happens
      // 1) Client did not send an ACK but close the handle.
      // 2) Client did not recive the server's response within timeout
      // In ether case, we can safly close the connection.
      LOG(WARNING) << "Client doesn't send ACK message";
    }

    // Can safely close the handle as server can recive an ACK message.
    ::DisconnectNamedPipe(handle_.get());
  }

  connected_ = false;
}

// old interface
IPCClient::IPCClient(const string &name)
    : connected_(false),
      ipc_path_manager_(NULL),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, "");
}

IPCClient::IPCClient(const string &name, const string &server_path)
    : connected_(false),
      ipc_path_manager_(NULL),
      last_ipc_error_(IPC_NO_ERROR) {
  Init(name, server_path);
}

void IPCClient::Init(const string &name, const string &server_path) {
  last_ipc_error_ = IPC_NO_CONNECTION;

  // TODO(taku): ICPClientMutex doesn't take IPC path name into consideration.
  // Currently, it is not a critical problem, as we only have single
  // channel (session).
  ScopedReleaseMutex ipc_mutex(Singleton<IPCClientMutex>::get()->get());

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
#ifdef _DEBUG
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

    HANDLE handle = ::CreateFile(wserver_address.c_str(),
                                 GENERIC_READ | GENERIC_WRITE,
                                 0, NULL, OPEN_EXISTING,
                                 FILE_FLAG_OVERLAPPED |
                                 SECURITY_SQOS_PRESENT |
                                 SECURITY_IDENTIFICATION |
                                 SECURITY_EFFECTIVE_ONLY,
                                 NULL);

    if (INVALID_HANDLE_VALUE != handle) {
      handle_.reset(handle);
      if (!manager->IsValidServer(mozc::GetServerProcessId(handle_.get()),
                                  server_path)) {
        LOG(ERROR) << "Connecting to invalid server";
        last_ipc_error_ = IPC_INVALID_SERVER;
        return;
      }

      last_ipc_error_ = IPC_NO_ERROR;
      connected_ = true;
      return;
    } else {
      DLOG(ERROR) << "CreateFile failed: " << ::GetLastError();
    }

    if (ERROR_PIPE_BUSY != ::GetLastError()) {
      LOG(ERROR) << "Server is not running: " << ::GetLastError();
      manager->Clear();
      continue;
    }

    // wait for 10 second until server is ready
    // TODO(taku): control the timeout via flag.
#ifdef _DEBUG
    const int kNamedPipeTimeout = 100000;   // 100 sec
#else
    const int kNamedPipeTimeout = 10000;    // 10 sec
#endif
    DLOG(ERROR) << "Server is busy. waiting for "
                << kNamedPipeTimeout << " msec";
    if (!::WaitNamedPipe(wserver_address.c_str(),
                         kNamedPipeTimeout)) {
      LOG(ERROR) << "WaitNamedPipe failed: " << ::GetLastError();
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

  if (!SendIPCMessage(handle_.get(), request, request_size, timeout,
                      &last_ipc_error_)) {
    LOG(ERROR) << "SendIPCMessage() failed";
    return false;
  }

  if (!RecvIPCMessage(handle_.get(), response, response_size, timeout,
                      &last_ipc_error_)) {
    LOG(ERROR) << "RecvIPCMessage() failed";
    return false;
  }

  // Client sends an ACK message to Server to notify that
  // client can read the message successfully.
  const char kAckMessage[1] = { 0 };
  IPCErrorType unused_error_type;
  SendIPCMessage(handle_.get(), kAckMessage, 1, timeout, &unused_error_type);

  return true;
}
}  // namespace mozc

#endif  // OS_WINDOWS
