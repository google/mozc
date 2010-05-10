// Copyright 2010, Google Inc.
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

#include "dictionary/dictionary_file.h"

#include <cstdio>
#include "base/base.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
TEST(DictionaryFileTest, Basic) {
  const string dfn = FLAGS_test_tmpdir + "/test-dictionary";
  const string fn1 = FLAGS_test_tmpdir + "/sec1";
  const string fn2 = FLAGS_test_tmpdir + "/sec2";

  FILE *fp1 = fopen(fn1.c_str(), "w");
  CHECK(fp1) << "failed to open temporary file";
  fwrite("0123456789", 10, 1, fp1);
  fclose(fp1);
  FILE *fp2 = fopen(fn2.c_str(), "w");
  CHECK(fp2) << "failed to open temporary file";
  fwrite("9876543210", 10, 1, fp2);
  fclose(fp2);

  DictionaryFile dfw;
  dfw.Open(dfn.c_str(), true);
  dfw.AddSection("sec1", fn1.c_str());
  dfw.AddSection("sec2", fn2.c_str());
  dfw.Write();

  DictionaryFile dfr;
  dfr.Open(dfn.c_str(), false);
  int len;
  dfr.GetSection("sec1", &len);
  EXPECT_EQ(10, len);
  dfr.GetSection("sec2", &len);
  EXPECT_EQ(10, len);
}
}  // namespace mozc
