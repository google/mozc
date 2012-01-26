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

#include "base/base.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "converter/connector_interface.h"
#include "converter/sparse_connector.h"

namespace mozc {
namespace {

#include "converter/embedded_connection_data.h"

const int kCacheSize = 1024;

const int kInvalidCacheKey = -1;

// Use Thread Local Storage for Cache of the Connector.
TLS_KEYWORD bool g_cache_initialized = false;
TLS_KEYWORD int  g_cache_key[kCacheSize];
TLS_KEYWORD int  g_cache_value[kCacheSize];

inline int GetHashValue(uint16 rid, uint16 lid) {
  // multiplying '3' makes the conversion speed faster.
  return (3 * rid + lid) % kCacheSize;
}

class ConnectorInitializer {
 public:
  ConnectorInitializer()
      : sparse_connector_(new SparseConnector(
          kConnectionData_data, kConnectionData_size)),
        cached_connector_(new CachedConnector(
            sparse_connector_.get())) {}

  ConnectorInterface *Get() const {
    return cached_connector_.get();
  }

 private:
  scoped_ptr<SparseConnector> sparse_connector_;
  scoped_ptr<CachedConnector> cached_connector_;
};

ConnectorInterface *g_connector = NULL;
}  // namespace

CachedConnector::CachedConnector(ConnectorInterface *connector)
    : connector_(connector) {}
CachedConnector::~CachedConnector() {}

int CachedConnector::GetTransitionCost(uint16 rid, uint16 lid) const {
  InitializeCache();

  // We should mutex lock if HAVE_TLS is false. Mac OS doesn't support TLS.
  // However, we don't call it at this moment with the following reason.
  // 1) On desktop, we can assume that converter is executed on
  // single thread environment
  // 2) Can see about 20% performance drop with Mutex lock.

  const uint32 index = SparseConnector::EncodeKey(rid, lid);
  const int bucket = GetHashValue(rid, lid);
  if (g_cache_key[bucket] != index) {
    // Simply overwrite previous key/value.
    g_cache_key[bucket] = index;
    g_cache_value[bucket] = connector_->GetTransitionCost(rid, lid);
  }

  return g_cache_value[bucket];
}

void CachedConnector::InitializeCache() const {
  if (g_cache_initialized) {
    return;
  }
  VLOG(2) << "Initializing Cache for CachedConnector.";
  for (int i = 0; i < kCacheSize; ++i) {
    g_cache_key[i] = kInvalidCacheKey;
  }
  g_cache_initialized = true;
}

// Test code can use this method to get acceptable error.
int CachedConnector::GetResolution() const {
  return connector_->GetResolution();
}

void CachedConnector::ClearCache() {
  g_cache_initialized = false;
}

ConnectorInterface *ConnectorFactory::GetConnector() {
  if (g_connector == NULL) {
    return Singleton<ConnectorInitializer>::get()->Get();
  } else {
    return g_connector;
  }
}
}  // namespace mozc
