// Copyright 2010-2020, Google Inc.
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

#include "net/http_client_mock.h"

#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "net/http_client.h"

namespace mozc {
bool HTTPClientMock::Get(const std::string &url,
                         const HTTPClient::Option &option,
                         std::string *output) const {
  std::string header;
  return DoRequest(url, false, "", option, &header, output);
}

bool HTTPClientMock::Head(const std::string &url,
                          const HTTPClient::Option &option,
                          std::string *output) const {
  std::string body;
  return DoRequest(url, false, "", option, output, &body);
}

bool HTTPClientMock::Post(const std::string &url, const std::string &data,
                          const HTTPClient::Option &option,
                          std::string *output) const {
  std::string header;
  return DoRequest(url, true, data, option, &header, output);
}

bool HTTPClientMock::DoRequest(const std::string &url, const bool check_data,
                               const std::string &data,
                               const HTTPClient::Option &option,
                               std::string *header, std::string *body) const {
  if (execution_time_ > 0) {
    Util::Sleep(execution_time_);
  }

  if (failure_mode_) {
    VLOG(2) << "failure mode";
    *header = "failure mode";
    return false;
  }

  if (result_.expected_url != url) {
    LOG(WARNING) << "Expected URL is not same as Actual URL";
    LOG(WARNING) << "  expected: " << result_.expected_url;
    LOG(WARNING) << "  actual:   " << url;
    *header = "wrong url";
    return false;
  }

  // Check BODY field of access
  if (check_data && result_.expected_request != data) {
    LOG(WARNING) << "Expected request is not same as actual request";
    LOG(WARNING) << "  expected: " << result_.expected_request;
    LOG(WARNING) << "  actual:   " << data;
    *header = "wrong parameters";
    return false;
  }

  // Check HTTP request header field
  for (int i = 0; i < option_.headers.size(); ++i) {
    bool match = false;
    for (int j = 0; !match && j < option.headers.size(); ++j) {
      match = (option_.headers[i] == option.headers[j]);
    }
    if (!match) {
      *header = "wrong header";
      return false;
    }
  }

  *body = result_.expected_result;
  *header = "OK";
  return true;
}

}  // namespace mozc
