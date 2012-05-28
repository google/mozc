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

#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "net/jsonpath.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace net {

class JsonPathTest : public testing::Test {
 public:
  virtual void SetUp() {}

  string Parse(const string &json, const string &jsonpath) {
    Json::Reader reader;
    Json::Value root;
    EXPECT_TRUE(reader.parse(json, root))
        << reader.getFormattedErrorMessages()
        << " "
        << json;
    vector<const Json::Value *> output;
    if (!JsonPath::Parse(root, jsonpath, &output)) {
      return "ERROR";
    }
    string result;
    for (size_t i = 0; i < output.size(); ++i) {
      const Json::Value *value = output[i];
      if (!value->isNull() && !value->isArray() && !value->isObject()) {
        if (!result.empty()) {
          result.append(" ");
        }
        result.append(value->asString());
      }
    }
    return result;
  }

  virtual void TearDown() {}
};

TEST_F(JsonPathTest, BasicTest) {
  const char kInput[] = "{"
      "\"books\": ["
      "    {\"title\":\"foo1\",\"author\":\"bar1\"},"
      "    {\"title\":\"foo2\",\"author\":\"bar2\"},"
      "    {\"title\":\"foo3\",\"author\":\"bar3\"}"
      "],"
      "\"papers\": {"
      "   \"year\": 2011,"
      "   \"month\": 11,"
      "   \"date\": 10,"
      "   \"list\" : ["
      "     {\"title\":\"Foo1\",\"author\":\"Bar2\"},"
      "     {\"title\":\"Foo2\",\"author\":\"Bar2\"},"
      "     {\"title\":\"Foo3\",\"author\":\"Bar3\"}"
      "   ]"
      "},"
      "\"title\": \"hello\""
      "}";

  // Parse error
  EXPECT_EQ("ERROR", Parse(kInput, ""));
  EXPECT_EQ("ERROR", Parse(kInput, "$"));
  EXPECT_EQ("ERROR", Parse(kInput, "$.foo...bar"));
  EXPECT_EQ("ERROR", Parse(kInput, "$."));
  EXPECT_EQ("ERROR", Parse(kInput, "."));
  EXPECT_EQ("ERROR", Parse(kInput, "$.."));
  EXPECT_EQ("ERROR", Parse(kInput, "$.*."));
  EXPECT_EQ("ERROR", Parse(kInput, "$.foo]["));
  EXPECT_EQ("ERROR", Parse(kInput, "$.foo[."));
  EXPECT_EQ("ERROR", Parse(kInput, "$.foo[]"));
  EXPECT_EQ("ERROR", Parse(kInput, "$.foo[10]]"));

  // Normal expression
  EXPECT_EQ("hello", Parse(kInput, "$.title"));
  EXPECT_EQ("", Parse(kInput, "$.books.title"));
  EXPECT_EQ("", Parse(kInput, "$.books[0]"));
  EXPECT_EQ("foo1", Parse(kInput, "$.books[0].title"));
  EXPECT_EQ("foo1 foo2 foo3", Parse(kInput, "$.books[*].title"));
  EXPECT_EQ("foo2", Parse(kInput, "$.books[1].title"));
  EXPECT_EQ("foo2", Parse(kInput, "$.*[1].title"));
  EXPECT_EQ("foo1 foo2 foo3", Parse(kInput, "$.*[*].title"));
  // Object names are fetched by alphabetical order.
  EXPECT_EQ("bar1 foo1", Parse(kInput, "$.books[0].*"));
  EXPECT_EQ("bar1 foo1 bar3 foo3", Parse(kInput, "$.books[0,2].*"));
  EXPECT_EQ("bar3 foo3", Parse(kInput, "$.books[-1].*"));

  EXPECT_EQ("Foo1", Parse(kInput, "$.papers.list[0].title"));
  EXPECT_EQ("Foo1 Foo2 Foo3", Parse(kInput, "$.papers.list[*].title"));
  EXPECT_EQ("Foo1", Parse(kInput, "$.papers.*[0].title"));
  EXPECT_EQ("10 11 2011", Parse(kInput, "$.papers.*"));
  EXPECT_EQ("10", Parse(kInput, "$.papers.date"));

  EXPECT_EQ("hello foo1 foo2 foo3 Foo1 Foo2 Foo3",
            Parse(kInput, "$..title"));
  EXPECT_EQ("Foo1 Foo2 Foo3",
            Parse(kInput, "$.papers..title"));
  EXPECT_EQ("bar1 foo1 bar2 foo2 bar3 foo3",
            Parse(kInput, "$.books..*"));

  // Bracket expression
  EXPECT_EQ("hello", Parse(kInput, "$.['title']"));
  EXPECT_EQ("", Parse(kInput, "$['books']['title']"));
  EXPECT_EQ("", Parse(kInput, "$['books'][0]"));
  EXPECT_EQ("foo1", Parse(kInput, "$['books'][0]['title']"));
  EXPECT_EQ("foo1 foo2 foo3", Parse(kInput, "$['books'][*]['title']"));
  EXPECT_EQ("foo2", Parse(kInput, "$['books'][1]['title']"));
  EXPECT_EQ("foo2", Parse(kInput, "$[*][1]['title']"));
  EXPECT_EQ("foo1 foo2 foo3", Parse(kInput, "$[*][*]['title']"));
  // Object names are fetched by alphabetical order.
  EXPECT_EQ("bar1 foo1", Parse(kInput, "$['books'][0][*]"));
  EXPECT_EQ("bar1 foo1 bar3 foo3", Parse(kInput, "$['books'][0,2][*]"));
  EXPECT_EQ("bar3 foo3", Parse(kInput, "$['books'][-1][*]"));

  EXPECT_EQ("Foo1", Parse(kInput, "$['papers']['list'][0]['title']"));
  EXPECT_EQ("Foo1 Foo2 Foo3",
            Parse(kInput, "$['papers']['list'][*]['title']"));
  EXPECT_EQ("Foo1", Parse(kInput, "$['papers'][*][0]['title']"));
  EXPECT_EQ("10 11 2011", Parse(kInput, "$['papers'][*]"));
  EXPECT_EQ("10", Parse(kInput, "$['papers']['date']"));

  EXPECT_EQ("hello foo1 foo2 foo3 Foo1 Foo2 Foo3",
            Parse(kInput, "$..['title']"));
  EXPECT_EQ("Foo1 Foo2 Foo3",
            Parse(kInput, "$['papers']..['title']"));
  EXPECT_EQ("bar1 foo1 bar2 foo2 bar3 foo3",
            Parse(kInput, "$['books']..[*]"));

  EXPECT_EQ("2011 11 10",
            Parse(kInput, "$['papers']['year','month','date']"));
  EXPECT_EQ("11",
            Parse(kInput, "$['papers']['foo','month','bar']"));
}

TEST_F(JsonPathTest, SliceTest) {
  const char kInput[] = "{ \"a\": [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ] }";
  EXPECT_EQ("ERROR", Parse(kInput, "$.a[]"));
  EXPECT_EQ("0", Parse(kInput, "$.a[0]"));
  EXPECT_EQ("3", Parse(kInput, "$.a[3]"));
  EXPECT_EQ("", Parse(kInput, "$.a[100]"));
  EXPECT_EQ("", Parse(kInput, "$.a[-100]"));
  EXPECT_EQ("0", Parse(kInput, "$.a[0:1]"));
  EXPECT_EQ("0 1", Parse(kInput, "$.a[0:2]"));
  EXPECT_EQ("5 6", Parse(kInput, "$.a[5:7]"));
  EXPECT_EQ("9", Parse(kInput, "$.a[-1]"));
  EXPECT_EQ("0 1 2 3 4 5 6 7 8", Parse(kInput, "$.a[:-1]"));
  EXPECT_EQ("9", Parse(kInput, "$.a[-1:]"));
  EXPECT_EQ("7 8 9", Parse(kInput, "$.a[-3:]"));
  EXPECT_EQ("6 7", Parse(kInput, "$.a[6:-2]"));
  EXPECT_EQ("", Parse(kInput, "$.a[5:2]"));
  EXPECT_EQ("", Parse(kInput, "$.a[-2:-3]"));
  EXPECT_EQ("0 2", Parse(kInput, "$.a[0:4:2]"));
  EXPECT_EQ("8 6 4", Parse(kInput, "$.a[8:2:-2]"));
  EXPECT_EQ("", Parse(kInput, "$.a[0:4:-2]"));
  EXPECT_EQ("4 3", Parse(kInput, "$.a[4:2:-1]"));
  EXPECT_EQ("", Parse(kInput, "$.a[4:2:1]"));
  EXPECT_EQ("4 5 6", Parse(kInput, "$.a[4:-3:1]"));
  EXPECT_EQ("0 1 2 3 4 2 3 4 5 6", Parse(kInput, "$.a[0:3,3:5,2:-3]"));
}
}  // net
}  // mozc
