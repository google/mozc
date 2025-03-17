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

#include "engine/modules.h"

#include <memory>
#include <utility>

#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_matcher.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace engine {

TEST(ModulesTest, Init) {
  Modules modules;

  EXPECT_EQ(modules.GetPosMatcher(), nullptr);
  EXPECT_EQ(modules.GetSegmenter(), nullptr);
  EXPECT_EQ(modules.GetUserDictionary(), nullptr);
  EXPECT_EQ(modules.GetPosGroup(), nullptr);

  ASSERT_OK(modules.Init(std::make_unique<testing::MockDataManager>()));

  EXPECT_NE(modules.GetPosMatcher(), nullptr);
  EXPECT_NE(modules.GetSegmenter(), nullptr);
  EXPECT_NE(modules.GetUserDictionary(), nullptr);
  EXPECT_NE(modules.GetPosGroup(), nullptr);
}

TEST(ModulesTest, Preset) {
  Modules modules;

  testing::MockDataManager mock_data_manager;

  // PosMatcher
  auto pos_matcher = std::make_unique<dictionary::PosMatcher>(
      mock_data_manager.GetPosMatcherData());
  const dictionary::PosMatcher *pos_matcher_ptr = pos_matcher.get();
  EXPECT_NE(pos_matcher_ptr, nullptr);
  modules.PresetPosMatcher(std::move(pos_matcher));

  // UserDictionary
  auto user_dictionary = std::make_unique<dictionary::MockUserDictionary>();
  const dictionary::MockUserDictionary *user_dictionary_ptr =
      user_dictionary.get();
  EXPECT_NE(user_dictionary_ptr, nullptr);
  modules.PresetUserDictionary(std::move(user_dictionary));

  // SuffixDictionary
  auto suffix_dictionary = std::make_unique<dictionary::MockDictionary>();
  const dictionary::DictionaryInterface *suffix_dictionary_ptr =
      suffix_dictionary.get();
  EXPECT_NE(suffix_dictionary_ptr, nullptr);
  modules.PresetSuffixDictionary(std::move(suffix_dictionary));

  // Dictionary
  auto dictionary = std::make_unique<dictionary::MockDictionary>();
  const dictionary::DictionaryInterface *dictionary_ptr = dictionary.get();
  EXPECT_NE(dictionary_ptr, nullptr);
  modules.PresetDictionary(std::move(dictionary));

  ASSERT_OK(modules.Init(std::make_unique<testing::MockDataManager>()));

  EXPECT_EQ(modules.GetPosMatcher(), pos_matcher_ptr);
  EXPECT_EQ(modules.GetUserDictionary(), user_dictionary_ptr);
  EXPECT_EQ(modules.GetSuffixDictionary(), suffix_dictionary_ptr);
  EXPECT_EQ(modules.GetDictionary(), dictionary_ptr);
}

}  // namespace engine
}  // namespace mozc
