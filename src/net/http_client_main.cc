// Copyright 2010, Google Inc.
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
#include <iostream>
#include <fstream>
#include "base/base.h"
#include "base/mmap.h"
#include "net/http_client.h"
#include "net/proxy_manager.h"

DEFINE_string(url, "", "url");
DEFINE_string(method, "GET", "method");
DEFINE_string(post_data, "", "post data");
DEFINE_string(post_data_file, "", "post_data_file");
DEFINE_string(output, "", "output file");
DEFINE_int32(max_data_size, 10 * 1024 * 1024, "maximum data size");
DEFINE_int32(timeout, 60000, "connection timeout");
DEFINE_bool(include_header, false, "include header in output");
DEFINE_bool(use_proxy, true, "use the proxy or not");

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::HTTPClient::Option option;
  option.include_header = FLAGS_include_header;
  option.max_data_size = FLAGS_max_data_size;
  option.timeout = FLAGS_timeout;

  mozc::DummyProxyManager dummy_proxy;
  if (!FLAGS_use_proxy) {
    mozc::ProxyManager::SetProxyManager(&dummy_proxy);
  }

  const char *post_data = NULL;
  size_t post_size = 0;

  if (!FLAGS_post_data.empty()) {
    post_data = FLAGS_post_data.data();
    post_size = FLAGS_post_data.size();
  } else if (!FLAGS_post_data_file.empty()) {
    mozc::Mmap<char> mmap;
    CHECK(mmap.Open(FLAGS_post_data_file.c_str()));
    post_data = mmap.begin();
    post_size = mmap.GetFileSize();
  }

  if (FLAGS_output.empty()) {
    bool ret = false;
    string output;
    if (FLAGS_method == "GET") {
      ret = mozc::HTTPClient::Get(FLAGS_url, option, &output);
    } else if (FLAGS_method == "HEAD") {
      ret = mozc::HTTPClient::Head(FLAGS_url, option, &output);
    } else if (FLAGS_method == "POST") {
      ret = mozc::HTTPClient::Post(FLAGS_url,
                                   post_data, post_size,
                                   option, &output);
    }

    if (ret) {
      std::cout << output << std::endl;
    }
  } else {
    ofstream output(FLAGS_output.c_str(), ios::out | ios::binary);
    bool ret = false;
    if (FLAGS_method == "GET") {
      ret = mozc::HTTPClient::Get(FLAGS_url, option, &output);
    } else if (FLAGS_method == "HEAD") {
      ret = mozc::HTTPClient::Head(FLAGS_url, option, &output);
    } else if (FLAGS_method == "POST") {
      ret = mozc::HTTPClient::Post(FLAGS_url,
                                   post_data, post_size,
                                   option, &output);
    }
  }

  return 0;
}
