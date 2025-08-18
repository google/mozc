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

#ifndef MOZC_DICTIONARY_SYSTEM_KEY_EXPANSION_TABLE_H_
#define MOZC_DICTIONARY_SYSTEM_KEY_EXPANSION_TABLE_H_

#include <cstdint>
#include <cstring>

#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {

// Very thin wrapper class to check the edge annotated character is hit the
// expanded key or not.
// Note that this class is very small so it's ok to be copied.
class ExpandedKey {
 public:
  explicit ExpandedKey(const uint32_t* data) : data_(data) {}

  bool IsHit(char value) const {
    return (data_[value / 32] >> (value % 32)) & 1;
  }

 private:
  const uint32_t* data_;
};

// Table to keep the key expanding information.
// Implementation Note: This class holds a 256x256 bitmap table.
// The client class (typically LoudsTrie) can check if the value is hit to
// the expanded key or not.
// TODO(hidehiko): We should have yet another way for Key Expansion.
//   For example, by holding expanded characters directly, and iterate
//   both sorted edge annotated characters and expanded keys.
//   Check the performance of it, and if it's efficient, move to it.
class KeyExpansionTable {
 public:
  KeyExpansionTable() {
    // Initialize with identity matrix.
    for (size_t i = 0; i < 256; ++i) {
      SetBit(i, i);
    }
  }

  KeyExpansionTable(const KeyExpansionTable&) = delete;
  KeyExpansionTable& operator=(const KeyExpansionTable&) = delete;

  // Add expanding data of the given key.
  void Add(const char key, const absl::string_view data) {
    for (size_t i = 0; i < data.length(); ++i) {
      SetBit(key, data[i]);
    }
  }

  ExpandedKey ExpandKey(char key) const { return ExpandedKey(table_[key]); }

  // Returns the default (no-effective) KeyExpansionTable instance.
  // (in other words, the result holds identity-bitmap matrix).
  static const KeyExpansionTable& GetDefaultInstance() {
    static const KeyExpansionTable table;
    return table;
  }

 private:
  // Set a bit corresponding (key -> value) to '1'.
  void SetBit(char key, char value) {
    table_[key][value / 32] |= (1 << (value % 32));
  }

  // 256x256 (key -> value) bit map matrix.
  uint32_t table_[256][256 / 32] = {};
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_KEY_EXPANSION_TABLE_H_
