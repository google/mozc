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

#ifndef MOZC_DICTIONARY_DICTIONARY_IMPL_H_
#define MOZC_DICTIONARY_DICTIONARY_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"

namespace mozc {
namespace dictionary {

class DictionaryImpl : public DictionaryInterface {
 public:
  // Initializes a dictionary with given dictionaries and POS data.  The system
  // and value dictionaries are owned by this class but the user dictionary is
  // just a reference and to be deleted by the caller. Note that the user
  // dictionary is not a const reference because this class may reload the user
  // dictionary.
  // TODO(noriyukit): Currently DictionaryInterface::Reload() is not used and
  // thus user_dictionary can be const as well. We can make it const after
  // clarifying the ownership of the user dictionary and changing code so that
  // the owner reloads it.
  DictionaryImpl(std::unique_ptr<const DictionaryInterface> system_dictionary,
                 std::unique_ptr<const DictionaryInterface> value_dictionary,
                 const UserDictionaryInterface &user_dictionary,
                 const PosMatcher &pos_matcher);

  DictionaryImpl(const DictionaryImpl &) = delete;
  DictionaryImpl &operator=(const DictionaryImpl &) = delete;

  ~DictionaryImpl() override;

  bool HasKey(absl::string_view key) const override;
  bool HasValue(absl::string_view value) const override;
  void LookupPredictive(absl::string_view key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override;
  void LookupPrefix(absl::string_view key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override;

  void LookupExact(absl::string_view key,
                   const ConversionRequest &conversion_request,
                   Callback *callback) const override;

  void LookupReverse(absl::string_view str,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override;

  bool LookupComment(absl::string_view key, absl::string_view value,
                     const ConversionRequest &conversion_request,
                     std::string *comment) const override;
  void PopulateReverseLookupCache(absl::string_view str) const override;
  void ClearReverseLookupCache() const override;

 private:
  enum LookupType {
    PREDICTIVE,
    PREFIX,
    REVERSE,
    EXACT,
  };

  // Used to check POS IDs.
  const PosMatcher &pos_matcher_;

  // Main three dictionaries.
  std::unique_ptr<const DictionaryInterface> system_dictionary_;
  std::unique_ptr<const DictionaryInterface> value_dictionary_;
  const UserDictionaryInterface &user_dictionary_;

  // Convenient container to handle the above three dictionaries as one
  // composite dictionary.
  std::vector<const DictionaryInterface *> dics_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_IMPL_H_
