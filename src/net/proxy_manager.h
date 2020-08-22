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

#ifndef MOZC_NET_PROXY_MANAGER_H_
#define MOZC_NET_PROXY_MANAGER_H_

#include <string>

#include "base/port.h"

namespace mozc {
class ProxyManagerInterface;

// ProxyManager is a class to access the OS preferences and to use the
// extracted proxy configuration for http_client.  Therefore
// ProxyManager highly depends on OS-provided mechanism, so the
// implementation differs according to the platform.
// TODO(mukai): Currently ProxyManager doesn't work on Linux.
class ProxyManager {
 public:
  // Get the system configuration proxy info.  Stores the hostname /
  // port number to "hostdata" (scheme://host:port format) and
  // username / password to "authdata" (username:password format).
  // Returns true if the system actually uses a proxy.  Returns false
  // otherwise.
  static bool GetProxyData(const std::string &url, std::string *hostdata,
                           std::string *authdata);

  // Inject a dependency for unittesting
  static void SetProxyManager(ProxyManagerInterface *proxy_manager);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ProxyManager);
};

// Implements ProxyManager implementation.
class ProxyManagerInterface {
 public:
  virtual bool GetProxyData(const std::string &url, std::string *hostdata,
                            std::string *authdata) = 0;

  ProxyManagerInterface();
  virtual ~ProxyManagerInterface();
};

// Dummy proxy manager is a proxy manager which does nothing.  This
// class is the default proxy manager for Windows/Linux.
class DummyProxyManager : public ProxyManagerInterface {
 public:
  virtual bool GetProxyData(const std::string &url, std::string *hostdata,
                            std::string *authdata);
};

}  // namespace mozc

#endif  // MOZC_NET_PROXY_MANAGER_H_
