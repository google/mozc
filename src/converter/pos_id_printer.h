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

// Providing the string representation of POS id.

#ifndef MOZC_CONVERTER_POS_ID_PRINTER_H_
#define MOZC_CONVERTER_POS_ID_PRINTER_H_

#include <string>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "base/file_stream.h"

namespace mozc {
namespace internal {

// Example:
// PosIdPrinter pos_id_printer(InputFileStream("id.def"));
// EXPECT_EQ(pos_id_printer.IdToString(1934), "名詞,サ変接続,*,*,*,*,*");
// EXPECT_EQ(pos_id_printer.IdToString(-1),  // invalid id
//           "");
class PosIdPrinter {
 public:
  explicit PosIdPrinter(InputFileStream id_def);
  // Movable.
  PosIdPrinter(PosIdPrinter&&) = default;
  PosIdPrinter& operator=(PosIdPrinter&&) = default;

  ~PosIdPrinter() = default;

  // Returns a string_view to the POS string for the given id.
  // For an invalid id, returns empty string.
  absl::string_view IdToString(int id) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  absl::flat_hash_map<int, std::string> id_to_pos_map_;
};

}  // namespace internal
}  // namespace mozc

#endif  // MOZC_CONVERTER_POS_ID_PRINTER_H_
