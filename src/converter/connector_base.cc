// Copyright 2010-2014, Google Inc.
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
#include "data_manager/data_manager_interface.h"

namespace mozc {

using converter::CachedConnector;

ConnectorBase *ConnectorBase::CreateFromDataManager(
    const DataManagerInterface &data_manager) {
#ifdef OS_ANDROID
  const int kCacheSize = 256;
#else
  const int kCacheSize = 1024;
#endif  // OS_ANDROID
  const char *connection_data = NULL;
  size_t connection_data_size = 0;
  data_manager.GetConnectorData(&connection_data, &connection_data_size);
  return new ConnectorBase(connection_data, connection_data_size, kCacheSize);
}

ConnectorBase::ConnectorBase(const char *connection_data,
                             size_t connection_size,
                             int cache_size)
    : sparse_connector_(new SparseConnector(connection_data, connection_size)),
      cached_connector_(new CachedConnector(sparse_connector_.get(),
                                            cache_size)) {}

ConnectorBase::~ConnectorBase() {}


int ConnectorBase::GetTransitionCost(uint16 rid, uint16 lid) const {
  return cached_connector_->GetTransitionCost(rid, lid);
}

int ConnectorBase::GetResolution() const {
  return cached_connector_->GetResolution();
}

}  // namespace mozc
