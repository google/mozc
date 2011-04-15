// Copyright 2010-2011, Google Inc.
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

#include <string.h>
#include "base/base.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

DEFINE_string(test_string, "hogehoge", "test_string");
DEFINE_int32(test_int32, 20, "test_int32");
DEFINE_int64(test_int64,   29051773239673121LL, "test_int64");
DEFINE_uint64(test_uint64, 84467440737095516LL, "test_uint64");
DEFINE_bool(test_bool, false, "test_bool");
DEFINE_double(test_double, 0.5, "test_double");

namespace mozc {

namespace {
char *strdup_with_new(const char *str) {
  char *result = new char [strlen(str) + 1];
  strcpy(result, str);
  return result;
}
}   // namespace

namespace {
TEST(FlagsTest, FlagsBasicTest) {
  EXPECT_EQ(false, FLAGS_test_bool);
  EXPECT_EQ(20, FLAGS_test_int32);
  EXPECT_EQ(29051773239673121LL, FLAGS_test_int64);
  EXPECT_EQ(84467440737095516LL, FLAGS_test_uint64);
  EXPECT_EQ("hogehoge", FLAGS_test_string);
  EXPECT_LT(0.4, FLAGS_test_double);
  EXPECT_GT(0.6, FLAGS_test_double);

  char **argv = new char * [7];
  argv[0] = strdup_with_new("test");
  argv[1] = strdup_with_new("--test_int32=11214141");
  argv[2] = strdup_with_new("--test_string=test");
  argv[3] = strdup_with_new("--test_bool=true");
  argv[4] = strdup_with_new("--test_double=1.5");
  argv[5] = strdup_with_new("--test_int64=8172141141413124");
  argv[6] = strdup_with_new("--test_uint64=9414041694169841");
  int argc = 7;
  mozc_flags::ParseCommandLineFlags(&argc, &argv, false);
  for (size_t i = 0; i < 7; ++i) {
    delete [] argv[i];
  }
  delete [] argv;

  EXPECT_EQ(true, FLAGS_test_bool);
  EXPECT_EQ(11214141, FLAGS_test_int32);
  EXPECT_EQ(8172141141413124LL, FLAGS_test_int64);
  EXPECT_EQ(9414041694169841LL, FLAGS_test_uint64);
  EXPECT_EQ("test", FLAGS_test_string);
  EXPECT_LT(1.4, FLAGS_test_double);
  EXPECT_GT(1.6, FLAGS_test_double);
}
}  // namespace
}  // namespace mozc
