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
#include "dictionary/dictionary_interface.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/pos_matcher.h"
#include "testing/base/public/gunit.h"

namespace mozc {

TEST(Dictionary_test, basic) {
  // delete without Open()
  DictionaryInterface *d1 = DictionaryFactory::GetDictionary();
  EXPECT_TRUE(d1 != NULL);

  DictionaryInterface *d2 = DictionaryFactory::GetDictionary();
  EXPECT_TRUE(d2 != NULL);

  EXPECT_EQ(d1, d2);
}

TEST(Dictionary_test, WordSuppressionTest) {
  DictionaryInterface *d = DictionaryFactory::GetDictionary();
  SuppressionDictionary *s =
      SuppressionDictionary::GetSuppressionDictionary();

  NodeAllocator allocator;

  // "ぐーぐるは"
  const char kQuery[] = "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90"
      "\xE3\x82\x8B\xE3\x81\xAF";

  // "ぐーぐる"
  const char kKey[]  = "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90"
      "\xE3\x82\x8B";

  // "グーグル"
  const char kValue[] = "\xE3\x82\xB0\xE3\x83\xBC\xE3"
      "\x82\xB0\xE3\x83\xAB";

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

TEST(Dictionary_test, DisableSpellingCorrectionTest) {
  DictionaryInterface *d = DictionaryFactory::GetDictionary();
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

TEST(Dictionary_test, DisableZipCodeConversionTest) {
  DictionaryInterface *d = DictionaryFactory::GetDictionary();
  NodeAllocator allocator;

  const char kQuery[] = "154-0000";

  {
    config::Config config;
    config.set_use_zip_code_conversion(true);
    config::ConfigHandler::SetConfig(config);

    Node *node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      if (POSMatcher::IsZipcode(node->lid)) {
        EXPECT_TRUE(GET_CONFIG(use_zip_code_conversion));
      }
    }

    config.set_use_zip_code_conversion(false);
    config::ConfigHandler::SetConfig(config);;
    node = d->LookupPrefix(kQuery, strlen(kQuery), &allocator);
    for (; node != NULL; node = node->bnext) {
      EXPECT_FALSE(POSMatcher::IsZipcode(node->lid));
    }
  }
}

TEST(Dictionary_test, DisableT13nConversionTest) {
  DictionaryInterface *d = DictionaryFactory::GetDictionary();
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
}  // namespace mozc
