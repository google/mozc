// Copyright 2010-2014, Google Inc.
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
#include <string>

#include "base/system_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary_stub.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace dictionary {
namespace {

struct DictionaryData {
  scoped_ptr<DictionaryInterface> user_dictionary;
  scoped_ptr<SuppressionDictionary> suppression_dictionary;
  const POSMatcher *pos_matcher;
  scoped_ptr<DictionaryInterface> dictionary;
};

DictionaryData *CreateDictionaryData() {
  DictionaryData *ret = new DictionaryData;
  testing::MockDataManager data_manager;
  ret->pos_matcher = data_manager.GetPOSMatcher();
  const char *dictionary_data = NULL;
  int dictionary_size = 0;
  data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);
  DictionaryInterface *sys_dict =
      SystemDictionary::CreateSystemDictionaryFromImage(dictionary_data,
                                                        dictionary_size);
  DictionaryInterface *val_dict =
      ValueDictionary::CreateValueDictionaryFromImage(*ret->pos_matcher,
                                                      dictionary_data,
                                                      dictionary_size);
  ret->user_dictionary.reset(new UserDictionaryStub);
  ret->suppression_dictionary.reset(new SuppressionDictionary);
  ret->dictionary.reset(new DictionaryImpl(sys_dict,
                                           val_dict,
                                           ret->user_dictionary.get(),
                                           ret->suppression_dictionary.get(),
                                           ret->pos_matcher));
  return ret;
}

}  // namespace

class DictionaryImplTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
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
};

TEST_F(DictionaryImplTest, WordSuppressionTest) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  SuppressionDictionary *s = data->suppression_dictionary.get();

  // "ぐーぐるは"
  const char kQuery[] =
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B\xE3\x81\xAF";
  const char kKey[] =
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";  // "ぐーぐる"
  const char kValue[] =
      "\xE3\x82\xB0\xE3\x83\xBC\xE3\x82\xB0\xE3\x83\xAB";  // "グーグル"
  {
    s->Lock();
    s->Clear();
    s->AddEntry(kKey, kValue);
    s->UnLock();
    CheckKeyValueExistenceCallback callback(kKey, kValue);
    d->LookupPrefix(kQuery, false, &callback);
    EXPECT_FALSE(callback.found());
  }
  {
    s->Lock();
    s->Clear();
    s->UnLock();
    CheckKeyValueExistenceCallback callback(kKey, kValue);
    d->LookupPrefix(kQuery, false, &callback);
    EXPECT_TRUE(callback.found());
  }
}

TEST_F(DictionaryImplTest, DisableSpellingCorrectionTest) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();

  // "あぼがど" -> "アボカド", which is in the test dictionary.
  const char kKey[] = "\xE3\x81\x82\xE3\x81\xBC\xE3\x81\x8C\xE3\x81\xA9";
  const char kValue[] = "\xE3\x82\xA2\xE3\x83\x9C\xE3\x82\xAB\xE3\x83\x89";
  {
    config::Config config;
    config.set_use_spelling_correction(true);
    config::ConfigHandler::SetConfig(config);

    CheckSpellingExistenceCallback callback(kKey, kValue);
    d->LookupPrefix(kKey, false, &callback);
    EXPECT_TRUE(callback.found());

    config.set_use_spelling_correction(false);
    config::ConfigHandler::SetConfig(config);;

    CheckSpellingExistenceCallback callback2(kKey, kValue);
    d->LookupPrefix(kKey, false, &callback2);
    EXPECT_FALSE(callback2.found());
  }
}

TEST_F(DictionaryImplTest, DisableZipCodeConversionTest) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();

  // "100-0000" -> "東京都千代田区", which is in the test dictionary.
  const char kKey[] = "100-0000";
  const char kValue[] = "\xE6\x9D\xB1\xE4\xBA\xAC\xE9\x83\xBD\xE5\x8D"
                        "\x83\xE4\xBB\xA3\xE7\x94\xB0\xE5\x8C\xBA";
  {
    config::Config config;
    config.set_use_zip_code_conversion(true);
    config::ConfigHandler::SetConfig(config);

    CheckZipCodeExistenceCallback callback(kKey, kValue, data->pos_matcher);
    d->LookupPrefix(kKey, false, &callback);
    EXPECT_TRUE(callback.found());

    config.set_use_zip_code_conversion(false);
    config::ConfigHandler::SetConfig(config);;

    CheckZipCodeExistenceCallback callback2(kKey, kValue, data->pos_matcher);
    d->LookupPrefix(kKey, false, &callback2);
    EXPECT_FALSE(callback2.found());
  }
}

TEST_F(DictionaryImplTest, DisableT13nConversionTest) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  // "ぐーぐる" -> "Google"
  const char kKey[] =
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B";
  const char kValue[] = "Google";
  {
    config::Config config;
    config.set_use_t13n_conversion(true);
    config::ConfigHandler::SetConfig(config);

    CheckEnglishT13nCallback callback(kKey, kValue);
    d->LookupPrefix(kKey, false, &callback);
    EXPECT_TRUE(callback.found());

    config.set_use_t13n_conversion(false);
    config::ConfigHandler::SetConfig(config);;

    CheckEnglishT13nCallback callback2(kKey, kValue);
    d->LookupPrefix(kKey, false, &callback2);
    EXPECT_FALSE(callback2.found());
  }
}

TEST_F(DictionaryImplTest, LookupComment) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  string comment;
  EXPECT_FALSE(d->LookupComment("key", "value", &comment));
  EXPECT_TRUE(comment.empty());

  // If key or value is "comment", UserDictionaryStub returns
  // "UserDictionaryStub" as comment.
  EXPECT_TRUE(d->LookupComment("key", "comment", &comment));
  EXPECT_EQ("UserDictionaryStub", comment);
}

}  // namespace dictionary
}  // namespace mozc
