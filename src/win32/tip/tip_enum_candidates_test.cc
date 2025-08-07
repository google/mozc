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

#include "win32/tip/tip_enum_candidates.h"

#include <ctffunc.h>
#include <wil/com.h>
#include <wil/resource.h>

#include <array>
#include <string>
#include <vector>

#include "testing/gmock.h"
#include "testing/gunit.h"
#include "win32/tip/gmock_matchers.h"

namespace mozc::win32::tsf {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::NotNull;

TEST(TipEnumCandidatesTest, BasicScenario) {
  const std::vector<std::wstring> test_strings = {L"Hello", L"World"};
  TipEnumCandidates candidates(test_strings);

  // Run this block twice to test Reset().
  for (int i = 0; i < 2; ++i) {
    std::array<ITfCandidateString *, 2> strings;
    EXPECT_EQ(candidates.Next(0, strings.data(), nullptr), S_OK);
    EXPECT_EQ(candidates.Reset(), S_OK);
    ULONG fetched = 0;
    ASSERT_EQ(candidates.Next(2, strings.data(), &fetched), S_OK);
    ASSERT_EQ(fetched, 2);
    EXPECT_THAT(strings,
                ElementsAre(AllOf(CandidateStringIndexIs(0),
                                  CandidateStringIs(test_strings[0])),
                            AllOf(CandidateStringIndexIs(1),
                                  CandidateStringIs(test_strings[1]))));
    for (int i = 0; i < fetched; ++i) {
      strings[i]->Release();
    }
    EXPECT_EQ(candidates.Next(0, strings.data(), &fetched), S_OK);
    EXPECT_EQ(fetched, 0);
    EXPECT_EQ(candidates.Next(1, strings.data(), &fetched), S_FALSE);
    EXPECT_EQ(fetched, 0);
  }

  wil::com_ptr_nothrow<IEnumTfCandidates> cloned;
  EXPECT_EQ(candidates.Clone(cloned.put()), S_OK);
  ASSERT_THAT(cloned, NotNull());
  EXPECT_EQ(cloned->Skip(1), S_OK);
  std::array<ITfCandidateString *, 2> strings;
  ULONG fetched = 0;
  ASSERT_EQ(cloned->Next(2, strings.data(), &fetched), S_FALSE);
  ASSERT_EQ(fetched, 1);
  EXPECT_THAT(strings[0], AllOf(CandidateStringIndexIs(1),
                                CandidateStringIs(test_strings[1])));
  strings[0]->Release();
}

TEST(TipEnumCandidatesTest, Nullptr) {
  TipEnumCandidates candidates({L"mozc"});
  EXPECT_EQ(candidates.Next(0, nullptr, nullptr), E_INVALIDARG);
  EXPECT_EQ(candidates.Clone(nullptr), E_INVALIDARG);
}

TEST(TipEnumCandidatesTest, Skip) {
  TipEnumCandidates enum_candidates({L"Hello", L"World"});
  EXPECT_EQ(enum_candidates.Skip(0), S_OK);
  EXPECT_EQ(enum_candidates.Skip(1), S_OK);
  EXPECT_EQ(enum_candidates.Skip(2), S_FALSE);
}

}  // namespace
}  // namespace mozc::win32::tsf
