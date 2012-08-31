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

#include "data_manager/chromeos/chromeos_data_manager.h"

#include "data_manager/data_manager_test_base.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace chromeos {

namespace {
#include "data_manager/chromeos/chromeos_segmenter_inl.h"
}  // namespace

class ChromeOsDataManagerTest : public DataManagerTestBase {
 protected:
  ChromeOsDataManagerTest()
      : DataManagerTestBase(new ChromeOsDataManager,
                            kLSize,
                            kRSize,
                            IsBoundaryInternal,
                            "data/dictionary_chromeos/connection.txt",
                            1,
                            "data/dictionary_chromeos/dictionary00.txt,"
                            "data/dictionary_chromeos/dictionary01.txt,"
                            "data/dictionary_chromeos/dictionary02.txt,"
                            "data/dictionary_chromeos/dictionary03.txt,"
                            "data/dictionary_chromeos/dictionary04.txt,"
                            "data/dictionary_chromeos/dictionary05.txt,"
                            "data/dictionary_chromeos/dictionary06.txt,"
                            "data/dictionary_chromeos/dictionary07.txt,"
                            "data/dictionary_chromeos/dictionary08.txt,"
                            "data/dictionary_chromeos/dictionary09.txt",
                            "data/dictionary_chromeos/suggestion_filter.txt") {}
};

TEST_F(ChromeOsDataManagerTest, AllTests) {
  RunAllTests();
}

}  // namespace chromeos
}  // namespace mozc
