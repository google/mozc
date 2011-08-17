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

#ifndef MOZC_CONVERTER_CONNECTOR_H_
#define MOZC_CONVERTER_CONNECTOR_H_

#include "base/base.h"

namespace mozc {

class ConnectorInterface {
 public:
  virtual int GetTransitionCost(uint16 rid, uint16 lid) const = 0;
  // Test code can use this method to get acceptable error.
  virtual int GetResolution() const = 0;

  static const int16 kInvalidCost = 30000;

 protected:
  ConnectorInterface() {}
  virtual ~ConnectorInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectorInterface);
};

// The result of GetTransitionCost() is cached with TLS.
// Note that the cache is created as a global variable. If you
// pass two different connectors, the cache variable will be shared.
// Since we can assume that real Connector is a singleton object,
// this restriction will not be a big issue.
class CachedConnector : public ConnectorInterface {
 public:
  CachedConnector(ConnectorInterface *connector);
  ~CachedConnector();

  virtual int GetTransitionCost(uint16 rid, uint16 lid) const;
  virtual int GetResolution() const;

  // Clear cache explicitly.
  static void ClearCache();

 private:
  void InitializeCache() const;
  ConnectorInterface *connector_;
};

class ConnectorFactory {
 public:
  // return singleton object
  static ConnectorInterface *GetConnector();

  // dependency injection for unittesting
  static void SetConnector(ConnectorInterface *connector);
};
}  // namespace mozc

#endif  // MOZC_CONVERTER_CONNECTOR_H_
