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

#ifndef MOZC_CONVERTER_SPARSE_CONNECTOR_H_
#define MOZC_CONVERTER_SPARSE_CONNECTOR_H_

#include "base/base.h"
#include "converter/connector_interface.h"

namespace mozc {

class SparseArrayImage;

class SparseConnector : public ConnectorInterface {
 public:
  SparseConnector(const char *ptr, size_t size);
  virtual ~SparseConnector();

  static const uint16 kSparseConnectorMagic = 0x4141;

  virtual int GetTransitionCost(uint16 rid, uint16 lid) const;

  // It is better to store rid in higher bit as loop for rid is outside
  // of lid loop.
  inline static uint32 EncodeKey(int lid, int rid) {
    return (rid << 16) + lid;
  }

 private:
  scoped_ptr<SparseArrayImage> array_image_;
  const int16 *default_cost_;

  DISALLOW_COPY_AND_ASSIGN(SparseConnector);
};

class SparseConnectorBuilder {
 public:
  static void Compile(const char *text_file,
                      const char *binary_file);
};
}  // mozc

#endif  // MOZC_CONVERTER_SPARSE_CONNECTOR_H_
