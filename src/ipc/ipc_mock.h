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

#ifndef MOZC_IPC_IPC_MOCK_H_
#define MOZC_IPC_IPC_MOCK_H_

#include <string>
#include "ipc/ipc.h"

namespace mozc {

class IPCClientFactoryMock;  // declared below.

class IPCClientMock : public IPCClientInterface {
 public:
  IPCClientMock(IPCClientFactoryMock *caller);
  virtual bool Connected() const;
  virtual uint32 GetServerProtocolVersion() const;
  virtual const string &GetServerProductVersion() const;
  virtual uint32 GetServerProcessId() const;
  virtual bool Call(const char *request,
                    size_t request_size,
                    char *response,
                    size_t *response_size,
                    int32 timeout);

  virtual IPCErrorType GetLastIPCError() const {
    return IPC_NO_ERROR;
  }

  void set_connection(const bool connection) {
    connected_ = connection;
  }
  void set_result(const bool result) {
    result_ = result;
  }
  void set_server_protocol_version(const uint32 server_protocol_version) {
    server_protocol_version_ = server_protocol_version;
  }
  void set_server_product_version(const string &server_product_version) {
    server_product_version_ = server_product_version;
  }
  void set_server_process_id(const uint32 server_process_id) {
    server_process_id_ = server_process_id;
  }
  void set_response(const string &response) {
    response_ = response;
  }

 private:
  IPCClientFactoryMock* caller_;
  bool connected_;
  uint32 server_protocol_version_;
  string server_product_version_;
  uint32 server_process_id_;
  bool result_;
  string response_;

  DISALLOW_COPY_AND_ASSIGN(IPCClientMock);
};

class IPCClientFactoryMock : public IPCClientFactoryInterface {
 public:
  IPCClientFactoryMock();

  virtual IPCClientInterface *NewClient(const string &unused_name,
                                        const string &path_name);

  virtual IPCClientInterface *NewClient(const string &unused_name);

  // This function is supporsed to be used by unittests.
  const string &GetGeneratedRequest() const;

  // This function is supporsed to be used by IPCClientMock
  void SetGeneratedRequest(const string &request);

  // This function is supporsed to be used by unittests.
  void SetMockResponse(const string &response);

  // This function is supporsed to be used by unittests.
  void SetConnection(const bool connection);

  // This function is supporsed to be used by unittests.
  void SetResult(const bool result);

  // This function is supporsed to be used by unittests.
  void SetServerProtocolVersion(const uint32 server_protocol_version);

  // This function is supporsed to be used by unittests.
  void SetServerProductVersion(const string &server_protocol_version);

  // This function is supporsed to be used by unittests.
  void SetServerProcessId(const uint32 server_process_id);

 private:
  IPCClientMock *NewClientMock();

  bool connection_;
  bool result_;
  uint32 server_protocol_version_;
  string server_product_version_;
  uint32 server_process_id_;
  string request_;
  string response_;

  DISALLOW_COPY_AND_ASSIGN(IPCClientFactoryMock);
};

}  // namespace mozc
#endif  // MOZC_IPC_IPC_MOCK_H_
