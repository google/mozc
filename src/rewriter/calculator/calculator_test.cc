// Copyright 2010-2013, Google Inc.
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

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

#include "base/base.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "rewriter/calculator/calculator_interface.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_srcdir);

namespace mozc {
namespace {

const char kTestDir[] = "data/test/calculator/";

// Runs calculation with |expression| and compares the result and |expect|.
void VerifyCalculation(const CalculatorInterface *calculator,
                       const string &expression,
                       const string &expected) {
  string result;
  EXPECT_TRUE(calculator->CalculateString(expression, &result))
      << expression << "  expected = " << expected;
  const double result_val = atof(result.c_str());
  const double expected_val = atof(expected.c_str());
  const double err = fabs(result_val - expected_val);

  EXPECT_DOUBLE_EQ(expected_val, result_val)
      << "comparison: " << result_val << " vs " << expected_val << endl
      << "error: " << err << endl
      << "expr = " << expression << endl
      << "result = " << result;
}

// Runs calculation without any verification.
// For smoke test.
// This method is not used on some build environment, so we need to suppress
// warnings.
MOZC_CLANG_PUSH_WARNING();
MOZC_CLANG_DISABLE_WARNING(unused-function);
void RunCalculation(const CalculatorInterface *calculator,
                    const string &expression) {
  string result;
  calculator->CalculateString(expression, &result);
}
MOZC_CLANG_POP_WARNING();

// Runs calculation and compare results in PRINTED string.
void VerifyCalculationInString(const CalculatorInterface *calculator,
                               const string &expression,
                               const string &expected) {
  string result;
  EXPECT_TRUE(calculator->CalculateString(expression, &result))
      << expression << "  expected = " << expected;
  EXPECT_EQ(expected, result)
      << "expr = " << expression << endl;
}

// Tries to calculate |wrong_key| and returns true if it fails.
void VerifyRejection(const CalculatorInterface *calculator,
                     const string &wrong_key) {
  string result;
  EXPECT_FALSE(calculator->CalculateString(wrong_key, &result))
      << "expression: " << wrong_key << endl;
}

}  // anonymous namespace

TEST(CalculatorTest, BasicTest) {
  CalculatorInterface *calculator = CalculatorFactory::GetCalculator();

  // These are not expressions
  // apparently
  VerifyRejection(calculator, "test");
  // Expressoin must be ended with equal '='.
  VerifyRejection(calculator, "5+4");
  // Expression must include at least one operator other than parentheses.
  VerifyRejection(calculator, "111=");
  VerifyRejection(calculator, "(5)=");
  // Expression must include at least one number.
  VerifyRejection(calculator, "()=");

  // Test for each operators
  VerifyCalculation(calculator, "38+2.5=", "40.5");
  VerifyCalculation(calculator, "5.5-21=", "-15.5");
  VerifyCalculation(calculator, "4*2.1=", "8.4");
  VerifyCalculation(calculator, "8/2=", "4");
  // "15・3="
  VerifyCalculation(calculator, "15\xE3\x83\xBB""3=", "5");
  VerifyCalculation(calculator, "100%6=", "4");
  VerifyCalculation(calculator, "2^10=", "1024");
  VerifyCalculation(calculator, "4*-2=", "-8");
  VerifyCalculation(calculator, "-10.3+3.5=", "-6.8");

  // Full width cases (some operators may appear as full width character).
  // "１２３４５＋６７８９０＝"
  VerifyCalculation(calculator,
                    "\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x94\xEF\xBC"
                    "\x95\xEF\xBC\x8B\xEF\xBC\x96\xEF\xBC\x97\xEF\xBC\x98\xEF"
                    "\xBC\x99\xEF\xBC\x90\xEF\xBC\x9D",
                    "80235");
  // "5−1="
  VerifyCalculation(calculator, "5\xE2\x88\x92""1=", "4");
  // "-ー3+5="
  VerifyCalculation(calculator, "-\xE3\x83\xBC""3+5=", "8");
  // "1．5＊2="
  VerifyCalculation(calculator, "1\xEF\xBC\x8E""5\xEF\xBC\x8A""2=", "3");
  // "10／2="
  VerifyCalculation(calculator, "10\xEF\xBC\x8F""2=", "5");
  // "2＾ー2="
  VerifyCalculation(calculator, "2\xEF\xBC\xBE\xE3\x83\xBC""2=", "0.25");
  // "13％3="
  VerifyCalculation(calculator, "13\xEF\xBC\x85""3=", "1");
  // "（1+1）*2="
  VerifyCalculation(calculator,
                                "\xEF\xBC\x88""1+1\xEF\xBC\x89*2=",
                                "4");

  // Expressions with more than one operator.
  VerifyCalculation(calculator, "(1+2)-4=", "-1");
  VerifyCalculation(calculator, "5*(2+3)=", "25");
  VerifyCalculation(calculator, "(70-((3+2)*4))%8=", "2");

  // Issue 3082576: 7472.4 - 7465.6 = 6.7999999999993 is not expected.
  VerifyCalculationInString(calculator, "7472.4-7465.6=", "6.8");
}

// Test large number of queries.  Test data is located at
// data/test/calculator/testset.txt.
// In this file, each test case is written in one line in the format
// "expression=answer".  Answer is suppressed if the expression is invalid,
// i.e. it is a false test.
TEST(CalculatorTest, StressTest) {
  const string filename = FileUtil::JoinPath(FLAGS_test_srcdir,
                                             string(kTestDir) + "testset.txt");
  EXPECT_TRUE(FileUtil::FileExists(filename)) << "Could not read: " << filename;

  CalculatorInterface *calculator = CalculatorFactory::GetCalculator();

  ifstream finput(filename.c_str());
  string line;
  int lineno = 0;
  while (getline(finput, line)) {
    ++lineno;

    // |line| is of format "expression=answer".
    const size_t index_of_equal = line.find('=');
    DCHECK(index_of_equal != string::npos);
    const size_t query_length = index_of_equal + 1;
    const string query(line, 0, query_length);

#if defined(OS_ANDROID) && defined(__i386__)
    // StressTest seems to be overkill. Many false-positives makes maintainance
    // harder.
    // So it might be better to use this test case as "smoke test".
    // But for now we do it only for x86 Android, which faces test failure.
    RunCalculation(calculator, query);
#else  // OS_ANDROID && __i386__
    if (line.size() == query_length) {
      // False test
      VerifyRejection(calculator, line);
      continue;
    }
    const string answer(line, query_length);
    VerifyCalculation(calculator, query, answer);
#endif  // OS_ANDROID && __i386__
  }
  LOG(INFO) << "done " << lineno << " tests from " << filename << endl;
}

}  // namespace mozc
