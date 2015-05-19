// Copyright 2010-2012, Google Inc.
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

#include "base/base.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
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
  DictionaryInterface *sys_dict = data_manager.CreateSystemDictionary();
  DictionaryInterface *val_dict = data_manager.CreateValueDictionary();
  ret->user_dictionary.reset(new UserDictionaryStub);
  ret->suppression_dictionary.reset(new SuppressionDictionary);
  ret->pos_matcher = data_manager.GetPOSMatcher();
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
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
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
};

TEST_F(DictionaryImplTest, WordSuppressionTest) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  SuppressionDictionary *s = data->suppression_dictionary.get();

  NodeAllocator allocator;

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
    Node *node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    bool found = false;
    for (; node != NULL; node = node->bnext) {
      if (node->key == kKey && node->value == kValue) {
        found = true;
      }
    }
    EXPECT_FALSE(found);
  }
  {
    s->Lock();
    s->Clear();
    s->UnLock();
    Node *node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    bool found = false;
    for (; node != NULL; node = node->bnext) {
      if (node->key == kKey && node->value == kValue) {
        found = true;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_F(DictionaryImplTest, DisableSpellingCorrectionTest) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  // "しゅみれーしょん"
  const char kQuery[] = "\xE3\x81\x97\xE3\x82\x85\xE3\x81\xBF"
      "\xE3\x82\x8C\xE3\x83\xBC\xE3\x81\x97\xE3\x82\x87\xE3\x82\x93";

  {
    config::Config config;
    config.set_use_spelling_correction(true);
    config::ConfigHandler::SetConfig(config);

    Node *node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      if (node->attributes & Node::SPELLING_CORRECTION) {
        EXPECT_TRUE(GET_CONFIG(use_spelling_correction));
      }
    }

    config.set_use_spelling_correction(false);
    config::ConfigHandler::SetConfig(config);;
    node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      EXPECT_FALSE(node->attributes & Node::SPELLING_CORRECTION);
    }
  }
}

TEST_F(DictionaryImplTest, DisableZipCodeConversionTest) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  const char kQuery[] = "154-0000";

  {
    config::Config config;
    config.set_use_zip_code_conversion(true);
    config::ConfigHandler::SetConfig(config);

    const POSMatcher *pos_matcher = data->pos_matcher;
    Node *node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      if (pos_matcher->IsZipcode(node->lid)) {
        EXPECT_TRUE(GET_CONFIG(use_zip_code_conversion));
      }
    }

    config.set_use_zip_code_conversion(false);
    config::ConfigHandler::SetConfig(config);;
    node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      EXPECT_FALSE(pos_matcher->IsZipcode(node->lid));
    }
  }
}

TEST_F(DictionaryImplTest, DisableT13nConversionTest) {
  scoped_ptr<DictionaryData> data(CreateDictionaryData());
  DictionaryInterface *d = data->dictionary.get();
  NodeAllocator allocator;

  // "いんたーねっと"
  const char kQuery[] = "\xE3\x81\x84\xE3\x82\x93\xE3\x81\x9F"
      "\xE3\x83\xBC\xE3\x81\xAD\xE3\x81\xA3\xE3\x81\xA8";

  {
    config::Config config;
    config.set_use_t13n_conversion(true);
    config::ConfigHandler::SetConfig(config);

    Node *node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      if (Util::IsEnglishTransliteration(node->value)) {
        EXPECT_TRUE(GET_CONFIG(use_t13n_conversion));
      }
    }

    config.set_use_t13n_conversion(false);
    config::ConfigHandler::SetConfig(config);;
    node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      EXPECT_FALSE(Util::IsEnglishTransliteration(node->value));
    }
  }
}

}  // namespace dictionary
}  // namespace mozc
