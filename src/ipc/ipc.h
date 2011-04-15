// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_IPC_IPC_H_
#define MOZC_IPC_IPC_H_

#ifdef OS_WINDOWS
#include "windows.h"
#endif

#ifdef OS_MACOSX
#include <mach/mach.h>
#endif  // OS_OS_MACOSX

#include <string>
#include "base/scoped_handle.h"
#include "base/thread.h"
#include "base/base.h"

namespace mozc {

enum {
  IPC_REQUESTSIZE = 16 * 8192,
  IPC_RESPONSESIZE = 16 * 8192,
};

// increment this value if protocol has changed.
enum {
  IPC_PROTOCOL_VERSION = 3,
};

enum IPCErrorType {
  IPC_NO_ERROR,
  IPC_NO_CONNECTION,
  IPC_TIMEOUT_ERROR,
  IPC_READ_ERROR,
  IPC_WRITE_ERROR,
  IPC_INVALID_SERVER,
  IPC_UNKNOWN_ERROR,
  IPC_ERROR_TYPE_SIZE
};

class IPCServerThread;

class IPCClientInterface {
 public:
  virtual ~IPCClientInterface();

  virtual bool Connected() const = 0;
  virtual bool Call(const char *request,
                    size_t request_size,
                    char *response,
                    size_t *response_size,
                    int32 timeout) = 0;

  virtual uint32 GetServerProtocolVersion() const = 0;
  virtual const string &GetServerProductVersion() const = 0;
  virtual uint32 GetServerProcessId() const = 0;

  // return last error
  virtual IPCErrorType GetLastIPCError() const = 0;
};

#ifdef OS_MACOSX
class MachPortManagerInterface {
 public:
  virtual ~MachPortManagerInterface() {}

  // If the mach port can be obtained successfully, set the specified
  // "port" and returns true.  Otherwise port doesn't change and
  // returns false.
  virtual bool GetMachPort(const string &name, mach_port_t *port) = 0;

  // Returns true if the connecting server is running, checked via
  // OS-depended way.  This method can be defined differently for testing.
  virtual bool IsServerRunning(const string &name) const = 0;
};
#endif  // OS_MACOSX

class IPCPathManager;

// Synchronous, Single-thread IPC Client
// Usage:
//  IPCClient con("name", "/foo/bar/server");
//  string request = "foo";
//  char result[32];
//  uint32 size = sizeof(result);
//  CHECK(con.Connected());
//  CHECK(con.Call(request.data(),
//                 request.size(),
//                 result, &size 1000);
//                 // wait for 1000msec
class IPCClient : public IPCClientInterface {
 public:
  // connect to an IPC server named "name".
  // You can pass an absolute server_path to make sure
  // the client is connecting a valid server.
  // If server_path is empty, no validation is executed.
  IPCClient(const string &name, const string &server_path);

  // old interface
  // same as IPCClient(name, "");
  explicit IPCClient(const string &name);

  virtual ~IPCClient();

  // Return true if the connection is available
  bool Connected() const;

  // Return server protocol version
  uint32 GetServerProtocolVersion() const;

  const string &GetServerProductVersion() const;

  uint32 GetServerProcessId() const;


  // Synchronous IPC call:
  // Client request is encoded in 'request' whose size is request_size.
  // Server response is stored in 'response'.
  // Need to pass the maximum size of response before calling IPC.
  // Return true when IPC finishes successfully.
  // When Server doesn't send response within timeout, 'Call' returns false.
  // When timeout (in msec) is set -1, 'Call' waits forever.
  // Note that on Linux, Call() closes the socket_. This means you cannot call
  // the Call() function more than once.
  bool Call(const char *request,
            size_t request_size,
            char *response,
            size_t *response_size,
            int32 timeout);  // msec

  IPCErrorType GetLastIPCError() const {
    return last_ipc_error_;
  }

  // terminate the server process named |name|
  // Do not use it unless version mismatch happens
  static bool TerminateServer(const string &name);

#ifdef OS_MACOSX
  void SetMachPortManager(MachPortManagerInterface *manager) {
    mach_port_manager_ = manager;
  }
#endif

 private:
  void Init(const string &name, const string &server_path);

#ifdef OS_WINDOWS
  // Windows
  ScopedHandle handle_;
#elif defined(OS_MACOSX)
  string name_;
  MachPortManagerInterface *mach_port_manager_;
#else
  int socket_;
#endif
  bool connected_;
  IPCPathManager *ipc_path_manager_;
  IPCErrorType last_ipc_error_;
};

class IPCClientFactoryInterface {
 public:
  virtual ~IPCClientFactoryInterface();
  virtual IPCClientInterface *NewClient(const string &name,
                                        const string &path_name) = 0;

  // old interface for backward compatiblity.
  // same as NewClient(name, "");
  virtual IPCClientInterface *NewClient(const string &name) = 0;
};

// Creates IPCClient object.
class IPCClientFactory : public IPCClientFactoryInterface {
 public:
  virtual ~IPCClientFactory();

  // new inteface
  virtual IPCClientInterface *NewClient(const string &name,
                                        const string &path_name);

  // old interface for backward compatiblity.
  // same as NewClient(name, "");
  virtual IPCClientInterface *NewClient(const string &name);

  // Return a singleton instance.
  static IPCClientFactory *GetIPCClientFactory();
};

// Synchronous, Single-thread IPC Server
// Usage:
// class MyEchoServer: public IPCServer {
//  public:
//   virtual bool Process(const char *input, uint32 isize,
//                        char *output, uint32 *osize) {
//      implement a logic in Process
//      return true;
//   }
//  };
//  // listen 10 connections, and timeout is 1000msec
//  MyEchoServer server("/tmp/.soket", 10, 1000);
//  CHECK(server.Connected());
//  server.Loop();
class IPCServer {
 public:
  // Make IPCServer instance:
  // name: name of this server
  // num_connections: maximum number of connections per server.
  // timeout: After a client makes a connection, the client needs to
  //          send a request within 'timeout'. If timeout is -1,
  //          IPCServer waits forever. Default setting is -1.
  // TODO(taku): timeout is not implemented properly
  IPCServer(const string &name,
            int32 num_connections,
            int32 timeout);
  virtual ~IPCServer();

  // Return true if the connectoin is available
  bool Connected() const;

  // Implement a server algorithm in subclass.
  // If 'Process' return false, server finishes select loop
  virtual bool Process(const char *request,
                       size_t request_size,
                       char *response,
                       size_t *response_size) {
    return true;
  }

  // Start select loop. It goes into infinite loop.
  void Loop();

  // Start select loop and return immediately.
  // It invokes a thread internally.
  void LoopAndReturn();

  // Wait until the thread ends
  void Wait();

  // Terminate select loop from other thread
  // On Win32, we make a control event to terminate
  // main loop gracefully. On Mac/Linux, we simply
  // call TerminateThread()
#ifdef OS_WINDOWS
  void Terminate();
#else
  void Terminate() {
    server_thread_->Terminate();
  }
#endif

#ifdef OS_MACOSX
  void SetMachPortManager(MachPortManagerInterface *manager) {
    mach_port_manager_ = manager;
  }
#endif

 private:
  class IPCServerThread: public Thread {
   public:
    explicit IPCServerThread(IPCServer *server)
        : server_(server) {}
    virtual void Run() {
      if (server_ != NULL) {
        server_->Loop();
      }
    }
   private:
    IPCServer *server_;
  };

  char request_[IPC_REQUESTSIZE];
  char response_[IPC_RESPONSESIZE];
  bool connected_;
  scoped_ptr<IPCServerThread> server_thread_;

#ifdef OS_WINDOWS
  ScopedHandle handle_;  // Windows
  ScopedHandle event_;   // ctrl event
#elif defined(OS_MACOSX)
  string server_address_;
  string name_;
  MachPortManagerInterface *mach_port_manager_;
#else
  int socket_;
  string server_address_;
#endif

  int timeout_;
};

}   // namespace mozc
#endif  // MOZC_IPC_IPC_H_
