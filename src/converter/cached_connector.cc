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

#include "converter/cached_connector.h"

#include "base/base.h"
#include "base/thread.h"
#include "converter/connector_interface.h"
#include "converter/sparse_connector.h"

namespace mozc {
namespace {
const int kInvalidCacheKey = -1;

inline int GetHashValue(uint16 rid, uint16 lid, int hash_mask) {
  return ((3 * rid + lid) & hash_mask);
  // The above value is equivalent to:
  //  return (3 * rid + lid) % cache_size;
  // The performance is important here, as this is called so many times.

  // Multiplying '3' makes the conversion speed faster.
  // The result hash value becomes reasonalbly random.
}
}  // namespace

CachedConnector::CachedConnector(ConnectorInterface *connector,
                                 bool *cache_initialized,
                                 int *cache_key,
                                 int *cache_value,
                                 int cache_size)
    : connector_(connector),
      cache_initialized_(cache_initialized),
      cache_key_(cache_key),
      cache_value_(cache_value),
      cache_size_(cache_size),
      hash_mask_(cache_size - 1) {
  // Check if the cache_size is 2^k form.
  DCHECK_EQ(0, cache_size & (cache_size - 1));
}

CachedConnector::~CachedConnector() {}

int CachedConnector::GetTransitionCost(uint16 rid, uint16 lid) const {
  InitializeCache();

  // We should mutex lock if HAVE_TLS is false. Mac OS doesn't support TLS.
  // However, we don't call it at this moment with the following reason.
  // 1) On desktop, we can assume that converter is executed on
  // single thread environment
  // 2) Can see about 20% performance drop with Mutex lock.

  const uint32 index = SparseConnector::EncodeKey(rid, lid);
  const int bucket = GetHashValue(rid, lid, hash_mask_);
  if (cache_key_[bucket] != index) {
    // Simply overwrite previous key/value.
    cache_key_[bucket] = index;
    cache_value_[bucket] = connector_->GetTransitionCost(rid, lid);
  }

  return cache_value_[bucket];
}

void CachedConnector::InitializeCache() const {
  if (*cache_initialized_) {
    return;
  }
  VLOG(2) << "Initializing Cache for CachedConnector.";
  for (int i = 0; i < cache_size_; ++i) {
    cache_key_[i] = kInvalidCacheKey;
  }
  *cache_initialized_ = true;
}

// Test code can use this method to get acceptable error.
int CachedConnector::GetResolution() const {
  return connector_->GetResolution();
}

void CachedConnector::ClearCache() {
  *cache_initialized_ = false;
}
}  // namespace mozc
