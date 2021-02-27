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

#include "composer/internal/typing_model.h"

#include <cstdint>
#include <limits>
#include <memory>

#include "base/port.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace composer {

const uint8_t TypingModel::kNoData = std::numeric_limits<uint8_t>::max();
const int TypingModel::kInfinity = (2 << 20);  // approximately equals 1e+6

TypingModel::TypingModel(const char *characters, size_t characters_size,
                         const uint8_t *cost_table, size_t cost_table_size,
                         const int32_t *mapping_table)
    : character_to_radix_table_(
          new unsigned char[std::numeric_limits<unsigned char>::max()]),
      characters_size_(characters_size),
      cost_table_(cost_table),
      cost_table_size_(cost_table_size),
      mapping_table_(mapping_table) {
  for (size_t i = 0; i < characters_size; ++i) {
    character_to_radix_table_[characters[i]] = i + 1;
  }
}

TypingModel::~TypingModel() = default;

int TypingModel::GetCost(absl::string_view key) const {
  size_t index = GetIndex(key);
  if (index >= cost_table_size_) {
    return kInfinity;
  }
  uint8_t cost_index = cost_table_[index];
  return cost_index == kNoData ? kInfinity : mapping_table_[cost_index];
}

size_t TypingModel::GetIndex(absl::string_view key) const {
  const unsigned int radix = characters_size_ + 1;
  size_t index = 0;
  for (size_t i = 0; i < key.length(); ++i) {
    index = index * radix + character_to_radix_table_[key[i]];
  }
  return index;
}

// static
std::unique_ptr<const TypingModel> TypingModel::CreateTypingModel(
    const mozc::commands::Request::SpecialRomanjiTable &special_romanji_table,
    const DataManagerInterface &data_manager) {
  const char *key = nullptr;
  switch (special_romanji_table) {
    case mozc::commands::Request::TWELVE_KEYS_TO_HIRAGANA:
      key = "typing_model_12keys-hiragana.tsv";
      break;
    case mozc::commands::Request::FLICK_TO_HIRAGANA:
      key = "typing_model_flick-hiragana.tsv";
      break;
    case mozc::commands::Request::TOGGLE_FLICK_TO_HIRAGANA:
      key = "typing_model_toggle_flick-hiragana.tsv";
      break;
    case mozc::commands::Request::QWERTY_MOBILE_TO_HIRAGANA:
      key = "typing_model_qwerty_mobile-hiragana.tsv";
      break;
    case mozc::commands::Request::GODAN_TO_HIRAGANA:
      key = "typing_model_godan-hiragana.tsv";
      break;
    default:
      return nullptr;
  }

  const absl::string_view data = data_manager.GetTypingModel(key);
  if (data.empty()) {
    return nullptr;
  }
  // Parse the binary image of typing model.  See gen_typing_model.py for file
  // format.
  const uint32_t characters_size =
      *reinterpret_cast<const uint32_t *>(data.data());
  const char *characters = data.data() + 4;

  size_t offset = 4 + characters_size;
  if (offset % 4 != 0) {
    offset += 4 - offset % 4;
  }
  const uint32_t cost_table_size =
      *reinterpret_cast<const uint32_t *>(data.data() + offset);
  const uint8_t *cost_table =
      reinterpret_cast<const uint8_t *>(data.data() + offset + 4);

  offset += 4 + cost_table_size;
  if (offset % 4 != 0) {
    offset += 4 - offset % 4;
  }
  const int32_t *mapping_table =
      reinterpret_cast<const int32_t *>(data.data() + offset);

  return std::unique_ptr<const TypingModel>(new TypingModel(
      characters, characters_size, cost_table, cost_table_size, mapping_table));
}

}  // namespace composer
}  // namespace mozc
