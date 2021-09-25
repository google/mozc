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

#include "gui/base/encoding_util.h"

#include <cstdint>

#include "base/port.h"
#include "base/util.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

#include "gui/base/sjis_to_ucs2_table.h"

// Each character of SJIS is encoded in one or two bytes.
//
// For first byte, there are 4 valid ranges (closed intervals):
//   * FirstByteRange1: [0x00, 0x80]
//   * FirstByteRange2: [0x81, 0x9F]
//   * FirstByteRange3: [0xA1, 0xDF]
//   * FirstByteRange4: [0xE0, 0xFF]
// Ranges 2 and 4 are for two bytes encoding, so one more byte is needed to
// decode a character.
//
// For second byte, there are 2 valid ranges (closed intervals):
//   * SecondByteRange1: [0x40, 0x7E]
//   * SecondByteRange2: [0x80, 0xFF]
// Two byte characters are decoded using the conversion table defined in
// sjis_to_ucs2_table.h.
inline bool IsInFirstByteRange1(uint8_t byte) { return byte <= 0x80; }

inline bool IsInFirstByteRange2(uint8_t byte) {
  return 0x81 <= byte && byte <= 0x9F;
}

inline bool IsInFirstByteRange3(uint8_t byte) {
  return 0xA1 <= byte && byte <= 0xDF;
}

inline bool IsInFirstByteRange4(uint8_t byte) { return 0xE0 <= byte; }

inline bool IsInSecondByteRange1(uint8_t byte) {
  return 0x40 <= byte && byte <= 0x7E;
}

inline bool IsInSecondByteRange2(uint8_t byte) { return 0x80 <= byte; }

size_t ComputeIndex(uint8_t first, uint8_t second) {
  size_t first_index = 0;
  if (IsInFirstByteRange2(first)) {
    // first_index = "offset of first in FirstByteRange2".
    first_index = first - 0x81;
  } else if (IsInFirstByteRange4(first)) {
    // first_index = "offset of first in FirstByteRange4" +
    //               length(FirstByteRange2)
    first_index = (first - 0xE0) + (0x9F - 0x81 + 1);
  }

  size_t second_index = 0;
  if (IsInSecondByteRange1(second)) {
    // second_index = "offset of second in SecondByteRange1";
    second_index = second - 0x40;
  } else if (IsInSecondByteRange2(second)) {
    // second_index = "offset of second in SecondByteRange2" +
    //                length(SecondByteRange1)
    second_index = (second - 0x80) + (0x7E - 0x40 + 1);
  }

  // width = length(SecondByteRange1) + length(SecondByteRange2)
  const size_t width = (0x7E - 0x40 + 1) + (0xFF - 0x80 + 1);
  return first_index * width + second_index;
}

bool SjisToUtf8Internal(absl::string_view input, std::string *output) {
  bool expect_first_byte = true;
  uint8_t first_byte = 0;
  for (const char c : input) {
    const uint8_t byte = static_cast<uint8_t>(c);

    if (expect_first_byte) {
      if (IsInFirstByteRange1(byte)) {
        Util::Ucs4ToUtf8Append(byte, output);
      } else if (IsInFirstByteRange3(byte)) {
        Util::Ucs4ToUtf8Append(byte + 0xFEC0, output);
      } else if (IsInFirstByteRange2(byte) || IsInFirstByteRange4(byte)) {
        first_byte = byte;
        expect_first_byte = false;
      } else {
        return false;  // Invalid first byte.
      }
      continue;
    }

    if (!IsInSecondByteRange1(byte) && !IsInSecondByteRange2(byte)) {
      return false;
    }
    const size_t index = ComputeIndex(first_byte, byte);
    if (index >= sizeof(kSJISToUCS2Table)) {
      return false;
    }
    const uint16_t ucs2 = kSJISToUCS2Table[index];
    if (ucs2 == 0) {
      return false;
    }
    Util::Ucs4ToUtf8Append(ucs2, output);
    expect_first_byte = true;
  }
  return expect_first_byte;
}

}  // namespace

void EncodingUtil::SjisToUtf8(const std::string &input, std::string *output) {
  output->clear();
  if (!SjisToUtf8Internal(input, output)) {
    output->clear();
  }
}

}  // namespace mozc
