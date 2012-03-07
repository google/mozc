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

#ifndef MOZC_CONVERTER_CACHED_CONNECTOR_H_
#define MOZC_CONVERTER_CACHED_CONNECTOR_H_

#include "base/port.h"
#include "converter/connector_interface.h"

namespace mozc {
// The result of GetTransitionCost() is cached with TLS.
// Note that the cache is created as a global variable. If you
// pass two different connectors, the cache variable will be shared.
// Since we can assume that real Connector is a singleton object,
// this restriction will not be a big issue.
class CachedConnector : public ConnectorInterface {
 public:
  // Use cache that defined at other place.
  CachedConnector(ConnectorInterface *connector,
                  bool *cache_initialized,
                  int *cache_key,
                  int *cache_value,
                  int cache_size);
  virtual ~CachedConnector();

  virtual int GetTransitionCost(uint16 rid, uint16 lid) const;
  virtual int GetResolution() const;

  // Clear cache explicitly.
  void ClearCache();

 private:
  void InitializeCache() const;

  ConnectorInterface *connector_;
  // Pointers to the chache.
  // Cache should be created as global variables.
  // For the performance, we are assuming the cache is an array, not vector.
  bool *cache_initialized_;
  int *cache_key_;
  int *cache_value_;
  const int cache_size_;
};
}  // namespace mozc

#endif  // MOZC_CONVERTER_CACHED_CONNECTOR_H_
