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

#include "converter/history_reconstructor.h"

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace converter {
namespace {
// Extracts the last substring that consists of the same script type.
// Returns true if the last substring is successfully extracted.
//   Examples:
//   - "" -> false
//   - "x " -> "x" / ALPHABET
//   - "x  " -> false
//   - "C60" -> "60" / NUMBER
//   - "200x" -> "x" / ALPHABET
//   (currently only NUMBER and ALPHABET are supported)
bool ExtractLastTokenWithScriptType(absl::string_view text,
                                    std::string* last_token,
                                    Util::ScriptType* last_script_type) {
  last_token->clear();
  *last_script_type = Util::SCRIPT_TYPE_SIZE;

  // Allow one whitespace at the end.
  if (text.ends_with(' ')) {
    text.remove_suffix(1);
    if (text.ends_with(' ')) {
      return false;
    }
  }
  if (text.empty()) {
    return false;
  }

  absl::string_view rest = text;
  const UnicodeChar last_ch = Utf8AsUnicodeChar(rest).back();
  const Util::ScriptType token_script_type =
      Util::GetScriptType(last_ch.char32());
  rest.remove_suffix(last_ch.utf8().size());
  while (!rest.empty()) {
    const UnicodeChar ch = Utf8AsUnicodeChar(rest).back();
    if (ch.char32() == ' ' ||
        Util::GetScriptType(ch.char32()) != token_script_type) {
      break;
    }
    rest.remove_suffix(ch.utf8().size());
  }

  *last_script_type = token_script_type;
  last_token->assign(text.data() + rest.size(), text.size() - rest.size());
  return true;
}
}  // namespace

HistoryReconstructor::HistoryReconstructor(
    const dictionary::PosMatcher& pos_matcher)
    : pos_matcher_(pos_matcher) {}

bool HistoryReconstructor::ReconstructHistory(absl::string_view preceding_text,
                                              Segments* segments) const {
  std::string key;
  std::string value;
  uint16_t id;
  if (!GetLastConnectivePart(preceding_text, &key, &value, &id)) {
    return false;
  }

  Segment* segment = segments->add_segment();
  segment->set_key(key);
  segment->set_segment_type(Segment::HISTORY);
  Candidate* candidate = segment->push_back_candidate();
  candidate->rid = id;
  candidate->lid = id;
  candidate->content_key = key;
  candidate->key = std::move(key);
  candidate->content_value = value;
  candidate->value = std::move(value);
  candidate->attributes = Attribute::NO_LEARNING;
  return true;
}

bool HistoryReconstructor::GetLastConnectivePart(
    const absl::string_view preceding_text, std::string* key,
    std::string* value, uint16_t* id) const {
  key->clear();
  value->clear();
  *id = pos_matcher_.GetGeneralNounId();

  Util::ScriptType last_script_type = Util::SCRIPT_TYPE_SIZE;
  std::string last_token;
  if (!ExtractLastTokenWithScriptType(preceding_text, &last_token,
                                      &last_script_type)) {
    return false;
  }

  // Currently only NUMBER and ALPHABET are supported.
  switch (last_script_type) {
    case Util::NUMBER: {
      *key = japanese_util::FullWidthAsciiToHalfWidthAscii(last_token);
      *value = std::move(last_token);
      *id = pos_matcher_.GetNumberId();
      return true;
    }
    case Util::ALPHABET: {
      *key = japanese_util::FullWidthAsciiToHalfWidthAscii(last_token);
      *value = std::move(last_token);
      *id = pos_matcher_.GetUniqueNounId();
      return true;
    }
    default:
      return false;
  }
}

}  // namespace converter
}  // namespace mozc
