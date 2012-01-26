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

#include <string>

#include "net/http_client.h"
#include "net/http_client_mock.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

namespace mozc {

class HTTPClientMockTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SetDefaultResult();
    client_.set_option(HTTPClient::Option());
  }

  void SetDefaultResult() {
    HTTPClientMock::Result result;
    result.expected_url = "url";
    result.expected_request = "request";
    result.expected_result = "result";
    client_.set_result(result);
  }

  void SetDefaultOption() {
    HTTPClient::Option option;
    option.include_header = true;
    option.headers.push_back("header0");
    option.headers.push_back("header1");
    client_.set_option(option);
  }

  HTTPClientMock client_;
};

TEST_F(HTTPClientMockTest, SimpleGet) {
  string output;
  EXPECT_TRUE(client_.Get("url", &output));
  EXPECT_EQ("result", output);
}

TEST_F(HTTPClientMockTest, SimplePost) {
  string output;
  EXPECT_TRUE(client_.Post("url", "request", &output));
  EXPECT_EQ("result", output);
  EXPECT_FALSE(client_.Post("url", "request&extra", &output));
}

TEST_F(HTTPClientMockTest, GetWithOption) {
  SetDefaultOption();
  EXPECT_TRUE(true);

  string output;
  HTTPClient::Option option;
  option.headers.push_back("header0");
  EXPECT_FALSE(client_.Get("url", option, &output));
  option.headers.push_back("header1");
  EXPECT_TRUE(client_.Get("url", option, &output));
  // Header field can have external line ohter than expected
  option.headers.push_back("header2");
  EXPECT_TRUE(client_.Get("url", option, &output));
}

TEST_F(HTTPClientMockTest, PostWithOption) {
  SetDefaultOption();
  string output;
  HTTPClient::Option option;
  option.include_header = true;
  option.headers.push_back("header0");
  EXPECT_FALSE(client_.Post("url", "request", option, &output));
  option.headers.push_back("header1");
  EXPECT_TRUE(client_.Post("url", "request", option, &output));
  // Header field can have external lines ohter than expected
  option.headers.push_back("header2");
  EXPECT_TRUE(client_.Post("url", "request", option, &output));
}

}  // namespace mozc
