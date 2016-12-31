// Copyright 2010-2017, Google Inc.
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

namespace mozc {
namespace dictionary {
namespace {

struct DictionaryData {
  std::unique_ptr<DictionaryInterface> user_dictionary;
  std::unique_ptr<SuppressionDictionary> suppression_dictionary;
  POSMatcher pos_matcher;
  std::unique_ptr<DictionaryInterface> dictionary;
};

DictionaryData *CreateDictionaryData() {
  DictionaryData *ret = new DictionaryData;
  testing::MockDataManager data_manager;
  ret->pos_matcher.Set(data_manager.GetPOSMatcherData());
  const char *dictionary_data = NULL;
  int dictionary_size = 0;
  data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);
  SystemDictionary *sys_dict =
      SystemDictionary::Builder(dictionary_data, dictionary_size).Build();
  ValueDictionary *val_dict =
      new ValueDictionary(ret->pos_matcher, &sys_dict->value_trie());
  ret->user_dictionary.reset(new UserDictionaryStub);
  ret->suppression_dictionary.reset(new SuppressionDictionary);
  ret->dictionary.reset(new DictionaryImpl(sys_dict,
                                           val_dict,
                                           ret->user_dictionary.get(),
                                           ret->suppression_dictionary.get(),
                                           &ret->pos_matcher));
  return ret;
}

}  // namespace

class DictionaryImplTest : public ::testing::Test {
 protected:
  DictionaryImplTest() {
    convreq_.set_config(&config_);
  }

  virtual void SetUp() {
    config::ConfigHandler::GetDefaultConfig(&config_);
  }

  class CheckKeyValueExistenceCallback : public DictionaryInterface::Callback {
   public:
    CheckKeyValueExistenceCallback(StringPiece key, StringPiece value)
        : key_(key), value_(value), found_(false) {}

    virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                               const Token &token) {
      if (token.key == key_ && token.value == value_) {
        found_ = true;
        return TRAVERSE_DONE;
      }
      return TRAVERSE_CONTINUE;
    }

    bool found() const { return found_; }

   private:
    const StringPiece key_, value_;
    bool found_;
  };

  class CheckSpellingExistenceCallback : public DictionaryInterface::Callback {
   public:
    CheckSpellingExistenceCallback(StringPiece key, StringPiece value)
        : key_(key), value_(value), found_(false) {}

    virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                               const Token &token) {
      if (token.key == key_ && token.value == value_ &&
          (token.attributes & Token::SPELLING_CORRECTION)) {
        found_ = true;
        return TRAVERSE_DONE;
      }
      return TRAVERSE_CONTINUE;
    }

    bool found() const { return found_; }

   private:
    const StringPiece key_, value_;
    bool found_;
  };

  class CheckZipCodeExistenceCallback : public DictionaryInterface::Callback {
   public:
    explicit CheckZipCodeExistenceCallback(StringPiece key, StringPiece value,
                                           const POSMatcher *pos_matcher)
        : key_(key), value_(value), pos_matcher_(pos_matcher), found_(false) {}

    virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                               const Token &token) {
      if (token.key == key_ && token.value == value_ &&
          pos_matcher_->IsZipcode(token.lid)) {
        found_ = true;
        return TRAVERSE_DONE;
      }
      return TRAVERSE_CONTINUE;
    }

    bool found() const { return found_; }

   private:
    const StringPiece key_, value_;
    const POSMatcher *pos_matcher_;
    bool found_;
  };

  class CheckEnglishT13nCallback : public DictionaryInterface::Callback {
   public:
    CheckEnglishT13nCallback(StringPiece key, StringPiece value)
        : key_(key), value_(value), found_(false) {}

    virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                               const Token &token) {
      if (token.key == key_ && token.value == value_ &&
          Util::IsEnglishTransliteration(token.value)) {
        found_ = true;
        return TRAVERSE_DONE;
      }
      return TRAVERSE_CONTINUE;
    }

    bool found() const { return found_; }

   private:
    const StringPiece key_, value_;
    bool found_;
  };

  // Pair of DictionaryInterface's lookup method and query text.
  struct LookupMethodAndQuery {
    void (DictionaryInterface::*lookup_method)(
        StringPiece,
        const ConversionRequest &,
        DictionaryInterface::Callback *) const;
    const char *query;
  };

  ConversionRequest convreq_;
  config::Config config_;
};

TEST_F(DictionaryImplTest, WordSuppressionTest) {
  std::unique_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  SuppressionDictionary *s = data->suppression_dictionary.get();

  const char kKey[] =
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";  // "ぐーぐる"
  const char kValue[] =
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB";  // "グーグル"

  const LookupMethodAndQuery kTestPair[] = {
    // "ぐーぐるは"
    {&DictionaryInterface::LookupPrefix,
     "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\xAF"},
    // "ぐーぐ"
    {&DictionaryInterface::LookupPredictive,
     "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90"},
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
  std::unique_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();

  // "あぼがど" -> "アボカド", which is in the test dictionary.
  const char kKey[] = "\xE3\x81\x82\xE3\x81\xBC\xE3\x81\x8C\xE3\x81\xA9";
  const char kValue[] = "\xE3\x82\xA2\xE3\x83\x9C\xE3\x82\xAB\xE3\x83\x89";

  const LookupMethodAndQuery kTestPair[] = {
    // "あぼがど"
    {&DictionaryInterface::LookupPrefix, kKey},
    // "あぼ"
    {&DictionaryInterface::LookupPredictive, "\xE3\x81\x82\xE3\x81\xBC"},
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
  std::unique_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();

  // "100-0000" -> "東京都千代田区", which is in the test dictionary.
  const char kKey[] = "100-0000";
  const char kValue[] = "\xE6\x9D\xB1\xE4\xBA\xAC\xE9\x83\xBD\xE5\x8D"
                        "\x83\xE4\xBB\xA3\xE7\x94\xB0\xE5\x8C\xBA";

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
  std::unique_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  // "ぐーぐる" -> "Google"
  const char kKey[] =
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
  const char kValue[] = "Google";

  const LookupMethodAndQuery kTestPair[] = {
    {&DictionaryInterface::LookupPrefix, kKey},
    // "ぐー"
    {&DictionaryInterface::LookupPredictive, "\xE3\x81\x90\xE3\x83\xBC"},
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
  std::unique_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  string comment;
  EXPECT_FALSE(d->LookupComment("key", "value", convreq_, &comment));
  EXPECT_TRUE(comment.empty());

  // If key or value is "comment", UserDictionaryStub returns
  // "UserDictionaryStub" as comment.
  EXPECT_TRUE(d->LookupComment("key", "comment", convreq_, &comment));
  EXPECT_EQ("UserDictionaryStub", comment);
}

}  // namespace dictionary
}  // namespace mozc
