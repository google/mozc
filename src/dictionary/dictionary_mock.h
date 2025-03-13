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

#ifndef MOZC_DICTIONARY_DICTIONARY_MOCK_H_
#define MOZC_DICTIONARY_DICTIONARY_MOCK_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"

namespace mozc {
namespace dictionary {

class MockDictionary : public DictionaryInterface {
 public:
  static constexpr int kDefaultCost = 0;
  static constexpr int kDefaultPosId = 1;

  MockDictionary() = default;
  ~MockDictionary() override = default;

  MOCK_METHOD(bool, HasKey, (absl::string_view key), (const, override));
  MOCK_METHOD(bool, HasValue, (absl::string_view value), (const, override));
  MOCK_METHOD(void, LookupPredictive,
              (absl::string_view key,
               const ConversionRequest &conversion_request, Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupPrefix,
              (absl::string_view key,
               const ConversionRequest &conversion_request, Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupExact,
              (absl::string_view key,
               const ConversionRequest &conversion_request, Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupReverse,
              (absl::string_view str,
               const ConversionRequest &conversion_request, Callback *callback),
              (const, override));
  MOCK_METHOD(bool, LookupComment,
              (absl::string_view key, absl::string_view value,
               const ConversionRequest &conversion_request,
               std::string *comment),
              (const, override));
  MOCK_METHOD(void, PopulateReverseLookupCache, (absl::string_view str),
              (const, override));
  MOCK_METHOD(void, ClearReverseLookupCache, (), (const, override));
};

class MockUserDictionary : public UserDictionaryInterface {
 public:
  static constexpr int kDefaultCost = 0;
  static constexpr int kDefaultPosId = 1;

  MockUserDictionary() = default;
  ~MockUserDictionary() override = default;

  MOCK_METHOD(bool, HasKey, (absl::string_view key), (const, override));
  MOCK_METHOD(bool, HasValue, (absl::string_view value), (const, override));
  MOCK_METHOD(void, LookupPredictive,
              (absl::string_view key,
               const ConversionRequest &conversion_request, Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupPrefix,
              (absl::string_view key,
               const ConversionRequest &conversion_request, Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupExact,
              (absl::string_view key,
               const ConversionRequest &conversion_request, Callback *callback),
              (const, override));
  MOCK_METHOD(void, LookupReverse,
              (absl::string_view str,
               const ConversionRequest &conversion_request, Callback *callback),
              (const, override));
  MOCK_METHOD(bool, LookupComment,
              (absl::string_view key, absl::string_view value,
               const ConversionRequest &conversion_request,
               std::string *comment),
              (const, override));
  MOCK_METHOD(void, PopulateReverseLookupCache, (absl::string_view str),
              (const, override));
  MOCK_METHOD(void, ClearReverseLookupCache, (), (const, override));
  MOCK_METHOD(bool, Reload, (), (override));

  MOCK_METHOD(void, WaitForReloader, (), (override));

  MOCK_METHOD(std::vector<std::string>, GetPosList, (), (const, override));
  MOCK_METHOD(bool, Load, (const user_dictionary::UserDictionaryStorage &),
              (override));
  MOCK_METHOD(bool, IsSuppressedEntry,
              (absl::string_view key, absl::string_view value),
              (const, override));
  MOCK_METHOD(bool, HasSuppressedEntries, (), (const, override));
};

class MockCallback : public DictionaryInterface::Callback {
 public:
  MockCallback() = default;
  ~MockCallback() override = default;

  MOCK_METHOD(ResultType, OnKey, (absl::string_view key), (override));

  MOCK_METHOD(ResultType, OnActualKey,
              (absl::string_view key, absl::string_view actual_key,
               int num_expanded),
              (override));

  MOCK_METHOD(ResultType, OnToken,
              (absl::string_view key, absl::string_view expanded_key,
               const Token &token_info),
              (override));
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_MOCK_H_
