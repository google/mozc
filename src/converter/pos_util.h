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

#ifndef MOZC_CONVERTER_POS_UTIL_H_
#define MOZC_CONVERTER_POS_UTIL_H_

#include <string>
#include <vector>
#include "base/base.h"

namespace mozc {

class POSUtil {
 public:
  // load data/dictioanry/id.def
  void Open(const string &id_file);

  // return id of feature defined in id.def
  uint16 id(const string &feature) const;

  // return all ids which matches the feature.
  // it only takes prefix match
  bool ids(const string &feature, vector<uint16> *ids) const;

  // Return a set of ids regeared as numbers
  const vector<uint16> &number_ids() const {
    return number_ids_;
  }

  // Return a set of ids regeared as functional words
  const vector<uint16> &functional_word_ids() const {
    return functional_word_ids_;
  }

  // Workaround:
  // use id_size as a zipcode id.
  // TODO(toshiyuki): modify this after defining POS for zipcode.
  // we will introduce a special POS for zipcode.
  const uint16 zipcode_id() const {
    return ids_.size();
  }

 private:
  vector<pair<string, uint16> > ids_;
  vector<uint16> number_ids_;
  vector<uint16> functional_word_ids_;
};
}  // namespace mozc

#endif  // MOZC_CONVERTER_POS_UTIL_H_
