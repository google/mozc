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

#include "converter/pos_id_printer.h"

#include <istream>
#include <map>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/util.h"

namespace mozc {
namespace internal {

PosIdPrinter::PosIdPrinter(std::istream *id_def) {
  if (id_def == nullptr) {
    return;
  }

  std::string line;
  std::vector<std::string> columns;
  while (std::getline(*id_def, line)) {
    columns.clear();
    Util::SplitStringUsing(line, " ", &columns);
    CHECK_EQ(2, columns.size());
    const int id = NumberUtil::SimpleAtoi(columns[0]);
    id_to_pos_map_[id] = columns[1];
  }
}

PosIdPrinter::~PosIdPrinter() {}

std::string PosIdPrinter::IdToString(int id) const {
  std::map<int, std::string>::const_iterator iter = id_to_pos_map_.find(id);
  if (iter == id_to_pos_map_.end()) {
    return "";
  }
  return iter->second;
}

}  // namespace internal
}  // namespace mozc
