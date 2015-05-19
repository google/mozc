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
#include <iostream>

#include "base/base.h"
#include "base/file_stream.h"
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

  if (!FLAGS_post_data_file.empty()) {
    char buffer[2048];
    mozc::InputFileStream ifs(FLAGS_post_data_file.c_str(),
                              ios::in | ios::binary);
    FLAGS_post_data = "";
    do {
      ifs.read(buffer, sizeof(buffer));
      FLAGS_post_data.append(buffer);
    } while (!ifs.eof());
  }

  bool ret = false;
  string output;
  if (FLAGS_method == "GET") {
    ret = mozc::HTTPClient::Get(FLAGS_url, option, &output);
  } else if (FLAGS_method == "HEAD") {
    ret = mozc::HTTPClient::Head(FLAGS_url, option, &output);
  } else if (FLAGS_method == "POST") {
    ret = mozc::HTTPClient::Post(FLAGS_url, FLAGS_post_data, option, &output);
  }

  std::cout << (ret ? "Request succeeded" : "Request failed") << std::endl;
  if (FLAGS_output.empty()) {
    // Do not output if the request failed not to break your terminal.
    if (ret) {
      std::cout << output << std::endl;
    }
  } else {
    mozc::OutputFileStream ofs(FLAGS_output.c_str(), ios::out | ios::binary);
    // Output even if the request failed.
    ofs << output << std::endl;
  }

  return 0;
}
