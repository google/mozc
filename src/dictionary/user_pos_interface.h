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

#ifndef MOZC_DICTIONARY_USER_POS_INTERFACE_H_
#define MOZC_DICTIONARY_USER_POS_INTERFACE_H_

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"

namespace mozc {

// Provides a list of part-of-speech (POS) used by Mozc.  This minimal interface
// is used by GUI tools so that we can minimize the data embedded in
// executables.
class PosListProviderInterface {
 public:
  virtual ~PosListProviderInterface() = default;

  // Sets possible list of POS which Mozc can handle.
  virtual void GetPosList(std::vector<std::string> *pos_list) const = 0;
};

// Interface of the helper class used by POS.
class UserPosInterface : public PosListProviderInterface {
 public:
  struct Token {
    std::string key;
    std::string value;
    uint16_t id = 0;
    uint16_t attributes = 0;
    // The actual cost of user dictionary entries are populated
    // in the dictionary lookup time via PopulateTokenFromUserPosToken.
    std::string comment;  // This field comes from user dictionary.

    // Attribute is used to dynamically assign cost, and is independent from the
    // POS.
    enum Attribute {
      SHORTCUT = 1,  // Added via android shortcut, which has no explicit POS.
      ISOLATED_WORD = 2,    // 短縮よみ
      SUGGESTION_ONLY = 4,  //  SUGGESTION only.
      NON_JA_LOCALE = 8     // Locale is not Japanese.
    };

    inline void add_attribute(Attribute attr) { attributes |= attr; }
    inline bool has_attribute(Attribute attr) const {
      return attributes & attr;
    }
    inline void remove_attribute(Attribute attr) { attributes &= ~attr; }

    void swap(Token &other) noexcept {
      static_assert(std::is_nothrow_swappable_v<std::string>);
      using std::swap;
      swap(key, other.key);
      swap(value, other.value);
      swap(id, other.id);
      swap(attributes, other.attributes);
      swap(comment, other.comment);
    }
    friend void swap(Token &lhs, Token &rhs) noexcept { lhs.swap(rhs); }
  };

  // Returns true if the given string is one of the POSes Mozc can handle.
  virtual bool IsValidPos(absl::string_view pos) const = 0;

  // Returns iid from Mozc POS. If the pos has inflection, this method only
  // returns the ids of base form.
  virtual bool GetPosIds(absl::string_view pos, uint16_t *id) const = 0;

  // Converts the given tuple (key, value, pos, locale) to Token.  If the pos
  // has inflection, this function expands possible inflections automatically.
  virtual bool GetTokens(absl::string_view key, absl::string_view value,
                         absl::string_view pos, absl::string_view locale,
                         std::vector<Token> *tokens) const = 0;

  bool GetTokens(absl::string_view key, absl::string_view value,
                 absl::string_view pos, std::vector<Token> *tokens) const {
    return GetTokens(key, value, pos, "", tokens);
  }
};

}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_POS_INTERFACE_H_
