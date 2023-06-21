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

#include "data_manager/oss/oss_data_manager.h"

#include <string>
#include <utility>

#include "data_manager/data_manager_test_base.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace oss {
namespace {

#include "data_manager/oss/segmenter_inl.inc"

std::pair<std::string, std::string> GetTypingModelEntry(
    const std::string &fname) {
  return std::pair<std::string, std::string>(
      fname, mozc::testing::GetSourceFileOrDie(
                 {"data_manager", "oss", fname + ".data"}));
}

}  // namespace

class OssDataManagerTest : public DataManagerTestBase {
 protected:
  OssDataManagerTest()
      : DataManagerTestBase(
            new OssDataManager, kLSize, kRSize, IsBoundaryInternal,
            mozc::testing::GetSourceFileOrDie(
                {"data_manager", "oss", "connection_single_column.txt"}),
            1,
            mozc::testing::GetSourceFilesInDirOrDie(
                {MOZC_DICT_DIR_COMPONENTS, "dictionary_oss"},
                {"dictionary00.txt", "dictionary01.txt", "dictionary02.txt",
                 "dictionary03.txt", "dictionary04.txt", "dictionary05.txt",
                 "dictionary06.txt", "dictionary07.txt", "dictionary08.txt",
                 "dictionary09.txt"}),
            mozc::testing::GetSourceFilesInDirOrDie(
                {MOZC_DICT_DIR_COMPONENTS, "dictionary_oss"},
                {"suggestion_filter.txt"}),
            {
                GetTypingModelEntry("typing_model_12keys-hiragana.tsv"),
                GetTypingModelEntry("typing_model_flick-hiragana.tsv"),
                GetTypingModelEntry("typing_model_godan-hiragana.tsv"),
                GetTypingModelEntry("typing_model_qwerty_mobile-hiragana.tsv"),
                GetTypingModelEntry("typing_model_toggle_flick-hiragana.tsv"),
            }) {}
};

TEST_F(OssDataManagerTest, AllTests) { RunAllTests(); }

}  // namespace oss
}  // namespace mozc
