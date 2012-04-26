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

#include "converter/connector_base.h"

#include "base/base.h"
#include "converter/cached_connector.h"
#include "converter/sparse_connector.h"

namespace mozc {

ConnectorBase::ConnectorBase(const char *connection_data,
                             size_t connection_size,
                             bool *cache_initialized,
                             int *cache_key,
                             int *cache_value,
                             int cache_size)
    : sparse_connector_(new SparseConnector(connection_data, connection_size)),
      cached_connector_(new CachedConnector(sparse_connector_.get(),
                                            cache_initialized,
                                            cache_key,
                                            cache_value,
                                            cache_size)) {}

ConnectorBase::~ConnectorBase() {}


int ConnectorBase::GetTransitionCost(uint16 rid, uint16 lid) const {
  return cached_connector_->GetTransitionCost(rid, lid);
}

int ConnectorBase::GetResolution() const {
  return cached_connector_->GetResolution();
}

}  // namespace mozc
