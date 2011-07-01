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

#include <string>
#include "base/util.h"
#include "net/http_client.h"
#include "net/http_client_mock.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/upload_util.h"

namespace mozc {
namespace usage_stats {

TEST(UploadUtilTest, UploadTest) {
  HTTPClientMock client;
  HTTPClient::SetHTTPClientHandler(&client);
  const string base_url = "http://clients4.google.com/tbproxy/usagestats";
  {
    HTTPClientMock::Result result;
    result.expected_url = base_url + "?sourceid=ime&hl=ja&v=test";
    result.expected_request = "Test&100&Count:c=100";
    client.set_result(result);

    UploadUtil uploader;
    vector<pair<string, string> > params;
    params.push_back(make_pair("hl", "ja"));
    params.push_back(make_pair("v", "test"));
    uploader.SetHeader("Test", 100, params);
    uploader.AddCountValue("Count", 100);
    EXPECT_TRUE(uploader.Upload());

    uploader.RemoveAllValues();
    result.expected_request = "Test&100";
    client.set_result(result);
    EXPECT_TRUE(uploader.Upload());

    uploader.AddCountValue("Count", 1000);
    EXPECT_FALSE(uploader.Upload());
  }

  {
    HTTPClientMock::Result result;
    result.expected_url = base_url + "?sourceid=ime&hl=ja&v=test";
    result.expected_request = "Test&100&Timing:t=20;20;10;30";
    client.set_result(result);

    UploadUtil uploader;
    vector<pair<string, string> > params;
    params.push_back(make_pair("hl", "ja"));
    params.push_back(make_pair("v", "test"));
    uploader.SetHeader("Test", 100, params);
    uploader.AddTimingValue("Timing", 20, 20, 10, 30);
    EXPECT_TRUE(uploader.Upload());
  }

  {
    HTTPClientMock::Result result;
    result.expected_url = base_url + "?sourceid=ime&hl=ja&v=test";
    result.expected_request = "Test&100&Integer:i=-10";
    client.set_result(result);

    UploadUtil uploader;
    vector<pair<string, string> > params;
    params.push_back(make_pair("hl", "ja"));
    params.push_back(make_pair("v", "test"));
    uploader.SetHeader("Test", 100, params);
    uploader.AddIntegerValue("Integer", -10);
    EXPECT_TRUE(uploader.Upload());
  }

  {
    HTTPClientMock::Result result;
    result.expected_url = base_url + "?sourceid=ime&hl=ja&v=test";
    result.expected_request = "Test&100&Boolean:b=f";
    client.set_result(result);

    UploadUtil uploader;
    vector<pair<string, string> > params;
    params.push_back(make_pair("hl", "ja"));
    params.push_back(make_pair("v", "test"));
    uploader.SetHeader("Test", 100, params);
    uploader.AddBooleanValue("Boolean", false);
    EXPECT_TRUE(uploader.Upload());
  }

  {
    HTTPClientMock::Result result;
    result.expected_url = base_url + "?sourceid=ime&hl=ja&v=test";
    result.expected_request = "Test&100&C:c=1&T:t=1;1;1;1&I:i=1&B:b=t";
    client.set_result(result);

    UploadUtil uploader;
    vector<pair<string, string> > params;
    params.push_back(make_pair("hl", "ja"));
    params.push_back(make_pair("v", "test"));
    uploader.SetHeader("Test", 100, params);
    uploader.AddCountValue("C", 1);
    uploader.AddTimingValue("T", 1, 1, 1, 1);
    uploader.AddIntegerValue("I", 1);
    uploader.AddBooleanValue("B", true);
    EXPECT_TRUE(uploader.Upload());
  }
}
}  // namespace usage_stats
}  // namespace mozc
