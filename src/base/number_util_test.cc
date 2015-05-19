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

#include "base/number_util.h"

#include "base/port.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

TEST(NumberUtilTest, SimpleItoa) {
  EXPECT_EQ("0",   NumberUtil::SimpleItoa(0));
  EXPECT_EQ("123", NumberUtil::SimpleItoa(123));
  EXPECT_EQ("-1",  NumberUtil::SimpleItoa(-1));

  EXPECT_EQ("2147483647",  NumberUtil::SimpleItoa(kint32max));
  EXPECT_EQ("-2147483648", NumberUtil::SimpleItoa(kint32min));
  EXPECT_EQ("4294967295",  NumberUtil::SimpleItoa(kuint32max));

  EXPECT_EQ("9223372036854775807",  NumberUtil::SimpleItoa(kint64max));
  EXPECT_EQ("-9223372036854775808", NumberUtil::SimpleItoa(kint64min));
  EXPECT_EQ("18446744073709551615", NumberUtil::SimpleItoa(kuint64max));
}

TEST(NumberUtilTest, SimpleAtoi) {
  EXPECT_EQ(0, NumberUtil::SimpleAtoi("0"));
  EXPECT_EQ(123, NumberUtil::SimpleAtoi("123"));
  EXPECT_EQ(-1, NumberUtil::SimpleAtoi("-1"));
}

TEST(NumberUtilTest, SafeStrToInt32) {
  int32 value = 0xDEADBEEF;

  EXPECT_TRUE(NumberUtil::SafeStrToInt32("0", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32("+0", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32("-0", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32(" \t\r\n\v\f-0 \t\r\n\v\f", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32("012345678", &value));
  EXPECT_EQ(12345678, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32("-012345678", &value));
  EXPECT_EQ(-12345678, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32("-2147483648", &value));
  EXPECT_EQ(kint32min, value);  // min of 32-bit signed integer
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32("2147483647", &value));
  EXPECT_EQ(kint32max, value);  // max of 32-bit signed integer
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32(" 1", &value));
  EXPECT_EQ(1, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt32("2 ", &value));
  EXPECT_EQ(2, value);

  EXPECT_FALSE(NumberUtil::SafeStrToInt32("0x1234", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt32("-2147483649", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt32("2147483648", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt32("18446744073709551616", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt32("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt32("0.", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt32(".0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt32("", &value));

  // Test for StringPiece input.
  const char *kString = "123 abc 789";
  EXPECT_TRUE(NumberUtil::SafeStrToInt32(StringPiece(kString, 3),
                                         &value));
  EXPECT_EQ(123, value);
  EXPECT_FALSE(NumberUtil::SafeStrToInt32(StringPiece(kString + 4, 3),
                                          &value));
  EXPECT_TRUE(NumberUtil::SafeStrToInt32(StringPiece(kString + 8, 3),
                                         &value));
  EXPECT_EQ(789, value);
  EXPECT_TRUE(NumberUtil::SafeStrToInt32(StringPiece(kString + 7, 4),
                                         &value));
  EXPECT_EQ(789, value);
}

TEST(NumberUtilTest, SafeStrToInt64) {
  int64 value = 0xDEADBEEF;

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64("0", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64("+0", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64("-0", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64(" \t\r\n\v\f-0 \t\r\n\v\f", &value));
  EXPECT_EQ(0, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64("012345678", &value));
  EXPECT_EQ(12345678, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64("-012345678", &value));
  EXPECT_EQ(-12345678, value);
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64("-9223372036854775808", &value));
  EXPECT_EQ(kint64min, value);  // min of 64-bit signed integer
  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToInt64("9223372036854775807", &value));
  EXPECT_EQ(kint64max, value);  // max of 64-bit signed integer

  EXPECT_FALSE(NumberUtil::SafeStrToInt64("-9223372036854775809",  // overflow
                                          &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt64("9223372036854775808",  // overflow
                                          &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt64("0x1234", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt64("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt64("0.", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt64(".0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToInt64("", &value));

  // Test for StringPiece input.
  const char *kString = "123 abc 789";
  EXPECT_TRUE(NumberUtil::SafeStrToInt64(StringPiece(kString, 3),
                                         &value));
  EXPECT_EQ(123, value);
  EXPECT_FALSE(NumberUtil::SafeStrToInt64(StringPiece(kString + 4, 3),
                                          &value));
  EXPECT_TRUE(NumberUtil::SafeStrToInt64(StringPiece(kString + 8, 3),
                                         &value));
  EXPECT_EQ(789, value);
}

TEST(NumberUtilTest, SafeStrToUInt32) {
  uint32 value = 0xDEADBEEF;

  EXPECT_TRUE(NumberUtil::SafeStrToUInt32("0", &value));
  EXPECT_EQ(0, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt32(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(0, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt32("012345678", &value));
  EXPECT_EQ(12345678, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt32("4294967295", &value));
  EXPECT_EQ(4294967295u, value);  // max of 32-bit unsigned integer

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt32(" 1", &value));
  EXPECT_EQ(1, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeStrToUInt32("2 ", &value));
  EXPECT_EQ(2, value);

  EXPECT_FALSE(NumberUtil::SafeStrToUInt32("-0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt32("0x1234", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt32("4294967296", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt32("18446744073709551616", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt32("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt32("0.", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt32(".0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt32("", &value));

  // Test for StringPiece input.
  const char *kString = "123 abc 789";
  EXPECT_TRUE(NumberUtil::SafeStrToUInt32(StringPiece(kString, 3),
                                          &value));
  EXPECT_EQ(123, value);
  EXPECT_FALSE(NumberUtil::SafeStrToUInt32(StringPiece(kString + 4, 3),
                                           &value));
  EXPECT_TRUE(NumberUtil::SafeStrToUInt32(StringPiece(kString + 8, 3),
                                          &value));
  EXPECT_EQ(789, value);
  EXPECT_TRUE(NumberUtil::SafeStrToUInt32(StringPiece(kString + 7, 4),
                                          &value));
  EXPECT_EQ(789, value);
}

TEST(NumberUtilTest, SafeHexStrToUInt32) {
  uint32 value = 0xDEADBEEF;

  EXPECT_TRUE(NumberUtil::SafeHexStrToUInt32("0", &value));
  EXPECT_EQ(0, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(
      NumberUtil::SafeHexStrToUInt32(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(0, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeHexStrToUInt32("0ABCDE", &value));
  EXPECT_EQ(0xABCDE, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeHexStrToUInt32("0abcde", &value));
  EXPECT_EQ(0xABCDE, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeHexStrToUInt32("FFFFFFFF", &value));
  EXPECT_EQ(0xFFFFFFFF, value);  // max of 32-bit unsigned integer

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeHexStrToUInt32("ffffffff", &value));
  EXPECT_EQ(0xFFFFFFFF, value);  // max of 32-bit unsigned integer

  EXPECT_FALSE(NumberUtil::SafeHexStrToUInt32("-0", &value));
  EXPECT_FALSE(
      NumberUtil::SafeHexStrToUInt32("100000000", &value));  // overflow
  EXPECT_FALSE(NumberUtil::SafeHexStrToUInt32("GHIJK", &value));
  EXPECT_FALSE(NumberUtil::SafeHexStrToUInt32("0.", &value));
  EXPECT_FALSE(NumberUtil::SafeHexStrToUInt32(".0", &value));
  EXPECT_FALSE(NumberUtil::SafeHexStrToUInt32("", &value));

  // Test for StringPiece input.
  const char *kString = "123 abc 5x";
  EXPECT_TRUE(NumberUtil::SafeHexStrToUInt32(StringPiece(kString, 3),
                                             &value));
  EXPECT_EQ(291, value);
  EXPECT_TRUE(NumberUtil::SafeHexStrToUInt32(StringPiece(kString + 4, 3),
                                             &value));
  EXPECT_EQ(2748, value);
  EXPECT_FALSE(NumberUtil::SafeHexStrToUInt32(StringPiece(kString + 8, 2),
                                              &value));
}

TEST(NumberUtilTest, SafeOctStrToUInt32) {
  uint32 value = 0xDEADBEEF;

  EXPECT_TRUE(NumberUtil::SafeOctStrToUInt32("0", &value));
  EXPECT_EQ(0, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(
      NumberUtil::SafeOctStrToUInt32(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(0, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeOctStrToUInt32("012345", &value));
  EXPECT_EQ(012345, value);

  value = 0xDEADBEEF;
  EXPECT_TRUE(NumberUtil::SafeOctStrToUInt32("37777777777", &value));
  EXPECT_EQ(0xFFFFFFFF, value);  // max of 32-bit unsigned integer

  EXPECT_FALSE(NumberUtil::SafeOctStrToUInt32("-0", &value));
  EXPECT_FALSE(
      NumberUtil::SafeOctStrToUInt32("40000000000", &value));  // overflow
  EXPECT_FALSE(NumberUtil::SafeOctStrToUInt32("9AB", &value));
  EXPECT_FALSE(NumberUtil::SafeOctStrToUInt32("0.", &value));
  EXPECT_FALSE(NumberUtil::SafeOctStrToUInt32(".0", &value));
  EXPECT_FALSE(NumberUtil::SafeOctStrToUInt32("", &value));

  // Test for StringPiece input.
  const char *kString = "123 456 789";
  EXPECT_TRUE(NumberUtil::SafeOctStrToUInt32(StringPiece(kString, 3),
                                             &value));
  EXPECT_EQ(83, value);
  EXPECT_TRUE(NumberUtil::SafeOctStrToUInt32(StringPiece(kString + 4, 3),
                                             &value));
  EXPECT_EQ(302, value);
  EXPECT_FALSE(NumberUtil::SafeOctStrToUInt32(StringPiece(kString + 8, 3),
                                              &value));
}

TEST(NumberUtilTest, SafeStrToUInt64) {
  uint64 value = 0xDEADBEEF;

  EXPECT_TRUE(NumberUtil::SafeStrToUInt64("0", &value));
  EXPECT_EQ(0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToUInt64(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToUInt64("012345678", &value));
  EXPECT_EQ(12345678, value);
  EXPECT_TRUE(NumberUtil::SafeStrToUInt64("18446744073709551615", &value));
  EXPECT_EQ(18446744073709551615ull, value);  // max of 64-bit unsigned integer

  EXPECT_FALSE(NumberUtil::SafeStrToUInt64("-0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt64("18446744073709551616",  // overflow
                                     &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt64("0x1234", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt64("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt64("0.", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt64(".0", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToUInt64("", &value));

  // Test for StringPiece input.
  const char *kString = "123 abc 789";
  EXPECT_TRUE(NumberUtil::SafeStrToUInt64(StringPiece(kString, 3),
                                          &value));
  EXPECT_EQ(123, value);
  EXPECT_FALSE(NumberUtil::SafeStrToUInt64(StringPiece(kString + 4, 3),
                                           &value));
  EXPECT_TRUE(NumberUtil::SafeStrToUInt64(StringPiece(kString + 8, 3),
                                          &value));
  EXPECT_EQ(789, value);
}

TEST(NumberUtilTest, SafeStrToDouble) {
  double value = 1.0;

  EXPECT_TRUE(NumberUtil::SafeStrToDouble("0", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("-0", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("1.0e1", &value));
  EXPECT_EQ(10.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("-5.0e-1", &value));
  EXPECT_EQ(-0.5, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble(".0", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("0.", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("0.0", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("1.7976931348623158e308", &value));
  EXPECT_EQ(1.7976931348623158e308, value);  // approximated representation
                                             // of max of double
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("-1.7976931348623158e308", &value));
  EXPECT_EQ(-1.7976931348623158e308, value);
  // GCC accepts hexadecimal number, but VisualC++ does not.
#ifndef _MSC_VER
#ifdef OS_ANDROID
  // strtod in android OS doesn't accept 0x1234, so skip the following test.
#else
  EXPECT_TRUE(NumberUtil::SafeStrToDouble("0x1234", &value));
  EXPECT_EQ(static_cast<double>(0x1234), value);
#endif  // not OS_ANDROID
#endif

  EXPECT_FALSE(
      NumberUtil::SafeStrToDouble("1.7976931348623159e308",
                                  &value));  // overflow
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("-1.7976931348623159e308", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToDouble(".", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("", &value));
#ifdef _MSC_VER
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("0x1234", &value));
#endif
#ifdef OS_ANDROID
  // We do the same test done above, because;
  // 1) the tool to exclude ifdef blocks doesn't support 'or' condition, and
  // 2) it's better to keep the test for _MSC_VER in open source code.
  EXPECT_FALSE(NumberUtil::SafeStrToDouble("0x1234", &value));
#endif

  // Test for StringPiece input.
  const char *kString = "0.01 3.1415 double";
  EXPECT_TRUE(NumberUtil::SafeStrToDouble(StringPiece(kString, 4),
                                          &value));
  EXPECT_EQ(0.01, value);
  EXPECT_TRUE(NumberUtil::SafeStrToDouble(StringPiece(kString + 5, 6),
                                          &value));
  EXPECT_EQ(3.1415, value);
  EXPECT_FALSE(NumberUtil::SafeStrToDouble(StringPiece(kString + 12, 6),
                                           &value));
}

TEST(NumberUtilTest, SafeStrToFloat) {
  float value = 1.0;

  EXPECT_TRUE(NumberUtil::SafeStrToFloat("0", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToFloat(" \t\r\n\v\f0 \t\r\n\v\f", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToFloat("-0", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToFloat("1.0e1", &value));
  EXPECT_EQ(10.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToFloat("-5.0e-1", &value));
  EXPECT_EQ(-0.5, value);
  EXPECT_TRUE(NumberUtil::SafeStrToFloat(".0", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToFloat("0.", &value));
  EXPECT_EQ(0.0, value);
  EXPECT_TRUE(NumberUtil::SafeStrToFloat("0.0", &value));
  EXPECT_EQ(0.0, value);
  // GCC accepts hexadecimal number, but VisualC++ does not.
#ifndef _MSC_VER
#ifdef OS_ANDROID
  // strtod in android OS doesn't accept 0x1234, so skip the following test.
#else
  EXPECT_TRUE(NumberUtil::SafeStrToFloat("0x1234", &value));
  EXPECT_EQ(static_cast<float>(0x1234), value);
#endif  // not OS_ANDROID
#endif

  EXPECT_FALSE(NumberUtil::SafeStrToFloat("3.4028236e38",  // overflow
                                          &value));
  EXPECT_FALSE(NumberUtil::SafeStrToFloat("-3.4028236e38", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToFloat("3e", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToFloat(".", &value));
  EXPECT_FALSE(NumberUtil::SafeStrToFloat("", &value));
#ifdef _MSC_VER
  EXPECT_FALSE(NumberUtil::SafeStrToFloat("0x1234", &value));
#endif
#ifdef OS_ANDROID
  // We do the same test done above, because;
  // 1) the tool to exclude ifdef blocks doesn't support 'or' condition, and
  // 2) it's better to keep the test for _MSC_VER in open source code.
  EXPECT_FALSE(NumberUtil::SafeStrToFloat("0x1234", &value));
#endif

  // Test for StringPiece input.
  const char *kString = "0.01 3.14 float";
  EXPECT_TRUE(NumberUtil::SafeStrToFloat(StringPiece(kString, 4),
                                         &value));
  EXPECT_EQ(0.01f, value);
  EXPECT_TRUE(NumberUtil::SafeStrToFloat(StringPiece(kString + 5, 4),
                                         &value));
  EXPECT_EQ(3.14f, value);
  EXPECT_FALSE(NumberUtil::SafeStrToFloat(StringPiece(kString + 10, 5),
                                          &value));
}

TEST(NumberUtilTest, StrToFloat) {
  EXPECT_EQ(0.0, NumberUtil::StrToFloat("0"));
  EXPECT_EQ(0.0, NumberUtil::StrToFloat(" \t\r\n\v\f0 \t\r\n\v\f"));
  EXPECT_EQ(0.0, NumberUtil::StrToFloat("-0"));
  EXPECT_EQ(10.0, NumberUtil::StrToFloat("1.0e1"));
  EXPECT_EQ(-0.5, NumberUtil::StrToFloat("-5.0e-1"));
  EXPECT_EQ(0.0, NumberUtil::StrToFloat(".0"));
  EXPECT_EQ(0.0, NumberUtil::StrToFloat("0."));
  EXPECT_EQ(0.0, NumberUtil::StrToFloat("0.0"));

  // Test for StringPiece input.
  const char *kString = "0.01 3.14";
  EXPECT_EQ(0.01f, NumberUtil::StrToFloat(StringPiece(kString, 4)));
  EXPECT_EQ(3.14f, NumberUtil::StrToFloat(StringPiece(kString + 5, 4)));
}

TEST(NumberUtilTest, IsArabicNumber) {
  EXPECT_FALSE(NumberUtil::IsArabicNumber(""));

  EXPECT_TRUE(NumberUtil::IsArabicNumber("0"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("1"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("2"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("3"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("4"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("5"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("6"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("7"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("8"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("9"));

  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x90"));  // ０
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x91"));  // １
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x92"));  // ２
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x93"));  // ３
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x94"));  // ４
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x95"));  // ５
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x96"));  // ６
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x97"));  // ７
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x98"));  // ８
  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x99"));  // ９

  EXPECT_TRUE(NumberUtil::IsArabicNumber("0123456789"));
  EXPECT_TRUE(NumberUtil::IsArabicNumber("01234567890123456789"));

  EXPECT_TRUE(NumberUtil::IsArabicNumber("\xEF\xBC\x91\xEF\xBC\x90"));  // １０

  EXPECT_FALSE(NumberUtil::IsArabicNumber("abc"));
  EXPECT_FALSE(NumberUtil::IsArabicNumber("\xe5\x8d\x81"));  // 十
  EXPECT_FALSE(NumberUtil::IsArabicNumber("\xe5\x84\x84"));  // 億
  // グーグル
  EXPECT_FALSE(NumberUtil::IsArabicNumber(
      "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab"));
}

TEST(NumberUtilTest, IsDecimalInteger) {
  EXPECT_FALSE(NumberUtil::IsDecimalInteger(""));

  EXPECT_TRUE(NumberUtil::IsDecimalInteger("0"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("1"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("2"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("3"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("4"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("5"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("6"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("7"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("8"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("9"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("0123456789"));
  EXPECT_TRUE(NumberUtil::IsDecimalInteger("01234567890123456789"));

  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x90"));  // ０
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x91"));  // １
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x92"));  // ２
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x93"));  // ３
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x94"));  // ４
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x95"));  // ５
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x96"));  // ６
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x97"));  // ７
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x98"));  // ８
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x99"));  // ９

  // １０
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xEF\xBC\x91\xEF\xBC\x90"));
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xe5\x8d\x81"));  // 十
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("\xe5\x84\x84"));  // 億
  EXPECT_FALSE(NumberUtil::IsDecimalInteger("abc"));
  // グーグル
  EXPECT_FALSE(NumberUtil::IsDecimalInteger(
      "\xe3\x82\xb0\xe3\x83\xbc\xe3\x82\xb0\xe3\x83\xab"));
}

TEST(NumberUtilTest, KanjiNumberToArabicNumber) {
  const char *inputs[] = {
    // "十", "百", "千", "万", "億", "兆", "京"
    "\xe5\x8d\x81", "\xe7\x99\xbe", "\xe5\x8d\x83", "\xe4\xb8\x87",
    "\xe5\x84\x84", "\xe5\x85\x86", "\xe4\xba\xac"};
  const char *expects[] = {"10", "100", "1000", "10000", "100000000",
                           "1000000000000", "10000000000000000"};

  for (size_t i = 0; i < arraysize(inputs); ++i) {
    string arabic;
    NumberUtil::KanjiNumberToArabicNumber(inputs[i], &arabic);
    EXPECT_EQ(expects[i], arabic);
  }
}

TEST(NumberUtilTest, NormalizeNumbers) {
  // An element has input, expected Kanji output, and exepcted Arabic output.
  const char *success_data[][3] = {
    // "一"
    {"\xE4\xB8\x80", "\xE4\xB8\x80", "1"},
    // "九"
    {"\xE4\xB9\x9D", "\xE4\xB9\x9D", "9"},
    // "十"
    {"\xE5\x8D\x81", "\xE5\x8D\x81", "10"},
    // "十五"
    {"\xe5\x8d\x81\xe4\xba\x94", "\xe5\x8d\x81\xe4\xba\x94", "15"},
    // "二十"
    {"\xE4\xBA\x8C\xE5\x8D\x81", "\xE4\xBA\x8C\xE5\x8D\x81", "20"},
    // "三十五"
    {"\xE4\xB8\x89\xE5\x8D\x81\xE4\xBA\x94",
     "\xE4\xB8\x89\xE5\x8D\x81\xE4\xBA\x94", "35"},
    // "百"
    {"\xE7\x99\xBE", "\xE7\x99\xBE", "100"},
    // "二百"
    {"\xE4\xBA\x8C\xE7\x99\xBE", "\xE4\xBA\x8C\xE7\x99\xBE", "200"},
    // "二百十"
    {"\xE4\xBA\x8C\xE7\x99\xBE\xE5\x8D\x81",
     "\xE4\xBA\x8C\xE7\x99\xBE\xE5\x8D\x81", "210"},
    // "二百五十"
    {"\xE4\xBA\x8C\xE7\x99\xBE\xE4\xBA\x94\xE5\x8D\x81",
     "\xE4\xBA\x8C\xE7\x99\xBE\xE4\xBA\x94\xE5\x8D\x81", "250"},
    // "七百七十七"
    {"\xE4\xB8\x83\xE7\x99\xBE\xE4\xB8\x83\xE5\x8D\x81\xE4\xB8\x83",
     "\xE4\xB8\x83\xE7\x99\xBE\xE4\xB8\x83\xE5\x8D\x81\xE4\xB8\x83", "777"},
    // "千"
    {"\xe5\x8d\x83", "\xe5\x8d\x83", "1000"},
    // "一千"
    {"\xE4\xB8\x80\xE5\x8D\x83", "\xE4\xB8\x80\xE5\x8D\x83", "1000"},
    // "八千"
    {"\xE5\x85\xAB\xE5\x8D\x83", "\xE5\x85\xAB\xE5\x8D\x83", "8000"},
    // "八千七百三十九"
    {"\xE5\x85\xAB\xE5\x8D\x83\xE4\xB8\x83\xE7\x99\xBE\xE4\xB8\x89\xE5\x8D\x81"
     "\xE4\xB9\x9D",
     "\xE5\x85\xAB\xE5\x8D\x83\xE4\xB8\x83\xE7\x99\xBE\xE4\xB8\x89\xE5\x8D\x81"
     "\xE4\xB9\x9D", "8739"},
    // "一万二十五"
    {"\xe4\xb8\x80\xe4\xb8\x87\xe4\xba\x8c\xe5\x8d\x81\xe4\xba\x94",
     "\xe4\xb8\x80\xe4\xb8\x87\xe4\xba\x8c\xe5\x8d\x81\xe4\xba\x94", "10025"},
    // 2^64 - 1
    // "千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十五"
    {"\xe5\x8d\x83\xe5\x85\xab\xe7\x99\xbe\xe5\x9b\x9b\xe5\x8d\x81\xe5\x9b\x9b"
     "\xe4\xba\xac\xe5\x85\xad\xe5\x8d\x83\xe4\xb8\x83\xe7\x99\xbe\xe5\x9b\x9b"
     "\xe5\x8d\x81\xe5\x9b\x9b\xe5\x85\x86\xe4\xb8\x83\xe7\x99\xbe\xe4\xb8\x89"
     "\xe5\x8d\x81\xe4\xb8\x83\xe5\x84\x84\xe4\xb9\x9d\xe7\x99\xbe\xe4\xba\x94"
     "\xe5\x8d\x81\xe4\xba\x94\xe4\xb8\x87\xe5\x8d\x83\xe5\x85\xad\xe7\x99\xbe"
     "\xe5\x8d\x81\xe4\xba\x94",
     "\xe5\x8d\x83\xe5\x85\xab\xe7\x99\xbe\xe5\x9b\x9b\xe5\x8d\x81\xe5\x9b\x9b"
     "\xe4\xba\xac\xe5\x85\xad\xe5\x8d\x83\xe4\xb8\x83\xe7\x99\xbe\xe5\x9b\x9b"
     "\xe5\x8d\x81\xe5\x9b\x9b\xe5\x85\x86\xe4\xb8\x83\xe7\x99\xbe\xe4\xb8\x89"
     "\xe5\x8d\x81\xe4\xb8\x83\xe5\x84\x84\xe4\xb9\x9d\xe7\x99\xbe\xe4\xba\x94"
     "\xe5\x8d\x81\xe4\xba\x94\xe4\xb8\x87\xe5\x8d\x83\xe5\x85\xad\xe7\x99\xbe"
     "\xe5\x8d\x81\xe4\xba\x94",
     "18446744073709551615"},
    // "百億百"
    {"\xE7\x99\xBE\xE5\x84\x84\xE7\x99\xBE",
     "\xE7\x99\xBE\xE5\x84\x84\xE7\x99\xBE", "10000000100"},
    // "一千京"
    {"\xe4\xb8\x80\xe5\x8d\x83\xe4\xba\xac",
     "\xe4\xb8\x80\xe5\x8d\x83\xe4\xba\xac", "10000000000000000000"},

    // Old Kanji numbers
    // "零"
    {"\xE9\x9B\xB6", "\xE9\x9B\xB6", "0"},
    // "拾"
    {"\xe6\x8b\xbe", "\xe6\x8b\xbe", "10"},
    // "拾四"
    {"\xe6\x8b\xbe\xe5\x9b\x9b", "\xe6\x8b\xbe\xe5\x9b\x9b", "14"},
    // "廿"
    {"\xE5\xBB\xBF", "\xE5\xBB\xBF", "20"},
    // "廿万廿"
    {"\xe5\xbb\xbf\xe4\xb8\x87\xe5\xbb\xbf",
     "\xe5\xbb\xbf\xe4\xb8\x87\xe5\xbb\xbf", "200020"},
    // "弐拾参"
    {"\xe5\xbc\x90\xe6\x8b\xbe\xe5\x8f\x82",
     "\xe5\xbc\x90\xe6\x8b\xbe\xe5\x8f\x82", "23"},
    // "零弐拾参"
    {"\xe9\x9b\xb6\xe5\xbc\x90\xe6\x8b\xbe\xe5\x8f\x82",
     "\xe9\x9b\xb6\xe5\xbc\x90\xe6\x8b\xbe\xe5\x8f\x82", "23"},

    // Array of Kanji number digits
    // "〇"
    {"0", "\xe3\x80\x87", "0"},
    // "〇〇"
    {"00", "\xe3\x80\x87\xe3\x80\x87", "0"},
    // "二三五"
    {"\xe4\xba\x8c\xe4\xb8\x89\xe4\xba\x94",
     "\xe4\xba\x8c\xe4\xb8\x89\xe4\xba\x94", "235"},
    // "０１２", "〇一二"
    {"\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92",
     "\xe3\x80\x87\xe4\xb8\x80\xe4\xba\x8c", "12"},
    // "二零一一"
    {"\xE4\xBA\x8C\xE9\x9B\xB6\xE4\xB8\x80\xE4\xB8\x80",
     "\xE4\xBA\x8C\xE9\x9B\xB6\xE4\xB8\x80\xE4\xB8\x80", "2011"},

    // Combinations of several types
    // "二三五万四三"
    {"\xe4\xba\x8c\xe4\xb8\x89\xe4\xba\x94\xe4\xb8\x87\xe5\x9b\x9b\xe4\xb8\x89",
     "\xe4\xba\x8c\xe4\xb8\x89\xe4\xba\x94\xe4\xb8\x87\xe5\x9b\x9b\xe4\xb8\x89",
     "2350043"},
    // "二百三五万一"
    {"\xe4\xba\x8c\xe7\x99\xbe\xe4\xb8\x89\xe4\xba\x94\xe4\xb8\x87\xe4\xb8\x80",
     "\xe4\xba\x8c\xe7\x99\xbe\xe4\xb8\x89\xe4\xba\x94\xe4\xb8\x87\xe4\xb8\x80",
     "2350001"},
    // "2十5", "二十五"
    {"2""\xe5\x8d\x81""5", "\xe4\xba\x8c\xe5\x8d\x81\xe4\xba\x94", "25"},
    // "2千四十３"
    {"2""\xe5\x8d\x83\xe5\x9b\x9b\xe5\x8d\x81\xef\xbc\x93",
     // "二千四十三"
     "\xe4\xba\x8c\xe5\x8d\x83\xe5\x9b\x9b\xe5\x8d\x81\xe4\xb8\x89", "2043"},
    // "九０", "九〇"
    {"\xE4\xB9\x9D\xEF\xBC\x90", "\xE4\xB9\x9D\xE3\x80\x87", "90"},
  };

  for (size_t i = 0; i < arraysize(success_data); ++i) {
    string arabic_output = "dummy_text_arabic";
    string kanji_output = "dummy_text_kanji";
    EXPECT_TRUE(NumberUtil::NormalizeNumbers(success_data[i][0], true,
                                             &kanji_output, &arabic_output));
    EXPECT_EQ(success_data[i][1], kanji_output);
    EXPECT_EQ(success_data[i][2], arabic_output);
  }

  // An element has input, expected Kanji output, and exepcted Arabic output.
  const char *success_notrim_data[][3] = {
    // "０１２", "〇一二"
    {"\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92",
     "\xe3\x80\x87\xe4\xb8\x80\xe4\xba\x8c", "012"},
    // "０00", "〇〇〇"
    {"\xef\xbc\x90\x30\x30", "\xe3\x80\x87\xe3\x80\x87\xe3\x80\x87",
     "000"},
    // "００１２", "〇〇一二"
    {"\xef\xbc\x90\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92",
     "\xe3\x80\x87\xe3\x80\x87\xe4\xb8\x80\xe4\xba\x8c", "0012"},
    // "０零０１２", "〇零〇一二"
    {"\xef\xbc\x90\xe9\x9b\xb6\xef\xbc\x90\xef\xbc\x91\xef\xbc\x92",
     "\xe3\x80\x87\xe9\x9b\xb6\xe3\x80\x87\xe4\xb8\x80\xe4\xba\x8c", "00012"},
    // "〇"
    {"0", "\xe3\x80\x87", "0"},
    // "〇〇"
    {"00", "\xe3\x80\x87\xe3\x80\x87", "00"},
  };

  for (size_t i = 0; i < arraysize(success_notrim_data); ++i) {
    string arabic_output = "dummy_text_arabic";
    string kanji_output = "dummy_text_kanji";
    EXPECT_TRUE(NumberUtil::NormalizeNumbers(success_notrim_data[i][0], false,
                                             &kanji_output, &arabic_output));
    EXPECT_EQ(success_notrim_data[i][1], kanji_output);
    EXPECT_EQ(success_notrim_data[i][2], arabic_output);
  }

  // Test data expected to fail
  const char *fail_data[] = {
    // 2^64
    // "千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十六"
    "\xe5\x8d\x83\xe5\x85\xab\xe7\x99\xbe\xe5\x9b\x9b\xe5\x8d\x81\xe5\x9b\x9b"
    "\xe4\xba\xac\xe5\x85\xad\xe5\x8d\x83\xe4\xb8\x83\xe7\x99\xbe\xe5\x9b\x9b"
    "\xe5\x8d\x81\xe5\x9b\x9b\xe5\x85\x86\xe4\xb8\x83\xe7\x99\xbe\xe4\xb8\x89"
    "\xe5\x8d\x81\xe4\xb8\x83\xe5\x84\x84\xe4\xb9\x9d\xe7\x99\xbe\xe4\xba\x94"
    "\xe5\x8d\x81\xe4\xba\x94\xe4\xb8\x87\xe5\x8d\x83\xe5\x85\xad\xe7\x99\xbe"
    "\xe5\x8d\x81\xe5\x85\xad",
    // "てすと"
    "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8",
    // "てすと２"
    "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8\xef\xbc\x92",
    // "一十"
    "\xE4\xB8\x80\xE5\x8D\x81",
    // "一百"
    "\xE4\xB8\x80\xE7\x99\xBE",
    // "万二千三百四十五" (lack of number before "万")
    "\xE4\xB8\x87\xE4\xBA\x8C\xE5\x8D\x83\xE4\xB8\x89\xE7\x99\xBE\xE5\x9B\x9B"
    "\xE5\x8D\x81\xE4\xBA\x94",
    // "三億一京" (large base, "京", after small one, "億")
    "\xE4\xB8\x89\xE5\x84\x84\xE4\xB8\x80\xE4\xBA\xAC",
    // "三百四百" (same base appears twice)
    "\xE4\xB8\x89\xE7\x99\xBE\xE5\x9B\x9B\xE7\x99\xBE",
    // "五億六億" (same base appears twice)
    "\xE4\xBA\x94\xE5\x84\x84\xE5\x85\xAD\xE5\x84\x84",
    // "二十三十" (same base appears twice)
    "\xE4\xBA\x8C\xE5\x8D\x81\xE4\xB8\x89\xE5\x8D\x81",
    // "二十百" (relatively large base "百" after "十")
    "\xE4\xBA\x8C\xE5\x8D\x81\xE7\x99\xBE",
    // "一二三四五六七八九十"
    "\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89\xe5\x9b\x9b\xe4\xba\x94\xe5\x85\xad"
    "\xe4\xb8\x83\xe5\x85\xab\xe4\xb9\x9d\xe5\x8d\x81",
    // "九九八十一"
    "\xE4\xB9\x9D\xE4\xB9\x9D\xE5\x85\xAB\xE5\x8D\x81\xE4\xB8\x80",
  };

  for (size_t i = 0; i < arraysize(fail_data); ++i) {
    string arabic_output, kanji_output;
    EXPECT_FALSE(NumberUtil::NormalizeNumbers(fail_data[i], true,
                                              &kanji_output, &arabic_output));
  }
}

TEST(NumberUtilTest, NormalizeNumbersWithSuffix) {
  {
    // Checks that kanji_output and arabic_output is cleared.
    // "一個"
    const string input = "\xE4\xB8\x80\xE5\x80\x8B";
    string arabic_output = "dummy_text_arabic";
    string kanji_output = "dummy_text_kanji";
    string suffix = "dummy_text_suffix";
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                 &kanji_output,
                                                 &arabic_output,
                                                 &suffix));
    EXPECT_EQ("\xE4\xB8\x80", kanji_output);
    EXPECT_EQ("1", arabic_output);
    // "個"
    EXPECT_EQ("\xE5\x80\x8B", suffix);
  }

  {
    // "一万二十五個"
    const string input = "\xe4\xb8\x80\xe4\xb8\x87\xe4\xba\x8c\xe5\x8d\x81\xe4"
                         "\xba\x94\xE5\x80\x8B";
    string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                 &kanji_output,
                                                 &arabic_output,
                                                 &suffix));
    EXPECT_EQ("\xe4\xb8\x80\xe4\xb8\x87\xe4\xba\x8c\xe5\x8d\x81\xe4\xba\x94",
              kanji_output);
    EXPECT_EQ("10025", arabic_output);
    // "個"
    EXPECT_EQ("\xE5\x80\x8B", suffix);
  }

  {
    // "二百三五万一番目"
    const string input = "\xe4\xba\x8c\xe7\x99\xbe\xe4\xb8\x89\xe4\xba\x94\xe4"
                         "\xb8\x87\xe4\xb8\x80\xE7\x95\xAA\xE7\x9B\xAE";
    string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                 &kanji_output,
                                                 &arabic_output,
                                                 &suffix));
    // "二百三五万一"
    EXPECT_EQ("\xe4\xba\x8c\xe7\x99\xbe\xe4\xb8\x89\xe4\xba\x94\xe4"
              "\xb8\x87\xe4\xb8\x80", kanji_output);
    EXPECT_EQ("2350001", arabic_output);
    // "番目"
    EXPECT_EQ("\xE7\x95\xAA\xE7\x9B\xAE", suffix);
  }

  {
    // "てすと"
    const string input = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8";
    string arabic_output, kanji_output, suffix;
    EXPECT_FALSE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                  &kanji_output,
                                                  &arabic_output,
                                                  &suffix));
  }

  {
    // "てすと２"
    const string input = "\xe3\x81\xa6\xe3\x81\x99\xe3\x81\xa8\xef\xbc\x92";
    string arabic_output, kanji_output, suffix;
    EXPECT_FALSE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                  &kanji_output,
                                                  &arabic_output,
                                                  &suffix));
  }

  // Tests for numbers less than 10.
  {
    // "零セット"
    const string input = "\xE9\x9B\xB6\xE3\x82\xBB\xE3\x83\x83\xE3\x83\x88";
    string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                 &kanji_output,
                                                 &arabic_output,
                                                 &suffix));
    // "零"
    EXPECT_EQ("\xE9\x9B\xB6", kanji_output);
    EXPECT_EQ("0", arabic_output);
    // "セット"
    EXPECT_EQ("\xE3\x82\xBB\xE3\x83\x83\xE3\x83\x88", suffix);
  }

  {
    // "九０ぷよ"
    const string input = "\xE4\xB9\x9D\xEF\xBC\x90\xE3\x81\xB7\xE3\x82\x88";
    string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                 &kanji_output,
                                                 &arabic_output,
                                                 &suffix));
    // "九〇"
    EXPECT_EQ("\xE4\xB9\x9D\xE3\x80\x87",
              kanji_output);
    EXPECT_EQ("90", arabic_output);
    // "ぷよ"
    EXPECT_EQ("\xE3\x81\xB7\xE3\x82\x88", suffix);
  }

  {
    // "三五$"
    const string input = "\xE4\xB8\x89\xE4\xBA\x94$";
    string arabic_output, kanji_output, suffix;
    EXPECT_TRUE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                 &kanji_output,
                                                 &arabic_output,
                                                 &suffix));
    // "三五"
    EXPECT_EQ("\xE4\xB8\x89\xE4\xBA\x94", kanji_output);
    EXPECT_EQ("35", arabic_output);
    EXPECT_EQ("$", suffix);
  }

  {
    // "二十三十に" (same base appears twice)
    const string input = "\xE4\xBA\x8C\xE5\x8D\x81\xE4\xB8\x89\xE5\x8D\x81"
        "\xE3\x81\xAB";
    string arabic_output, kanji_output, suffix;
    EXPECT_FALSE(NumberUtil::NormalizeNumbersWithSuffix(input, true,
                                                  &kanji_output,
                                                  &arabic_output,
                                                  &suffix));
  }
}

TEST(NumberUtilTest, ArabicToWideArabicTest) {
  string arabic;
  vector<NumberUtil::NumberString> output;

  arabic = "12345";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToWideArabic(arabic, &output));
  ASSERT_EQ(output.size(), 2);
  // "１２３４５"
  EXPECT_EQ("\xE4\xB8\x80\xE4\xBA\x8C"
            "\xE4\xB8\x89\xE5\x9B\x9B\xE4\xBA\x94", output[0].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_KANJI_ARABIC, output[0].style);
  // "一二三四五"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x92"
            "\xEF\xBC\x93\xEF\xBC\x94\xEF\xBC\x95", output[1].value);
  EXPECT_EQ(NumberUtil::NumberString::DEFAULT_STYLE, output[1].style);

  arabic = "00123";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToWideArabic(arabic, &output));
  ASSERT_EQ(output.size(), 2);
  // "００１２３"
  EXPECT_EQ("\xE3\x80\x87\xE3\x80\x87"
            "\xE4\xB8\x80\xE4\xBA\x8C\xE4\xB8\x89", output[0].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_KANJI_ARABIC, output[0].style);
  // "〇〇一二三"
  EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93", output[1].value);
  EXPECT_EQ(NumberUtil::NumberString::DEFAULT_STYLE, output[1].style);

  arabic = "abcde";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToWideArabic(arabic, &output));
  EXPECT_EQ(output.size(), 0);

  arabic = "012abc345";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToWideArabic(arabic, &output));
  EXPECT_EQ(output.size(), 0);

  arabic = "0.001";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToWideArabic(arabic, &output));
  EXPECT_EQ(output.size(), 0);

  arabic = "-100";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToWideArabic(arabic, &output));
  EXPECT_EQ(output.size(), 0);

  arabic = "18446744073709551616";  // UINT_MAX + 1
  EXPECT_TRUE(NumberUtil::ArabicToWideArabic(arabic, &output));
  // "１８４４６７４４０７３７０９５５１６１６"
  EXPECT_EQ("\xE4\xB8\x80\xE5\x85\xAB\xE5\x9B\x9B\xE5\x9B\x9B\xE5\x85"
            "\xAD\xE4\xB8\x83\xE5\x9B\x9B\xE5\x9B\x9B\xE3\x80\x87\xE4"
            "\xB8\x83\xE4\xB8\x89\xE4\xB8\x83\xE3\x80\x87\xE4\xB9\x9D"
            "\xE4\xBA\x94\xE4\xBA\x94\xE4\xB8\x80\xE5\x85\xAD\xE4\xB8"
            "\x80\xE5\x85\xAD",
            output[0].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_KANJI_ARABIC, output[0].style);
}

namespace {
const int kMaxCandsInArabicToKanjiTest = 4;
struct ArabicToKanjiTestData {
  const char *input;
  const int expect_num;
  const char *expect_value[kMaxCandsInArabicToKanjiTest];
  const NumberUtil::NumberString::Style
  expect_style[kMaxCandsInArabicToKanjiTest];
};
}  // namespace

// ArabicToKanji TEST
TEST(NumberUtilTest, ArabicToKanjiTest) {
  const NumberUtil::NumberString::Style kOldKanji =
      NumberUtil::NumberString::NUMBER_OLD_KANJI;
  const NumberUtil::NumberString::Style kKanji =
      NumberUtil::NumberString::NUMBER_KANJI;
  const NumberUtil::NumberString::Style kHalfArabicKanji =
      NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_HALFWIDTH;
  const NumberUtil::NumberString::Style kFullArabicKanji =
      NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_FULLWIDTH;

  const ArabicToKanjiTestData kData[] = {
    // "零"
    {"0", 1, {"\xE9\x9B\xB6"}, {kOldKanji}},
    // "零"
    {"00000", 1, {"\xE9\x9B\xB6"}, {kOldKanji}},
    // "二", "弐"
    {"2", 2, {"\xE4\xBA\x8C", "\xE5\xBC\x90"}, {kKanji, kOldKanji}},
    // "壱拾" is needed to avoid mistakes. Please refer http://b/6422355
    // for details.
    // "十", "壱拾", "拾"
    {"10", 3, {"\xE5\x8D\x81", "\xE5\xA3\xB1\xE6\x8B\xBE", "\xE6\x8B\xBE"},
     {kKanji, kOldKanji, kOldKanji}},
    // "百", "壱百"
    {"100", 2, {"\xE7\x99\xBE", "\xE5\xA3\xB1\xE7\x99\xBE"},
     {kKanji, kOldKanji}},
    // "千", "壱阡", "阡"
    {"1000", 3, {"\xE5\x8D\x83", "\xE5\xA3\xB1\xE9\x98\xA1", "\xE9\x98\xA1"},
     {kKanji, kOldKanji, kOldKanji}},
    {"20", 3,
     // "二十", "弐拾", "廿"
     {"\xE4\xBA\x8C\xE5\x8D\x81", "\xE5\xBC\x90\xE6\x8B\xBE", "\xE5\xBB\xBF"},
     {kKanji, kOldKanji, kOldKanji}},
    {"11111", 4,
     // "1万1111"
     {"1" "\xE4\xB8\x87" "1111",
      // "１万１１１１"
      "\xEF\xBC\x91\xE4\xB8\x87\xEF\xBC\x91\xEF\xBC\x91\xEF\xBC\x91"
      "\xEF\xBC\x91",
      // "一万千百十一"
      "\xE4\xB8\x80\xE4\xB8\x87\xE5\x8D\x83\xE7\x99\xBE\xE5\x8D\x81"
      "\xE4\xB8\x80",
      // "壱萬壱阡壱百壱拾壱"
      "\xE5\xA3\xB1\xE8\x90\xAC\xE5\xA3\xB1\xE9\x98\xA1\xE5\xA3\xB1"
      "\xE7\x99\xBE\xE5\xA3\xB1\xE6\x8B\xBE\xE5\xA3\xB1"},
     {kHalfArabicKanji, kFullArabicKanji, kKanji, kOldKanji}},
    {"12345", 4,
     // "1万2345"
     {"1" "\xE4\xB8\x87" "2345",
      // "１万２３４５"
      "\xEF\xBC\x91\xE4\xB8\x87\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x94"
      "\xEF\xBC\x95",
      // "一万二千三百四十五"
      "\xE4\xB8\x80\xE4\xB8\x87\xE4\xBA\x8C\xE5\x8D\x83\xE4\xB8\x89"
      "\xE7\x99\xBE\xE5\x9B\x9B\xE5\x8D\x81\xE4\xBA\x94",
      // "壱萬弐阡参百四拾五"
      "\xE5\xA3\xB1\xE8\x90\xAC\xE5\xBC\x90\xE9\x98\xA1\xE5\x8F\x82"
      "\xE7\x99\xBE\xE5\x9B\x9B\xE6\x8B\xBE\xE4\xBA\x94"},
     {kHalfArabicKanji, kFullArabicKanji, kKanji, kOldKanji}},
    {"100002345", 4,
     // "1億2345"
     {"1" "\xE5\x84\x84" "2345",
      // "１億２３４５"
      "\xEF\xBC\x91\xE5\x84\x84\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x94"
      "\xEF\xBC\x95",
      // "一億二千三百四十五"
      "\xE4\xB8\x80\xE5\x84\x84\xE4\xBA\x8C\xE5\x8D\x83\xE4\xB8\x89"
      "\xE7\x99\xBE\xE5\x9B\x9B\xE5\x8D\x81\xE4\xBA\x94",
      // "壱億弐阡参百四拾五"
      "\xE5\xA3\xB1\xE5\x84\x84\xE5\xBC\x90\xE9\x98\xA1\xE5\x8F\x82"
      "\xE7\x99\xBE\xE5\x9B\x9B\xE6\x8B\xBE\xE4\xBA\x94"},
     {kHalfArabicKanji, kFullArabicKanji, kKanji, kOldKanji}},
    {"18446744073709551615", 4,
     // "1844京6744兆737億955万1615"
     {"1844" "\xE4\xBA\xAC" "6744" "\xE5\x85\x86" "737" "\xE5\x84\x84"
      "955" "\xE4\xB8\x87" "1615",
      // "１８４４京６７４４兆７３７億９５５万１６１５"
      "\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x94\xEF\xBC\x94\xE4\xBA\xAC"
      "\xEF\xBC\x96\xEF\xBC\x97\xEF\xBC\x94\xEF\xBC\x94\xE5\x85\x86"
      "\xEF\xBC\x97\xEF\xBC\x93\xEF\xBC\x97\xE5\x84\x84\xEF\xBC\x99"
      "\xEF\xBC\x95\xEF\xBC\x95\xE4\xB8\x87\xEF\xBC\x91\xEF\xBC\x96"
      "\xEF\xBC\x91\xEF\xBC\x95",
      // "千八百四十四京六千七百四十四兆七百三十七億九百五十五万千六百十五"
      "\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5\x9B\x9B\xE5\x8D\x81"
      "\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85\xAD\xE5\x8D\x83\xE4\xB8\x83"
      "\xE7\x99\xBE\xE5\x9B\x9B\xE5\x8D\x81\xE5\x9B\x9B\xE5\x85\x86"
      "\xE4\xB8\x83\xE7\x99\xBE\xE4\xB8\x89\xE5\x8D\x81\xE4\xB8\x83"
      "\xE5\x84\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94\xE5\x8D\x81"
      "\xE4\xBA\x94\xE4\xB8\x87\xE5\x8D\x83\xE5\x85\xAD\xE7\x99\xBE"
      "\xE5\x8D\x81\xE4\xBA\x94",
      // "壱阡八百四拾四京六阡七百四拾四兆七百参拾七億九百五拾五萬"
      // "壱阡六百壱拾五"
      "\xE5\xA3\xB1\xE9\x98\xA1\xE5\x85\xAB\xE7\x99\xBE\xE5\x9B\x9B"
      "\xE6\x8B\xBE\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85\xAD\xE9\x98\xA1"
      "\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B\xE6\x8B\xBE\xE5\x9B\x9B"
      "\xE5\x85\x86\xE4\xB8\x83\xE7\x99\xBE\xE5\x8F\x82\xE6\x8B\xBE"
      "\xE4\xB8\x83\xE5\x84\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94"
      "\xE6\x8B\xBE\xE4\xBA\x94\xE8\x90\xAC\xE5\xA3\xB1\xE9\x98\xA1"
      "\xE5\x85\xAD\xE7\x99\xBE\xE5\xA3\xB1\xE6\x8B\xBE\xE4\xBA\x94"},
     {kHalfArabicKanji, kFullArabicKanji, kKanji, kOldKanji}},
  };

  for (size_t i = 0; i < arraysize(kData); ++i) {
    vector<NumberUtil::NumberString> output;
    ASSERT_LE(kData[i].expect_num, kMaxCandsInArabicToKanjiTest);
    EXPECT_TRUE(NumberUtil::ArabicToKanji(kData[i].input, &output));
    ASSERT_EQ(output.size(), kData[i].expect_num)
        << "on conversion of '" << kData[i].input << "'";
    for (int j = 0; j < kData[i].expect_num; ++j) {
      EXPECT_EQ(kData[i].expect_value[j], output[j].value)
          << "input : " << kData[i].input << "\nj : " << j;
      EXPECT_EQ(kData[i].expect_style[j], output[j].style)
          << "input : " << kData[i].input << "\nj : " << j;
    }
  }

  const char *kFailInputs[] = {
    "asf56789", "0.001", "-100", "123456789012345678901"
  };
  for (size_t i = 0; i < arraysize(kFailInputs); ++i) {
    vector<NumberUtil::NumberString> output;
    EXPECT_FALSE(NumberUtil::ArabicToKanji(kFailInputs[i], &output));
    ASSERT_EQ(output.size(), 0) << "input : " << kFailInputs[i];
  }
}

// ArabicToSeparatedArabic TEST
TEST(NumberUtilTest, ArabicToSeparatedArabicTest) {
  string arabic;
  vector<NumberUtil::NumberString> output;

  // Test data expected to succeed
  const char* kSuccess[][3] = {
    // "４"
    {"4", "4", "\xEF\xBC\x94"},
    // "１２３，４５６，７８９"
    {"123456789", "123,456,789", "\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93"
     "\xEF\xBC\x8C\xEF\xBC\x94\xEF\xBC\x95\xEF\xBC\x96\xEF\xBC\x8C"
     "\xEF\xBC\x97\xEF\xBC\x98\xEF\xBC\x99"},
    // "１，２３４，５６７．８９"
    {"1234567.89", "1,234,567.89", "\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x92"
     "\xEF\xBC\x93\xEF\xBC\x94\xEF\xBC\x8C\xEF\xBC\x95\xEF\xBC\x96"
     "\xEF\xBC\x97\xEF\xBC\x8E\xEF\xBC\x98\xEF\xBC\x99"},
    // UINT64_MAX + 1
    {"18446744073709551616", "18,446,744,073,709,551,616", NULL},
  };

  for (size_t i = 0; i < arraysize(kSuccess); ++i) {
    arabic = kSuccess[i][0];
    output.clear();
    EXPECT_TRUE(NumberUtil::ArabicToSeparatedArabic(arabic, &output));
    ASSERT_EQ(output.size(), 2);
    EXPECT_EQ(kSuccess[i][1], output[0].value);
    EXPECT_EQ(NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH,
              output[0].style);
    if (kSuccess[i][2]) {
      EXPECT_EQ(kSuccess[i][2], output[1].value);
      EXPECT_EQ(NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH,
                output[1].style);
    }
  }

  // Test data expected to fail
  const char* kFail[] = {
    "0123456789", "asdf0123456789", "0.001", "-100",
  };

  for (size_t i = 0; i < arraysize(kFail); ++i) {
    arabic = kFail[i];
    output.clear();
    EXPECT_FALSE(NumberUtil::ArabicToSeparatedArabic(arabic, &output));
    ASSERT_EQ(output.size(), 0);
  }
}

// ArabicToOtherForms
TEST(NumberUtilTest, ArabicToOtherFormsTest) {
  string arabic;
  vector<NumberUtil::NumberString> output;

  arabic = "5";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 3);

  // "Ⅴ"
  EXPECT_EQ("\xE2\x85\xA4", output[0].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_ROMAN_CAPITAL, output[0].style);

  // "ⅴ"
  EXPECT_EQ("\xE2\x85\xB4", output[1].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_ROMAN_SMALL, output[1].style);

  // "⑤"
  EXPECT_EQ("\xE2\x91\xA4", output[2].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_CIRCLED, output[2].style);

  arabic = "0123456789";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "asdf0123456789";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "0.001";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "-100";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "18446744073709551616";  // UINT64_MAX + 1
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherForms(arabic, &output));
}

// ArabicToOtherRadixes
TEST(NumberUtilTest, ArabicToOtherRadixesTest) {
  string arabic;
  vector<NumberUtil::NumberString> output;

  arabic = "1";
  output.clear();
  // "1" is "1" in any radixes.
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));

  arabic = "2";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 1);

  arabic = "8";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 2);
  EXPECT_EQ("010", output[0].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_OCT, output[0].style);
  EXPECT_EQ("0b1000", output[1].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_BIN, output[1].style);

  arabic = "16";
  output.clear();
  EXPECT_TRUE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 3);
  EXPECT_EQ("0x10", output[0].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_HEX, output[0].style);
  EXPECT_EQ("020", output[1].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_OCT, output[1].style);
  EXPECT_EQ("0b10000", output[2].value);
  EXPECT_EQ(NumberUtil::NumberString::NUMBER_BIN, output[2].style);

  arabic = "asdf0123456789";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "0.001";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "-100";
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
  ASSERT_EQ(output.size(), 0);

  arabic = "18446744073709551616";  // UINT64_MAX + 1
  output.clear();
  EXPECT_FALSE(NumberUtil::ArabicToOtherRadixes(arabic, &output));
}

}  // namespace
}  // namespace mozc
