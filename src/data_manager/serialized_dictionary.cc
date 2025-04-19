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

#include "data_manager/serialized_dictionary.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <istream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/log/check.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/bits.h"
#include "base/container/serialized_string_array.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/number_util.h"

namespace mozc {
namespace {

using CompilerToken = SerializedDictionary::CompilerToken;
using TokenList = SerializedDictionary::TokenList;

struct CompareByCost {
  bool operator()(const std::unique_ptr<CompilerToken> &t1,
                  const std::unique_ptr<CompilerToken> &t2) const {
    return t1->cost < t2->cost;
  }
};

void LoadTokens(std::istream *ifs, std::map<std::string, TokenList> *dic) {
  dic->clear();
  std::string line;
  while (!std::getline(*ifs, line).fail()) {
    std::vector<std::string> fields =
        absl::StrSplit(line, '\t', absl::SkipEmpty());
    CHECK_GE(fields.size(), 4);
    auto token = std::make_unique<CompilerToken>();
    const std::string &key = fields[0];
    token->value = fields[4];
    CHECK(NumberUtil::SafeStrToUInt16(fields[1], &token->lid));
    CHECK(NumberUtil::SafeStrToUInt16(fields[2], &token->rid));
    CHECK(NumberUtil::SafeStrToInt16(fields[3], &token->cost));
    token->description = (fields.size() > 5) ? fields[5] : "";
    token->additional_description = (fields.size() > 6) ? fields[6] : "";
    (*dic)[key].push_back(std::move(token));
  }

  for (auto &kv : *dic) {
    std::sort(kv.second.begin(), kv.second.end(), CompareByCost());
  }
}

}  // namespace

SerializedDictionary::SerializedDictionary(absl::string_view token_array,
                                           absl::string_view string_array_data)
    : token_array_(token_array) {
  DCHECK(VerifyData(token_array, string_array_data));
  string_array_.Set(string_array_data);
}

SerializedDictionary::IterRange SerializedDictionary::equal_range(
    absl::string_view key) const {
  // TODO(noriyukit): Instead of comparing key as string, we can do binary
  // search using key index to minimize string comparison cost.
  return std::equal_range(begin(), end(), key);
}

std::pair<absl::string_view, absl::string_view> SerializedDictionary::Compile(
    std::istream *input, std::unique_ptr<uint32_t[]> *output_token_array_buf,
    std::unique_ptr<uint32_t[]> *output_string_array_buf) {
  std::map<std::string, TokenList> dic;
  LoadTokens(input, &dic);
  return Compile(dic, output_token_array_buf, output_string_array_buf);
}

std::pair<absl::string_view, absl::string_view> SerializedDictionary::Compile(
    const std::map<std::string, TokenList> &dic,
    std::unique_ptr<uint32_t[]> *output_token_array_buf,
    std::unique_ptr<uint32_t[]> *output_string_array_buf) {
  static_assert(std::endian::native == std::endian::little);

  // Build a mapping from string to its index in a serialized string array.
  // Note that duplicate keys share the same index, so data is slightly
  // compressed.
  absl::btree_map<std::string, uint32_t> string_index;
  for (const auto &kv : dic) {
    // This phase just collects all the strings and temporarily assigns 0 as
    // index.
    string_index[kv.first] = 0;
    for (const auto &token_ptr : kv.second) {
      string_index[token_ptr->value] = 0;
      string_index[token_ptr->description] = 0;
      string_index[token_ptr->additional_description] = 0;
    }
  }
  {
    // This phase assigns index in ascending order of strings.
    uint32_t index = 0;
    for (auto &kv : string_index) {
      kv.second = index++;
    }
  }

  // Build a token array binary data.
  absl::string_view token_array;
  {
    std::string buf;
    for (const auto &kv : dic) {
      const uint32_t key_index = string_index[kv.first];
      const size_t prev_size = buf.size();
      buf.resize(buf.size() + kv.second.size() * kTokenByteLength);
      auto iter = buf.begin() + prev_size;
      for (const auto &token_ptr : kv.second) {
        const uint32_t value_index = string_index[token_ptr->value];
        const uint32_t desc_index = string_index[token_ptr->description];
        const uint32_t adddesc_index =
            string_index[token_ptr->additional_description];
        iter = StoreUnaligned<uint32_t>(key_index, iter);
        iter = StoreUnaligned<uint32_t>(value_index, iter);
        iter = StoreUnaligned<uint32_t>(desc_index, iter);
        iter = StoreUnaligned<uint32_t>(adddesc_index, iter);
        iter = StoreUnaligned<uint16_t>(token_ptr->lid, iter);
        iter = StoreUnaligned<uint16_t>(token_ptr->rid, iter);
        iter = StoreUnaligned<uint16_t>(token_ptr->cost, iter);
        iter = StoreUnaligned<uint16_t>(static_cast<uint16_t>(0), iter);
      }
    }
    *output_token_array_buf =
        std::make_unique<uint32_t[]>((buf.size() + 3) / 4);
    memcpy(output_token_array_buf->get(), buf.data(), buf.size());
    token_array = absl::string_view(
        reinterpret_cast<const char *>(output_token_array_buf->get()),
        buf.size());
  }

  // Build a string array.
  absl::string_view string_array;
  {
    // Copy the map keys to vector.  Note: since map's iteration is ordered,
    // each string is placed at the desired index.
    std::vector<absl::string_view> strings;
    for (const auto &kv : string_index) {
      // Guarantee that the string is inserted at its indexed position.
      CHECK_EQ(strings.size(), kv.second);
      strings.emplace_back(kv.first);
    }
    string_array = SerializedStringArray::SerializeToBuffer(
        strings, output_string_array_buf);
  }

  return std::pair<absl::string_view, absl::string_view>(token_array,
                                                         string_array);
}

void SerializedDictionary::CompileToFiles(
    const std::string &input, const std::string &output_token_array,
    const std::string &output_string_array) {
  InputFileStream ifs(input);
  CHECK(ifs.good());
  std::map<std::string, TokenList> dic;
  LoadTokens(&ifs, &dic);
  CompileToFiles(dic, output_token_array, output_string_array);
}

void SerializedDictionary::CompileToFiles(
    const std::map<std::string, TokenList> &dic,
    const std::string &output_token_array,
    const std::string &output_string_array) {
  std::unique_ptr<uint32_t[]> buf1, buf2;
  const std::pair<absl::string_view, absl::string_view> data =
      Compile(dic, &buf1, &buf2);
  CHECK(VerifyData(data.first, data.second));
  CHECK_OK(FileUtil::SetContents(output_token_array, data.first));
  CHECK_OK(FileUtil::SetContents(output_string_array, data.second));
}

bool SerializedDictionary::VerifyData(absl::string_view token_array_data,
                                      absl::string_view string_array_data) {
  if (token_array_data.size() % kTokenByteLength != 0) {
    return false;
  }
  SerializedStringArray string_array;
  if (!string_array.Init(string_array_data)) {
    return false;
  }
  for (const char *ptr = token_array_data.data();
       ptr != token_array_data.data() + token_array_data.size();
       ptr += kTokenByteLength) {
    const uint32_t *u32_ptr = reinterpret_cast<const uint32_t *>(ptr);
    if (u32_ptr[0] >= string_array.size() ||
        u32_ptr[1] >= string_array.size() ||
        u32_ptr[2] >= string_array.size() ||
        u32_ptr[3] >= string_array.size()) {
      return false;
    }
  }
  return true;
}

}  // namespace mozc
