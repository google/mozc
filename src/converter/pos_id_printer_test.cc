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

#include "converter/pos_id_printer.h"

#include <string>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/flags.h"
#include "base/scoped_ptr.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_srcdir);

namespace mozc {
namespace internal {
namespace {
const char kTestIdDef[] =
    "data/test/dictionary/id.def";
}  // namespace

class PosIdPrinterTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    const string test_id_def_path =
        FileUtil::JoinPath(FLAGS_test_srcdir, kTestIdDef);
    pos_id_.reset(new InputFileStream(test_id_def_path.c_str()));
    pos_id_printer_.reset(new PosIdPrinter(pos_id_.get()));
  }

  scoped_ptr<InputFileStream> pos_id_;
  scoped_ptr<PosIdPrinter> pos_id_printer_;
};

TEST_F(PosIdPrinterTest, BasicIdTest) {
  EXPECT_EQ("名詞,サ変接続,*,*,*,*,*",
            pos_id_printer_->IdToString(1934));
  EXPECT_EQ("名詞,サ変接続,*,*,*,*,*,使用",
            pos_id_printer_->IdToString(1935));
  EXPECT_EQ("BOS/EOS,*,*,*,*,*,*",
            pos_id_printer_->IdToString(0));
}

TEST_F(PosIdPrinterTest, InvalidId) {
  EXPECT_EQ("", pos_id_printer_->IdToString(-1));
}

TEST_F(PosIdPrinterTest, NullInput) {
  PosIdPrinter pos_id_printer(NULL);
  EXPECT_EQ("", pos_id_printer.IdToString(-1));
  EXPECT_EQ("", pos_id_printer.IdToString(1934));
}

}  // namespace internal
}  // namespace mozc
