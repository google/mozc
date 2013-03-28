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

#ifndef MOZC_NET_HTTP_CLIENT_H_
#define MOZC_NET_HTTP_CLIENT_H_

#include <string>
#include <vector>

#include "base/base.h"
#include "base/port.h"

namespace mozc {

// Simple synchronous HTTP client
//
// Usage:
// 1) Simple GET
// string output;
// CHECK(mozc::HTTPClient::Get("http://www.google.com/", &output));
//
// 2) GET with some manual options.
// mozc::HTTPClient::Option option;
// option.timeout = 200;     // get data within 200msec
// option.headers.push_back("Host: foo.bar.com"); // replace/add header
// CHECK(mozc::HTTPClient::Get("http://www.google.com/", option, &output));
//
// 3) Rewrite an actual implementation (for unittesting)
// class MyHandler: public HTTPClientInterface { .. };
// MyHandler handler;
// mozc::HTTPClient::SetHTTPClientInterface(handler);
class HTTPClientInterface;

class HTTPClient {
 public:
  struct Option {
    bool   include_header;       // include header in output
    size_t max_data_size;        // maximum data size client can retrieve
    int32  timeout;              // timeout
    vector<string> headers;      // additional headers;

    // set default parameter
    Option()
        : include_header(false),
          max_data_size(10 * 1024 * 1024),  // 10Mbyte
          timeout(600000) {}  // 10min
  };

  // Synchronous HTTP GET, HEAD and POST

  // string interface
  static bool Get(const string &url, string *output);
  static bool Head(const string &url, string *output);
  static bool Post(const string &url, const string &data, string *output);

  static bool Get(const string &url, const Option &option, string *output);
  static bool Head(const string &url, const Option &option, string *output);
  static bool Post(const string &url, const string &data,
                   const Option &option, string *output);

  // Inject a dependency for unittesting
  static void SetHTTPClientHandler(const HTTPClientInterface *handler);

 private:
  HTTPClient() {}
  virtual ~HTTPClient() {}
};

// Interface class for injecting an actual implementation
class HTTPClientInterface {
 public:
  HTTPClientInterface() {}
  virtual ~HTTPClientInterface() {}

  virtual bool Get(const string &url, const HTTPClient::Option &option,
                   string *output) const = 0;
  virtual bool Head(const string &url, const HTTPClient::Option &option,
                    string *output) const = 0;
  virtual bool Post(const string &url, const string &data,
                    const HTTPClient::Option &option,
                    string *output) const = 0;
};
}  // namespace mozc
#endif  // MOZC_NET_HTTP_CLIENT_H_
