// Copyright 2010-2018, Google Inc.
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

#include "data_manager/chromeos/chromeos_data_manager.h"

#include "data_manager/data_manager_test_base.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"

namespace mozc {
namespace chromeos {
namespace {

#include "data_manager/chromeos/segmenter_inl.inc"

}  // namespace

class ChromeOsDataManagerTest : public DataManagerTestBase {
 protected:
  ChromeOsDataManagerTest()
      : DataManagerTestBase(
            new ChromeOsDataManager,
            kLSize,
            kRSize,
            IsBoundaryInternal,
            mozc::testing::GetSourceFileOrDie(
                {"data_manager", "chromeos", "connection_single_column.txt"}),
            1,
            mozc::testing::GetSourceFilesInDirOrDie(
                {"data", "dictionary_chromeos"},
                {"dictionary00.txt",
                 "dictionary01.txt",
                 "dictionary02.txt",
                 "dictionary03.txt",
                 "dictionary04.txt",
                 "dictionary05.txt",
                 "dictionary06.txt",
                 "dictionary07.txt",
                 "dictionary08.txt",
                 "dictionary09.txt"}),
            mozc::testing::GetSourceFilesInDirOrDie(
                {"data", "dictionary_chromeos"},
                {"suggestion_filter.txt"}),
            // ChromeOS doesn't include typing correction models.
            {}) {}
};

TEST_F(ChromeOsDataManagerTest, AllTests) {
  RunAllTests();
}

}  // namespace chromeos
}  // namespace mozc
