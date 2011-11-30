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

#include "net/http_client_mock.h"

#include <string>

#include "base/base.h"
#include "base/util.h"
#include "net/http_client.h"

namespace mozc {

bool HTTPClientMock::Get(const string &url, string *output) const {
  return DoRequest(url, false, "", false, HTTPClient::Option(), output);
}

bool HTTPClientMock::Head(const string &url, string *output) const {
  return DoRequest(url, false, "", false, HTTPClient::Option(), output);
}

bool HTTPClientMock::Post(const string &url, const string &data,
                          string *output) const {
  return DoRequest(url, true, data, false, HTTPClient::Option(), output);
}

bool HTTPClientMock::Get(const string &url, const HTTPClient::Option &option,
                         string *output) const {
  return DoRequest(url, false, "", true, option, output);
}

bool HTTPClientMock::Head(const string &url, const HTTPClient::Option &option,
                          string *output) const {
  return DoRequest(url, false, "", true, option, output);
}

bool HTTPClientMock::Post(const string &url, const string &data,
                          const HTTPClient::Option &option,
                          string *output) const {
  return DoRequest(url, true, data, true, option, output);
}

bool HTTPClientMock::Post(const string &url, const char *data,
                          size_t data_size, const HTTPClient::Option &option,
                          string *output) const {
  string trimed_data = string(data, data_size);
  return DoRequest(url, true, trimed_data, true, option, output);
}

bool HTTPClientMock::Get(const string &url, ostream *stream) const {
  return DoRequestWithStream(url, false, "", false, HTTPClient::Option(),
                             stream);
}

bool HTTPClientMock::Head(const string &url, ostream *stream) const {
  return DoRequestWithStream(url, false, "", false, HTTPClient::Option(),
                             stream);
}

bool HTTPClientMock::Post(const string &url, const string &data,
                          ostream *stream) const {
  return DoRequestWithStream(url, true, data, false, HTTPClient::Option(),
                             stream);
}

bool HTTPClientMock::Get(const string &url, const HTTPClient::Option &option,
                         ostream *stream) const {
  return DoRequestWithStream(url, false, "", true, option, stream);
}

bool HTTPClientMock::Head(const string &url, const HTTPClient::Option &option,
                          ostream *stream) const {
  return DoRequestWithStream(url, false, "", true, option, stream);
}

bool HTTPClientMock::Post(const string &url, const string &data,
                          const HTTPClient::Option &option,
                          ostream *stream)  const {
  return DoRequestWithStream(url, true, data, true, option, stream);
}

bool HTTPClientMock::Post(const string &url, const char *data,
                          size_t data_size, const HTTPClient::Option &option,
                          ostream *stream) const {
  const string trimed_data = string(data, data_size);
  return DoRequestWithStream(url, true, trimed_data, true, option, stream);
}

bool HTTPClientMock::DoRequest(const string &url, const bool check_data,
                               const string &data, const bool check_option,
                               const HTTPClient::Option &option,
                               string *output) const {
  if (execution_time_ > 0) {
    Util::Sleep(execution_time_);
  }

  if (failure_mode_) {
    VLOG(2) << "failure mode";
    return false;
  }

  if (result_.expected_url != url) {
    LOG(WARNING) << "Expected URL is not same as Actual URL";
    LOG(WARNING) << "  expected: " << result_.expected_url;
    LOG(WARNING) << "  actual:   " << url;
    return false;
  }

  // Check BODY field of access
  if (check_data && result_.expected_request != data) {
    LOG(WARNING) << "Expected request is not same as actual request";
    LOG(WARNING) << "  expected: " << result_.expected_request;
    LOG(WARNING) << "  actual:   " << data;
    return false;
  }

  if (!check_option) {
    output->assign(result_.expected_result);
    return true;
  }

  // Check HTTP header field
  for (int i = 0; i < option_.headers.size(); ++i) {
    bool match = false;
    for (int j = 0; !match && j < option.headers.size(); ++j) {
      match = (option_.headers[i] == option.headers[j]);
    }
    if (!match) {
      return false;
    }
  }

  output->assign(result_.expected_result);
  return true;
}

bool HTTPClientMock::DoRequestWithStream(const string &url,
                                         const bool check_body,
                                         const string &data,
                                         const bool check_option,
                                         const HTTPClient::Option &option,
                                         ostream *stream) const {
  string output;
  bool result = DoRequest(url, check_body, data, check_option, option,
                          &output);
  *stream << output;
  return result;
}

};  // namespace mozc
