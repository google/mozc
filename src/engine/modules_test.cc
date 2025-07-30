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
#include "engine/supplemental_model_interface.h"
#include "testing/gunit.h"

namespace mozc {
namespace engine {

TEST(ModulesTest, CreateTest) {
  std::unique_ptr<const engine::Modules> modules =
      Modules::Create(std::make_unique<testing::MockDataManager>()).value();
}

TEST(ModulesTest, BuildTwiceTest) {
  ModulesPresetBuilder builder;

  EXPECT_TRUE(builder.Build(std::make_unique<testing::MockDataManager>()).ok());
  EXPECT_FALSE(
      builder.Build(std::make_unique<testing::MockDataManager>()).ok());
}

TEST(ModulesTest, PresetTest) {
  testing::MockDataManager mock_data_manager;

  // PosMatcher
  auto pos_matcher = std::make_unique<dictionary::PosMatcher>(
      mock_data_manager.GetPosMatcherData());
  const dictionary::PosMatcher *pos_matcher_ptr = pos_matcher.get();

  // UserDictionary
  auto user_dictionary = std::make_unique<dictionary::MockUserDictionary>();
  const dictionary::MockUserDictionary *user_dictionary_ptr =
      user_dictionary.get();

  // SuffixDictionary
  auto suffix_dictionary = std::make_unique<dictionary::MockDictionary>();
  const dictionary::DictionaryInterface *suffix_dictionary_ptr =
      suffix_dictionary.get();

  // Dictionary
  auto dictionary = std::make_unique<dictionary::MockDictionary>();
  const dictionary::DictionaryInterface *dictionary_ptr = dictionary.get();

  std::unique_ptr<const engine::Modules> modules =
      ModulesPresetBuilder()
          .PresetPosMatcher(std::move(pos_matcher))
          .PresetUserDictionary(std::move(user_dictionary))
          .PresetSuffixDictionary(std::move(suffix_dictionary))
          .PresetDictionary(std::move(dictionary))
          .Build(std::make_unique<testing::MockDataManager>())
          .value();

  EXPECT_EQ(&modules->GetPosMatcher(), pos_matcher_ptr);
  EXPECT_EQ(&modules->GetUserDictionary(), user_dictionary_ptr);
  EXPECT_EQ(&modules->GetSuffixDictionary(), suffix_dictionary_ptr);
  EXPECT_EQ(&modules->GetDictionary(), dictionary_ptr);
}

TEST(ModulesTest, SupplementalModelTest) {
  std::unique_ptr<Modules> modules1 =
      Modules::Create(std::make_unique<testing::MockDataManager>()).value();
  std::unique_ptr<Modules> modules2 =
      Modules::Create(std::make_unique<testing::MockDataManager>()).value();

  // Returns the same non-null static instance by default.
  EXPECT_TRUE(&modules1->GetSupplementalModel());
  EXPECT_EQ(&modules1->GetSupplementalModel(),
            &modules2->GetSupplementalModel());

  auto supplemental_model = std::make_unique<SupplementalModelStub>();
  // TODO(taku): Avoid sharing the pointer of std::unique_ptr.
  const SupplementalModelInterface *supplemental_model_ptr =
      supplemental_model.get();
  std::unique_ptr<Modules> modules3 =
      ModulesPresetBuilder()
          .PresetSupplementalModel(std::move(supplemental_model))
          .Build(std::make_unique<testing::MockDataManager>())
          .value();

  EXPECT_NE(&modules1->GetSupplementalModel(),
            &modules3->GetSupplementalModel());
  EXPECT_EQ(supplemental_model_ptr, &modules3->GetSupplementalModel());

  // The default static instance is used again.
  std::unique_ptr<Modules> modules4 =
      Modules::Create(std::make_unique<testing::MockDataManager>()).value();
  EXPECT_EQ(&modules1->GetSupplementalModel(),
            &modules4->GetSupplementalModel());
}

}  // namespace engine
}  // namespace mozc
