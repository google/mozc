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

#include "composer/special_key.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/util.h"

namespace mozc::composer::internal {
namespace {

struct Block {
  size_t open_pos;
  size_t close_pos;
};

// Use [U+F000, U+F8FF] to represent special keys (e.g. {!}, {abc}).
// The range of Unicode PUA is [U+E000, U+F8FF], and we use them from U+F000.
// * The range of [U+E000, U+F000) is used for user defined PUA characters.
// * The users can still use [U+F000, U+F8FF] for their user dictionary.
//   but, they should not use them for composing rules.
constexpr char32_t kSpecialKeyBegin = 0xF000;
constexpr char32_t kSpecialKeyEnd = 0xF8FF;

// U+000F and U+000E are used as fallback for special keys that are not
// registered in the table. "{abc}" is converted to "\u000Fabc\u000E".
constexpr char kSpecialKeyOpen[] = "\u000F";   // Shift-In of ASCII (1 byte)
constexpr char kSpecialKeyClose[] = "\u000E";  // Shift-Out of ASCII (1 byte)

constexpr bool IsSpecialKey(char32_t c) {
  return (kSpecialKeyBegin <= c && c <= kSpecialKeyEnd);
}

std::optional<Block> FindBlock(const absl::string_view input,
                               const absl::string_view open,
                               const absl::string_view close) {
  size_t open_pos = input.find(open);
  if (open_pos == absl::string_view::npos) {
    return std::nullopt;
  }
  size_t close_pos = input.find(close, open_pos);
  if (close_pos == absl::string_view::npos) {
    return std::nullopt;
  }
  return Block{open_pos, close_pos};
}

using OnKeyFound = absl::AnyInvocable<std::string(const absl::string_view)>;

std::string ParseBlock(absl::string_view input, OnKeyFound callback) {
  std::string output;
  while (!input.empty()) {
    std::optional<Block> block = FindBlock(input, "{", "}");
    if (!block) {
      absl::StrAppend(&output, input);
      break;
    }

    absl::StrAppend(&output, input.substr(0, block->open_pos));

    // The both sizes of "{" and "}" is 1.
    const absl::string_view key = input.substr(
        block->open_pos + 1, block->close_pos - block->open_pos - 1);
    if (key == "{") {
      // "{{}" is treated as "{".
      absl::StrAppend(&output, "{");
    } else {
      absl::StrAppend(&output, callback(key));
    }
    input = absl::ClippedSubstr(input, block->close_pos + 1);
  }
  return output;
}

}  // namespace

std::string SpecialKeyMap::Register(const absl::string_view input) {
  OnKeyFound callback = [this](const absl::string_view key) {
    if (auto it = map_.find(key); it != map_.end()) {
      return it->second;  // existing entry
    }
    char32_t keycode = kSpecialKeyBegin + map_.size();
    if (keycode > kSpecialKeyEnd) {
      // 2304 (0x900 = [Begin, End]) is the max size of special keys.
      keycode = kSpecialKeyEnd;
      LOG(WARNING) << "The size of special keys exceeded: " << key;
    }
    // New special key is replaced with a Unicode PUA and registered.
    const std::string special_key = Util::CodepointToUtf8(keycode);
    map_.emplace(key, special_key);
    return special_key;
  };
  return ParseBlock(input, std::move(callback));
}

std::string SpecialKeyMap::Parse(const absl::string_view input) const {
  OnKeyFound callback = [this](const absl::string_view key) {
    if (auto it = map_.find(key); it != map_.end()) {
      return it->second;  // existing entry
    }
    // Unregistered key is replaced with the fallback format.
    LOG(WARNING) << "Unregistered special key: " << key;
    return absl::StrCat(kSpecialKeyOpen, key, kSpecialKeyClose);
  };
  return ParseBlock(input, std::move(callback));
}

absl::string_view TrimLeadingSpecialKey(absl::string_view input) {
  // Check if the first character is a Unicode PUA converted from a special key.
  char32_t first_char;
  absl::string_view rest;
  Util::SplitFirstChar32(input, &first_char, &rest);
  if (IsSpecialKey(first_char)) {
    return input.substr(input.size() - rest.size());
  }

  // Check if the input starts from open and close of a special key.
  if (!input.starts_with(kSpecialKeyOpen)) {
    return input;
  }
  size_t close_pos = input.find(kSpecialKeyClose, 1);
  if (close_pos == absl::string_view::npos) {
    return input;
  }
  return input.substr(close_pos + 1);
}

std::string DeleteSpecialKeys(absl::string_view input) {
  std::string output;
  while (!input.empty()) {
    std::optional<Block> block =
        FindBlock(input, kSpecialKeyOpen, kSpecialKeyClose);
    if (!block) {
      absl::StrAppend(&output, input);
      break;
    }

    absl::StrAppend(&output, input.substr(0, block->open_pos));
    // The size of kSpecialKeyClose is 1.
    input = absl::ClippedSubstr(input, block->close_pos + 1);
  }

  // Delete Unicode PUA characters converted from special keys.
  std::u32string codepoints = Util::Utf8ToUtf32(output);
  auto last =
      std::remove_if(codepoints.begin(), codepoints.end(), IsSpecialKey);
  if (last == codepoints.end()) {
    return output;
  }
  codepoints.erase(last, codepoints.end());
  return Util::Utf32ToUtf8(codepoints);
}

}  // namespace mozc::composer::internal
