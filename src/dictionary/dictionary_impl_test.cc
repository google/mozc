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

#include "dictionary/dictionary_impl.h"

#include <cstring>
#include <memory>
#include <string>

#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/node_allocator.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary_stub.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {
namespace {

struct DictionaryData {
  std::unique_ptr<DictionaryInterface> user_dictionary;
  std::unique_ptr<SuppressionDictionary> suppression_dictionary;
  POSMatcher pos_matcher;
  std::unique_ptr<DictionaryInterface> dictionary;
};

std::unique_ptr<DictionaryData> CreateDictionaryData() {
  auto ret = absl::make_unique<DictionaryData>();
  testing::MockDataManager data_manager;
  ret->pos_matcher.Set(data_manager.GetPOSMatcherData());
  const char *dictionary_data = nullptr;
  int dictionary_size = 0;
  data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);
  std::unique_ptr<SystemDictionary> sys_dict =
      SystemDictionary::Builder(dictionary_data, dictionary_size)
          .Build()
          .value();
  auto val_dict = absl::make_unique<ValueDictionary>(ret->pos_matcher,
                                                     &sys_dict->value_trie());
  ret->user_dictionary = absl::make_unique<UserDictionaryStub>();
  ret->suppression_dictionary = absl::make_unique<SuppressionDictionary>();
  ret->dictionary = absl::make_unique<DictionaryImpl>(
      std::move(sys_dict), std::move(val_dict), ret->user_dictionary.get(),
      ret->suppression_dictionary.get(), &ret->pos_matcher);
  return ret;
}

}  // namespace

class DictionaryImplTest : public ::testing::Test {
 protected:
  DictionaryImplTest() { convreq_.set_config(&config_); }

  void SetUp() override { config::ConfigHandler::GetDefaultConfig(&config_); }

  class CheckKeyValueExistenceCallback : public DictionaryInterface::Callback {
   public:
    CheckKeyValueExistenceCallback(absl::string_view key,
                                   absl::string_view value)
        : key_(key), value_(value), found_(false) {}

    ResultType OnToken(absl::string_view /* key */,
                       absl::string_view /* actual_key */,
                       const Token &token) override {
      if (token.key == key_ && token.value == value_) {
        found_ = true;
        return TRAVERSE_DONE;
      }
      return TRAVERSE_CONTINUE;
    }

    bool found() const { return found_; }

   private:
    const absl::string_view key_, value_;
    bool found_;
  };

  class CheckSpellingExistenceCallback : public DictionaryInterface::Callback {
   public:
    CheckSpellingExistenceCallback(absl::string_view key,
                                   absl::string_view value)
        : key_(key), value_(value), found_(false) {}

    ResultType OnToken(absl::string_view /* key */,
                       absl::string_view /* actual_key */,
                       const Token &token) override {
      if (token.key == key_ && token.value == value_ &&
          (token.attributes & Token::SPELLING_CORRECTION)) {
        found_ = true;
        return TRAVERSE_DONE;
      }
      return TRAVERSE_CONTINUE;
    }

    bool found() const { return found_; }

   private:
    const absl::string_view key_, value_;
    bool found_;
  };

  class CheckZipCodeExistenceCallback : public DictionaryInterface::Callback {
   public:
    explicit CheckZipCodeExistenceCallback(absl::string_view key,
                                           absl::string_view value,
                                           const POSMatcher *pos_matcher)
        : key_(key), value_(value), pos_matcher_(pos_matcher), found_(false) {}

    ResultType OnToken(absl::string_view /* key */,
                       absl::string_view /* actual_key */,
                       const Token &token) override {
      if (token.key == key_ && token.value == value_ &&
          pos_matcher_->IsZipcode(token.lid)) {
        found_ = true;
        return TRAVERSE_DONE;
      }
      return TRAVERSE_CONTINUE;
    }

    bool found() const { return found_; }

   private:
    const absl::string_view key_, value_;
    const POSMatcher *pos_matcher_;
    bool found_;
  };

  class CheckEnglishT13nCallback : public DictionaryInterface::Callback {
   public:
    CheckEnglishT13nCallback(absl::string_view key, absl::string_view value)
        : key_(key), value_(value), found_(false) {}

    ResultType OnToken(absl::string_view /* key */,
                       absl::string_view /* actual_key */,
                       const Token &token) override {
      if (token.key == key_ && token.value == value_ &&
          Util::IsEnglishTransliteration(token.value)) {
        found_ = true;
        return TRAVERSE_DONE;
      }
      return TRAVERSE_CONTINUE;
    }

    bool found() const { return found_; }

   private:
    const absl::string_view key_, value_;
    bool found_;
  };

  // Pair of DictionaryInterface's lookup method and query text.
  struct LookupMethodAndQuery {
    void (DictionaryInterface::*lookup_method)(
        absl::string_view, const ConversionRequest &,
        DictionaryInterface::Callback *) const;
    const char *query;
  };

  ConversionRequest convreq_;
  config::Config config_;
};

TEST_F(DictionaryImplTest, WordSuppressionTest) {
  std::unique_ptr<DictionaryData> data = CreateDictionaryData();
  DictionaryInterface *d = data->dictionary.get();
  SuppressionDictionary *s = data->suppression_dictionary.get();

  const char kKey[] = "ぐーぐる";
  const char kValue[] = "グーグル";

  const LookupMethodAndQuery kTestPair[] = {
      {&DictionaryInterface::LookupPrefix, "ぐーぐるは"},
      {&DictionaryInterface::LookupPredictive, "ぐーぐ"},
  };

  // First add (kKey, kValue) to the suppression dictionary; thus it should not
  // be looked up.
  s->Lock();
  s->Clear();
  s->AddEntry(kKey, kValue);
  s->UnLock();
  for (size_t i = 0; i < arraysize(kTestPair); ++i) {
    CheckKeyValueExistenceCallback callback(kKey, kValue);
    (d->*kTestPair[i].lookup_method)(kTestPair[i].query, convreq_, &callback);
    EXPECT_FALSE(callback.found());
  }

  // Clear the suppression dictionary; thus it should now be looked up.
  s->Lock();
  s->Clear();
  s->UnLock();
  for (size_t i = 0; i < arraysize(kTestPair); ++i) {
    CheckKeyValueExistenceCallback callback(kKey, kValue);
    (d->*kTestPair[i].lookup_method)(kTestPair[i].query, convreq_, &callback);
    EXPECT_TRUE(callback.found());
  }
}

TEST_F(DictionaryImplTest, DisableSpellingCorrectionTest) {
  std::unique_ptr<DictionaryData> data = CreateDictionaryData();
  DictionaryInterface *d = data->dictionary.get();

  // "あぼがど" -> "アボカド", which is in the test dictionary.
  const char kKey[] = "あぼがど";
  const char kValue[] = "アボカド";

  const LookupMethodAndQuery kTestPair[] = {
      {&DictionaryInterface::LookupPrefix, kKey},
      {&DictionaryInterface::LookupPredictive, "あぼ"},
  };

  // The spelling correction entry (kKey, kValue) should be found if spelling
  // correction flag is set in the config.
  config_.set_use_spelling_correction(true);
  for (size_t i = 0; i < arraysize(kTestPair); ++i) {
    CheckSpellingExistenceCallback callback(kKey, kValue);
    (d->*kTestPair[i].lookup_method)(kTestPair[i].query, convreq_, &callback);
    EXPECT_TRUE(callback.found());
  }

  // Without the flag, it should be suppressed.
  config_.set_use_spelling_correction(false);
  for (size_t i = 0; i < arraysize(kTestPair); ++i) {
    CheckSpellingExistenceCallback callback(kKey, kValue);
    (d->*kTestPair[i].lookup_method)(kTestPair[i].query, convreq_, &callback);
    EXPECT_FALSE(callback.found());
  }
}

TEST_F(DictionaryImplTest, DisableZipCodeConversionTest) {
  std::unique_ptr<DictionaryData> data = CreateDictionaryData();
  DictionaryInterface *d = data->dictionary.get();

  // "100-0000" -> "東京都千代田区", which is in the test dictionary.
  const char kKey[] = "100-0000";
  const char kValue[] = "東京都千代田区";

  const LookupMethodAndQuery kTestPair[] = {
      {&DictionaryInterface::LookupPrefix, kKey},
      {&DictionaryInterface::LookupPredictive, "100"},
  };

  // The zip code entry (kKey, kValue) should be found if the flag is set in the
  // config.
  config_.set_use_zip_code_conversion(true);
  for (size_t i = 0; i < arraysize(kTestPair); ++i) {
    CheckZipCodeExistenceCallback callback(kKey, kValue, &data->pos_matcher);
    (d->*kTestPair[i].lookup_method)(kTestPair[i].query, convreq_, &callback);
    EXPECT_TRUE(callback.found());
  }

  // Without the flag, it should be suppressed.
  config_.set_use_zip_code_conversion(false);
  for (size_t i = 0; i < arraysize(kTestPair); ++i) {
    CheckZipCodeExistenceCallback callback(kKey, kValue, &data->pos_matcher);
    (d->*kTestPair[i].lookup_method)(kTestPair[i].query, convreq_, &callback);
    EXPECT_FALSE(callback.found());
  }
}

TEST_F(DictionaryImplTest, DisableT13nConversionTest) {
  std::unique_ptr<DictionaryData> data = CreateDictionaryData();
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  const char kKey[] = "ぐーぐる";
  const char kValue[] = "Google";

  const LookupMethodAndQuery kTestPair[] = {
      {&DictionaryInterface::LookupPrefix, kKey},
      {&DictionaryInterface::LookupPredictive, "ぐー"},
  };

  // The T13N entry (kKey, kValue) should be found if the flag is set in the
  // config.
  config_.set_use_t13n_conversion(true);
  for (size_t i = 0; i < arraysize(kTestPair); ++i) {
    CheckEnglishT13nCallback callback(kKey, kValue);
    (d->*kTestPair[i].lookup_method)(kTestPair[i].query, convreq_, &callback);
    EXPECT_TRUE(callback.found());
  }

  // Without the flag, it should be suppressed.
  config_.set_use_t13n_conversion(false);
  for (size_t i = 0; i < arraysize(kTestPair); ++i) {
    CheckEnglishT13nCallback callback(kKey, kValue);
    (d->*kTestPair[i].lookup_method)(kTestPair[i].query, convreq_, &callback);
    EXPECT_FALSE(callback.found());
  }
}

TEST_F(DictionaryImplTest, LookupComment) {
  std::unique_ptr<DictionaryData> data = CreateDictionaryData();
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  std::string comment;
  EXPECT_FALSE(d->LookupComment("key", "value", convreq_, &comment));
  EXPECT_TRUE(comment.empty());

  // If key or value is "comment", UserDictionaryStub returns
  // "UserDictionaryStub" as comment.
  EXPECT_TRUE(d->LookupComment("key", "comment", convreq_, &comment));
  EXPECT_EQ("UserDictionaryStub", comment);
}

}  // namespace dictionary
}  // namespace mozc
