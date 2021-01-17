// Copyright 2010-2021, Google Inc.
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

#ifndef MOZC_CONVERTER_SEGMENTER_H_
#define MOZC_CONVERTER_SEGMENTER_H_

#include "base/port.h"

namespace mozc {

class DataManagerInterface;
struct Node;

class Segmenter {
 public:
  static Segmenter *CreateFromDataManager(
      const DataManagerInterface &data_manager);

  // This class does not take the ownership of pointer parameters.
  Segmenter(size_t l_num_elements, size_t r_num_elements, const uint16 *l_table,
            const uint16 *r_table, size_t bitarray_num_bytes,
            const char *bitarray_data, const uint16 *boundary_data);
  ~Segmenter();

  bool IsBoundary(const Node &lnode, const Node &rnode,
                  bool is_single_segment) const;
  bool IsBoundary(uint16 rid, uint16 lid) const;
  int32 GetPrefixPenalty(uint16 lid) const;
  int32 GetSuffixPenalty(uint16 rid) const;

 private:
  const size_t l_num_elements_;
  const size_t r_num_elements_;
  const uint16 *l_table_;
  const uint16 *r_table_;
  const size_t bitarray_num_bytes_;
  const char *bitarray_data_;
  const uint16 *boundary_data_;

  DISALLOW_COPY_AND_ASSIGN(Segmenter);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_SEGMENTER_H_
