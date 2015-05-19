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

#include <string>

#include "net/http_client.h"

namespace mozc {

// TODO(team): Use gMock instead of this Mock class. Few tests should be
//     changed for the replacement.

// This is a mock for tests using HTTP requests.
//
// Usage:
// 1) GET method (option string is ignored)
// mozc::HTTPClientMock::Result result;
// result.expected_url = "http://www.google.com/";
// result.expected_result = "Hello, Google!";
// HTTPClientMock mock_client;
// mock_client.set_result(result);
// mozc::HTTPClient::SetHTTPClientHandler(&mock_client);
// string output;
// CHECK_TRUE(mozc::HTTPClient::Get("http://www.google.com/", &output));
// CHECK_EQ("Hello, Google!", output);
//
// 2) POST method
// mozc::HTTPClientMock::Result result;
// result.expected_url = "http://www.google.com/";
// result.expected_request = "q=google";
// result.expected_result = "Hello, Google!";
// HTTPClientMock mock_client;
// mock_client.set_result(result);
// mozc::HTTPClient::SetHTTPClientHandler(&mock_client);
// string output;
// CHECK_TRUE(mozc::HTTPClient::Post("http://www.google.com/", "q=google",
//                                   &output));
// CHECK_EQ("Hello, Google!", output);

// TODO(hsumita): Currently, HTTPClient::Option is ignored.
//                Let's check it to write test. (b/4509250)
class HTTPClientMock : public HTTPClientInterface {
 public:
  HTTPClientMock() : failure_mode_(false), execution_time_(0) {}

  bool Get(const string &url, const HTTPClient::Option &option,
           string *output) const;
  bool Head(const string &url, const HTTPClient::Option &option,
            string *output) const;
  bool Post(const string &url, const string &data,
            const HTTPClient::Option &option, string *output) const;

  struct Result {
    string expected_url;
    string expected_request;
    string expected_result;
  };

  // HTTP request fails if failure_mode is true.
  void set_failure_mode(bool failure_mode) {
    failure_mode_ = failure_mode;
  }

  void set_result(const Result &result) {
    result_ = result;
  }
  void set_option(const HTTPClient::Option &option) {
    option_ = option;
  }

  void set_execution_time(int execution_time) {
    execution_time_ = execution_time;
  }

 private:
  // Check if url or data in body field are as expectations and returns
  // if all of them are correct.  |header| should have a HTTP response header,
  // but it gets other strings for simplicity.
  // |body| will have a HTTP response content.
  bool DoRequest(const string &url, const bool check_data,
                 const string &data, const HTTPClient::Option &option,
                 string *header, string *output) const;
  bool failure_mode_;
  Result result_;
  int execution_time_;
  HTTPClient::Option option_;
};

};  // namespace mozc
